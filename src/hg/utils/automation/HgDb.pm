#
# HgDb: Class interface to the mysql databases
# Get's relevant mysql connect data from the .hg.conf file
#
# DO NOT EDIT the /cluster/bin/scripts copy of this file --
# edit ~/kent/src/hg/utils/automation/HgDb.pm instead.

# $Id: HgDb.pm,v 1.2 2008/07/10 23:51:18 larrym Exp $

package HgDb;

use warnings;
use strict;

use Carp;
use DBI;

use vars qw(@ISA @EXPORT_OK);
use Exporter;

@ISA = qw(Exporter);

sub new
{
# $args{DB} is required
# $args{USER}, $args{PASSWORD} and $args{HOST} are optional (and override .hg.conf values)
    my ($class, %args) = (@_);
    my $ref = {};
    my $db = $args{DB} or die "Missing \$args{DB}";
    my $dsn = "DBI:mysql:$args{DB}";
    my $confFile = "$ENV{HOME}/.hg.conf";
    if(! -e $confFile) {
        die "Cannot locate conf file: '$confFile'";
    }
    open(CONF, $confFile);
    my $conf = join("", <CONF>);
    close(CONF);
    my ($host, $user, $password);
    if($conf =~ /db\.host=(.*)/) {
        $host = $1;
    }
    if($args{HOST}) {
        $host = $args{HOST};
    }
    if($conf =~ /db\.user=(.*)/) {
        $user = $1;
    }
    if($args{USER}) {
        $host = $args{USER};
    }
    if($conf =~ /db\.password=(.*)/) {
        $password = $1;
    }
    if($args{PASSWORD}) {
        $host = $args{PASSWORD};
    }
    if($host) {
        $dsn .= ";host=$host";
    }
    my $dbh = DBI->connect($dsn, $user, $password) or die "Couldn't connect to db: $db";
    $ref->{DBH} = $dbh;
    $ref->{HOST} = $host;
    $ref->{USER} = $user;
    $ref->{PASSWORD} = $password;
    bless $ref, 'HgDb';
    return $ref;
}

sub execute
{
# Execute given query with @params substituted for placeholders in the query
# Returns $sth
    my ($db, $query, @params) = (@_);
    my $sth = $db->{DBH}->prepare($query) or die "prepare for query '$query' failed; error: " . $db->{DBH}->errstr;
    my $rv = $sth->execute(@params) or die "execute for query '$query' failed; error: " . $db->{DBH}->errstr;
    return $sth;
}

1;
