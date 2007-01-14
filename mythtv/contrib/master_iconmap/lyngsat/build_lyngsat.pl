#!/usr/bin/perl -w
#
# Parse lyngsat-logo and build an info file of all channels and URLs
#
# Requires wget at the moment because I'm too lazy to write this with libwww.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @copyright Silicon Mechanics
#

# Debug on or off
    our $DEBUG = 0;
# User agent (IE 6)
    our $UserAgent = 'Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1; .NET CLR 1.1.4322)';
# Output files
    our $db_file = 'station_info.sqlite';

# Includes
    use DBI;
    use HTTP::Request;
    use LWP::UserAgent;
    use Fcntl qw(F_SETFD);
    use File::Temp qw(tempfile);
    use Getopt::Long;

# Connect to the database
    my $dbh = DBI->connect("dbi:SQLite:dbname=$db_file",'','');
    if (!$dbh || $DBI::errstr) {
        die "Couldn't open/create $db_file SQLite database.\n";
    }
    END {
        no warnings;
        $dbh->disconnect if ($dbh);
    }

# Make sure the database is up to date
    db_update();

# Load cli arguments
    my ($scan, $output, $usage);
    GetOptions('scan|rescan'    => \$scan,
               'output:s'       => \$output,
               'usage|help'     => \&print_usage,
              );

# Figure out what we're doing
    if ($scan) {
        do_scan();
        print "\nDone.\n";
    }

# Output
    elsif (defined $output) {
        if ($output =~ /miss/i) {
            my $sh = $dbh->prepare('SELECT stations.station_id, stations.name
                                      FROM stations
                                           LEFT JOIN callsigns
                                                  ON callsigns.station_id = stations.station_id
                                     WHERE callsigns.callsign IS NULL');
            $sh->execute();
            while (my($id, $name) = $sh->fetchrow_array()) {
                print "$id,$name\n";
            }
            $sh->finish;
        }
        else {
            my $sh = $dbh->prepare('SELECT name, icon, logo FROM stations ORDER BY name');
            $sh->execute();
            while (my($name, $icon, $logo) = $sh->fetchrow_array()) {
                print "$name\n    [ls-icon]/$icon\n    [ls-logo]/$logo\n";
            }
            $sh->finish;
        }
    }

# Otherwise, just print the usage message
    else {
        print_usage();
    }

# Done
    exit;

################################################################################

# Do the scan
    sub do_scan {
    # Reset the enabled flag on all stations
        $dbh->do('UPDATE stations SET enabled=0');
    # Prepare some helpful queries
        my $qsh  = $dbh->prepare('SELECT station_id FROM stations WHERE name=? AND lyng_id=?');
        my $updh = $dbh->prepare('UPDATE stations SET enabled=1, icon=?, logo=? WHERE station_id=?');
        my $insh = $dbh->prepare('INSERT INTO stations (name,icon,logo,lyng_id,enabled) VALUES (?,?,?,?,1)');
    # Parse each letter page
        foreach my $letter ('num', 'a' .. 'z') {
        # Keep track of the stations for this run
            my %stations;
        # Grab the main letter page
            print "wget: http://www.lyngsat-logo.com/tv/$letter.html\n";
            my $page = wget("http://www.lyngsat-logo.com/tv/$letter.html",
                            "http://www.lyngsat-logo.com/tv/a.html");
        # First, we load the overview page and get all of the small icon URLs
            my $num_pages = 1;
            while ($page =~ m#<td\b[^>]*><a\b[^>]+?\bhref="(.*?/${letter}_\d+\.html)"[^>]*>
                              <img\b[^>]+?\bsrc="([^"]+)"[^>]*>
                              </a></td>\s*<td\b[^>]*>(?:<[^>]+>)*?
                              <a[^>]+?href="\1">\s*([^<]+?)\s*</a>
                             #gsx) {
                my $pagelink = $1;
                my $img      = $2;
                my $station  = $3;
            # Some smarts to file the icon/logo appropriately
                if ($img =~ s#^.*?/icon/##) {
                    $stations{$station}{'icon'} = $img;
                }
                elsif ($img =~ s#^.*?/logo/##) {
                    $stations{$station}{'logo'} = $img;
                }
            # Count how many detail pages are listed for this letter
                $pagelink =~ s#^.*?/${letter}_(\d+)\.html$#$1#;
                $num_pages = $pagelink if ($pagelink > $num_pages);
            }
        # Now we parse each of the detail pages
            print "Parsing pages 1 .. $num_pages\n";
            foreach my $num (1 .. $num_pages) {
                print "wget: http://www.lyngsat-logo.com/tv/${letter}_$num.html\n";
                $page = wget("http://www.lyngsat-logo.com/tv/${letter}_$num.html",
                             "http://www.lyngsat-logo.com/tv/$letter.html");
                while ($page =~ m#<font\b[^>]*><b>
                                  \s*([^<]+?)\s*
                                  </b></font><br>
                                  <img\b[^>]+?\bsrc="([^"]+)"[^>]*><br>
                                  \s*<font\b[^>]*>[^<]+?<br>
                                  \s*(\d+)\s*<
                                 #gsx) {
                    my $station = $1;
                    my $img     = $2;
                    $stations{$station}{'lyng_id'} = $3;
                # Some smarts to file the icon/logo appropriately
                    if ($img =~ s#^.*?/icon/##) {
                        $stations{$station}{'icon'} ||= $img;
                    }
                    elsif ($img =~ s#^.*?/logo/##) {
                        $stations{$station}{'logo'} = $img;
                    }
                }
            }
        # Build the output file
            print 'Writing ', uc($letter), " stations to the database\n";
            foreach my $station (keys %stations) {
                $qsh->execute($station, $stations{$station}{'lyng_id'});
                my ($id) = $qsh->fetchrow_array();
                if ($id) {
                    $updh->execute($stations{$station}{'icon'},
                                   $stations{$station}{'logo'},
                                   $id);
                }
                else {
                    $insh->execute($station,
                                   $stations{$station}{'icon'},
                                   $stations{$station}{'logo'},
                                   $stations{$station}{'lyng_id'});
                }
            }
            print "\n";
        }
    # Cleanup
        $insh->finish;
        $updh->finish;
        $qsh->finish;
    }

# Calls wget to retrieve a URL, and returns some helpful information along with
# the page contents.  Usage: wget(url, [referrer])
    sub wget {
        my ($url, $referer) = @_;
    # Set up the HTTP::Request object
        my $req = HTTP::Request->new('GET', $url);
        $req->referer($referer) if ($referer);
    # Make the request
        my $ua = LWP::UserAgent->new(keep_alive => 1);
        $ua->agent($UserAgent);
        my $response = $ua->request($req);
    # Return the results
        return undef unless ($response->is_success);
        return $response->content;
    }

# Keeps the database schema up to date
    sub db_update {
        my $db_vers = 0;
        if (-s "$db_file" > 0) {
            my $sh = $dbh->prepare('SELECT MAX(vers) FROM db_vers');
            $sh->execute();
            ($db_vers) = $sh->fetchrow_array();
            $sh->finish;
        }
    # Leave early if we're up to date
        return if ($db_vers == 1);
    # No database
        if ($db_vers < 1) {
            $dbh->do('CREATE TABLE db_vers (
                        vers        INT
                      )');
            $dbh->do('CREATE TABLE callsigns (
                        callsign    TEXT PRIMARY KEY,
                        xmltvid     TEXT,
                        station_id  INT
                      )');
            $dbh->do('CREATE INDEX station_ids ON callsigns (station_id)');
            # Need to use the full INTEGER name here or sqlite won't treat it
            # like ROWID:  http://www.sqlite.org/autoinc.html
            $dbh->do('CREATE TABLE stations (
                        station_id  INTEGER PRIMARY KEY,
                        name        TEXT,
                        lyng_id     INT,
                        icon        TEXT,
                        logo        TEXT,
                        enabled     BOOL
                      )');
            $dbh->do('CREATE INDEX names ON stations (name, lyng_id)');
        # Increment the db version
            $db_vers++;
            $dbh->do('DELETE FROM db_vers');
            $dbh->do('INSERT INTO db_vers (vers) VALUES (?)', undef, $db_vers);
        }
    }

# Print the usage message
    sub print_usage {
        print <<EOF;

usage:  $0
options:

    --scan

        Scan lyngsat for updated station info

    --output

        Display the traditional breakdown of station, icon and log URLs.
        Use with the shell's > operator to store into a file like
        lingsat_stations.txt.

    --output missing

        Prints the ID and name fields for every lyngsat station that does not
        have any matching callsign entries.

    --usage

        Display this message

EOF
        exit;
    }
