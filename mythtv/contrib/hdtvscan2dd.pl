#!/usr/bin/perl -w
#Last Updated: 2005.05.15 (xris)
#
#  hdtvscan2dd.pl:
#
#   Connects to the mythtv database and imports scanned hdtv channel id's
#     into an existing DataDirect listing.  Thanks to mchou for the original
#     shell script that this is based on.
#
#   1. Run mythtvsetup
#   2. Create a profile called "HDTV (dd)" and attach it to a valid listing from
#      your DataDirect account.
#   3. Create a profile called "HDTV (scan)" but don't attach it to any listing
#      source.
#   4. Associate the "scan" profile with your air2pc tuner card
#   5. Exit mythtvsetup and run mythfilldatabase
#   6. Open mythtvsetup again
#   7. Go into the Channel Editor and perform a "Full" channel scan into the
#      "scan" profile.
#   9. Exit mythtvsetup and run this script.
#  10. Restart mythbackend...
#

# Load DBI
    use DBI;

# Some variables we'll use here
    my ($db_host, $db_user, $db_name, $db_pass);

# Read the mysql.txt file in use by MythTV.
# could be in a couple places, so try the usual suspects
    my $found = 0;
    my @mysql = ("/usr/local/share/mythtv/mysql.txt",
                 "/usr/share/mythtv/mysql.txt",
                 "/etc/mythtv/mysql.txt",
                 "/usr/local/etc/mythtv/mysql.txt",
                 "$ENV{HOME}/.mythtv/mysql.txt",
                 "mysql.txt"
                );
    foreach my $file (@mysql) {
        next unless (-e $file);
        $found = 1;
        open(CONF, $file) or die "Unable to open $file:  $!\n\n";
        while (my $line = <CONF>) {
        # Cleanup
            next if ($line =~ /^\s*#/);
            $line =~ s/^str //;
            chomp($line);
        # Split off the var=val pairs
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
        }
        close CONF;
    }
    die "Unable to locate mysql.txt:  $!\n\n" unless ($found && $db_host);

# Connect to the database
    $dbh = DBI->connect("dbi:mysql:database=$db_name:host=$db_host", $db_user, $db_pass)
        or die "Cannot connect to database: $!\n\n";

# Make sure we disconnect
    END {
        $dbh->disconnect if ($dbh);
    }

# Pull out the scanned source
    my %sources;
    my %sourceid;
    my $sh = $dbh->prepare('SELECT name, sourceid FROM videosource WHERE lineupid IS NULL');
    $sh->execute();
    die "$DBI::errstr\n\n" if ($DBI::errstr);
    while (my $row = $sh->fetchrow_hashref) {
        $sources{$row->{'sourceid'}} = $row->{'name'};
        $sourceid{'scan'}            = $row->{'sourceid'};
    }
# More than one scanned source?
    if ($sh->rows > 1) {
        # need some work here
        die "More than one 'scan' type sourceid was found.  I don't support this yet.\n";
    }
# Finish
    $sh->finish;

# Not found?
    if (!$sourceid{'scan'}) {
        die "Couldn't find a video source without a datadirect lineup id.\n",
            "Please create a blank video source and do not attach it to DataDirect.\n\n";
    }

# Bring up the possible other choices
    undef %sources;
    $sh = $dbh->prepare('SELECT name, sourceid FROM videosource'
                          .' WHERE lineupid IS NOT NULL AND lineupid NOT RLIKE "^DI(TV|SH)"'
                          .' ORDER BY name');
    $sh->execute();
    die "$DBI::errstr\n\n" if ($DBI::errstr);
    while (my $row = $sh->fetchrow_hashref) {
        $sources{$row->{'sourceid'}} = $row->{'name'};
        $sourceid{'dd'}              = $row->{'sourceid'};
    }
# More than one dd source?
    if ($sh->rows > 1) {
        # need some work here
        die "More than one 'datadirect' type sourceid was found.  I don't support this yet.\n";
    }
# Finish
    $sh->finish;

# Not found?
    if (!$sourceid{'dd'}) {
        die "Couldn't find a valid video source with a datadirect lineup id.\n",
            "Please create a video source for your local/cable broadcasts and\n",
            " attach it to a DataDirect lineup.\n\n";
    }

# Copy some required info from the scan list to the dd list
    my $rows_updated = 0;
    $sh = $dbh->prepare('SELECT * FROM channel WHERE sourceid=?');
    my $sh2 = $dbh->prepare('UPDATE channel SET mplexid=?, serviceid=?, atscsrcid=?, freqid=?'
                           .' WHERE sourceid=? AND REPLACE(channum, "_", "")=?');
    $sh->execute($sourceid{'scan'});
    die "$DBI::errstr\n\n" if ($DBI::errstr);
    while (my $row = $sh->fetchrow_hashref) {
        $sh2->execute($row->{'mplexid'}, $row->{'serviceid'}, $row->{'atscrcid'}, $row->{'freqid'},
                      $sourceid{'dd'},   $row->{'channum'});
        die "$DBI::errstr\n\n" if ($DBI::errstr);
        $rows_updated += $sh2->rows;
    }
    $sh2->finish;
    $sh->finish;

# Trap an error
    if ($rows_updated < 1) {
        die "Couldn't find any matching rows between the scanned and DataDirect listings.\n\n";
    }

# Update the cardinput with the preferred sourceid and starting channel
    $sh = $dbh->prepare('SELECT channum FROM channel WHERE sourceid=? ORDER BY REPLACE(channum,"_","")+0 ASC LIMIT 1');
    $sh->execute($sourceid{'dd'});
    die "$DBI::errstr\n\n" if ($DBI::errstr);
    my ($startchan) = $sh->fetchrow_array();
    $sh->finish;

    $dbh->do('UPDATE cardinput SET sourceid=?, startchan=? WHERE sourceid=?', undef,
             $sourceid{'dd'}, $startchan,
             $sourceid{'scan'});

# Copy the scanned dtv_multiplex settings over to the dd source
    $dbh->do('DELETE FROM dtv_multiplex WHERE sourceid=?', undef,
             $sourceid{'dd'});
    $dbh->do('UPDATE dtv_multiplex SET sourceid=? WHERE sourceid=?', undef,
             $sourceid{'dd'}, $sourceid{'scan'});

# Remove the scan source from the database
    $dbh->do('DELETE FROM videosource WHERE sourceid=?', undef,
             $sourceid{'scan'});
    $dbh->do('DELETE FROM channel WHERE sourceid=?', undef,
             $sourceid{'scan'});


