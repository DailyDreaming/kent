#!/usr/bin/env perl

# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/doDbSnp.pl instead.

use Getopt::Long;
use warnings;
use strict;
use FindBin qw($Bin);
use lib "$Bin";
use HgAutomate;
use HgRemoteScript;
use HgStepManager;


my $stepper = new HgStepManager(
    [ { name => 'download',   func => \&downloadFiles },
#*** translate needs separate step for loading the local dbSNP db, so we don't have to 
#*** drop the db every time we need to -continue translate.
      { name => 'translate',  func => \&translateData },
      { name => 'load',       func => \&loadTables },
    ]
			       );

my $base = $0;
$base =~ s/^(.*\/)?//;

# Hardcoded commands / paths:
my $wget = 'wget --timestamping --no-verbose';
my $ftpShared = 'ftp://ftp.ncbi.nih.gov/snp/database/shared_data';
my $ftpSnpDb = 'ftp://ftp.ncbi.nih.gov/snp/database/organism_data/$orgDir';
my $ftpOrgSchema = 'ftp://ftp.ncbi.nlm.nih.gov/snp/database/organism_schema/$orgDir';
my $ftpSharedSchema = 'ftp://ftp.ncbi.nlm.nih.gov/snp/database/shared_schema';
my $dbSnpRoot = '/hive/data/outside/dbSNP';
# Some ContigInfo columns -- if there are any changes, garbage results
# should trigger errors.
my $ctgIdCol = 1;
my $contigAccCol = 3;
my $groupLabelCol = 12;
# This is for an hg19-specific mess: we unintentionally used a mitochondrion
# that is not the revised Cambridge Reference Sequence (rCRS) that is the
# standard.  dbSNP produces rCRS coords, which must be translated to our
# chrM.  Since there are small indels, liftOver is required.  See snp131
# section of makeDb/doc/hg19.txt for how this chain file was generated.
my $hg19MitoLiftOver = '/hive/data/outside/dbSNP/131/human/NC_012920ToChrM.over.chain';

# Option defaults:
my $dbHost = 'hgwdev';
my $workhorse = 'hgwdev';
my $fileServer = 'hgwdev';

sub usage {
  # Usage / help / self-documentation:
  my ($status, $detailed) = @_;
  # Basic help (for incorrect usage):
  print STDERR "
usage: $base config.ra
options:
    -buildDir dir         Use dir instead of default $dbSnpRoot/<build>/
";
  print STDERR $stepper->getOptionHelp();
  print STDERR &HgAutomate::getCommonOptionHelp('dbHost' => $dbHost,
						'workhorse' => '$workhorse',
					        'fileServer' => '$fileServer');
  print STDERR "
Automates the first steps of building a genome database:
    download:   FTP dbSNP database dump files from NCBI.  Examine them
                for assembly IDs and halt if there is ambiguity that
                needs to be resolved by developer.
    translate:  Work the dbSNP tables and fasta headers into our local
                BED+ representation.
                Note: the workhorse machine used for this step must have mysql.
    load:       Load snp* tables into our database.
To see detailed information about what should appear in config.ra,
run \"$base -help\".
";
  # Detailed help (-help):
  print STDERR "
-----------------------------------------------------------------------------
Required config.ra settings:

db xxxYyyN
  - The UCSC database name and assembly identifier.  xxx is the first three
    letters of the genus and Yyy is the first three letters of the species.
    N is the sequence number of this species' build at UCSC.  For some
    species that we have been processing since before this convention, the
    pattern is xyN.

build NNN
  - The dbSNP build identifier (e.g. 132 in November 2010).

buildAssembly NN_N
  - The assembly identifier used in dbSNP's table/dumpfile names (e.g. 37_1
    for human 132).

orgDir XXXXX_NNNNN
  - Subdirectory of ftp://ftp.ncbi.nlm.nih.gov/snp/database/organism_data/
    e.g. human_9606 (commonName_taxonomyId)

-----------------------------------------------------------------------------
Conditionally required config.ra settings:

1. Required when there is more than one assembly label in dbSNP's ContigInfo
   table, ${groupLabelCol}th column (group_label).  If there is more than one, this script
   will halt at the end of the download step and display a list of labels.
   The developer needs to figure out which label(s) are the ones we need.
   For most non-human species, there is only one so this isn't needed.
   However, for human, as of build 132, there are four (CRA_TCAGchr7v2,
   Celera, GRCh37, HuRef) and the only one we want is GRCh37.  There can
   be multiple labels, e.g. when we include haplo-chroms that don't share
   the same label as the main assembly (see makeDb/doc/hg18.txt's snp130
   process).

refAssemblyLabel XXXXX
  - The assembly label that appears in the ContigInfo table dump,
    ${groupLabelCol}th column (group_label) and corresponds to the sequences included
    in UCSC's database.  If there is more than one label, this can be a
    comma-separated list.
    *** This script assumes that these labels are not numeric and distinct
    *** enough to never match inappropriate columns' values.
    *** If this is not the case you'll need to edit this script.


2. Required when we need to liftUp NCBI's contigs to chroms:

liftUp FFFFF
  - Absolute path to the liftUp file that maps NCBI's contig names to
    UCSC chroms.

3. Required when ContigInfo (after filtering by refAssemblyLabel if specified)
   contains sequences that are not in UCSC's assembly (liftUp if specified,
   or chrom.sizes.

ignoreDbSnpContigs REGEX
  - Regular expression of dbSNP contigs that cannot be liftUp'd to UCSC
    sequences, e.g. if we got one version of randoms from Baylor but NCBI
    got another.  The regular expression is interpreted by egrep -vw.
    If ContigInfo's ${groupLabelCol}th (group_label) column includes a label that
    covers all such contigs *and* does not cover any contigs that we need
    to keep, it is better to omit that label from refAssemblyLabel (above).
    *** This script assumes that these contig accessions are not numeric and
    *** distinct enough to never match other columns' values.
    *** If this is not the case you'll need to edit this script.

" if ($detailed);
  print STDERR "\n";
  exit $status;
} # usage


# Globals:
# Command line argument:
my $CONFIG;
# Command line option vars:
use vars @HgAutomate::commonOptionVars;
use vars @HgStepManager::optionVars;
use vars qw/
    $opt_buildDir
    /;
# Required config parameters:
my ($db, $build, $buildAssembly, $orgDir);
# Conditionally required config parameters:
my ($refAssemblyLabel, $liftUp, $ignoreDbSnpContigs);
# Other globals:
my ($buildDir, $commonName, $assemblyLabelFile, $endNotes);
# These dbSNP table/file names vary by build and assembly but
# table desc is stable:
my ($ContigInfo, $ContigLoc, $ContigLocusId, $MapInfo);

sub checkOptions {
  # Make sure command line options are valid/supported.
  my $ok = GetOptions(@HgStepManager::optionSpec,
		      'buildDir=s',
		      @HgAutomate::commonOptionSpec,
		     );
  &usage(1) if (!$ok);
  &usage(0, 1) if ($opt_help);
  &HgAutomate::processCommonOptions();
  my $err = $stepper->processOptions();
  usage(1) if ($err);
  $dbHost = $opt_dbHost if (defined $opt_dbHost);
  $fileServer = $opt_fileServer if ($opt_fileServer);
  $workhorse = $opt_workhorse if ($opt_workhorse);
} # checkOptions

#*** FIXME --- begin code lifted from makeGenomeDb.pl -- need to libify

sub parseConfig {
  # Parse config.ra file and return hashRef of var->val.
  my ($configFile) = @_;
  my %config = ();
  my $fh = &HgAutomate::mustOpen($configFile);
  while (<$fh>) {
    next if (/^\s*#/ || /^\s*$/);
    if (/^\s*(\w+)\s*(.*)$/) {
      my ($var, $val) = ($1, $2);
      if (! exists $config{$var}) {
	$config{$var} = $val;
      } else {
	die "Duplicate definition for $var line $. of config file $configFile.\n";
      }
    } else {
      die "Can't parse line $. of config file $configFile:\n$_\n";
    }
  }
  close($fh);
  return \%config;
} # parseConfig

sub requireVar {
  # Ensure that var is in %config and return its value.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var}
    || die "Error: $CONFIG is missing required variable \"$var\".\n" .
      "For a detailed list of required variables, run \"$base -help\".\n";
  delete $config->{$var};
  return $val;
} # requireVar

sub optionalVar {
  # If var has a value in %config, return it.
  # Remove it from %config so we can check for unrecognized contents.
  my ($var, $config) = @_;
  my $val = $config->{$var};
  delete $config->{$var} if ($val);
  return $val;
} # optionalVar

sub checkConfigEmpty {
  # If config hashRef has any vars not removed by requireVar() or
  # optionalVar(), die with error -- config might have misspelled var.
  my ($config, $base, $configFile) = (@_);
  my @stragglers = sort keys %{$config};
  if (scalar(@stragglers) > 0) {
    die "Error: config file $configFile has unrecognized variables:\n" .
      "    " . join(", ", @stragglers) . "\n" .
      "For a detailed list of supported variables, run \"$base -help\".\n";
  }
} # checkConfigEmpty

#*** FIXME --- end code lifted from makeGenomeDb.pl -- need to libify

sub checkConfig {
  # Make sure config hashRef contains the required variables.
  my ($config) = (@_);
  # Required variables.
  $db = &requireVar('db', $config);
  $build = &requireVar('build', $config);
  $buildAssembly = &requireVar('buildAssembly', $config);
  $orgDir = &requireVar('orgDir', $config);
  # Conditionally required variables -- optional here, but they might be
  # required later on in some cases.
  $refAssemblyLabel = &optionalVar('refAssemblyLabel', $config);
  $liftUp = &optionalVar('liftUp', $config);
  $ignoreDbSnpContigs = &optionalVar('ignoreDbSnpContigs', $config);
  # Make sure no unrecognized variables were given.
  &checkConfigEmpty($config, $CONFIG, $base);
} # checkConfig


#########################################################################
# * step: download [fileServer]

sub getDbSnpAssemblyLabels {
  return ('ref') if ($opt_debug);
  my @labels;
  my $LAB = HgAutomate::mustOpen("$assemblyLabelFile");
  while (<$LAB>) {
    chomp;
    push @labels, $_;
  }
  close($LAB);
  if (@labels == 0) {
    die "$assemblyLabelFile is empty -- has $ContigInfo format changed?";
  }
  return @labels;
}


sub demandAssemblyLabel {
  # Show developer the valid assembly labels, at least one of which must
  # appear in $CONFIG's refAssemblyLabel value.
  my @labels = @_;
  my $message =  <<_EOF_

 *** This release contains more than one assembly label.
 *** Please examine this list in case we need to exclude any of these:

_EOF_
    ;
  $message .= join("\n", @labels);
  my $refAssemblyLabelDef = "refAssembly " . join(',', @labels);
  $message .= <<_EOF_

 *** Add refAssemblyLabel to $CONFIG.  If keeping all labels, it will
 *** look like this:

$refAssemblyLabelDef

 *** Edit out any of those that are not included in $db (e.g. Celera).
 *** Then restart this script with -continue=translate .

_EOF_
    ;
  die $message;
} # checkAssemblyLabel

  # Check for multiple reference assembly labels -- developer may need to exclude some.


sub translateSql {
  return if ($opt_debug);
  # Translate dbSNP's flavor of SQL create statements into mySQL.
  # This is computationally trivial so it doesn't matter where it runs.
  my $schemaDir = "$buildDir/$commonName/schema";
  # First the organism-specific tables from $ftpSnpDb:
  my @orgTables = ($ContigInfo, $ContigLoc, $ContigLocusId, $MapInfo);
  push @orgTables, qw( SNP SNPAlleleFreq SNP_bitfield Batch SubSNP SNPSubSNPLink );
  my $tables = join('|', @orgTables);
  my $SQLIN = HgAutomate::mustOpen("zcat $schemaDir/${orgDir}_table.sql.gz |");
  my $SQLOUT = HgAutomate::mustOpen("> $schemaDir/table.sql");
  my $sepBak = $/;
  $/ = "\nGO\n\n\n";
  my $tableCount = 0;
  while (<$SQLIN>) {
    next unless /^CREATE TABLE \[($tables)\]/;
    s/[\[\]]//g;  s/GO\n\n/;/;  s/smalldatetime/datetime/g;
    s/ON PRIMARY//g;  s/COLLATE//g;  s/Latin1_General_BIN//g;
    s/IDENTITY (1, 1) NOT NULL /NOT NULL AUTO_INCREMENT, PRIMARY KEY (id)/g;
    s/nvarchar/varchar/g;  s/set quoted/--set quoted/g;
    s/(image|varchar\s+\(\d+\))/BLOB/g;
    print $SQLOUT $_;
    $tableCount++;
  }
  close($SQLIN);
  # And shared table from $ftpShared, described in a separate sql file:
  my @sharedTables = qw( Allele );
  $tables = join('|', @sharedTables);
  $SQLIN = HgAutomate::mustOpen("zcat $schemaDir/dbSNP_main_table.sql.gz |" .
				   "sed -re 's/\r//g;' |");
  while (<$SQLIN>) {
    next unless /^CREATE TABLE \[$tables\]/;
    s/[\[\]]//g;  s/GO\n\n\n/;\n/;  s/smalldatetime/datetime/g;
    print $SQLOUT $_;
    $tableCount++;
  }
  close($SQLIN);
  close($SQLOUT);
  $/ = $sepBak;
  my $expected = (@orgTables + @sharedTables);
  if ($tableCount != $expected) {
    die "Expected to process $expected CREATE statements but got $tableCount\n\t";
  }
} # translateSql


sub downloadFiles {
  # Fetch database dump files via anonymous FTP from NCBI
  # and translate dbSNP SQL to mySQL.
#*** It would be nice to kick off the ftp of rs_fasta files in parallel because
#*** we won't need them until after a significant amount of processing on db dumpfiles.
#*** I don't want to parallelize too much & hog NCBI FTP.
  my $runDir = $buildDir;
  &HgAutomate::mustMkdir("$buildDir/shared");
  &HgAutomate::mustMkdir("$buildDir/$commonName");
  &HgAutomate::mustMkdir("$buildDir/$commonName/data");
  &HgAutomate::mustMkdir("$buildDir/$commonName/schema");
  &HgAutomate::mustMkdir("$buildDir/$commonName/rs_fasta");
  my $whatItDoes =
    "It downloads a bunch of dbSNP database table dump files.";
  my $bossScript = new HgRemoteScript("$runDir/download_${commonName}_${build}.csh",
				      $fileServer, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
# Get field encodings -- if there are changes or additions to the
# encoding of the corresponding fields, you might need to update
# snpNcbiToUcsc, hgTracks, hgc and hgTrackUi (see also
# hg/lib/snp125Ui.c).
cd $buildDir/shared
$wget $ftpShared/LocTypeCode.bcp.gz
$wget $ftpShared/SnpClassCode.bcp.gz
$wget $ftpShared/SnpFunctionCode.bcp.gz
$wget $ftpShared/SnpValidationCode.bcp.gz
# Also fetch Allele, so we can translate allele IDs in SNPAlleleFreq
# into base values.
$wget $ftpShared/Allele.bcp.gz

# Get $commonName-specific data:
cd $buildDir/$commonName
$wget ftp://ftp.ncbi.nih.gov/snp/00readme.txt
cd $buildDir/$commonName/data
set orgDir = $orgDir
# $ContigLoc table has coords, orientation, loc_type, and refNCBI allele
$wget $ftpSnpDb/$ContigLoc.bcp.gz
# $ContigLocusId table has functional annotations
$wget $ftpSnpDb/$ContigLocusId.bcp.gz
$wget $ftpSnpDb/$ContigInfo.bcp.gz
# $MapInfo has alignment weights
$wget $ftpSnpDb/$MapInfo.bcp.gz
# SNP has univar_id, validation status and heterozygosity
$wget $ftpSnpDb/SNP.bcp.gz
# New info as of 132: allele freq, 'clinical' bit, SNP submitter handles
$wget $ftpSnpDb/SNPAlleleFreq.bcp.gz
$wget $ftpSnpDb/SNP_bitfield.bcp.gz
$wget $ftpSnpDb/Batch.bcp.gz
$wget $ftpSnpDb/SubSNP.bcp.gz
$wget $ftpSnpDb/SNPSubSNPLink.bcp.gz

# Get schema
cd $buildDir/$commonName/schema
$wget $ftpOrgSchema/${orgDir}_table.sql.gz
$wget $ftpSharedSchema/dbSNP_main_table.sql.gz

# Get fasta files
# using headers of fasta files for molType, class, observed
cd $buildDir/$commonName/rs_fasta
$wget ftp://ftp.ncbi.nih.gov/snp/organisms/$orgDir/rs_fasta/\\*.gz

# Extract the set of assembly labels in case we need to exclude any.
zcat $buildDir/$commonName/data/$ContigInfo.bcp.gz | cut -f $groupLabelCol | uniq | sort -u \\
  > $assemblyLabelFile
_EOF_
    );

  $bossScript->execute();

  &translateSql();
  # Check for multiple reference assembly labels -- developer may need to exclude some.
  my @labels = &getDbSnpAssemblyLabels();
  if (@labels > 1) {
    &demandAssemblyLabel(@labels);
  }
}


#########################################################################
# * step: translate [workhorse]

sub checkAssemblySpec {
  # If $assemblyLabelFile contains more than one label, make sure that
  # refAssemblyLabel is in $CONFIG and includes only labels found in
  # $assemblyLabelFile.  Return labels that are omitted from the spec.
  my @labels = &getDbSnpAssemblyLabels();
  if (@labels > 1) {
    if ($refAssemblyLabel) {
      my %rejectLabels = map {$_ => 1} @labels;
      my %labelHash = map {$_ => 1} @labels;
      my @specLabels = split(',', $refAssemblyLabel);
      foreach my $l (@specLabels) {
	if (! exists $labelHash{$l}) {
	  die "\nConfig Error: refAssemblyLabel value '$l' is not recognized -- " .
	    "must be one of {" . join(", ", @labels) . "}\n" ;
	}
	delete $rejectLabels{$l};
      }
      return keys %rejectLabels;
    }
    &demandAssemblyLabel(@labels);
  }
  return ();
} # checkAssemblySpec


sub tryToMakeLiftUp {
  # If it looks like we need a liftUp file, see if ContigInfo has enough info
  # to make at least a partial one.
  my ($runDir, $chromSizesFile, $grepOutLabels, $grepOutContigs) = @_;
  my $CS = &HgAutomate::mustOpen($chromSizesFile);
  my %chromSizes;
  while (<$CS>) {
    chomp;  my ($chr, $size) = split("\t");
    $chromSizes{$chr} = $size;
  }
  close($CS);
  my $ciPipe = "zcat $runDir/data/$ContigInfo.bcp.gz $grepOutLabels $grepOutContigs |";
  my $CI = &HgAutomate::mustOpen($ciPipe);
  my $liftUpFile = "$buildDir/$commonName/suggested.lft";
  my $LU = &HgAutomate::mustOpen(">$liftUpFile");
  my $noInfoSeqFile = "$buildDir/$commonName/cantLiftUpSeqNames.txt";
  my $NI = &HgAutomate::mustOpen(">$noInfoSeqFile");
  my ($noInfo, $gotInfo) = (0, 0);
  while (<$CI>) {
    my (undef, undef, $contig, undef, undef, $chr, $chromStart, $chromEnd) = split("\t");
    if ($chromStart ne "") {
      $chr = "chrM" if ($chr eq "MT");
      if (! exists $chromSizes{$chr}) {
	if (exists $chromSizes{"chr$chr"}) {
	  $chr = "chr$chr";
	} else {
	  $chr = "no chr" if (! $chr);
	  print $NI "unrecognized newName\t$contig\n$chr\n";
	  $noInfo++;
	  next;
	}
      }
      my $oldSize = $chromEnd+1 - $chromStart;
      my $newSize = $chromSizes{$chr};
      print $LU join("\t", $chromStart, $contig, $oldSize, $chr, $newSize) . "\n";
      $gotInfo++;
    } else {
      $chr = "no chr" if (! $chr);
      print $NI "no coords\t$contig\t$chr\n";
      $noInfo++;
    }
  }
  close($CI);
  close($LU);
  close($NI);
  my $message = "";
  if ($gotInfo > 0) {
    $message = "\n*** $ContigInfo has coords for $gotInfo sequences; these have been written to
*** $liftUpFile .\n";
    if ($noInfo > 0) {
      $message .= "*** $noInfo lines of $ContigInfo.bcp.gz either had no lift-coords
*** or had unrecognized chrom names; see
*** $noInfoSeqFile .\n";
    }
  }
  return $message;
} # tryToMakeLiftUp


sub checkSequenceNames {
  # Make sure that the reference sequence names from ContigInfo
  # (limited to refAssemblyLabel spec if applicable) match the
  # sequence names in liftUp file (if spec'd) or chrom.sizes.
  my ($runDir, $grepOutLabels, $grepOutContigs) = @_;
  my $chromSizes = "$HgAutomate::clusterData/$db/chrom.sizes";
  my $ucscSeqCommand;
  if ($liftUp) {
    $ucscSeqCommand = "cut -f 2 $liftUp | sort > $runDir/ucscSeqs.txt";
  } else {
    if (! -e $chromSizes) {
      die "*** Can't find file $chromSizes, so can't check sequence names";
    }
    $ucscSeqCommand = "cut -f 1 $chromSizes | sort > $runDir/ucscSeqs.txt";
  }
  &HgAutomate::run("$HgAutomate::runSSH $workhorse $ucscSeqCommand");
  my $hg19MitoTweak = ($db eq 'hg19') ? "| grep -vw ^NC_012920" : "";
#*** ssh and pipes don't really jive.  mini-script?
  &HgAutomate::run("$HgAutomate::runSSH $workhorse " .
		   "zcat $runDir/data/$ContigInfo.bcp.gz $grepOutLabels  $grepOutContigs" .
		   "| cut -f $contigAccCol $hg19MitoTweak | sort " .
		   "  > $runDir/dbSnpContigs.txt;");
  &HgAutomate::verbose(1, "FYI First 10 UCSC sequences not in dbSNP (if any):\n");
#*** Also, HgAutomate::run should probably use SafePipe internally when there is a |
  &HgAutomate::run("comm -23 $runDir/ucscSeqs.txt $runDir/dbSnpContigs.txt | head");
  &HgAutomate::run("comm -13 $runDir/ucscSeqs.txt $runDir/dbSnpContigs.txt " .
		   "  > $runDir/dbSnpContigsNotInUcsc.txt");
  my $badContigCount = $opt_debug ? 0 : `wc -l < $runDir/dbSnpContigsNotInUcsc.txt`;
  if ($badContigCount > 0) {
    chomp $badContigCount;
    my $grepDesc = $refAssemblyLabel ? " (limited to $refAssemblyLabel)" : "";
    my $ucscSource = $liftUp ? $liftUp : $chromSizes;
    my $ContigInfoLiftUp = &tryToMakeLiftUp($runDir, $chromSizes, $grepOutLabels, $grepOutContigs);
    die "
*** $ContigInfo$grepDesc has $badContigCount contig_acc values
*** that are not in $ucscSource .
*** They are listed in $runDir/dbSnpContigsNotInUcsc.txt
$ContigInfoLiftUp
*** You must account for those in $CONFIG, in the liftUp file
*** and/or the ignoreDbSnpContigs regex.
*** Then run again with -continue=translate .
";
  }
} # checkSequenceNames


sub translateData {
  # Extract what we need from dbSNP's dump files and fasta headers.
  # Part of this involves loading up some of their database tables
  # locally and running a big left join query to pull the columns
  # of interest together.
  my @rejectLabels = &checkAssemblySpec();
  my $grepOutLabels = @rejectLabels ? "| egrep -vw '(" . join('|', @rejectLabels) . ")' " : "";
  my $grepOutContigs = $ignoreDbSnpContigs ? "| egrep -vw '$ignoreDbSnpContigs'" : "";
  my $catOrGrepOutMito = ($db eq 'hg19') ? "grep -vw ^NC_012920" : "cat";
  my $runDir = "$buildDir/$commonName";

  &checkSequenceNames($runDir, $grepOutLabels, $grepOutContigs);

  my $snpBase = "snp$build";
  my $tmpDb = $db . $snpBase;
  my $dataDir = "$runDir/data";
  my $whatItDoes =
"It loads a subset of dbSNP tables into a local mysql database,
adds info from fasta headers to the local database $tmpDb,
does a big join to pull the columns into the same order as in
the final $snpBase table,
lifts up contig coords to chrom if necessary,
processes fasta headers while concatenating fasta into one
giant file, and indexes the giant fasta file.";
  my $bossScript = new HgRemoteScript("$runDir/translate.csh",
				      $workhorse, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    # Work in local tmp disk to save some I/O time, and copy results back to
    # $runDir when done.
    #*** HgAutomate should probably have a sub that generates these lines given a prefix:
#breaks if TMPDIR not set -- which with -f I guess it wouldn't be:
#    if ("\$TMPDIR" == "" || ! -d \$TMPDIR) then
      if (-d /data/tmp) then
        setenv TMPDIR /data/tmp
      else
        if (-d /tmp) then
          setenv TMPDIR /tmp
        else
          echo "Can't find TMPDIR"
          exit 1
        endif
      endif
#    endif
    set tmpDir = `mktemp -d \$TMPDIR/$base.translate.XXXXXX`
    chmod 775 \$tmpDir
    pushd \$tmpDir

    # load dbSNP database tables into local mysql
    hgsql -e 'create database $tmpDb'
    hgsql $tmpDb < $runDir/schema/table.sql

    foreach t ($ContigInfo $MapInfo $ContigLocusId)
      zcat $dataDir/\$t.bcp.gz $grepOutLabels $grepOutContigs\\
      | perl -wpe 's/(\\d\\d:\\d\\d:\\d\\d)\\.0/\$1/g;' \\
      | hgLoadSqlTab -oldTable $tmpDb \$t placeholder stdin
    end

    # Make sure there are no orient != 0 contigs among those selected.
    set badCount = `hgsql $tmpDb -NBe \\
                      'select count(*) from $ContigInfo where orient != 0;'`
    if (\$badCount > 0) then
      echo "found \$badCount contigs in $ContigInfo with orient != 0"
      exit 1
    endif

    # $ContigLoc is huge, and we want just the reference contig mappings.
    # Keep lines only if they have a word match to some reference contig ID.
    # That probably will allow some false positives from coord matches,
    # but we can clean those up afterward.
    zcat $dataDir/$ContigInfo.bcp.gz $grepOutLabels\\
    | cut -f $ctgIdCol | sort -n > $ContigInfo.ctg_id.txt
    zcat $dataDir/$ContigLoc.bcp.gz \\
    | grep -Fwf $ContigInfo.ctg_id.txt \\
    | perl -wpe 's/(\\d\\d:\\d\\d:\\d\\d)\\.0/\$1/g;' \\
    | hgLoadSqlTab -oldTable $tmpDb $ContigLoc placeholder stdin
    # There are usually some empty-value warnings to ignore.
    # Get rid of those false positives:
    hgsql $tmpDb -e 'alter table $ContigLoc add index (ctg_id);'
    hgsql $tmpDb -e 'create table ContigLocFix select cl.* from $ContigLoc as cl, $ContigInfo as ci where cl.ctg_id = ci.ctg_id;'
    hgsql $tmpDb -e 'alter table ContigLocFix add index (ctg_id);'
    hgsql $tmpDb -e 'drop table $ContigLoc; \\
                         rename table ContigLocFix to $ContigLoc;'

    # mysql warnings about missing values in numeric columns cause hgLoadSqlTab
    # to return nonzero, and then this script terminates.  So fix missing values
    # to \\N (mysql's file representation of NULL):
    zcat $dataDir/SNP.bcp.gz \\
    | perl -pe 's/(\\d\\d:\\d\\d:\\d\\d)\\.0/\$1/g; chomp; \@w = split("\\t"); \\
                foreach \$col (1,2,3,4,5,7,11) { \$w[\$col] = "\\\\N" if (\$w[\$col] eq ""); } \\
                \$_ = join("\\t", \@w) . "\\n";' \\
    | hgLoadSqlTab -oldTable $tmpDb SNP placeholder stdin

#*** Add SNPAllele etc.

    foreach t ($ContigInfo $ContigLoc $ContigLocusId $MapInfo SNP)
      hgsql -N -B $tmpDb -e 'select count(*) from '\$t
    end

    #################### EXTRACT INFO FROM NCBI TABLES ####################
    # Glom each SNP's function codes together and load up a new $tmpDb table.
    # Also extract NCBI's annotations of coding SNPs' effects on translation.
    # We extract $ContigLocusId info only for reference assembly mapping.
    # Some SNP's functional annotations are for an alternate assembly, so we will
    # have no NCBI functional annotations to display for those (but our own are
    # available).
    # Add indices to tables for a big join:
    hgsql $tmpDb -e \\
      'alter table $ContigInfo add index (ctg_id); \\
       alter table $ContigLocusId add index (ctg_id);'
    hgsql $tmpDb -NBe 'select snp_id, ci.contig_acc, asn_from, asn_to, mrna_acc, \\
                           fxn_class, reading_frame, allele, residue, codon, cli.ctg_id \\
                           from $ContigLocusId as cli, $ContigInfo as ci \\
                           where cli.ctg_id = ci.ctg_id;' \\
      > ncbiFuncAnnotations.txt
    wc -l ncbiFuncAnnotations.txt
    # Ignore function code 8 (cds-reference, just means that some allele matches reference)
    # and glom functions for each SNP id:
    cut -f 1-4,6,11 ncbiFuncAnnotations.txt \\
    | sort -u -k1n,1n -k6n,6n -k3n,3n -k5n,5n \\
    | perl -we 'while (<>) { chomp; \\
                  (\$id, undef, \$s, \$e, \$f, \$c) = split; \\
                  if (defined \$prevId && \$id == \$prevId && \$c == \$prevC && \$s == \$prevS) { \\
                    \$prevFunc .= "\$f," unless (\$f == 8); \\
                  } else { \\
                    if (defined \$prevId) { \\
                      print "\$prevId\\t\$prevC\\t\$prevS\\t\$prevE\\t\$prevFunc\\n" if (\$prevFunc); \\
                    } \\
                    \$prevFunc = (\$f == 8) ? "" : "\$f,"; \\
                  } \\
                  (\$prevId, \$prevC, \$prevS, \$prevE) = (\$id, \$c, \$s, \$e); \\
                } \\
                print "\$prevId\\t\$prevC\\t\$prevS\\t\$prevE\\t\$prevFunc\\n" if (\$prevFunc);' \\
      > ucscFunc.txt

    wc -l ucscFunc.txt
    cat > ucscFunc.sql <<EOF
CREATE TABLE ucscFunc (
        snp_id int NOT NULL ,
        ctg_id int(10) NOT NULL ,
        asn_from int(10) NOT NULL ,
        asn_to int(10) NOT NULL ,
        fxn_class varchar(255) NOT NULL ,
        INDEX snp_id (snp_id),
        INDEX ctg_id (ctg_id)
);
EOF
    sleep 1
    hgLoadSqlTab $tmpDb ucscFunc{,.sql,.txt}
    # ucscFunc coords are NCBI's 0-based, fully-closed, 2-base-wide insertions.
    # We need to leave the coords alone here so ucscFunc can be joined below.
    # Make a list of SNPs with func anno's that are insertion SNPs, so we can use 
    # the list to determine what type of coord fix to apply to each annotation
    # when making snp130CodingDbSnp below.
    hgsql $tmpDb -e 'alter table $ContigLoc add index (snp_id);'
    hgsql $tmpDb -NBe \\
      'select ci.contig_acc, cl.asn_from, cl.asn_to, uf.snp_id \\
       from ucscFunc as uf, $ContigLoc as cl, $ContigInfo as ci \\
       where uf.snp_id = cl.snp_id and \\
             uf.ctg_id = cl.ctg_id and uf.asn_from = cl.asn_from and uf.asn_to = cl.asn_to and \\
             cl.loc_type = 3 and \\
             cl.ctg_id = ci.ctg_id' \\
      > ncbiFuncInsertions.ctg.bed
    wc -l ncbiFuncInsertions.ctg.bed

    # Extract observed allele, molType and snp class from FASTA headers gnl
    foreach rej (AltOnly NotOn)
      if (-e $runDir/rs_fasta/rs_ch\$rej.fas.gz) then
        mkdir -p $runDir/rs_fasta/rejects
        mv $runDir/rs_fasta/rs_ch\$rej.fas.gz $runDir/rs_fasta/rejects/
      endif
    end

#*** This would be a good place to check for missing flanking sequences

    zcat $runDir/rs_fasta/rs_ch*.fas.gz \\
    | grep '^>gnl' \\
    | perl -wpe 's/^\\S+rs(\\d+) .*mol="(\\w+)"\\|class=(\\d+)\\|alleles="([^"]+)"\\|build.*/\$1\\t\$4\\t\$2\\t\$3/ || die "Parse error line \$.:\\n\$_\\n\\t";' \\
    | sort -nu \\
      > ucscGnl.txt
#*** compare output of following 2 commands:
    wc -l ucscGnl.txt
#30144822 ucscGnl.txt
# weird -- it shrunk from the original 30152349
    cut -f 1 ucscGnl.txt | uniq | wc -l
#30144822
    cat > ucscGnl.sql <<EOF
CREATE TABLE ucscGnl (
        snp_id int NOT NULL ,
        observed varchar(255) NOT NULL,
        molType varchar(255) NOT NULL,
        class varchar(255) NULL ,
        INDEX snp_id (snp_id)
);
EOF
    hgLoadSqlTab $tmpDb ucscGnl{,.sql,.txt}

    # Add indices to tables for a big join:
    hgsql $tmpDb -e \\
      'alter table SNP        add index (snp_id); \\
       alter table $MapInfo    add index (snp_id);'

#***TODO: add SNPAlleleFreq to this query and snpNcbiToUcsc (bitfield/"clinical" too)

    # Big leftie join to bring together all of the columns that we want in $snpBase,
    # using all of the available joining info:
    hgsql $tmpDb -NBe \\
     'SELECT ci.contig_acc, cl.asn_from, cl.asn_to, cl.snp_id, cl.orientation, cl.allele, \\
             ug.observed, ug.molType, ug.class, \\
             s.validation_status, s.avg_heterozygosity, s.het_se, \\
             uf.fxn_class, cl.loc_type, mi.weight, cl.phys_pos_from \\
      FROM \\
      (((($ContigLoc as cl JOIN $ContigInfo as ci \\
               ON cl.ctg_id = ci.ctg_id) \\
          LEFT JOIN $MapInfo as mi ON mi.snp_id = cl.snp_id and mi.assembly = ci.group_label) \\
         LEFT JOIN SNP as s ON s.snp_id = cl.snp_id) \\
        LEFT JOIN ucscGnl as ug ON ug.snp_id = cl.snp_id) \\
       LEFT JOIN ucscFunc as uf ON uf.snp_id = cl.snp_id and uf.ctg_id = cl.ctg_id \\
                                and uf.asn_from = cl.asn_from;' \\
      > ucscNcbiSnp.ctg.bed
    wc -l ucscNcbiSnp.ctg.bed
#34610479 ucscNcbiSnp.ctg.bed

    # There are some weird cases of length=1 but locType=range... in all the cases
    # that I checked, the length really seems to be 1 so I'm not sure where they got
    # the locType=range.  Tweak locType in those cases so we can keep those SNPs:
#*** probably send warning to file instead of stderr, since we want to add to endNotes:
    $catOrGrepOutMito ucscNcbiSnp.ctg.bed \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} \\
           \$2 == \$3 && \$14 == 1 {\$14=2; \\
                                 if (numTweaked < 10) {print \$4 > "/dev/stderr";} \\
                                 numTweaked++;}  {print;} \\
           END{print numTweaked, "single-base, locType=range, tweaked locType" > "/dev/stderr";}' \\
_EOF_
		  );
  if ($liftUp) {
    $bossScript->add(<<_EOF_
    | liftUp ucscNcbiSnp.bed \\
        $liftUp warn stdin
_EOF_
		    );
  } else {
    $bossScript->add(<<_EOF_
      > ucscNcbiSnp.bed
_EOF_
		    );
  }
#*** add stderr output of awk command to endnotes at this point..:
#TODO: examine these again, report to dbSNP:
#118203330
#118203339
#118203340
#118203367
#118203389
#118203401
#118203405
#118203425
#118203428
#118203433
#588     single-base, locType=range, tweaked locType
  if ($db eq 'hg19') {
    $bossScript->add(<<_EOF_
    # For liftOver, convert 0-base fully-closed to 0-based half-open because liftOver
    # doesn't deal with 0-base items.  Fake out phys_pos_from to 0 because many coords
    # will differ, oh well.
    grep -w NC_012920 ucscNcbiSnp.ctg.bed \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} {\$3 += 1; \$16 = 0; print;}' \\
    | liftOver -bedPlus=3 stdin \\
        $hg19MitoLiftOver stdout chrM.unmapped \\
    | awk -F"\\t" 'BEGIN{OFS="\\t";} {\$3 -= 1; print;}' \\
    | sort -k2n,2n \\
      > chrMNcbiSnp.bed
    cat chrM.unmapped
#*** developer attention needed if there are more than a few.
    cat chrMNcbiSnp.bed >> ucscNcbiSnp.bed

_EOF_
		    );
  }
    $bossScript->add(<<_EOF_
    wc -l ucscNcbiSnp.bed
#34610476 ucscNcbiSnp.bed

    # Translate NCBI's encoding into UCSC's, and perform a bunch of checks.
    # Updated snpNcbiToUCSC for new MAX_SNPID (80M -> 120M), 
    # new named alleles oddball formats: CHLC.GGAA2D04, GDB:190880, SHGC-35515, =D22S272
    # new MAX_SNPSIZE (1k -> 16k)
#*** add output to endNotes:
    snpNcbiToUcsc ucscNcbiSnp.bed $HgAutomate::clusterData/$db/$db.2bit $snpBase
#*** add output to endNotes:
    head ${snpBase}Errors.bed
#*** add output to endNotes:
    wc -l snp*

    # Make one big fasta file.
#*** It's a monster: 23G for hg19 snp132!  Can we split by hashing rsId?
    zcat $runDir/rs_fasta/rs_ch*.fas.gz \\
    | perl -wpe 's/^>gnl\\|dbSNP\\|(rs\\d+) .*/>\$1/ || ! /^>/ || die;' \\
      > $snpBase.fa
    # Check for duplicates.
    grep ^\\>rs $snpBase.fa | sort > seqHeaders
#*** compare output of following 2 commands:
    wc -l seqHeaders
#30144822 seqHeaders
    uniq seqHeaders | wc -l
#30144822
    # Use hgLoadSeq to generate .tab output for sequence file offsets,
    # and keep only the columns that we need: acc and file_offset.
    # Index it and translate to snpSeq table format.
    hgLoadSeq -test placeholder $snpBase.fa
    cut -f 2,6 seq.tab > ${snpBase}Seq.tab
    rm seq.tab

# Compress (where possible -- not .fa unfortunately) and copy results back to
# $runDir
gzip *.txt *.bed *.tab
cp -p * $runDir/
rm *

# return to $runDir and clean up tmpDir
popd
rmdir \$tmpDir
_EOF_
    );

  $bossScript->execute();
}


#########################################################################
# * step: load [dbHost]

sub loadTables {
  my $snpBase = "snp$build";
  my $runDir = "$buildDir/$commonName";
  my $whatItDoes = "It loads the $snpBase* tables into $db.";
  my $bossScript = new HgRemoteScript("$runDir/load.csh",
				      $dbHost, $runDir, $whatItDoes, $CONFIG);
  $bossScript->add(<<_EOF_
    #*** HgAutomate should probably have a sub that generates these lines given a prefix:
#breaks if TMPDIR not set -- which with -f I guess it wouldn't be:
#    if ("\$TMPDIR" == "" || ! -d \$TMPDIR) then
      if (-d /data/tmp) then
        setenv TMPDIR /data/tmp
      else
        if (-d /tmp) then
          setenv TMPDIR /tmp
        else
          echo "Can't find TMPDIR"
          exit 1
        endif
      endif
#    endif

    # Load up main track tables.
    hgLoadBed -tab -onServer -tmpDir=\$TMPDIR -allowStartEqualEnd \\
      $db $snpBase -sqlTable=$snpBase.sql $snpBase.bed.gz

    hgLoadBed -tab -onServer -tmpDir=\$TMPDIR -allowStartEqualEnd \\
      $db ${snpBase}Exceptions -sqlTable=\$HOME/kent/src/hg/lib/snp125Exceptions.sql \\
      -renameSqlTable \\
      ${snpBase}Exceptions.bed.gz

    zcat ${snpBase}ExceptionDesc.tab.gz \\
    | hgLoadSqlTab $db ${snpBase}ExceptionDesc \$HOME/kent/src/hg/lib/snp125ExceptionDesc.sql stdin

    # Load up sequences.
    mkdir -p /gbdb/$db/snp
    if (-l /gbdb/$db/snp/$snpBase.fa) then
      rm /gbdb/$db/snp/$snpBase.fa
    endif
    ln -s $runDir/$snpBase.fa /gbdb/$db/snp/$snpBase.fa
    zcat ${snpBase}Seq.tab.gz \\
    | hgLoadSqlTab $db ${snpBase}Seq \$HOME/kent/src/hg/lib/snpSeq.sql stdin

    # Put in a link where one would expect to find the track build dir...
    if (-l $HgAutomate::clusterData/$db/bed/$snpBase) then
      rm $HgAutomate::clusterData/$db/bed/$snpBase
    endif
    ln -s $runDir $HgAutomate::clusterData/$db/bed/$snpBase

    # Look at the breakdown of exception categories:
    zcat ${snpBase}Exceptions.bed.gz | cut -f 5 | sort | uniq -c | sort -nr
_EOF_
    );

  $bossScript->execute();

#*** Tell developer to ask cluster-admin to pack the $snpBase table (or whatever tables we'll push)
#*** Show summaries e.g. exception breakdown to developer, esp. ones that dbSNP really should
#*** be prodded about.
}


#########################################################################
# main

# Prevent "Suspended (tty input)" hanging:
&HgAutomate::closeStdin();

&checkOptions();
&usage(1) if (scalar(@ARGV) != 1);

($CONFIG) = @ARGV;
my $config = &parseConfig($CONFIG);
&checkConfig($config);

$buildDir = $opt_buildDir ? $opt_buildDir : "$dbSnpRoot/$build";
($commonName = $orgDir) =~ s/_\d+$//;
$assemblyLabelFile = "$buildDir/$commonName/assemblyLabels.txt";
$ContigInfo = "b${build}_ContigInfo_$buildAssembly";
$ContigLoc = "b${build}_SNPContigLoc_$buildAssembly";
$ContigLocusId = "b${build}_SNPContigLocusId_$buildAssembly";
$MapInfo = "b${build}_SNPMapInfo_$buildAssembly";
$endNotes = "";

$stepper->execute();

my $stopStep = $stepper->getStopStep();
my $upThrough = ($stopStep eq 'load') ? "" :
  "  (through the '$stopStep' step)";

HgAutomate::verbose(1,
	"\n *** All done!$upThrough\n");

if ($endNotes) {
  #*** Should mail this to $ENV{'USER'} so it's not so easy to ignore.
  #*** Does mail work on all of our machines??  Even if it works on one,
  #*** we can ssh it.  Should be in an HgAutomate routine.
  HgAutomate::verbose(0,
		      "\n\nNOTES -- STUFF THAT YOU WILL HAVE TO DO --\n\n"
		      . "$endNotes\n"
		      );
}
