#!/usr/bin/perl -w
#Last Updated: 2004.10.03 (xris)
#
#   mythtv::recordings
#
#   Load the available recordings/shows, and perform any
#

package mythtv::recordings;

    use DBI;
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use mythtv::db;
    use mythtv::nuvinfo;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &load_finfo &load_recordings $video_dir @Files %Shows /;

    # These are available for export, but for the most part should only be needed here
        our @EXPORT_OK = qw/ &generate_showtime $num_shows /;
    }

# Variables we intend to export
    our ($video_dir, @Files, %Shows, $num_shows);

# Autoflush buffers
    $|++;

# Make sure we have the db filehandle
    die "Not connected to the database.\n" unless ($dbh);

#
#   load_recordings:
#   Load all known recordings
#
    sub load_recordings {
    # Let the user know what's going on
        clear();
        print "Loading MythTV recording info.\n";

    # Query variables we'll use below
        my ($sh, $sh2, $sh3, $q, $q2, $q3);

    # Find the directory where the recordings are located
        $q = "SELECT data FROM settings WHERE value='RecordFilePrefix' AND hostname=?";
        $sh = $dbh->prepare($q);
            $sh->execute($hostname) or die "Could not execute ($q):  $!\n\n";
        ($video_dir) = $sh->fetchrow_array;
        die "This host not configured for myth.\n(No RecordFilePrefix defined for $hostname in the settings table.)\n\n" unless ($video_dir);
        die "Recordings directory $video_dir doesn't exist!\n\n" unless (-d $video_dir);
        $video_dir =~ s/\/+$//;

    # Grab all of the video filenames
        opendir(DIR, $video_dir) or die "Can't open $video_dir:  $!\n\n";
        @Files = grep /\.nuv$/, readdir(DIR);
        closedir DIR;
        die "No recordings found!\n\n" unless (@Files);

    # Parse out the record data for each file
        $q  = "SELECT title, subtitle, description, hostname, cutlist FROM recorded WHERE chanid=? AND starttime=? AND endtime=?";
        $q2 = "SELECT mark FROM recordedmarkup WHERE chanid=? AND starttime=? AND type=6 ORDER BY mark DESC LIMIT 1";
        $q3 = "SELECT mark FROM recordedmarkup WHERE chanid=? AND starttime=? AND type=9 ORDER BY mark DESC LIMIT 1";

        $sh  = $dbh->prepare($q);
        $sh2 = $dbh->prepare($q2);
        $sh3 = $dbh->prepare($q3);

        my $count;
        foreach my $file (@Files) {
            next unless ($file =~ /\.nuv$/);
            next if ($file =~ /^ringbuf/);
            $count++;
        # Print a progress dot
            print "\r", sprintf('%.0f', 100 * ($count / @Files)), '% ';
        # Pull out the various parts that make up the filename
            my ($channel,
                $syear, $smonth, $sday, $shour, $sminute, $ssecond,
                $eyear, $emonth, $eday, $ehour, $eminute, $esecond) = $file =~/^([a-z0-9]+)_(....)(..)(..)(..)(..)(..)_(....)(..)(..)(..)(..)(..)\.nuv$/i;
        # Found a bad filename?
            unless ($channel) {
                print "Unknown filename format:  $file\n";
                next;
            }
        # Execute the query
            $sh->execute($channel, "$syear$smonth$sday$shour$sminute$ssecond", "$eyear$emonth$eday$ehour$eminute$esecond")
                or die "Could not execute ($q):  $!\n\n";
            my ($show, $episode, $description, $show_hostname, $cutlist) = $sh->fetchrow_array;
        # Skip shows without cutlists?
            next if (arg('require_cutlist') && !$cutlist);
        # Unknown file - someday we should report this
            next unless ($show);
            $sh2->execute($channel, "$syear$smonth$sday$shour$sminute$ssecond")
                or die "Could not execute ($q2):  $!\n\n";
            my ($lastgop) = $sh2->fetchrow_array;
            my $goptype = 6;

            if( !$lastgop ) {
                $sh3->execute($channel, "$syear$smonth$sday$shour$sminute$ssecond")
                    or die "Could not execute ($q3):  $!\n\n";
                ($lastgop) = $sh3->fetchrow_array;
                $goptype = 9;
            }
        # Defaults
            $episode     = 'Untitled'       unless ($episode =~ /\S/);
            $description = 'No Description' unless ($description =~ /\S/);
        #$description =~ s/(?:''|``)/"/sg;
            push @{$Shows{$show}}, {'filename'       => "$video_dir/$file",
                                    'channel'        => $channel,
                                    'start_time'     => "$syear$smonth$sday$shour$sminute$ssecond",
                                    'end_time'       => "$eyear$emonth$eday$ehour$eminute$esecond",
                                    'start_time_sep' => "$syear-$smonth-$sday-$shour-$sminute-$ssecond",
                                    'show_name'      => ($show          or ''),
                                    'title'          => ($episode       or ''),
                                    'description'    => ($description   or ''),
                                    'hostname'       => ($show_hostname or ''),
                                    'cutlist'        => ($cutlist       or ''),
                                    'lastgop'        => ($lastgop       or 0),
                                    'goptype'        => ($goptype       or 0),
                                    'showtime'       => generate_showtime($syear, $smonth, $sday, $shour, $sminute, $ssecond),
                                   # This field is too slow to populate here, so it will be populated in ui.pm on-demand
                                    'finfo'          => undef
                                   };
        }
        $sh->finish();
        print "\n";

    # We now have a hash of show names, containing an array of episodes
    # We should probably do some sorting by timestamp (and also count how many shows there are)
        $num_shows = 0;
        foreach my $show (sort keys %Shows) {
            @{$Shows{$show}} = sort {$a->{start_time} <=> $b->{start_time} || $a->{channel} <=> $b->{channel}} @{$Shows{$show}};
            $num_shows++;
        }

    # No shows found?
        die 'Found '.@Files." files, but no matching database entries.\n\n" unless ($num_shows);
    }


    sub load_finfo {
        my $episode = shift;
        return if ($episode->{'finfo'});
        %{$episode->{'finfo'}} = nuv_info($episode->{'filename'});
    }

#
#   generate_showtime:
#   Returns a nicely-formatted timestamp from a specified time
#
    sub generate_showtime {
        $showtime = '';
    # Get the requested date
        my ($year, $month, $day, $hour, $minute, $second) = @_;
        $month = int($month);
        $day   = int($day);
    # Get the current time, so we know whether or not to display certain fields (eg. year)
        my ($this_second, $this_minute, $this_hour, $ignore, $this_month, $this_year) = localtime;
        $this_year += 1900;
        $this_month++;
    # Default the meridian to AM
        my $meridian = 'AM';
    # Generate the showtime string
        $showtime .= "$month/$day";
        $showtime .= "/$year" unless ($year == $this_year);
        if ($hour == 0) {
            $hour = 12;
        }
        elsif ($hour > 12) {
            $hour -= 12;
            $meridian = 'PM';
        }
        $showtime .= ", $hour:$minute $meridian";
    # Return
        return $showtime;
    }

# vim:ts=4:sw=4:ai:et:si:sts=4
