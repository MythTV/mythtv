#!/usr/bin/perl -w
#
#

# DB
    our $db_file = 'station_info.sqlite';

# Includes
    use DBI;

# Connect to the database
    my $dbh = DBI->connect("dbi:SQLite:dbname=$db_file",'','');
    if (!$dbh || $DBI::errstr) {
        die "Couldn't open/create $db_file SQLite database.\n";
    }
    END {
        no warnings;
        $dbh->disconnect if ($dbh);
    }

$input = '../../uk_icons/lookup.txt';

open DATA, $input or die "Can't read $input:  $!\n";

my $qsh1 = $dbh->prepare('SELECT icon_id FROM icons WHERE icon=?');
my $qsh2 = $dbh->prepare('SELECT station_id
                            FROM stations
                           WHERE callsign=? AND country="uk"');
my $ish1 = $dbh->prepare('INSERT INTO stations
                                      (callsign,country)
                               VALUES (?,"uk")');
my $ish2 = $dbh->prepare('REPLACE INTO station_icons
                                       (station_id,icon_id)
                                VALUES (?,?)');

while (<DATA>) {
    s/\s+$//;
    s/^\s+//;
    my ($name, $url) = split(/\s*\|\s*/);
# See if we need to create the station record
    $qsh2->execute($name);
    my ($station_id) = $qsh2->fetchrow_array();
    if (!$station_id) {
        $ish1->execute($name);
        $qsh2->execute($name);
        ($station_id) = $qsh2->fetchrow_array();
        if (!$station_id) {
            die "Error inserting station record for $name\n";
        }
    }
# Skip ahead if we have no logo image
    next unless ($url =~ s#http://www.lyngsat-logo.com/logo/##);
# Insert a record for the icon, if we found a matching record
    $qsh1->execute($url);
    my ($icon_id) = $qsh1->fetchrow_array();
    if ($icon_id) {
        $ish2->execute($station_id, $icon_id);
    }
    else {
        print "Unknown station:  $name|$url\n";
    }
}

$qsh1->finish;
$qsh2->finish;
$ish1->finish;
$ish2->finish;

close DATA;
