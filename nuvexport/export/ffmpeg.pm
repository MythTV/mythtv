#!/usr/bin/perl -w
#Last Updated: 2005.03.02 (xris)
#
#  ffmpeg.pm
#
#    routines for setting up ffmpeg
#    Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ffmpeg;
    use base 'export::generic';

    use export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;

    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;

# Check for ffmpeg
    sub init_ffmpeg {
        my $self      = shift;
        my $audioonly = (shift or 0);
    # Make sure we have ffmpeg
        my $ffmpeg = find_program('ffmpeg')
            or push @{$self->{'errors'}}, 'You need ffmpeg to use this exporter.';
    # Audio only?
        $self->{'audioonly'} = $audioonly;
    # Gather the supported codecs
        my $formats = `$ffmpeg -formats 2>/dev/null`;
        $formats =~ s/^.+?\n\s*Codecs:\s*\n(.+?\n)\s*\n.*?$/$1/s;
        while ($formats =~ /^\s(.{6})\s(\S+)\s*$/mg) {
            $self->{'codecs'}{$2} = $1;
        }
    }

# Returns true or false for the requested codec
    sub can_decode {
        my $self  = shift;
        my $codec = shift;
        return ($self->{'codecs'}{$codec} && $self->{'codecs'}{$codec} =~ /^D/) ? 1 : 0;
    }
    sub can_encode {
        my $self  = shift;
        my $codec = shift;
        return ($self->{'codecs'}{$codec} && $self->{'codecs'}{$codec} =~ /^.E/) ? 1 : 0;
    }

# Load default settings
    sub load_defaults {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::load_defaults();
    # Not really anything to add
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
        my $skip = shift;
    # Gather generic settings
        $self->SUPER::gather_settings();
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
        my $suffix  = (shift or '');
    # Init the commands
        my $ffmpeg        = '';
        my $mythtranscode = '';

    # Load nuv info
        load_finfo($episode);

    # Set up the fifo dirs?
        if (-e "/tmp/fifodir_$$/vidout" || -e "/tmp/fifodir_$$/audout") {
            die "Possibly stale mythtranscode fifo's in /tmp/fifodir_$$/.\nPlease remove them before running nuvexport.\n\n";
        }

    # Here, we have to fork off a copy of mythtranscode (Do not use --fifosync with ffmpeg or it will hang)
        $mythtranscode = "$NICE mythtranscode --showprogress -p autodetect -c $episode->{channel} -s $episode->{start_time_sep} -f \"/tmp/fifodir_$$/\"";
        $mythtranscode .= ' --honorcutlist' if ($self->{use_cutlist});

        my $videofifo = "/tmp/fifodir_$$/vidout";
        my $videotype = 'rawvideo';
        my $crop_w;
        my $crop_h;

        if ($self->{'crop'}) {
            $crop_w = sprintf('%.0f', .02 * $episode->{'finfo'}{'width'});
            $crop_h = sprintf('%.0f', .02 * $episode->{'finfo'}{'height'});
            # keep crop numbers even
            $crop_w-- if ($crop_w > 0 && $crop_w % 2);
            $crop_h-- if ($crop_h > 0 && $crop_h % 2);
        }

        if ($self->{'audioonly'}) {
            $ffmpeg .= "cat /tmp/fifodir_$$/vidout > /dev/null | ";
        }
        else {
        # Do noise reduction -- ffmpeg's -nr flag doesn't seem to do anything other than prevent denoise from working
            if ($self->{'noise_reduction'}) {
                $ffmpeg .= "$NICE ffmpeg -f rawvideo";
                $ffmpeg .= " -s " . $episode->{'finfo'}{'width'} . "x" . $episode->{'finfo'}{'height'};
                $ffmpeg .= " -r " . $episode->{'finfo'}{'fps'};
                $ffmpeg .= " -i /tmp/fifodir_$$/vidout -f yuv4mpegpipe -";
                $ffmpeg .= " 2> /dev/null | ";
                $ffmpeg .= "$NICE yuvdenoise -F -r 16";
                if ($self->{'crop'}) {
                    $ffmpeg .= " -b $crop_w,$crop_h,-$crop_w,-$crop_h";
                }
                $ffmpeg .= " 2> /dev/null | ";
                $videofifo = '-';
                $videotype = 'yuv4mpegpipe';
            }
        }

    # Start the ffmpeg command
        $ffmpeg .= "$NICE ffmpeg -hq";
        if ($num_cpus > 1) {
            $ffmpeg .= ' -threads '.($num_cpus);
        }
        $ffmpeg .= " -y -f s16le";
        $ffmpeg .= " -ar " . $episode->{'finfo'}{'audio_sample_rate'};
        $ffmpeg .= " -ac " . $episode->{'finfo'}{'audio_channels'};
        $ffmpeg .= " -i /tmp/fifodir_$$/audout";
        if (!$self->{'audioonly'}) {
            $ffmpeg .= " -f $videotype";
            $ffmpeg .= " -s " . $episode->{'finfo'}{'width'} . "x" . $episode->{'finfo'}{'height'};
            $ffmpeg .= " -r " . $episode->{'finfo'}{'fps'};
            $ffmpeg .= " -i $videofifo";

        # Filters
            if ($self->{'deinterlace'}) {
                $ffmpeg .= " -deinterlace";
            }

            if ($self->{'crop'}) {

                $ffmpeg .= " -croptop $crop_h -cropbottom $crop_h";
                $ffmpeg .= " -cropleft $crop_w -cropright $crop_w";
            }
        }

    # Add any additional settings from the child module
        $ffmpeg .= ' '.$self->{'ffmpeg_xtra'};

    # Output directory set to null means the first pass of a multipass
        if (!$self->{'path'} || $self->{'path'} =~ /^\/dev\/null\b/) {
            $ffmpeg .= ' /dev/null';
        }
    # Add the output filename
        else {
            $ffmpeg .= ' '.shell_escape($self->get_outfile($episode, $suffix));
        }
    # ffmpeg pids
        my ($mythtrans_pid, $ffmpeg_pid, $mythtrans_h, $ffmpeg_h);

    # Create a directory for mythtranscode's fifo's
        mkdir("/tmp/fifodir_$$/", 0755) or die "Can't create /tmp/fifodir_$$/:  $!\n\n";
        ($mythtrans_pid, $mythtrans_h) = fork_command("$mythtranscode 2>&1");
        $children{$mythtrans_pid} = 'mythtranscode' if ($mythtrans_pid);
        fifos_wait("/tmp/fifodir_$$/");
        push @tmpfiles, "/tmp/fifodir_$$", "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";

    # Execute ffmpeg
        print "Starting ffmpeg.\n" unless ($DEBUG);
        ($ffmpeg_pid, $ffmpeg_h) = fork_command("$ffmpeg 2>&1");
        $children{$ffmpeg_pid} = 'ffmpeg' if ($ffmpeg_pid);

    # Get ready to count the frames that have been processed
        my ($frames, $fps, $start);
        $frames = 0;
        $fps = 0.0;
        $start = time();
        my $total_frames = $episode->{'lastgop'} * (($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? 12 : 15);
    # Keep track of any warnings
        my $warnings    = '';
        my $death_timer = 0;
        my $last_death  = '';
	# Wait for child processes to finish
        while ((keys %children) > 0) {
            my $l;
            my $pct;
        # Show progress
            if ($frames && $total_frames) {
                $pct = sprintf('%.2f', 100 * $frames / $total_frames);
                $fps = ($frames * 1.0) / (time() - $start);
            }
            else {
                $pct = "0.00";
            }
            printf "\rprocessed:  $frames of $total_frames frames ($pct\%%), %6.2f fps ", $fps;

        # Read from the ffmpeg handle
            while (has_data($ffmpeg_h) and $l = <$ffmpeg_h>) {
                if ($l =~ /frame=\s*(\d+)/) {
                    $frames = int($1);
                }
            # ffmpeg warnings?
                elsif ($l =~ /^Unknown.+?(codec|format)/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            # Segfault?
                elsif ($l =~ /^Segmentation\sfault/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            }
        # Read from the mythtranscode handle?
            while (has_data($mythtrans_h) and $l = <$mythtrans_h>) {
                if ($l =~ /Processed:\s*(\d+)\s*of\s*(\d+)\s*frames\s*\((\d+)\s*seconds\)/) {
                    if ($self->{'audioonly'}) {
                        $frames = int($1);
                    }
                    $total_frames = $2;
                }
            }
        # Has the deathtimer been started?  Stick around for awhile, but not too long
            if ($death_timer > 0 && time() - $death_timer > 30) {
                $str = "\n\n$last_death died early.";
                if ($warnings) {
                    $str .= "See ffmpeg warnings:\n\n$warnings";
                }
                else {
                    $str .= "Please use the --debug option to figure out what went wrong.\n\n";
                }
                die $str;
            }
        # The pid?
            $pid = waitpid(-1, &WNOHANG);
            if ($children{$pid}) {
                print "\n$children{$pid} finished.\n" unless ($DEBUG);
                $last_death  = $children{$pid};
                $death_timer = time();
                delete $children{$pid};
            }
        # Sleep for 1/10 second so we don't go too fast and annoy the cpu
            usleep(100000);
        }
    # Remove the fifodir?
        unlink "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";
        rmdir "/tmp/fifodir_$$";
    }


# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
