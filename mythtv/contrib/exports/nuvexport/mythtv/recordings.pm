#
# $Date$
# $Revision$
# $Author$
#
#   mythtv::recordings
#
#   Load the available recordings/shows, and perform any
#

package mythtv::recordings;

    use DBI;
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use mythtv::nuvinfo;
    use Date::Manip;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &load_finfo &load_recordings %Shows /;

    # These are available for export, but for the most part should only be needed here
        our @EXPORT_OK = qw/ $num_shows /;
    }

# Variables we intend to export
    our (%Shows, $num_shows);

# Autoflush buffers
    $|++;

# Load the following extra parameters from the commandline
    add_arg('date:s',       'Date format used for human-readable dates.');
    add_arg('show_deleted', 'Show recordings in the Deleted recgroup.');
    add_arg('show_livetv',  'Show recordings in the LiveTV recgroup.');

#
#   Load all known recordings
#
    sub load_recordings {
        my $Myth = shift;
    # Make sure we have the db filehandle
        die "Not connected to mythbackend.\n" unless ($Myth);

    # Let the user know what's going on
        clear();
        print "Loading MythTV recording info.\n";

    # Load the list
        my %rows = $Myth->backend_rows('QUERY_RECORDINGS Delete');

    # Nothing?!
        die "No valid recordings found!\n\n" unless (defined $rows{'rows'} && @{$rows{'rows'}});

        $num_shows = $count = 0;
        foreach $file (@{$rows{'rows'}}) {
            $count++;
        # Print the progress indicator
            print "\r", sprintf('%.0f', 100 * ($num_shows / @{$rows{'rows'}})), '% ' unless (arg('noprogress'));
        # Load into an object
            $file = $Myth->new_recording(@$file);
        # Skip shows without cutlists?
            if (arg('require_cutlist') && !$file->{'has_cutlist'}) {
                if (arg('infile')) {
                    print STDERR "WARNING:  Overriding --require_cutlist because file was requested with --infile.\n";
                }
                else {
                    next;
                }
            }
        # Skip files that aren't local (until file info is stored in the db)
            next unless ($file->{'local_path'} && -e $file->{'local_path'});
        # Skip files in 'Deleted' recgroup
            next if ($file->{'recgroup'} eq 'Deleted' && !arg('show_deleted'));
        # Skip files in 'Deleted' recgroup
            next if ($file->{'recgroup'} eq 'LiveTV' && !arg('show_livetv'));
        # Defaults
            $file->{'title'}       = 'Untitled'       unless ($file->{'title'} =~ /\S/);
            $file->{'subtitle'}    = 'Untitled'       unless ($file->{'subtitle'} =~ /\S/);
            $file->{'description'} = 'No Description' unless ($file->{'description'} =~ /\S/);
        # Get a user-configurable showtime string
            if (arg('date')) {
                $file->{'showtime'} = UnixDate("epoch $file->{'starttime'}", arg($date));
            }
            else {
                $file->{'showtime'} = UnixDate("epoch $file->{'starttime'}", '%a %b %e %I:%M %p');
            }
        # Add to the list
            push @{$Shows{$file->{'title'}}}, $file;
        # Counter
            $num_shows++;
        }
        print "\n";

    # No shows found?
        unless ($num_shows) {
            die "Found $count files, but no matching database entries.\n"
                .(arg('require_cutlist') ? "Perhaps you should try disabling require_cutlist?\n" : '')
                ."\n";
        }

    # We now have a hash of show names, containing an array of episodes
    # We should probably do some sorting by timestamp (and also count how many shows there are)
        foreach my $show (sort keys %Shows) {
            @{$Shows{$show}} = sort {$a->{'starttime'} <=> $b->{'starttime'} || $a->{'callsign'} cmp $b->{'callsign'}} @{$Shows{$show}};
        }
    }

    sub load_finfo {
        my $show = shift;
        return if ($show->{'finfo'});
        $show->load_file_info();
    # Aspect override?
        if ($exporter->val('force_aspect')) {
            $show->{'finfo'}{'aspect'}   = aspect_str($exporter->val('force_aspect'));
            $show->{'finfo'}{'aspect_f'} = aspect_float($exporter->val('force_aspect'));
        }
    }

# vim:ts=4:sw=4:ai:et:si:sts=4
