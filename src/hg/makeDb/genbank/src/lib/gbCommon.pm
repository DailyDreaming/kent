#
# Collecting of common functions used by genbank update.
#
package gbCommon;
use strict;
use English;
use warnings FATAL => qw(all);
use Carp;
use FindBin;
use File::Basename;
use FileHandle; 
use POSIX qw(strftime);
use POSIX ":sys_wait_h";  # they have got to be kidding

# hideous magic required by perl modules
BEGIN {
    use Exporter();
    @gbCommon::ISA = qw(Exporter);
    @gbCommon::EXPORT = qw(getTimeStamp getDateStamp prMsg setTaskName 
                           makeTimeFile loadTimeFile
                           beginTask beginTaskNoLock endTask gbError makeDir
                           makeFileDir removeDir renameFile getFileSize getFileModTime
                           runProg runProgNoAbort callProg runPipe md5Files
                           gbChmod getReleases getLastRelease getUpdates
                           parseOptEq inList getTmpDir readFile makeAbs
                           backgroundStart backgroundWait
                           findConf getConf getConfNo getDbConf getDbConfNo splitSpaceList
                           getHgConf setupHgConf callMysql runMysqlDump);
    
    # make stdout/stderr always line buffered
    STDOUT->autoflush(1);
    STDERR->autoflush(1);

    # Add bin directories to path.  Tools are normally taken from
    # /cluster/bin, however we allow them to be overridden by puting them
    # under gbRoot/bin/i386.  Use path to this script to find bin dir. Can't
    # use MACHTYPE as it is i686-pc-linux-gnu on the cluster.
    
    my $rootDir = dirname($FindBin::Bin);
    my $newPath;
    my $arch;
    if (defined($main::ENV{MACHTYPE})) {
        $arch = $main::ENV{MACHTYPE};
    } else {
        $arch = "i386";  # FIXME: hack for cron.
    }
    $newPath .= "$rootDir/bin:$rootDir/bin/$arch:/cluster/bin/$arch";
    $main::ENV{PATH} = $newPath . ":" . $main::ENV{PATH};

    # also allow for shared libraries in lib/$arch
    if (defined($main::ENV{LD_LIBRARY_PATH})) {
        $main::ENV{LD_LIBRARY_PATH} = "$rootDir/lib/$arch:$main::ENV{LD_LIBRARY_PATH}";
    } else {
        $main::ENV{LD_LIBRARY_PATH} = "$rootDir/lib/$arch";
    }

    # set umask to preserve group write
    umask 0022;
        
    @gbCommon::savedArgv = @ARGV;
}

# Enable to track programs execution for debugging.
$gbCommon::verbose = 0;

# Hash table of config parameters, read on first use.
$gbCommon::confFile = "etc/genbank.conf";
$gbCommon::conf = undef;

# Directory to use for var files
$gbCommon::varDir = "var";

# Generate a time stamp
sub getTimeStamp() {
    return strftime("%Y.%m.%d-%T", localtime(time()));
}

# Generate a date stamp
sub getDateStamp() {
    return strftime("%Y.%m.%d", localtime(time()));
}

# make a time file containing the time in seconds
sub makeTimeFile($) {
    my($timeFile) = @_;

    my $timeFileTmp = "$timeFile.tmp";
    open(TF, "> $timeFileTmp") || die("can't create time file: $timeFileTmp");
    print TF time() . "\n";
    close(TF) || die("closing time file: $timeFileTmp");
    renameFile($timeFileTmp, $timeFile);
}

# load a time from a file containing the interger time as the first column
# of the first ime
sub loadTimeFile($) {
    my($timeFile) = @_;
    # really sucks that TIME is global scope
    open(TIME, $timeFile) || die("can't open $timeFile");
    my $line = <TIME>;
    close(TIME) || die("close failed");

    my $time;
    if (defined($line)) {
        my @words = split(/\t| /, $line);
        if ($#words >= 0) {
            # make sure it's an int
            $time = int($words[0]);
        }
    }
    if (!defined($time)) {
        die("can't parse time from: $timeFile");
    }
    return $time
}

# define module variables use in tasks
my $beginTimeStamp = getTimeStamp();
my $taskName = "???";
$gbCommon::hostName = `hostname`;
chomp($gbCommon::hostName);
my $logFile;
my $lockFile;
my %tmpDirs;  # tmp directories, indexed by basename

# setup signal handlers to redirect die and warn
my $inSignal = 0;
$main::SIG{__DIE__} = sub {
    my($msg) = @_;
    chomp($msg);
    $inSignal = 1;
    gbError($msg);
};
$main::SIG{__WARN__} = sub {
    my($msg) = @_;
    chomp($msg);
    $inSignal = 1;
    gbError($msg);
};

# get the prefix for a message, also used in semaphores
sub getMsgPrefix() {
    return $gbCommon::hostName . " " . getTimeStamp() . " " . $taskName . ": ";
}

# Print a message with a hostname, timestamp and task.  Normally goes to
# log file since stdout is redirected.
sub prMsg($) {
    my($msg) = @_;
    print getMsgPrefix() . $msg . "\n";
}

# Create the semaphore lock file.  This contains a line with:
#    epochTime host
sub createLockFile($) {
    my($lockFileName) = @_;
    
    my $lockFileTmp = "$lockFileName.$$.$gbCommon::hostName.tmp";
    open(SEM, "> $lockFileTmp") || die("can't create lock file: $lockFileTmp");
    print SEM time() . "\t" . $gbCommon::hostName ."\n";
    close(SEM) || die("closing lock file: $lockFileTmp");

    # link will fail if lock was created since check.
    link($lockFileTmp, $lockFileName)
        || die("lock file create failed (are two processes running?): $lockFileName");
    unlink($lockFileTmp) || die("unlink of tmp lockfile failed: $lockFileTmp");
    $lockFile = $lockFileName;
}

# Create the running semaphore file. If a running or failed semaphore already
# exists, generate an error.
sub createRunSemaphore($) {
    my($runDir) = @_;
    makeDir($runDir);
    
    # use full path for better error message
    my $absRunDir = makeAbs($runDir);

    # check for existing lock files
    my @locks = glob("$absRunDir/*.lock");
    if ($#locks >= 0) {
        gbError("lock file exists, task maybe running or failed: "
                . join(',', @locks));
    }

    # Now create running semaphore
    createLockFile("$runDir/$taskName.lock");
}

# Generate an error message and exit. Signal handlers have been added to send
# die and warn here.
sub gbError($) {
    my($msg) = @_;

    # see perlvar man page about why this is needed
    if (defined($^S)) {
        die($msg);
    }

    prMsg("failed: $msg");
    if ($inSignal || $gbCommon::verbose) {
        # log strack trace if in signal or verbose
        confess();
    }
    exit(1);
}

# log the command line at was saved when module was loaded
sub logCmdLine() {
    prMsg("command: $PROGRAM_NAME " . join(" ", @gbCommon::savedArgv));
}

# Set the task name. This is used when a script is run that is not
# a top-level step script setting semaphores.
sub setTaskName($) {
    my($name) = @_;
    $taskName = $name;
    logCmdLine();
}

# Setup task logging, redirect the process stdout/stderr to a log file.
sub setupTaskLogging($$) {
    my($taskDir, $name) = @_;
    $taskName = $name;

    my $logDir = "$gbCommon::varDir/$taskDir/logs";
    makeDir($logDir);

    # redirect stdout/err to log file
    $logFile = "$logDir/$beginTimeStamp.$taskName.log";
    print "logFile: $logFile\n";
    open(STDOUT, '>', $logFile) || gbError("stdout redirect failed");
    open(STDERR, ">&STDOUT") || gbError("Can't dup stdout");
    STDOUT->autoflush(1);
    STDERR->autoflush(1);
}

# Begin a task.  Check semaphores, then redirect the process stdout/stderr to
# a log file, log begin message.
sub beginTask($$;$) {
    my($taskDir, $name, $msg) = @_;
    
    setupTaskLogging($taskDir, $name);

    my $runDir = "$gbCommon::varDir/$taskDir/run";
    makeDir($runDir);
    createRunSemaphore($runDir);
    if (defined($msg)) {
        prMsg("begin: $msg");
    } else {
        prMsg("begin");
    }
    logCmdLine();
}

# Begin a task without using semaphores, redirect the process
# stdout/stderr to a log file, log begin message.
sub beginTaskNoLock($$;$) {
    my($taskDir, $name, $msg) = @_;
    setupTaskLogging($taskDir, $name);
    if (defined($msg)) {
        prMsg("begin: $msg");
    } else {
        prMsg("begin");
    }
    logCmdLine();
}

# End the task, log message and remove running semaphore
sub endTask(;$) {
    my($msg) = @_;
    if (defined($msg)) {
        prMsg("finish: $msg");
    } else {
        prMsg("finish");
    }
    # unlink semaphore unless we are not doing locking
    if (defined($lockFile)) {
        unlink($lockFile);
    }
}

# set directory mode to rwxrwsr-x
sub setDirMode($) {
  my($dir) = @_;
  chmod(02775, $dir) || gbError("chmod $dir");
}

# create a directory (and parent directories) if they don't exist
# (pretty lame that perl doesn't have this).
sub makeDir($) {
  my($dir) = @_;
  my @parts = split("/", $dir);
  my $path;

  foreach my $part (@parts) {
      if ($part eq "") {
          # empty element, either absolute path or double //
          if (!defined($path)) {
              $path .= "/";  # absolute
          }
      } else {
          if (defined($path)) {
              $path .= "/";
          }
          $path .= $part;
          if (!-d $path) {
              mkdir($path) || gbError("mkdir $path");
              setDirMode($path);
          }
      }
  }
}

# create a directory  (and parent directories) for a file
sub makeFileDir($) {
  my($dir) = @_;
  makeDir(dirname($dir));
}

# remove a directory and it's contents.
sub removeDir($) {
  my($dir) = @_;
  runProg("rm -rf $dir");
}

# rename a file, generating an error if it fails.
sub renameFile($$) {
  my($old, $new) = @_;
  rename($old, $new) || gbError("rename failed: $old, $new");
}

# get the size of a file
sub getFileSize($) {
    my($path) = @_;
    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
       $atime,$mtime,$ctime,$blksize,$blocks) = stat($path);
    if (!defined($dev)) {
        gbError("can't stat: $path");
    }
    return $size;
}

# get the modtime of a file
sub getFileModTime($) {
    my($path) = @_;
    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
       $atime,$mtime,$ctime,$blksize,$blocks) = stat($path);
    if (!defined($dev)) {
        die("can't stat: $path");
    }
    return $mtime;
}

# Run a program, exit if an error occurs.
sub runProg($;$) {
  my($command, $quiet) = @_;
  if ($gbCommon::verbose && !$quiet) {
      print STDERR "$command\n";
  }
  my $stat = system($command);
  if ($stat != 0) {
    gbError("command failed: $command");
  }
}

# Run a program, returning exit code.  Will abort() if it didn't exit.
sub runProgNoAbort($) {
  my($command) = @_;
  if ($gbCommon::verbose) {
      print STDERR "$command\n";
  }
  my $stat = system($command);
  if (($stat >> 8) !=  0) {
    gbError("command failed: $command");
  }
  return $stat
}

# Run a command, returning the output and exiting if there
# is an error.
sub callProg($;$) {
  my($command, $quiet) = @_;
  if (!defined($quiet)) {
      $quiet = 0;
  }
  
  if ($gbCommon::verbose && !$quiet) {
      print STDERR "$command\n";
  }
  my $output = `$command`;
  if ($? != 0) {
    gbError("command failed: $command");
  }
  return $output;
}

# Run a pipe using csh to detect erros in the pipeline
# exit if an error occurs.  Warning: this will not report
# errors on nested pipes (e.g.: "(false | true) | true" does not fail.
sub runPipe($) {
  my($command) = @_;
  if ($gbCommon::verbose) {
      print STDERR "$command\n";
  }
  my $pid = fork();
  if ($pid == 0) {
      exec("/bin/csh", "-ef", "-c", $command);
      die("exec of /bin/csh failed");
  }
  if ($pid < 0) {
      die("can't fork");
  }
  my $retPid = waitpid($pid, 0);
  my $stat = $?;
  if ($retPid < 0) {
      die("child process lost");
  }
  if (($stat >> 8) != 0) {
    gbError("command failed: $command");
  }
}

# Compute md5 checksums on a list of files
sub md5Files($@) {
    my($outFile, @files) = @_;
    my $tmpFile = "$outFile.tmp";
    runProg("md5sum " . join(' ',sort(@files)) . " >$tmpFile");
    gbChmod($tmpFile);
    renameFile($tmpFile, $outFile);
}

# set the permission on a list of datafiles
sub gbChmod(@) {
    my(@files) = @_;
    foreach my $f (@files) {
        chmod(0444, $f) || gbError("can't change mode of $f");
    }
}

# get a list of releases for a particular step and srcDb.  Skip release
# directories that are not readable for testing.
sub getReleases($$) {
    my($step, $srcDb) = @_;
    my @rels = ();
    foreach my $file (glob("data/$step/$srcDb.*")) {
        if ( -r $file) {
            push(@rels, basename($file));
        }
    }
    return @rels;
}

# compare two release names to sort newest to oldest.
sub relCmp($$) {
    my($r1, $r2) = @_;
    my $rel1 = $r1;
    $rel1 =~ s/^[^.]+\.(.*)$/$1/;
    my $rel2 = $r2;
    $rel2 =~ s/^[^.]+\.(.*)$/$1/;
    # special handling for migration from faked to real refseq releases
    # fake release contains decimal number, real contains integer.
    if ($r1 =~ /^refseq\./) {
        my $real1 = ($rel1 =~ /^[0-9]+$/);
        my $real2 = ($rel2 =~ /^[0-9]+$/);
        if ($real1 && (!$real2)) {
            return 1;
        } elsif ((!$real1) && $real2) {
            return -1;
        }
        # all real or all fake, do normal compare
    }
    if ($rel1 > $rel2) {
        return 1;
    } elsif ($rel1 < $rel2) {
        return -1;
    } else {
        return 0;
    }
}

# Get the last releases for a particular step and srcDb.  Skip release
# directories that are not readable for testing.
sub getLastRelease($$) {
    my($step, $srcDb) = @_;
    my @rels = sort relCmp getReleases($step, $srcDb);
    if ($#rels >= 0) {
        return $rels[$#rels];
    } else {
        return undef;
    }
}

# get a sorted list of updates for a particular step and release, and optional
# genome database
sub getUpdates($$;$) {
    my($step, $release, $db) = @_;
    my $dir = "data/$step/$release";
    if (defined($db)) {
        $dir .= "/$db"; 
    }
    my @updates = ();
    if (-e "$dir/full") {
        @updates = "full";
    }
    my @dailies = ();
    foreach my $file (glob("$dir/daily.*")) {
        push(@dailies, basename($file));
    }
    push(@updates, sort(@dailies));
    return @updates;
}

# Parse an option in the form -opt=val
sub parseOptEq($) {
    my($opt) = @_;

    my $stat =  ($opt =~ /^-[^=]+=(.+)$/);
    if (!defined($stat)) {
        gbError("option $opt requires a value");
    }
    my $val = $1;
    return $val;
}

# check if string contained in a list.
sub inList($@) {
    my($str, @list) = @_;
    foreach my $el (@list) {
        if ($el eq $str) {
            return 1;
        }
    }
    return 0;
}

# get a temporary directory for this process, either in /var/tmp or
# a specified in the TMPDIR env
sub getTmpDir($) {
    my($baseName) = @_;

    if (!defined($tmpDirs{$baseName})) {
        if (defined($main::ENV{TMPDIR})) {
            $tmpDirs{$baseName} = "$main::ENV{TMPDIR}/$baseName.$gbCommon::hostName.$$";
        } else {
            $tmpDirs{$baseName} = "/var/tmp/$baseName.$$";
        }
    }
    makeDir($tmpDirs{$baseName});

    return $tmpDirs{$baseName};
}

# read the contents of a file into a string
sub readFile($) {
    my($file) = @_;
    my $str = "";
    open(FILE, $file) || die("can't open $file");
    my $line;
    while ($line = <FILE>) {
        $str .= $line;
    }
    close(FILE) || die("close failed");
    return $str;
}

# count of active pids
$gbCommon::pidCount = 0;

# Start a background process, save description of process in
# in %gbCommon::pidTable for error messages at wait.
sub backgroundStart($@) {
    my($desc, @cmd) = @_;
    if ($gbCommon::verbose) {
        prMsg("start $desc: " . join(" ", @cmd));
    }
    
    my $pid = fork();
    if (!defined($pid)) {
        die("can't fork $desc");
    }
    if ($pid == 0) {
        # close stdin to prevent rsh+csh problems
        open(STDIN, "</dev/null") || die "Can't redirect stdin to /dev/null";
        exec @cmd;
        die("exec failed: " . join(" ", @cmd));
    }
    $gbCommon::pidTable{$pid} = $desc;
    $gbCommon::pidCount++;
}

# Wait for the next background process to complete and check status
# return count of remaining processes.
sub backgroundWait() {
    my $pid = wait();
    my $stat= $?;
    if (!defined($gbCommon::pidTable{$pid})) {
        die("$pid returned by wait is not in pid table");
    }
    my $desc = $gbCommon::pidTable{$pid};
    $gbCommon::pidCount--;
    $gbCommon::pidTable{$pid} = undef;
    if ($stat != 0) {
        die("process $pid: wait status $stat: $desc");
    }
    if ($gbCommon::verbose) {
        prMsg("process finished: $desc");
    }
    
    return $gbCommon::pidCount;
}

# make a filename absolute
sub makeAbs($) {
    my($path) = @_;
    if (! ($path =~ /^\//)) {
        my $cwd = `pwd`; 
        chomp($cwd);
        $path = $cwd . "/" . $path;
    }
    return $path;
}

# trim whitespace from start and end.
sub trim($) {
    my($str) = @_;
    $str =~ s/^\s+//;
    $str =~ s/\s+$//;
    return $str;
}

# parse a line in the conf file, return (key, val) or undef.
sub parseConfLine($$) {
    my($file, $line) = @_;

    $line = trim($line);
    if (!(($line =~ /^$/) || ($line =~ /^\#/))) {
        if ($line =~ /^([^=]+)=(.*)$/) {
            return (trim($1), trim($2));
        } else {
            die("invalid line in $file: $line");
        }
    }
    return undef
}

# read the configuration file into global has.
sub loadConf() {
    open(CONFIG, $gbCommon::confFile) || die("can't open $gbCommon::confFile");
    my $line;
    while ($line = <CONFIG>) {
        my($key, $value) = parseConfLine($gbCommon::confFile, $line);
        if (defined($key)) {
            $gbCommon::conf{$key} = $value;
        }
    }
    close(CONFIG);
}

# get a configuration value, or undef if not defined.
sub findConf($) {
    my($name) = @_;
    if (!defined(%gbCommon::conf)) {
        loadConf();
    }
    return $gbCommon::conf{$name};
}

# get a configuration value, or error if not defined.
sub getConf($) {
    my($name) = @_;
    my $value = findConf($name);
    if (!defined($value)) {
        die("no $name in $gbCommon::confFile");
    }
    return $value;
}

# get a configuration value, or error if not defined.
# If the values is empty or "no", undef is returned.
sub getConfNo($) {
    my($name) = @_;
    my $value = getConf($name);
    if (($value eq "") || ($value eq "no")) {
        return undef;
    } else {
        return $value;
    }
}

# get a configuration value for a database, or the default, or an error
# if neither are specified
sub getDbConf($$) {
    my($db, $name) = @_;
    my $value = findConf("$db.$name");
    if (!defined($value)) {
        $value = findConf("default.$name");
    }
    if (!defined($value)) {
        die("no $db.$name or default for $name in $gbCommon::confFile");
    }
    return $value;
}

# get a configuration value for a database, or the default, or an error
# if neither are specified.  If the values is empty or "no", undef is returned.
sub getDbConfNo($$) {
    my($db, $name) = @_;
    my $value = getDbConf($db, $name);
    if (($value eq "") || ($value eq "no")) {
        return undef;
    } else {
        return $value;
    }
}

# convert a space seperate list into a list, if strList is undef, pass back
# an empty list
sub splitSpaceList($) {
    my($strList) = @_;
    if (!defined($strList)) {
        return ();
    } else {
        return split(/\s+/, $strList);
    }
}

# locate .hg.conf file to use.  This first checks the HGDB_CONF environment
# variable, then etc/.hg.conf, then ~/.hg.conf.  Error if not found or
# readable.
sub getHgConf() {
    my $hgConf = $ENV{"HGDB_CONF"};
    if (!defined($hgConf) || (! -r $hgConf)) {
        $hgConf = "etc/.hg.conf";
    }
    if (! -r $hgConf) {
        $hgConf = $ENV{"HOME"} . "/.hg.conf";
    }
    if (! -r $hgConf) {
        gbError("can't find readable .hg.conf");
    }
    return $hgConf;
}

# find the .hg.conf file to use and set HGDB_CONF env var
sub setupHgConf() {
    my $hgConf = getHgConf();
    $ENV{"HGDB_CONF"} = $hgConf;
}

# catch of .hg.conf users/password
my $hgUser;
my $hgPassword;

# get the db.user and db.password from ~/.hg.conf, caching the results.
sub getMysqlUser() {
   if (!defined($hgUser)) {
       my $hgConf = getHgConf();
       open(HGCONF, "<$hgConf") || die("can't open $hgConf");
       my $line;
       while (($line = <HGCONF>)) {
           my($key,$val) = parseConfLine($hgConf, $line);
           if (defined($key)) {
               if ($key eq "db.user") {
                   $hgUser = $val;
               } elsif ($key eq "db.password") {
                   $hgPassword = $val;
               }
           }
           
       }
       close(HGCONF);
       if (!defined($hgUser)) {
           die("$hgConf doesn't set db.user");
       }
       if (!defined($hgPassword)) {
           die("$hgConf doesn't set db.password");
       }
   }
   return ($hgUser, $hgPassword);
}

# execute a mysql with genome user/password, return the output.
sub callMysql($) {
    my($args) = @_;

    my($user, $pass) = getMysqlUser();
    # still not super secure, since ps can see env.
    $main::ENV{MYSQL_PWD} = $pass;
    return callProg("mysql -u$user $args");
}

# execute a mysqldump with genome user/password
sub runMysqlDump($) {
    my($args) = @_;

    my($user, $pass) = getMysqlUser();
    runProg("mysqldump -u$user -p$pass $args");
}

# perl requires a true value at the end
1;
