#
# $Date$
# $Revision$
# $Author$
#
#  generic.pm
#
#    generic routines for exporters
#

package export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;

    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use MythTV;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &fork_command &has_data &fifos_wait
                        /;
    }

# Load the following extra parameters from the commandline
    add_arg('path:s',                        'Save path (only used with the noserver option).');
    add_arg('filename|name:s',               'Format string for output names.');
    add_arg('underscores!',                  'Convert spaces to underscores for output filename.');
    add_arg('use_cutlist|cutlist!',          'Use the myth cutlist (or not).');
    add_arg('gencutlist!',                   'Generate a cutlist from the commercial flag list (don\'t forget --use_cutlist, too).');

# These aren't used by all modules, but the routine to define them is here, so here they live
    add_arg('height|v_res|h=s',              'Output height.');
    add_arg('width|h_res|w=s',               'Output width.');
    add_arg('deinterlace!',                  'Deinterlace video.');
    add_arg('noise_reduction|denoise|nr!',   'Enable noise reduction.');
    add_arg('fast_denoise|fast-denoise!',    'Use fast noise reduction instead of standard.');
    add_arg('nocrop',                        'Do not crop out broadcast overscan.');
    add_arg('crop_pct=f',                    'Percentage of overscan to crop (0-5%, defaults to 2%).');
    add_arg('crop_top=f',                    'Percentage of overscan to crop from the top. (0-20%)');
    add_arg('crop_right=f',                  'Percentage of overscan to crop from the right.');
    add_arg('crop_bottom=f',                 'Percentage of overscan to crop from the bottom. (0-20%)');
    add_arg('crop_left=f',                   'Percentage of overscan to crop from the left.');
    add_arg('out_aspect=s',                  'Force output aspect ratio.');

# Load defaults
    sub load_defaults {
        my $self = shift;
    # These defaults apply to all modules
        $self->{'defaults'}{'use_cutlist'}     = 1;
        $self->{'defaults'}{'noise_reduction'} = 1;
        $self->{'defaults'}{'deinterlace'}     = 1;
        $self->{'defaults'}{'crop_pct'}        = 2;
    # Disable all cropping
        if ($self->val('nocrop')) {
            $self->{'crop'}        = 0;
            $self->{'crop_pct'}    = 0;
            $self->{'crop_top'}    = 0;
            $self->{'crop_right'}  = 0;
            $self->{'crop_bottom'} = 0;
            $self->{'crop_left'}   = 0;
        }
    # Make sure the crop percentage is valid
        else {
        # Set any unset crop percentages
            unless (defined $self->val('crop_pct')) {
                $self->{'crop_pct'} = 0;
            }
            unless (defined $self->val('crop_top')) {
                $self->{'crop_top'} = $self->val('crop_pct');
            }
            unless (defined $self->val('crop_right')) {
                $self->{'crop_right'} = $self->val('crop_pct');
            }
            unless (defined $self->val('crop_bottom')) {
                $self->{'crop_bottom'} = $self->val('crop_pct');
            }
            unless (defined $self->val('crop_left')) {
                $self->{'crop_left'} = $self->val('crop_pct');
            }
        # Check for errors
            if ($self->val('crop_pct') < 0 || $self->val('crop_pct') > 5) {
                die "crop_pct must be a number between 0 and 5.\n";
            }
            if ($self->val('crop_top') < 0 || $self->val('crop_top') > 20) {
                die "crop_top must be a number between 0 and 20.\n";
            }
            if ($self->val('crop_right') < 0 || $self->val('crop_right') > 20) {
                die "crop_right must be a number between 0 and 20.\n";
            }
            if ($self->val('crop_bottom') < 0 || $self->val('crop_bottom') > 20) {
                die "crop_bottom must be a number between 0 and 20.\n";
            }
            if ($self->val('crop_left') < 0 || $self->val('crop_left') > 20) {
                die "crop_left must be a number between 0 and 20.\n";
            }
        # Are we cropping at all?
            if ($self->{'crop_pct'}
                    || $self->{'crop_top'}    || $self->{'crop_right'}
                    || $self->{'crop_bottom'} || $self->{'crop_left'}) {
                $self->{'crop'} = 1;
            }
        }
    # Clean up the aspect override, if one was passed in
        if ($self->val('out_aspect')) {
            $self->{'aspect_stretched'} = 1;
            my $aspect = $self->{'out_aspect'};
        # European decimals...
            $aspect =~ s/\,/\./;
        # In ratio format -- do the math
            if ($aspect =~ /^\d+:\d+$/) {
                my ($w, $h) = split /:/, $aspect;
                $aspect = $w / $h;
            }
        # Parse out decimal formats
            elsif ($aspect eq '1')     { $aspect =  1;     }
            elsif ($aspect =~ m/^1.3/) { $aspect =  4 / 3; }
            elsif ($aspect =~ m/^1.7/) { $aspect = 16 / 9; }
            $self->{'out_aspect'} = $aspect;
        }
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the save path, if requested
        $self->{'path'} = query_savepath($self->val('path'));
    # Ask the user if he/she wants to use the cutlist
        $self->{'use_cutlist'} = query_text('Enable Myth cutlist?',
                                            'yesno',
                                            $self->val('use_cutlist'));
    # Video settings
        if (!$self->{'audioonly'}) {
        # Noise reduction?
            if ($self->{'denoise_error'}) {
                query_text($self->{'denoise_error'}."\nNoise reduction has been disabled because of an error.  Press enter to continue.",
                           '', '');
            }
            else {
                $self->{'noise_reduction'} = query_text('Enable noise reduction (slower, but better results)?',
                                                        'yesno',
                                                        $self->val('noise_reduction'));
            }
        # Deinterlace video?
            $self->{'deinterlace'} = query_text('Enable deinterlacing?',
                                                'yesno',
                                                $self->val('deinterlace'));
        # Crop video to get rid of broadcast overscan
            if ($self->val('crop_pct') == $self->val('crop_top')
                    && $self->val('crop_pct') == $self->val('crop_right')
                    && $self->val('crop_pct') == $self->val('crop_bottom')
                    && $self->val('crop_pct') == $self->val('crop_left')) {
                while (1) {
                    my $pct = query_text('Crop broadcast overscan border (0-5%) ?',
                                         'float',
                                         $self->val('crop_pct'));
                    if ($pct < 0 || $pct > 5) {
                        print "Crop percentage must be between 0 and 5 percent.\n";
                    }
                    else {
                        $self->{'crop_pct'}      =
                          $self->{'crop_top'}    =
                          $self->{'crop_right'}  =
                          $self->{'crop_bottom'} =
                          $self->{'crop_left'}   = $pct;
                        last;
                    }
                }
            }
        # Custom cropping
            else {
                foreach my $side ('top', 'right', 'bottom', 'left') {
                    while (1) {
                        my $max = ($side eq 'top' || $side eq 'bottom') ? 20 : 5;
                        my $pct = query_text("Crop broadcast overscan $side border (0-5\%) ?",
                                             'float',
                                             $self->val("crop_$side"));
                        if ($pct < 0 || $pct > $max) {
                            print "Crop percentage must be between 0 and $max percent.\n";
                        }
                        else {
                            $self->{"crop_$side"} = $pct;
                            last;
                        }
                    }
                }
            }
        # Are we cropping at all?
            if ($self->{'crop_pct'}
                    || $self->{'crop_top'}    || $self->{'crop_right'}
                    || $self->{'crop_bottom'} || $self->{'crop_left'}) {
                $self->{'crop'} = 1;
            }
            else {
                $self->{'crop'} = 0;
            }
        }
    }

# Generate a cutlist
    sub gen_cutlist {
        my $self    = shift;
        my $episode = shift;
    # Don't generate a cutlist
        return unless ($self->val('gencutlist'));
    # Find mythcommflag
        my $mythcommflag = find_program('mythcommflag');
    # Nothing?
        die "Can't find mythcommflag.\n" unless ($mythcommflag);
    # Generate the cutlist
        system("$NICE $mythcommflag --gencutlist -c '$episode->{'chanid'}' -s '".unix_to_myth_time($episode->{'recstartts'})."'");
    }

# Check for a duplicate filename, and return a full path to the output filename
    sub get_outfile {
        my $self        = shift;
        my $episode     = shift;
        my $suffix      = ($self->{'suffix'} or shift);
        my $ignoredupes = (shift or 0); # Used for those times when we know/hope the filename already exists.
    # Specified a name format
        my $outfile = $self->val('filename');
           $outfile ||= '%T - %S';
    # Format
        $outfile = $episode->format_name($outfile, '-', '-', 0, $self->val('underscores'));
    # Avoid some "untitled" messiness
        $outfile =~ s/\s+-\s+Untitled//sg;
    # Make sure we don't have a duplicate filename
        if (!$ignoredupes && -e $self->{'path'}.'/'.$outfile.$suffix) {
            my $count = 1;
            my $out   = $outfile;
            while (-e $self->{'path'}.'/'.$out.$suffix) {
                $count++;
                $out = "$outfile.$count";
            }
            $outfile = $out;
       }
    # return
        return $self->{'path'}.'/'.$outfile.$suffix;
    }

# A routine to grab resolutions
    sub query_resolution {
        my $self = shift;
    # No height given -- use auto?
        if (!$self->val('width') || $self->val('width') =~ /^\s*\D/) {
            $self->{'width'} = $episodes[0]->{'finfo'}{'width'};
            print 'Default resolution based on ',
                  ($self->val('force_aspect') ? 'forced ' : ''),
                  $episodes[0]->{'finfo'}{'aspect'}, " aspect ratio.\n";
        }
        else {
            print "Default resolution based on requested dimensions.\n";
        }
    # Ask the user what resolution he/she wants
        while (1) {
            my $w = query_text('Width?',
                               'int',
                               $self->val('width'));
        # Make sure this is a multiple of 16
            if ($w % 16 == 0) {
                $self->{'width'} = $w;
                last;
            }
        # Alert the user
            print "Width must be a multiple of 16.\n";
            $self->{'width'} = int(($w + 8) / 16) * 16;
        }
    # Height will default to whatever is the appropriate aspect ratio for the width
    # someday, we should check the aspect ratio here, too...  Round up/down as needed.
        if (!$self->val('height') || $self->val('height') =~ /^\s*\D/) {
            $self->{'height'} = $self->{'width'} / $episodes[0]->{'finfo'}{'aspect_f'};
            $self->{'height'} = int(($self->{'height'} + 8) / 16) * 16;
        }
    # Ask about the height
        while (1) {
            my $h = query_text('Height?',
                               'int',
                               $self->{'height'});
        # Make sure this is a multiple of 16
            if ($h % 16 == 0) {
                $self->{'height'} = $h;
                last;
            }
        # Alert the user
            print "Height must be a multiple of 16.\n";
            $self->{'height'} = int(($h + 8) / 16) * 16;
        }
    }

# This subroutine forks and executes one system command - nothing fancy
    sub fork_command {
        my $command = shift;
        if ($DEBUG) {
            $command =~ s#\ 2>/dev/null##sg;
            print "\nforking:\n$command\n";
            return undef;
        }

    # Get read/write handles so we can communicate with the forked process
        my ($read, $write);
        pipe $read, $write;

    # Fork and return the child's pid
        my $pid = undef;
        if ($pid = fork) {
            close $write;
        # Return both the read handle and the pid?
            if (wantarray) {
                return ($pid, $read)
            }
        # Just the pid -- close the read handle
            else {
                close $read;
                return $pid;
            }
        }
    # $pid defined means that this is now the forked child
        elsif (defined $pid) {
            $is_child = 1;
            close $read;
        # Autoflush $write
            select((select($write), $|=1)[0]);
        # Run the requested command
            my ($data, $buffer) = ('', '');
            open(COM, "$command |") or die "couldn't run command:  $!\n$command\n";
            while (read(COM, $data, 10)) {
                next unless (length $data > 0);
            # Convert CR's to linefeeds so the data will flush properly
                $data =~ tr/\r/\n/s;
            # Some magic so that we only send whole lines (which helps us do
            # nonblocking reads on the other end)
                substr($data, 0, 0) = $buffer;
                $buffer  = '';
                if ($data !~ /\n$/) {
                    ($data, $buffer) = $data =~ /(.+\n)?([^\n]+)$/s;
                }
            # We have a line to print?
                if ($data && length $data > 0) {
                    print $write $data;
                }
            # Sleep for 1/20 second so we don't go too fast and annoy the cpu,
            # but still read fast enough that transcode won't slow down, either.
                usleep(5000);
            }
            close COM;
        # Print the return status of the child
            my $status = $? >> 8;
            print $write "!!! process $$ complete:  $status !!!\n";
        # Close the write handle
            close $write;
        # Exit using something that won't set off the END block
            POSIX::_exit($status);
        }
    # Couldn't fork, guess we have to quit
        die "Couldn't fork: $!\n\n$command\n\n";
    }

    sub has_data {
        my $fh = shift;
        my $r  = '';
        vec($r, fileno($fh), 1) = 1;
        my $can = select($r, undef, undef, 0);
        if ($can) {
            return vec($r, fileno($fh), 1);
        }
        return 0;
    }

    sub fifos_wait {
    # Sleep a bit to let mythtranscode start up
        my $fifodir       = shift;
        my $mythtrans_pid = shift;
        my $mythtrans_h   = shift;
        my $overload = 0;
        if (!$DEBUG) {
            while (++$overload < 30 && !(-e "$fifodir/audout" && -e "$fifodir/vidout" )) {
                sleep 1;
                print "Waiting for mythtranscode to set up the fifos.\n";
            # Check to see if mythtranscode died
                my $pid = waitpid(-1, &WNOHANG);
                if ($pid && $pid == $mythtrans_pid) {
                    print "\nmythtranscode finished.\n" unless ($DEBUG);
                    my $warnings = '';
                    while (has_data($mythtrans_h) and $l = <$mythtrans_h>) {
                        $warnings .= $l;
                    }
                    die "\n\nmythtranscode had died early:\n\n$warnings\n";
                }
            }
            unless (-e "$fifodir/audout" && -e "$fifodir/vidout") {
                die "Waited too long for mythtranscode to create its fifos.  Please try again.\n\n";
            }
        }
    }

    sub build_eta {
        my $self         = shift;
        my $frames       = shift;
        my $total_frames = shift;
        my $fps          = shift;
    # Return early?
        return 'unknown' unless ($total_frames && $fps);
    # Calculate a nice string
        my $eta_str      = '';
        my $eta          = ($total_frames - $frames) / $fps;
        if ($eta > 3600) {
            $eta_str .= int($eta / 3600).'h ';
            $eta      = ($eta % 3600);
        }
        if ($eta > 60) {
            $eta_str .= int($eta / 60).'m ';
            $eta      = ($eta % 60);
        }
        if ($eta > 0) {
            $eta_str .= int($eta).'s';
        }
        return $eta_str;
    }

    sub val {
        my $self = shift;
        my $arg  = shift;
        if (!defined $self->{$arg}) {
        # Look for a config option, a commandline option, or the code-specified default
            $self->{$arg} = arg($arg, $self->{'defaults'}{$arg}, $self);
        }
        return $self->{$arg};
    }

# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
