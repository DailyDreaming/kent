#
# Encode: support routines for ENCODE scripts
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/Encode.pm instead.
#
# $Id: Encode.pm,v 1.24 2008/10/01 22:57:33 larrym Exp $

package Encode;

use warnings;
use strict;

use File::stat;
use Cwd;

use RAFile;
use HgAutomate;

# Global constants
our $loadFile = "load.ra";
our $unloadFile = "unload.ra";
our $trackFile = "trackDb.ra";
our $pushQFile = "pushQ.sql";
our $dafVersion = "0.2.2";

our $fieldConfigFile = "fields.ra";
our $vocabConfigFile = "cv.ra";
our $labsConfigFile = "labs.ra";
our $autoCreatedPrefix = "autoCreated";

our $restrictedMonths = 9;

our $sqlCreate = "/cluster/bin/sqlCreate";
# Add type names to this list for types that can be loaded via .sql files (e.g. bed5FloatScore.sql)
# You also have to make sure the .sql file is copied into the $sqlCreate directory.
our @extendedTypes = ("narrowPeak", "broadPeak", "gappedPeak", "tagAlign", "pairedTagAlign", "bed5FloatScore");

sub newestFile
{
  # Get the most recently modified file from a list
    my @files = @_;
    my $newestTime = 0;
    my $newestFile = "";
    my $file = "";
    foreach $file (@files) {
        my $fileTime = (stat($file))->mtime;
        if ($fileTime > $newestTime) {
            $newestTime = $fileTime;
            $newestFile = $file;
        }
    }
    return $newestFile;
}

sub splitKeyVal
{
# split a line into key/value, using the FIRST white-space in the line; we also trim key/value strings
    my ($str) = @_;
    my $key = undef;
    my $val = undef;
    if($str =~ /([^\s]+)\s+(.+)/) {
        $key = $1;
        $val = $2;
        $key =~ s/^\s+//;
        $key =~ s/\s+$//;
        $val =~ s/^\s+//;
        $val =~ s/\s+$//;
    }
    return ($key, $val);
}


sub validateFieldList {
# validate the entries in a RA record or DDF header using fields.ra
# $file s/d be 'ddf' or 'dafHeader'
# Returns list of any errors that are found.
    my ($fields, $schema, $file) = @_;
    my %hash = map {$_ => 1} @{$fields};
    my @errors;
    if($file ne 'ddf' && $file ne 'dafHeader' && $file ne 'dafList') {
        push(@errors, "file argument '$file' is invalid");
    } else {
        # look for missing required fields
        for my $field (keys %{$schema}) {
            if(defined($schema->{$field}{file}) && $schema->{$field}{file} eq $file && $schema->{$field}{required} && !exists($hash{$field})) {
                push(@errors, "field '$field' not defined");
            }
        }

        # now look for fields in list that aren't in schema
        for my $field (@{$fields}) {
            if(!defined($schema->{$field}) || !defined($schema->{$field}{file}) || $schema->{$field}{file} ne $file) {
                push(@errors, "invalid field '$field'");
            }
        }
    }
    return @errors;
}

sub validateValueList {
# validate hash of values using fields.ra; $file s/d be 'ddf' or 'dafHeader'
# Returns list of any errors that are found.
    my ($values, $schema, $file) = @_;
    my @errors;
    for my $field (keys %{$values}) {
        my $val = $values->{$field};
        if(defined($schema->{$field}{file}) && $schema->{$field}{file} eq $file) {
            my $type = $schema->{$field}{type} || 'string';
            if($type eq 'bool') {
                if(lc($val) ne 'yes' && lc($val) ne 'no' && $val != 1 && $val != 0) {
                    push(@errors, "invalid boolean; field '$field', value '$val'; value must be 'yes' or 'no'");
                } else {
                    $values->{$field} = lc($val) eq 'yes' ? 1 : 0;
                }
            } elsif($type eq 'int') {
                if($val !~ /^\d+$/) {
                    push(@errors, "invalid integer; field '$field', value '$val'");
                }
            }
        }
    }
    return @errors;
}

sub readFile
{
# Return lines from given file, with EOLs chomp'ed off.
# Handles Macintosh, MS-DOS or Unix EOL characters.
# Reads whole file into memory, so should NOT be used for huge files.
    my ($file) = @_;
    my $oldEOL = $/;
    undef $/;
    open(FILE, $file) or die "ERROR: Can't open file \'$file\'\n";
    my $content = <FILE>;
    close(FILE);
    $/ = $oldEOL;

    # MS-DOS => Unix
    $content =~ s/\r\n/\n/g;
    # Mac => Unix
    $content =~ s/\r/\n/g;
    my @lines = split(/\n/, $content);
    return \@lines;
}

sub pipelineDb
{
    my ($instance) = @_;
    return "encpipeline_$instance";
}

sub projectDir
{
    my ($instance, $id) = @_;
    return "/cluster/data/encode/pipeline/encpipeline_$instance/$id";
}

sub getGrants
{
# The grants are called "labs" in the labs.ra file (for historical reasons).
    my ($configPath) = @_;
    my %grants;
    if(-e "$configPath/$labsConfigFile") {
        # tolerate missing labs.ra in dev trees.
        %grants = RAFile::readRaFile("$configPath/$labsConfigFile", "lab");
    }
    return \%grants;
}

sub getControlledVocab
{
# Returns hash indexed by the type's in the cv.ra file (e.g. "Cell Line", "Antibody")
    my ($configPath) = @_;
    my %terms = ();
    my %termRa = RAFile::readRaFile("$configPath/$vocabConfigFile", "term");
    foreach my $term (keys %termRa) {
        my $type = $termRa{$term}->{type};
        $terms{$type}->{$term} = $termRa{$term};
    }
    return %terms;
}

sub getFields
{
# Gather fields defined for DDF file. File is in 
# ra format:  field <name>, required <true|false>
    my ($configPath) = @_;
    my %fields = RAFile::readRaFile("$configPath/$fieldConfigFile", "field");

    # For convenience, convert "required" to a real boolean (1 or 0);
    for my $key (keys %fields) {
        if(exists($fields{$key}->{required})) {
            my $val = $fields{$key}->{required};
            $fields{$key}->{required} = lc($val) eq 'yes' ? 1 : 0;
        }
    }
    return \%fields;
}

sub validateAssembly {
    my ($val) = @_;
    if($val ne 'hg18') {
        return "Assembly '$val' is invalid (must be 'hg18')";
    } else {
        return ();
    }
}

sub getDaf
{
# Return reference to DAF hash, using newest DAF file found in $submitDir.
# hash keys are RA style plus an additional TRACKS key which is a nested hash for
# the track list at the end of the DAF file; e.g.:
# (lab => 'Myers', TRACKS => {'Alignments => {}, Signal => {}})
    my ($submitDir, $grants, $fields) = @_;

    # Read info from Project Information File.  Verify required fields
    # are present and that the project is marked active.
    my $wd = cwd();
    chdir($submitDir);
    my @glob = glob "*.DAF";
    push(@glob, glob "*.daf");
    my $dafFile = newestFile(@glob);
    if(!(-e $dafFile)) {
        die "Can't find the DAF file\n";
    }
    $dafFile = cwd() . "/" . $dafFile;
    HgAutomate::verbose(2, "Using newest DAF file \'$dafFile\'\n");
    chdir($wd);
    return parseDaf($dafFile, $grants, $fields);
}

sub parseDaf
{
# Identical to getDaf, but first argument is the DAF filename.
    my ($dafFile, $grants, $fields) = @_;
    my %daf = ();
    $daf{TRACKS} = {};
    my $lines = readFile("$dafFile");
    my $order = 1;

    while (@{$lines}) {
        my $line = shift @{$lines};
        # strip leading and trailing spaces
        $line =~ s/^ +//;
        $line =~ s/ +$//;
        # ignore comments and blank lines
        next if $line =~ /^#/;
        next if $line =~ /^$/;

        my ($key, $val) = splitKeyVal($line);
        if(!defined($key)) {
            next;
        }
        if ($key eq "view") {
            my %track = ();
            my $track = $val;
            # remember track of order, so we can prioritize tracks correctly
            $track{order} = $order++;
            $daf{TRACKS}->{$track} = \%track;
            HgAutomate::verbose(5, "  Found view: \'$track\'\n");
            while ($line = shift @{$lines}) {
                $line =~ s/^ +//;
                $line =~ s/ +$//;
                next if $line =~ /^#/;
                next if $line =~ /^$/;
                if ($line =~ /^view/) {
                    unshift @{$lines}, $line;
                    last;
                }
                my ($key, $val) = splitKeyVal($line);
                $track{$key} = $val;
                HgAutomate::verbose(5, "    line: '$line'; Property: $key = $val\n");
            }
            $track{required} = lc($track{required}) eq 'yes' ? 1 : 0;
            $track{hasReplicates} = lc($track{hasReplicates}) eq 'yes' ? 1 : 0;
        } else {
            HgAutomate::verbose(3, "DAF field: $key = $val\n");
            $daf{$key} = $val;
        }
    }

    # Validate fields
    my @tmp = grep(!/^TRACKS$/, keys %daf);
    my @errors = validateFieldList(\@tmp, $fields, 'dafHeader');

    if($daf{dafVersion} ne $dafVersion) {
        push(@errors, "dafVersion '$daf{dafVersion}' does not match current version: $dafVersion");
    }
    if(!keys(%{$daf{TRACKS}})) {
        push(@errors, "no views defined for project \'$daf{project}\' in DAF '$dafFile'");
    }
    if(!defined($grants->{$daf{grant}})) {
        push(@errors, "invalid lab '$daf{grant}' in DAF '$dafFile'");
    }
    push(@errors, validateAssembly($daf{assembly}));

    foreach my $view (keys %{$daf{TRACKS}}) {
        HgAutomate::verbose(4, "  View: $view\n");
        my %track = %{$daf{TRACKS}->{$view}};
        foreach my $key (keys %track) {
            HgAutomate::verbose(4, "    Setting: $key   Value: $track{$key}\n");
        }
        my @keys = keys %track;
        push(@errors, validateFieldList(\@keys, $fields, 'dafList'));
    }

    if (defined($daf{variables})) {
        my @variables = split (/\s*,\s*/, $daf{variables});
        my %variables;
        my $i = 0;
        foreach my $variable (@variables) {
            # replace underscore with space
            $variable =~ s/_/ /g;
            $variables[$i++] = $variable;
            $variables{$variable} = 1;
        }
        $daf{variableHash} = \%variables;
        $daf{variableArray} = \@variables;
    }
    if(@errors) {
        die "ERROR(s) in DAF '$dafFile':\n\n" . join("\n\n", @errors) . "\n";
    }
    return \%daf;
}

sub compositeTrackName
{
    my ($daf) = @_;
    return "wgEncode" . ucfirst(lc($daf->{lab})) . $daf->{dataType};
}

sub downloadDir
{
    my ($daf) = @_;
    return "/usr/local/apache/htdocs/goldenPath/$daf->{assembly}/" . compositeTrackName($daf);
}

sub daysInMonth
{
    # $mon and $year are in format returned by localtime
    my ($mon, $year) = @_;
    $year += 1900;
    if($mon == 1) {
        if ((!($year % 4) && ($year % 100)) || !($year % 400)) {
            return 29;
        } else {
            return 28;
        }
    } elsif ($mon == 3 || $mon == 5 || $mon == 8 || $mon == 10) {
        return 30;
    } else {
        return 31;
    }
}

sub restrictionDate
{
# calculate the "restrict until ..." date.
# now argument s/d be time().
# returns the standard time list; i.e.: ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)
    my ($now) = @_;
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($now);
    my $restrictedYear = $year;
    my $restrictedMon = $mon + $restrictedMonths;
    if($restrictedMon >= 12) {
        $restrictedYear++;
        $restrictedMon = ($mon + $restrictedMonths) % 12;
    }
    if($mday > daysInMonth($restrictedMon,$restrictedYear)) {
        # wrap to first when to avoid, for example, 2008-05-31 => 2009-02-31
        $mday = 1;
    }
    return ($sec,$min,$hour,$mday,$restrictedMon,$restrictedYear,$wday,$yday,$isdst);
}

sub wigMinMaxPlaceHolder
{
# This is a placeholder used to let the loader fixup the min/max ranges for wig's
    my ($tableName) = @_;
    return uc("${tableName}_MinMaxRange");
}

sub isZipped
{
    my ($filePath) = @_;
    return $filePath =~ m/\.gz$/;
}

1;
