#!/usr/bin/perl -w
#Last Updated: 2005.02.16 (xris)
#
#   nuvexport::ui
#
#   General user interface for nuvexport (show name, universal transcode parameters, etc.)
#

package nuv_export::ui;

    use File::Path;

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use mythtv::recordings;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &load_episodes  &load_cli_episodes
                          &query_savepath &query_exporters    &query_text
                        /;
    }

# Load episodes from the commandline
    sub load_cli_episodes {
        my @matches;
    # Performing a search
        if (arg('title') || arg('subtitle') || arg('description')) {
            foreach my $show (sort keys %Shows) {
            # Title search?
                my $title = arg('title');
                next unless (!$title || $show =~ /$title/si);
            # Search episodes
                foreach my $episode (@{$Shows{$show}}) {
                    my $subtitle    = arg('subtitle');
                    my $description = arg('description');
                    next unless (!$subtitle    || $episode->{'title'}       =~ /$subtitle/si);
                    next unless (!$description || $episode->{'description'} =~ /$description/si);
                    push @matches, $episode;
                }
            }
        # No matches?
            die "\nNo matching shows were found.\n\n" unless (@matches);
        }
    # Look for a filename or channel/starttime
        else {
            my $chanid    = arg('chanid');
            my $starttimg = arg('starttime');
        # Filename specified on the command line -- extract the chanid and starttime
            if (arg('infile')) {
                if (arg('infile') =~ /\b(\d+)_(\d+)_\d+\.nuv$/) {
                    $chanid    = $1;
                    $starttime = $2;
                }
                else {
                    die "Input filename does not match the MythTV recording name format.\n"
                       ."Please reference only files in your active MythTV video directory.\n";
                }
            }
        # Find the show
            if ($starttime || $chanid) {
            # We need both arguments
                die "Please specify both a starttime and chanid.\n" unless ($starttime && $chanid);
            # Make sure the requested show exists
                foreach my $show (sort keys %Shows) {
                    foreach my $episode (@{$Shows{$show}}) {
                        next unless ($chanid == $episode->{'channel'} && $starttime == $episode->{'start_time'});
                        push @matches, $episode;
                        last;
                    }
                    last if (@matches);
                }
            # Not found?
                die "Couldn't find a show on chanid $chanid at the specified time.\n" unless (@matches);
            }
        }
    # Only searching, not processing
        if (arg('search-only')) {
            if (@matches) {
                print "\nMatching Shows:\n\n";
                foreach my $episode (@matches) {
                    print "     title:  $episode->{'show_name'}\n",
                          "  subtitle:  $episode->{'title'}\n",
                          "    chanid:  $episode->{'channel'}\n",
                          "    starts:  $episode->{'start_time'}\n",
                          "      ends:  $episode->{'end_time'}\n",
                          "  filename:  $video_dir/$episode->{'filename'}\n\n";
                }
            }
            else {
                print "No matching shows were found.\n";
            }
            exit;
        }
    # Return
        return @matches;
    }
#
# Looks for matching episodes from commandline arguments, or queries the user
# Returns an array of matching episodes.
#
    sub load_episodes {
    # Keep track of the matching shows -- and grab any that are passed back in
        my @matches = @_;
    # We didn't find enough info on the commandline, so it's time to interact with the user
        my $stage   = @matches ? 'v' : 's';
        my $show    = undef;
        my $episode = undef;
        while ($stage ne 'x') {
        # Clear the screen
            clear();
        # Stage "quit" means, well, quit...
            if ($stage eq 'q') {
                exit;
            }
        # Are we asking the user which show to encode?
            if (!@matches && $stage ne 'v' && !$show || $stage eq 's') {
                my $tmp_show;
                ($stage, $tmp_show) = &query_shows(@matches);
                if ($tmp_show) {
                    $show  = $tmp_show;
                    $stage = 'e';
                }
            }
        # Nope.  What about the episode choice?
            elsif (!@matches || $stage eq 'e') {
                my @episodes;
                ($stage, @episodes) = &query_episodes($show, @matches);
                if (@episodes) {
                    push @matches, @episodes;
                    $stage = 'v';
                }
            }
        # Now we probably want to verify the chosen shows
            else {
                ($stage, @matches) = &confirm_shows(@matches);
            }
        }
    # Return the matched episodes
        return @matches;
    }

    sub query_shows {
        my @episodes = @_;
    # Build the query
        my ($count, @show_choices);
        my $query     = "\nYou have recorded the following shows:\n\n";
        my @shows     = sort keys %Shows;
        my $num_shows = @shows;
        foreach $show (@shows) {
        # Figure out if there are any episodes of this program that haven't already been selected
            my $num_episodes = @{$Shows{$show}};
            foreach my $selected (@episodes) {
                next unless ($show eq $selected->{'show_name'});
                $num_episodes--;
                last if ($num_episodes < 1);
            }
            next unless ($num_episodes > 0);
        # Increment the counter
            $count++;
        # Print out this choice, adjusting space where necessary
            $query .= '  ';
            $query .= ' ' if ($num_shows > 10  && $count < 10);
            $query .= ' ' if ($num_shows > 100 && $count < 100);
            $query .= "$count. ";
        # print out the name of this show, and an episode count
            $query .= "$show ($num_episodes episode".($num_episodes == 1 ? '' : 's').")\n";
            $show_choices[$count-1] = $show;
        }
    # Show the link to "verify"?
        $query .= "\n  d. Done selecting shows" if (@episodes);
    # Offer the remaining option, and the choice
        $query .= "\n  q. Quit\n\nChoose a show: ";
    # Query the user
        my $choice = query_text($query, 'string', '');
    # Another stage?
        return 'q' if ($choice =~ /^\W*q/i);           # Quit
        return 'v' if ($choice =~ /^\W*[dv]/i);        # Verify
    # Make sure the user made a valid show choice, and then move on to the next stage
        $choice =~ s/^\D*/0/s;  # suppress warnings
        if ($choice > 0 && $show_choices[$choice-1]) {
            return ('e', $show_choices[$choice-1]);
        }
    # No valid choice -- repeat this question
        return 's';
    }

    sub query_episodes {
        my $show         = shift;
        my @episodes     = @_;
        my $num_episodes = @{$Shows{$show}};
    # Only one episode recorded?
        return ('v', $Shows{$show}->[0]) if ($num_episodes == 1);
    # Define a newline + whitespace so we can tab out extra lines of episode description
        my $newline = "\n" . ' ' x (4 + length $num_episodes);
    # Build the query
        my $query = "\nYou have recorded the following episodes of $show:\n\n";
        my ($count, @episode_choices);
        foreach $episode (@{$Shows{$show}}) {
        # Figure out if there are any episodes of this program that haven't already been selected
            my $found = 0;
            foreach my $selected (@episodes) {
                next unless ($selected->{'channel'} == $episode->{'channel'}
                             && $selected->{'start_time'} eq $episode->{'start_time'});
                $found = 1;
                last;
            }
            next if ($found);
        # Increment the counter
            $count++;
        # Make sure we have finfo
            load_finfo($episode);
        # Print out this choice, adjusting space where necessary
            $query .= '  ';
            $query .= ' ' if ($num_episodes > 10  && $count < 10);
            $query .= ' ' if ($num_episodes > 100 && $count < 100);
            $query .= "$count. ";
        # Print out the show name
            $query .= "$episode->{'title'} " if ($episode->{'title'} && $episode->{'title'} ne 'Untitled');
            $query .= "($episode->{'showtime'}) ".$episode->{'finfo'}{'width'}.'x'.$episode->{'finfo'}->{'height'}.' '.$episode->{'finfo'}{'video_type'};
            $query .= wrap($episode->{'description'}, 80, $newline, '', $newline) if ($episode->{'description'});
            $query .= "\n";
            $episode_choices[$count-1] = $episode;
        }
    # Instructions
        $query .= "\n* Separate multiple episodes with spaces, or * for all shows.\n";
    # Show the link to "verify"?
        $query .= "\n  d. Done selecting shows" if (@episodes);
    # Offer the remaining options, and the choice
        $query .= "\n  r. Return to shows menu\n  q. Quit\n\nChoose a function, or desired episode(s): ";
    # Query the user
        my $choice = query_text($query, 'string', '');
    # Another stage?
        return 'q' if ($choice =~ /^\W*q/i);       # Quit
        return 's' if ($choice =~ /^\W*[rb]/i);    # Show
        return 'v' if ($choice =~ /^\W*[dv]/i);    # Verify
    # All episodes?
        if ($choice =~ /^\s*\*+\s*$/) {
            my @matches;
            foreach my $episode (@episode_choices) {
                push @matches, $episode;
            }
            return ('v', @matches) if (@matches);
        }
    # Make sure the user made a valid episode choice, and then move on to the next stage
        elsif ($choice =~ /\d/) {
            my @matches;
            foreach my $episode (split(/\D+/, $choice)) {
                next unless ($episode > 0 && $episode_choices[$episode-1]);
                push @matches, $episode_choices[$episode-1];
            }
            return ('v', @matches) if (@matches);
        }
    # No valid choice -- repeat this question
        return 'e';
    }

    sub confirm_shows {
        my @episodes = @_;
        my $num_episodes = @episodes;
    # Define a newline + whitespace so we can tab out extra lines of episode description
        my $newline = "\n" . ' ' x (4 + length $num_episodes);
    # Build the query
        my $count = 0;
        my $query = "\nYou have chosen to export $num_episodes episode" . ($num_episodes == 1 ? '' : 's') . ":\n\n";
        foreach my $episode (@episodes) {
            $count++;
        # Make sure we have finfo
            load_finfo($episode);
        # Print out this choice, adjusting space where necessary
            $query .= '  ';
            $query .= ' ' if ($num_episodes > 10 && $count < 10);
            $query .= ' ' if ($num_episodes > 100 && $count < 100);
            $query .= "$count. ";
        # Print out the show name
            $query .= "$episode->{'show_name'}:$newline";
            $query .= "$episode->{'title'} " if ($episode->{'title'} && $episode->{'title'} ne 'Untitled');
            $query .= "($episode->{'showtime'}) ".$episode->{'finfo'}{'width'}.'x'.$episode->{'finfo'}->{'height'}.' '.$episode->{'finfo'}{'video_type'};
            $query .= wrap($episode->{'description'}, 80, $newline, '', $newline) if ($episode->{'description'});
            $query .= "\n";
        }
    # Instructions
        $query .= "\n* Separate multiple episodes with spaces\n";
    # Offer the remaining options, and the choice
        $query .= "\n  c. Continue\n  n. Choose another show\n  q. Quit\n\nChoose a function, or episode(s) to remove: ";
    # Query the user
        my $choice = query_text($query, 'string', '');
    # Another stage?
        return 'q'              if ($choice =~ /^\W*q/i);       # Quit
        return ('x', @episodes) if ($choice =~ /^\W*c/i);       # Continue
        return ('s', @episodes) if ($choice =~ /^\W*[ns]/i);    # Choose another show
    # Make sure the user made a valid episode choice, and then move on to the next stage
        if ($choice =~ /\d/) {
            foreach my $episode (split(/\D+/, $choice)) {
                next unless ($episode > 0 && $episodes[$episode-1]);
                $episodes[$episode-1] = undef;
            }
            @episodes = grep { $_ } @episodes;
            return ('v', @episodes) if (@episodes);
            return ('s', @episodes);
        }
    # No valid choice -- repeat this question
        return ('v', @episodes);
    }

    sub query_exporters {
        my $export_prog = shift;

        die "No export modules were found!\n" unless (@Exporters);
    # Load the mode from the commandline?
        if (my $mode = arg('mode', ($PROGRAM_NAME =~ /^.*?nuvexport-([\w\-]+)$/) ? $1 : undef)) {
            foreach my $exporter (@Exporters) {
                next unless ($mode =~ /$exporter->{'cli'}/);
                return $exporter;
            }
            print "Unknown exporter mode:  $mode\n";
            exit -1;
        }
    # Build the query
        my $query = "Using $export_prog for exporting.\n"
                  . "What would you like to do?\n\n";
    # What are our function options?
        my ($count);
        foreach my $exporter (@Exporters) {
            $count++;
            $query .= (' ' x (3 - length($count)))."$count. ".$exporter->{'name'};
            $query .= ' (disabled)' unless ($exporter->{'enabled'});
            $query .= "\n";
        }
        $query .= "\n  q. Quit\n\nChoose a function: ";
    # Repeat until we have a valid response
        while (1) {
        # Clear the screen
            clear();
        # Query the user
            my $choice = query_text($query, 'string', '');
        # Quit?
            exit if ($choice =~ /^\W*q/i);
        # Suppress warnings
            $choice =~ s/^\D*/0/s;
        # Make sure that this function is enabled
            if (!$Exporters[$choice-1]->{'enabled'}) {
                if ($Exporters[$choice-1]->{'errors'} && @{$Exporters[$choice-1]->{'errors'}}) {
                    print "\n", join("\n", @{$Exporters[$choice-1]->{'errors'}}), "\n";
                }
                else {
                    print 'Function "'.$Exporters[$choice-1]->{'name'}."\" is disabled.\n";
                }
                print "Press ENTER to continue.\n";
                <STDIN>;
            }
            elsif ($Exporters[$choice-1]->{'enabled'}) {
            # Store this choice in the database
                return $Exporters[$choice-1];
            }
        }
    }

    sub query_savepath {
#        unless ($self->{'path'}) {
#            die "here, we should be loading the savepath from the mythvideo info in the database\n";
#        }
    # No need?
        return undef unless (arg('noserver') || arg('path'));
    # Grab a path from the commandline?
        my $path = (arg('path') or '');
        $path =~ s/\/+$//s;
        if ($is_cli) {
        # Need a pathname
            die "Please specify an output directory path with --path\n" unless ($path);
        # Doesn't exist?
            die "$path doesn't exist.\n" unless (-e $path);
        # Not a valid directory?
            die "$path exists, but is not a directory.\n" unless (-d $path);
        # Make sure it's writable
            die "$path isn't writable.\n" unless (-w $path);
        }
    # Where are we saving the files to?
        until ($path && -d $path && -w $path) {
        # Query
            if (!$path) {
                $path = query_text('Where would you like to export the files to?', 'string', '.');;
                $path =~ s/\/+$//s;
            }
        # Doesn't exist - query the user to create it
            if (!-e $path) {
                my $create = query_text("$path doesn't exist.  Create it?", 'yesno', 'Yes');
                if ($create) {
                   mkpath($path, 1, 0711) or die "Couldn't create $path:  $!\n\n";
                }
                else {
                    $path = undef;
                }
            }
        # Make sure this is a valid directory
            elsif (!-d $path) {
                print "$path exists, but is not a directory.\n";
                $path = undef;
            }
        # Not writable
            elsif (!-w $path) {
                print "$path isn't writable.\n";
                $path = undef;
            }
        }
        return $path;
    }

    sub query_text {
        my $text          = shift;
        my $type          = shift;
        my $default       = shift;
        my $default_extra = shift;
        my $return = undef;
    # Loop until we get a valid response
        while (1) {
        # Ask the question, get the answer
            print $text,
                  ($default ? " [$default]".($default_extra ? $default_extra : '').' '
                            : ' ');
            chomp($return = <STDIN>);
        # Nothing typed, is there a default value?
            unless ($return =~ /\S/) {
                next unless (defined $default);
                $return = $default;
            }
        # Looking for a boolean/yesno response?
            if ($type =~ /yes.*no|bool/i) {
                return $return =~ /^\W*[nf0]/i ? 0 : 1;
            }
        # Looking for an integer?
            elsif ($type =~ /int/i) {
                $return =~ s/^\D*/0/;
                if ($return != int($return)) {
                    print "Whole numbers only, please.\n";
                    next;
                }
                return $return + 0;
            }
        # Looking for a float?
            elsif ($type =~ /float/i) {
                $return =~ s/^\D*/0/;
                return $return + 0;
            }
        # Well, then we must be looking for a string
            else {
                return $return;
            }
        }
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
