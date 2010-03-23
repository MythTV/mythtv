#
# $Date$
# $Revision$
# $Author$
#
#  transcode.pm
#
#    routines for setting up transcode
#

package export::transcode;
    use base 'export::generic';

    use export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;
    use Config;

    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;
    use MythTV;

# Load the following extra parameters from the commandline
    add_arg('zoom_filter:s',          'Which zoom filter to use.');
    add_arg('force_mythtranscode!',   'Force use of mythtranscode, even on mpeg files.');
    add_arg('mythtranscode_cutlist!', "Force use of mythtranscode's cutlist instead of transcode's internal one (only works with --force_mythtranscode).");

# Check for transcode
    sub init_transcode {
        my $self = shift;
    # Make sure we have transcode
        my $transcode = find_program('transcode')
            or push @{$self->{'errors'}}, 'You need transcode to use this exporter.';
    # Check the transcode version
        my $data = `$transcode -version 2>&1`;
        if ($data =~ m/transcode\s+v(\d+\.\d+)\b/si) {
            $self->{'transcode_vers'}  = $1;
        }
        if ($self->{'transcode_vers'} < 1.1) {
            push @{$self->{'errors'}}, "This version of nuvexport requires transcode 1.1.\n";
        }
    }

# Load default settings
    sub load_defaults {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::load_defaults();
    # Not really anything to add
        $self->{'defaults'}{'force_mythtranscode'}   = 1;
        $self->{'defaults'}{'mythtranscode_cutlist'} = 1;
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
        my $skip = shift;
    # Gather generic settings
        $self->SUPER::gather_settings($skip ? $skip - 1 : 0);
        return if ($skip);
    # Zoom Filter
        if (defined $self->val('zoom_filter')) {
            if (!$self->val('zoom_filter')) {
                $self->{'zoom_filter'} = 'B_spline';
            }
            elsif ($self->val('zoom_filter') !~ /^(?:Lanczos3|Bell|Box|Mitchell|Hermite|B_spline|Triangle)$/) {
                die "Unknown zoom_filter:  ".$self->val('zoom_filter')."\n";
            }
        }
    }

    sub export {
        my $self       = shift;
        my $episode    = shift;
        my $suffix     = (shift or '');
        my $skip_audio = shift;
    # Init the commands
        my $transcode     = '';
        my $mythtranscode = '';

        my $width;
        my $height;
        my $pad_h;
        my $pad_w;

    # Generate a cutlist?
        $self->gen_cutlist($episode);

    # Start the transcode command
        $transcode = "$NICE transcode"
                   # No colored log messages (too much work to parse)
                    .' --log_no_color'
                   # Only print status every 16 frames -- prevents buffer-related slowdowns
                    .'  --progress_meter 2 --progress_rate 16'
                    ;
    # Take advantage of multiple CPU's?
    # * This has been disabled because it looks like it causes jerkiness.
    #    if ($num_cpus > 1) {
    #        $transcode .= ' -u 100,'.($num_cpus);
    #    }

    # Import aspect ratio
        if ($episode->{'finfo'}{'aspect'} eq '1:1') {
            $transcode .= ' --import_asr 1';
        }
        elsif ($episode->{'finfo'}{'aspect'} eq '4:3') {
            $transcode .= ' --import_asr 2';
        }
        elsif ($episode->{'finfo'}{'aspect'} eq '16:9') {
            $transcode .= ' --import_asr 3';
        }
        elsif ($episode->{'finfo'}{'aspect'} eq '2.21:1') {
            $transcode .= ' --import_asr 4';
        }

    # Output aspect ratio
        $self->{'out_aspect'} ||= $episode->{'finfo'}{'aspect_f'};

    # The output is actually a stretched aspect ratio
    # (like 480x480 for SVCD, which is 4:3)
        if ($self->{'aspect_stretched'}) {
        # Stretch the width to the full aspect ratio for calculating
            $width = int($self->{'height'} * $self->{'out_aspect'} + 0.5);
        # Calculate the height required to keep the source in aspect
            $height = $width / $episode->{'finfo'}{'aspect_f'};
        # Round to nearest even number
            $height = int(($height + 2) / 4) * 4;
        # Calculate how much to pad the height (both top & bottom)
            $pad_h = int(($self->{'height'} - $height) / 2);
        # Set the real width again
            $width = $self->{'width'};
        # No padding on the width
            $pad_w = 0;
        }
    # The output will need letter/pillarboxing
        else {
        # We need to letterbox
            if ($self->{'width'} / $self->{'height'} <= $self->{'out_aspect'}) {
                $width  = $self->{'width'};
                $height = $width / $episode->{'finfo'}{'aspect_f'};
                $height = int(($height + 2) / 4) * 4;
                $pad_h  = int(($self->{'height'} - $height) / 2);
                $pad_w  = 0;
            }
        # We need to pillarbox
            else {
                $height = $self->{'height'};
                $width  = $height * $episode->{'finfo'}{'aspect_f'};
                $width  = int(($width + 2) / 4) * 4;
                $pad_w  = int(($self->{'width'} - $width) / 2);
                $pad_h  = 0;
            }
        }

        $transcode .= ' --export_asr ';
        if ($self->{'out_aspect'} == 1) {
            $transcode .= '1';
        }
    # 4:3
        elsif ($self->{'out_aspect'} =~ m/^1.3/) {
            $transcode .= '2';
        }
    # 16:9
        elsif ($self->{'out_aspect'} =~ m/^1.7/) {
            $transcode .= '3';
        }
    # 2.21:1
        elsif ($self->{'out_aspect'} == 2.21) {
            $transcode .= '4';
        }

        my $fpsout = $self->{'out_fps'};
        $transcode .= " --export_fps $fpsout";
        if ($fpsout == 23.976) {
            $transcode .= ',1';
        }
        elsif ($fpsout == 24) {
            $transcode .= ',2';
        }
        elsif ($fpsout == 25) {
            $transcode .= ',3';
        }
        elsif ($fpsout == 29.97) {
            $transcode .= ',4';
        }
        elsif ($fpsout == 30) {
            $transcode .= ',5';
        }
        elsif ($fpsout == 50) {
            $transcode .= ',6';
        }
        elsif ($fpsout == 59.94) {
            $transcode .= ',7';
        }
        elsif ($fpsout == 60) {
            $transcode .= ',8';
        }
        elsif ($fpsout == 1) {
            $transcode .= ',9';
        }
        elsif ($fpsout == 5) {
            $transcode .= ',10';
        }
        elsif ($fpsout == 10) {
            $transcode .= ',11';
        }
        elsif ($fpsout == 12) {
            $transcode .= ',12';
        }
        elsif ($fpsout == 15) {
            $transcode .= ',13';
        }

        if ($fpsout != $episode->{'finfo'}{'fps'}) {
            $transcode .= ' -J fps=' . $episode->{'finfo'}{'fps'} . ':'
                         .$fpsout . ':pre';
        }

    # Resize & pad
        $transcode .= " -Z ${width}x$height";
        if ( $pad_h || $pad_w ) {
            $transcode .= ' -Y '.(-1*$pad_h).','.(-1*$pad_w).','.(-1*$pad_h).
                          ','.(-1*$pad_w);
        }

    # Not an mpeg, or force mythtranscode
        if ($self->val('force_mythtranscode') || !$episode->{'finfo'}{'is_mpeg'}) {
        # swap red/blue -- used with svcd, need to see if it's needed everywhere
            $transcode .= ' -k';
        # Set up the fifo dirs?
            if (-e "/tmp/fifodir_$$/vidout" || -e "/tmp/fifodir_$$/audout") {
                die "Possibly stale mythtranscode fifo's in /tmp/fifodir_$$/.\nPlease remove them before running nuvexport.\n\n";
            }
        # Here, we have to fork off a copy of mythtranscode (no need to use --fifosync with transcode -- it seems to do this on its own)
            my $mythtranscode_bin = find_program('mythtranscode');
            $mythtranscode = "$NICE $mythtranscode_bin --showprogress"
                            ." -p '$episode->{'transcoder'}'"
                            ." -c '$episode->{'chanid'}'"
                            ." -s '".unix_to_myth_time($episode->{'recstartts'})."'"
                            ." -f \"/tmp/fifodir_$$/\"";
        # On no-audio encodes, we need to do something to keep mythtranscode's audio buffers from filling up available RAM
        #    $mythtranscode .= ' --fifosync' if ($skip_audio);
        # Let mythtranscode handle the cutlist?
            if ($self->val('use_cutlist') && $self->val('mythtranscode_cutlist')) {
                $mythtranscode .= ' --honorcutlist';
            }
        # Figure out the input files
            $transcode .= " -i /tmp/fifodir_$$/vidout -p /tmp/fifodir_$$/audout"
                         .' -H 0 -x raw'
                         .' -g '.join('x', $episode->{'finfo'}{'width'}, $episode->{'finfo'}{'height'})
                         .' -f '.$episode->{'finfo'}{'fps'}.','
                         . (($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? 3 : 4)
                         .' -n 0x1'
                         .' -e '.join(',', $episode->{'finfo'}{'audio_sample_rate'}, $episode->{'finfo'}{'audio_bits_per_sample'}, $episode->{'finfo'}{'audio_channels'})
                         ;
        }
    # Is an mpeg
        else {
            $transcode .= ' -i '.shell_escape($episode->{'local_path'}).' -x ';
            if ($episode->{'finfo'}{'mpeg_stream_type'} eq 'mpegpes') {
                $transcode .= 'vob';
            }
            else {
                $transcode .= 'mpeg2';
            }
        }
    # Crop?
        if ($self->{'crop'}) {
            my $t = sprintf('%.0f', ($self->val('crop_top')    / 100) * $episode->{'finfo'}{'height'});
            my $r = sprintf('%.0f', ($self->val('crop_right')  / 100) * $episode->{'finfo'}{'width'});
            my $b = sprintf('%.0f', ($self->val('crop_bottom') / 100) * $episode->{'finfo'}{'height'});
            my $l = sprintf('%.0f', ($self->val('crop_left')   / 100) * $episode->{'finfo'}{'width'});
        # Keep the crop numbers even
            $t-- if ($t > 0 && $t % 2);
            $r-- if ($r > 0 && $r % 2);
            $b-- if ($b > 0 && $b % 2);
            $l-- if ($l > 0 && $l % 2);
        # crop
            $transcode .= " -j $t,$l,$b,$r" if ($t || $r || $b || $l);
        }
    # Use the cutlist?  (only for mpeg files -- nuv files are handled by mythtranscode)
        if ($self->{'use_cutlist'} && !$self->val('mythtranscode_cutlist')
                && $episode->{'cutlist'} && $episode->{'cutlist'} =~ /\d/) {
            my $cutlist = $episode->{'cutlist'};
            $cutlist =~ tr/ //d;
            $cutlist =~ tr/\n/ /;
            $transcode .= " -J skip=\"$cutlist\"";
        }
    # Filters
        if ($self->{'zoom_filter'}) {
            $transcode .= ' --zoom_filter '.$self->{'zoom_filter'};
        }
        if ($self->{'deinterlace'}) {
            $transcode .= " -J smartyuv";
            #smartyuv|smartdeinter|dilyuvmmx
        }
        if ($self->{'noise_reduction'}) {
            $transcode .= ' -J yuvdenoise=mode=';
            if ($self->arg('fast_denoise')) {
                $transcode .= 2;
            }
            elsif ($self->{'deinterlace'}) {
                $transcode .= 0;
            }
            else {
                $transcode .= 1;
            }
        }
    # Add any additional settings from the child module
        $transcode .= ' '.$self->{'transcode_xtra'};
    # Output directory set to null means the first pass of a multipass
        if (!$self->{'path'} || $self->{'path'} =~ /^\/dev\/null\b/) {
            $transcode .= ' -o /dev/null';
        }
    # Add the output filename
        else {
            $transcode .= ' -o '.shell_escape($self->get_outfile($episode, $suffix));
        }
    # Transcode pids
        my ($mythtrans_pid, $trans_pid, $mythtrans_h, $trans_h);
    # Set up and run mythtranscode?
        if ($mythtranscode) {
        # Create a directory for mythtranscode's fifo's
            mkdir("/tmp/fifodir_$$/", 0755) or die "Can't create /tmp/fifodir_$$/:  $!\n\n";
            ($mythtrans_pid, $mythtrans_h) = fork_command("$mythtranscode 2>&1");
            $children{$mythtrans_pid} = 'mythtranscode' if ($mythtrans_pid);
            fifos_wait("/tmp/fifodir_$$/");
            push @tmpfiles, "/tmp/fifodir_$$", "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";
        }
    # Execute transcode
        print "Starting transcode.\n" unless ($DEBUG);
        ($trans_pid, $trans_h) = fork_command("$transcode 2>&1");
        $children{$trans_pid} = 'transcode' if ($trans_pid);
    # Get ready to count the frames that have been processed
        my ($frames, $fps);
        $frames = 0;
        $fps    = 0.0;
        if (!$episode->{'total_frames'} || $episode->{'total_frames'} < 1) {
            $episode->{'total_frames'} = $episode->{'last_frame'} > 0
                                         ? $episode->{'last_frame'} - $episode->{'cutlist_frames'}
                                         : 0;
        }
    # Keep track of any warnings
        my $warnings    = '';
        my $death_timer = 0;
        my $last_death  = '';
	# Wait for child processes to finish
        while ((keys %children) > 0) {
            my ($l, $pct);
        # Show progress
            if ($frames && $episode->{'total_frames'}) {
                $pct = sprintf('%.2f', 100 * $frames / $episode->{'total_frames'});
            }
            else {
                $pct = '~';
            }
            unless (arg('noprogress')) {
                print "\rprocessed:  $frames of $episode->{'total_frames'} frames at $fps fps ($pct\%, eta: ",
                      $self->build_eta($frames, $episode->{'total_frames'}, $fps),
                      ')  ';
            }
        # Read from the transcode handle
            while (has_data($trans_h) and $l = <$trans_h>) {
                if ($l =~ /^encoding=/) {
                    my %status;
                    foreach my $pair (split(/\s/, $l)) {
                        my ($k,$v) = split(/=/, $pair);
                        $status{$k} = $v;
                    }
                    $frames = int($status{'frame'});
                    $fps    = $status{'fps'};
                }
            # Look for error messages
                elsif ($l =~ m/\]\W+warning/i) {
                    $warnings .= $l;
                }
                elsif ($l =~ m/\]\W+critical/i || $l =~ m/segmentation fault/i) {
                    $warnings .= $l;
                    die "\n\nTranscode had critical errors:\n\n$warnings";
                }
            }
        # Read from the mythtranscode handle?
            if ($mythtranscode && $mythtrans_pid) {
                while (has_data($mythtrans_h) and $l = <$mythtrans_h>) {
                    if ($l =~ /Processed:\s*(\d+)\s*of\s*(\d+)\s*frames\s*\((\d+)\s*seconds\)/) {
                    # Transcode behaves properly with audio streams, so we can
                    # ignore this.
                        #if ($self->{'audioonly'}) {
                        #    $frames = int($1);
                        #}
                    # mythtranscode is going to be the most accurate total frame count.
                        my $total = $2 - $episode->{'cutlist_frames'};
                        if ($episode->{'total_frames'} < $total) {
                            $episode->{'total_frames'} = $total;
                        }
                    }
                }
            }
        # Has the deathtimer been started?  Stick around for awhile, but not too long
            if ($death_timer > 0 && time() - $death_timer > 30) {
                $str = "\n\n$last_death died early.";
                if ($warnings) {
                    $str .= "See transcode warnings:\n\n$warnings";
                }
                else {
                    $str .= "Please use the --debug option to figure out what went wrong.\n"
                           ."http://www.mythtv.org/wiki/index.php/Nuvexport#Debug_Mode\n\n";
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
    # Remove the fifodir?  (in case we're doing multipass, so we don't generate errors on the next time through)
        if ($mythtranscode) {
            unlink "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";
            rmdir "/tmp/fifodir_$$";
        }
    }

# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
