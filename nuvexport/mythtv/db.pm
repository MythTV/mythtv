#!/usr/bin/perl -w
#Last Updated: 2004.08.25 (xris)
#
#  mythtv::db
#
#   Find and connect to the mythtv database, as well as load some mythtv settings.
#

package mythtv::db;

    use DBI;

# Load the shared utilities
    use nuv_export::shared_utils;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &mysql_escape $hostname $dbh /;
    }

# Variables we intend to export
    our ($hostname, $dbh);

# Some variables we'll use here
    my ($db_host, $db_user, $db_name, $db_pass);

# Get the hostname of this machine
    $hostname = `hostname`;
    chomp($hostname);

# Read the mysql.txt file in use by MythTV.
# could be in a couple places, so try the usual suspects
    open(CONF, "$ENV{HOME}/.mythtv/mysql.txt")
        or open(CONF, "/usr/share/mythtv/mysql.txt")
        or open(CONF, "/usr/local/share/mythtv/mysql.txt")
        or open(CONF, "/etc/mythtv/mysql.txt")
        or die ("Unable to locate mysql.txt:  $!\n\n");
    while (my $line = <CONF>) {
        chomp($line);
        $line =~ s/^str //;
        my ($var, $val) = split(/\=/, $line, 2);
        next unless ($var && $var =~ /\w/);
        if ($var eq 'DBHostName') {
            $db_host = $val;
        }
        elsif ($var eq 'DBUserName') {
            $db_user = $val;
        }
        elsif ($var eq 'DBName') {
            $db_name = $val;
        }
        elsif ($var eq 'DBPassword') {
            $db_pass = $val;
        }
        elsif ($var eq 'LocalHostName') {
            $hostname = $val;
        }
    }
    close CONF;

# Connect to the database
    $dbh = DBI->connect("dbi:mysql:database=$db_name:host=$db_host", $db_user, $db_pass)
        or die "Cannot connect to database: $!\n\n";


# Check the version of the nuvexport database
#    my $version;
#    my $sh = $dbh->prepare('SELECT value FROM nuvexport_settings WHERE var="version"');
#    if ($sh->execute()) {
#        ($version) = $sh->fetchrow_array;
#    }
#    $sh->finish;

# No version?  Create the tables
#    if (!$version) {
# Disabled for now....
#        $q = <<EOF;
#CREATE TABLE nuvexport_settings (
#    name    VARCHAR(255) PRIMARY KEY,
#    value   VARCHAR(255)
#)
#EOF
#        $dbh-do($q);
#        $dbh->do('REPLACE INTO nuvexport_settings (name, value) VALUES ("version", 1)');
#        $version = 1;
#        $q = <<EOF;
#CREATE TABLE nuvexport_work (
#    id          BIGINT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
#
#    chanid      INT UNSIGNED,
#    starttime   DATETIME,
#    mode        VARCHAR(32),
#    outfile     TEXT,
#
#    params      BLOB,
#
#    status      ENUM('pending', 'working', 'done') DEFAULT 'pending',
#    hostname    VARCHAR(255),
#    pct_done    TINYINT,
#    updated     TIMESTAMP
#)
#EOF
#        $dbh-do($q);
#    }

# Perform any table updates needed
    #if ($version < 2) {
    #    $version = 2;
    #}


#####
##  Beware:  subroutines lurk below!
#####

    sub mysql_escape {
        $string = shift;
        return 'NULL' unless (defined $string);
        $string =~ s/'/\\'/sg;
        return "'$string'";
    }


# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
