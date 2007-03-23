#!/usr/bin/perl -w
#
# Scan channels for missing artwork, and download if found.
#
# Also does searches for artwork URLs based on callsign or xmltvid, or displays
# the master iconmap xml file from services.mythtv.org.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @copyright MythTV
# @license   GPL
#

# User agent (IE 6)
    our $UserAgent = 'MythTV Channel Icon lookup bot';

# Base data URL
    our $data_url = 'http://services.mythtv.org/channel-icon/';

# Includes
    use HTTP::Request;
    use LWP::UserAgent;
    use Fcntl qw(F_SETFD);
    use Getopt::Long;
    use MythTV;

# Autoflush
    $|++;

# Load cli arguments
    my ($find_missing, $rescan,  $icon_dir,
        $callsign,     $xmltvid, $iconmap,
        $usage);
    GetOptions(
            # Find missing icons
               'find-missing'   => \$find_missing,
               'rescan'         => \$rescan,
               'icon-dir=s'     => \$icon_dir,
            # Lookup
               'callsign=s'     => \$callsign,
               'xmltvid=s'      => \$xmltvid,
               'iconmap'        => \$iconmap,
            # Misc
               'usage|help'     => \&print_usage,
              );

# Directory exists/writable/etc.?
    unless ($icon_dir) {
        if ($ENV{'HOME'}) {
            $icon_dir = "$ENV{'HOME'}/.mythtv/channels";
        }
        elsif ($ENV{'USER'}) {
            $icon_dir = "/home/$ENV{'USER'}/.mythtv/channels";
        }
        else {
            die "Unable to determine home directory:  blank HOME and USER environment vars.\n";
        }
    }
    die "Icon directory $icon_dir is not a directory.\n" unless (-d $icon_dir);
    die "Icon directory $icon_dir is not a writable.\n"  unless (-w $icon_dir);

# Ping the server
    my $data = wget("$data_url/ping");
    if (!$data || $data !~ /^\d+$/m) {
        print "Unknown ping response from services.mythtv.org:\n\n$data\n\n",
              "Please try again later.\n";
        exit;
    }

# Callsign lookup?
    elsif ($callsign) {
        my ($url, $err) = station_lookup('callsign', $callsign);
        print "$url\n";
    }

# xmltvid lookup?
    elsif ($xmltvid) {
        my ($url, $err) = station_lookup('xmltvid', $xmltvid);
        print "$url\n";
    }

# Print the master iconmap?
    elsif ($iconmap) {
        print wget('http://services.mythtv.org/channel-icon/master-iconmap');
    }

# Find missing channel links
    elsif ($find_missing) {
    # Connect to mythbackend
        my $Myth = new MythTV();
    # Load MythTV's channels
        $Myth->load_channels();
    # First, scan through the channels to build a lookup list (this lets us do
    # a bulk lookup, instead of blasting the webserver with a bunch of
    # individual queries.
        my $chan_csv;
        my $searchcount = 0;
        foreach my $chanid (keys %{$Myth->{'channels'}}) {
            my $channel = $Myth->{'channels'}{$chanid};
        # Skip channels we already have icons for?
            next if ($channel->{'icon'} && !$rescan);
        # Just in case there's a newline or something that'll make the csv
        # reader on the other end get confused
            my $name = $channel->{'name'};
            $name =~ s/\s+/ /sg;
        # Add this line to the csv
            $searchcount++;
            $chan_csv .= join(',', escape_csv($chanid),
                                   escape_csv($name),
                                   escape_csv($channel->{'xmltvid'}),
                                   escape_csv($channel->{'callsign'}),
                                   escape_csv($channel->{'dtv_transportid'}),
                                   escape_csv($channel->{'atsc_major_chan'}),
                                   escape_csv($channel->{'atsc_minor_chan'}),
                                   escape_csv($channel->{'dtv_networkid'}),
                                   escape_csv($channel->{'serviceid'}),
                             )."\n";
        }
        my $data = wget("$data_url/findmissing", '', {'csv' => $chan_csv});
        unless ($data =~ s/#\s*$//s) {
            print "No data was returned from your channel search.\n",
                  "Please try again later.\n";
            exit;
        }
    # Parse out all of our channel info
        my %matches;
        my $count = 0;
        foreach my $line (split /\n/, $data) {
            $count++;
            my ($chanid, $type, $id, $name, $url) = extract_csv($line);
            $matches{$chanid}{$type} = {'id'   => $id,
                                        'name' => $name,
                                        'url'  => $url};
        }
    # Notify the user about what's going to happen
        my $white = ' ' x (29 - length($count) - length($searchcount));
        print <<EOF;

+----------------------------------------------------------------------------+
|                                                                            |
|  MythTV.org returned info about $count of $searchcount channels.$white|
|                                                                            |
|  You will now be prompted to choose the correct icon URL for each of your  |
|  channels.  You will also have the opportunity to transmit your choices    |
|  back to mythtv.org so that others can benefit from your selections.       |
|                                                                            |
+----------------------------------------------------------------------------+

EOF
    # Prepare a db query
        my $sh = $Myth->{'dbh'}->prepare('UPDATE channel
                                             SET icon   = ?
                                           WHERE chanid = ?');
    # Parse our known channels to find matches, and keep track of the
    # results so they can be submitted back to the webserver.
        my $match_csv;
        $count = 0;
        foreach my $chanid (sort { $Myth->{'channels'}{$a}{'name'} cmp $Myth->{'channels'}{$b}{'name'} } keys %{$Myth->{'channels'}}) {
            my $channel = $Myth->{'channels'}{$chanid};
        # Skip invisible channels
            next unless ($channel->{'visible'});
        # Skip channels we already have icons for?
            next if ($channel->{'icon'} && !$rescan);
        # Load the fuzzy matches
            my $fuzzy = $matches{$chanid};
        # Perfect xmltv match?
            if ($fuzzy->{'xmltvid'}) {
            # Download the icon
                $data = wget($fuzzy->{'xmltvid'}{'url'});
                if ($data) {
                    my ($img) = $fuzzy->{'xmltvid'}{'url'} =~ /([^\/]+)$/;
                    if (open ICON, ">$icon_dir/$img") {
                        print ICON $data;
                        close ICON;
                    # Update the DB
                        $sh->execute("$icon_dir/$img", $channel->{'chanid'});
                    }
                    else {
                        print STDERR "Error writing icon file $icon_dir/$img :  $1\n";
                    }
                }
                else {
                    print STDERR "Error downloading icon: $fuzzy->{'xmltvid'}{'url'}";
                }
                $count++;
                next;
            }
        # Interactively search through the icons
            my ($icon, $icon_csv);
            while (1) {
                $icon = search_icons($channel->{'name'},
                                     "Found unrecognized channel:  #$channel->{'channum'} / $channel->{'callsign'} / $channel->{'name'}",
                                     $fuzzy);
                print "\n";
                last unless ($icon);
            # Build a csv string for this icon
                $icon_csv .= join(',', escape_csv($icon->{'id'}),
                                       escape_csv($channel->{'name'}),
                                       escape_csv($channel->{'xmltvid'}),
                                       escape_csv($channel->{'callsign'}),
                                       escape_csv($channel->{'dtv_transportid'}),
                                       escape_csv($channel->{'atsc_major_chan'}),
                                       escape_csv($channel->{'atsc_minor_chan'}),
                                       escape_csv($channel->{'dtv_networkid'}),
                                       escape_csv($channel->{'serviceid'})
                                 )."\n";
            # Make sure that the requested choice isn't currently blocked
                my $blocks = is_blocked($icon_csv);
                if ($blocks) {
                    print "This combination of channel and icon has been blocked by the MythTV admins.\n",
                          "The most common reason for this is that there is a better match available.\n\n",
                          "Blocked:  $blocks\n\n";
                # Accept input
                    print 'Are you still sure that you want to use this icon?  ';
                    my $choice = <STDIN>;
                    print "\n";
                    $icon = undef unless ($choice =~ /^\s*[yt1]/i)
                }
                last if ($icon);
            }
        # Exit the program?
            last unless (defined $icon);
        # Skipped icon?
            next unless ($icon);
        # Keep track of this match so we can submit it to the server later.
            $match_csv .= $icon_csv;
        # Download the icon
            $data = wget($icon->{'url'});
            if ($data) {
                my ($img) = $icon->{'url'} =~ /([^\/]+)$/;
                if (open ICON, ">$icon_dir/$img") {
                    print ICON $data;
                    close ICON;
                # Update the DB
                    $sh->execute("$icon_dir/$img", $channel->{'chanid'});
                }
                else {
                    print STDERR "Error writing icon file $icon_dir/$img :  $1\n";
                }
            }
            else {
                print STDERR "Error downloading icon: $fuzzy->{'xmltvid'}{'url'}";
            }
            $count++;
        }
    # Submit the found channels.
        if ($match_csv) {
            print "Submit channel information to mythtv.org?  ";
            my $choice = lc(<STDIN>);
            $choice = ($choice =~ /^y/) ? 1 : 0;
            if ($choice) {
                my $data = wget("$data_url/submit", '', {'csv' => $match_csv});
                if ($data =~ s/\s+#\s*$//s) {
                    if ($data =~ /^t:(\d+)$/m) {
                        print "Submitted $1 channels, mapping icons to:\n\n";
                        print "   xmltvid: $1\n" if ($data =~ /^x:(\d+)$/m && $1 > 0);
                        print "  callsign: $1\n" if ($data =~ /^c:(\d+)$/m && $1 > 0);
                        print "      atsc: $1\n" if ($data =~ /^a:(\d+)$/m && $1 > 0);
                        print "       dvb: $1\n" if ($data =~ /^d:(\d+)$/m && $1 > 0);
                        print "\nThank you.\n";
                    }
                    else {
                        print "No channel icons were accepted by the server.\n",
                              "Thank you for trying, though.\n";
                    }
                }
                else {
                    print "No data was returned from your channel submission.\n",
                          "Thank you for trying, though.\n";
                    exit;
                }
            }
        }
    # Report back to the user
        print "Imported $count channel icons into your MythTV database.\n";
    # Cleanup
        $sh->finish;
    }

# Otherwise, just print the usage message
    else {
        print_usage();
    }

# Done
    exit;

################################################################################

# Check if a particular combination of icon and channel have been blocked
    sub is_blocked {
        my $csv = shift;
        $data = wget("$data_url/checkblock", '', {'csv' => $csv});
        if ($data =~ /\w/ && $data =~ s/\s+#\s*$//s) {
            $data =~ s/\W+/,/s;
            return $data;
        }
        return '';
    }

# Search the web for a specific icon
    sub station_lookup {
        my $field = shift;
        my %params;
        if ($field =~ /xmltvid|callsign/i) {
            %params = ( $field => shift );
        }
        elsif ($field =~ /atsc/) {
            %params = ( 'transportid' => shift,
                        'major_chan'  => shift,
                        'minor_chan'  => shift,
                      );
        }
        elsif ($field =~ /dvb/) {
            %params = ( 'transportid' => shift,
                        'networkid'   => shift,
                        'serviceid'   => shift,
                      );
        }
        else {
            die "Unknown field $field passed to station_lookup()\n";
        }
    # Lookup, parse and return
        $data = wget("$data_url/lookup", '', \%params);
        if ($data =~ /^ERROR:\s/) {
            return ($data, 1);
        }
        else {
            my ($type, $url, $id, $name) = extract_csv($data);
            return ($url);
        }
    }

# Search icons interactively for a specified string
    sub search_icons {
        my $string = shift;
        my $prompt = shift;
        my $fuzzy  = shift;
    # Prompt about the fuzzy matches?
        if (ref $fuzzy eq 'HASH' && keys %{$fuzzy}) {
            print "$prompt\n    Recommended matches:\n\n";
        # Print the list of possible matches, and keep track
            my %hash = ();
            $i = 0;
            foreach my $key (sort { $fuzzy->{$a}{'name'} cmp $fuzzy->{$a}{'name'} } keys %{$fuzzy}) {
                $i++;
                $hash{$i} = $key;
                my $match = $fuzzy->{$key};
                print " $i) $match->{'name'}\n",
                      "    $match->{'url'}\n\n";
            }
        # Along with the manual options
            print " M) More options\n",
                  " S) Skip This Channel\n",
                  " E) Exit\n\n";
        # Accept input
            my $choice;
            while (1) {
                print 'Choice:  ';
                $choice = lc(<STDIN>);
                $choice =~ s/^\s+//;
                $choice =~ s/\s+$//;
            # Skip?
                return 0 if ($choice eq 's');
            # Exit completely?
                return undef if ($choice eq 'e');
            # Selected an icon
                return $fuzzy->{$hash{$choice}} if ($hash{$choice});
            # Other options
                last if ($choice eq 'm');
            }
        }
    # Loop and search until we get a good response
        my $manual;
        while (1) {
            print "$prompt\n";
            if ($manual) {
                print "    Manually searching for:  $manual\n";
            }
            print "\n";
        # Search
            my %matches;
            my $count = 0;
            my $data = wget("$data_url/search", '', {'s' => $manual ? "%$manual" : $string});
            if ($data =~ /\w/ && $data =~ s/\s+#\s*$//s) {
            # Parse out all of our channel info
                foreach my $line (split /\n/, $data) {
                    $count++;
                    my ($id, $name, $url) = extract_csv($line);
                    $matches{$count} = {'id'   => $id,
                                        'name' => $name,
                                        'url'  => $url};
                }
            }
        # Print the list of possible matches, and keep track
            my %hash = ();
            $i = 0;
            if ($count) {
                foreach my $key (sort { $matches{$a}{'name'} cmp $matches{$b}{'name'}; } keys %matches) {
                    $i++;
                    $hash{$i} = $key;
                    my $match = $matches{$key};
                    print ' ' x (length($count) - length($i)),
                          "$i) $match->{'name'}\n",
                          ' ' x (length($count) + 2),
                          "$match->{'url'}\n\n";
                }
            }
            else {
                print "No matching icons were found.\n\n";
            }
        # Along with the manual options
            if ($manual) {
                print ' ' x (length($count) - 1),
                      "D) Default Search\n";
            }
            print ' ' x (length($count) - 1),
                  "M) Manual Search\n",
                  ' ' x (length($count) - 1),
                  "S) Skip This Channel\n",
                  ' ' x (length($count) - 1),
                  "E) Exit\n\n";
        # Accept input
            my $choice;
            while (1) {
                print 'Choice:  ';
                $choice = lc(<STDIN>);
                $choice =~ s/^\s+//;
                $choice =~ s/\s+$//;
            # Skip?
                return 0 if ($choice eq 's');
            # Exit completely?
                return undef if ($choice eq 'e');
            # Selected an icon
                return $matches{$hash{$choice}} if ($hash{$choice});
            # Other options
                last if ($choice =~ /^[dm]$/);
            }
        # Back to the default search
            if ($choice =~ /d/i) {
                undef $manual;
            }
        # Manual search?  Prompt for the new search term
            if ($choice =~ /[gm]/i) {
                while (1) {
                    print 'Search for:  ';
                    $manual = <STDIN>;
                    $manual =~ s/^\s+//;
                    $manual =~ s/\s+$//;
                    last if ($manual =~ /\w/);
                }
            }
        # Add a line break for clarification
            print "\n\n";
        }
    }

# Calls wget to retrieve a URL, and returns some helpful information along with
# the page contents.  Usage: wget(url, [referrer])
    sub wget {
        my $url     = shift;
        my $referer = shift;
        my $params  = shift;
    # Set up the HTTP::Request object
        my $req;
        if ($params) {
            my $query;
        # Build the query
            foreach my $var (keys %{$params}) {
                $query .= '&' if ($query);
                $query .= urlencode($var).'='.urlencode($params->{$var});
            }
        # POST
            $req = HTTP::Request->new('POST', $url);
            $req->header('Content-Type'   => 'application/x-www-form-urlencoded',
                         'Content-Length' => length($query));
            $req->content($query);
        }
        else {
            $req = HTTP::Request->new('GET', $url);
        }
        $req->referer($referer) if ($referer);
    # Make the request
        my $ua = LWP::UserAgent->new(keep_alive => 1);
        $ua->agent($UserAgent);
        #$ua->timeout(300);
        my $response = $ua->request($req);
    # Return the results
        return undef unless ($response->is_success);
        return $response->content;
    }

# URL-encode a string
    sub urlencode {
        my $str = (shift or '');
        $str =~ s/([^\w*\.\-])/sprintf '%%%02x', ord $1/sge;
        return $str;
    }

# Escape a string for safe insertion into a csv file
    sub escape_csv {
        my $str = (shift or '');
        $str =~ s/"/""/g;
        return "\"$str\"";
    }

# Extract csv fields from a line
    sub extract_csv {
        my $line = shift;
        my @fields = ();
    # Clean up the line and split out the fields
        $line =~ s/\s*$/,/;                       # Convert end of lines to a comma so the next parser will work
        while ($line =~ m/(  (?!\")[^,]*            # Handle normal fields
                           | "(?:[\"\\]\"|.)*?"     # Handle quoted fields, escaped quotes as "" or \"
                           ),                       # End of line now has a comma, too
                         /mgx) {
            my $val = $1;
            $val = '' unless (defined $val);
        # Remove and unescape quotes
            if ($val =~ /^"(.+)"$/)  {
                if ($1 || length($val) > 0) {
                    $val = $1;
                }
                else {
                    $val = '';
                }
                $val =~ s/["\\]"/"/sg;
            }
        # Add the field to the list
            push @fields, $val;
        }
    # Return
        return @fields;
    }

# Print the usage message
    sub print_usage {
        print <<EOF;

usage:  $0 [options]

options:

    --find-missing

        Scan your database and download any missing channel icons.
        See also:  --rescan  --icon-dir

    --rescan

        Use in combination with --find-missing to rescan all of your channels
        for new icons.

    --icon-dir {path}

        Directory in which to store downloaded channel icons.  Used in
        combination with --find-missing.  Defaults to ~/.mythtv/channels/

    --callsign {string}

        Display the icon URL for the specified callsign.

    --xmltvid {string}

        Display the icon URL for the specified xmltvid.

    --iconmap

        Display the master iconmap from:
        http://services.mythtv.org/channel-icon/master-iconmap

    --usage

        Display this message

EOF
        exit;
    }
