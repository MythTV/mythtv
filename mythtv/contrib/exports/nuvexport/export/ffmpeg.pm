#
# $Date$
# $Revision$
# $Author$
#
#  ffmpeg.pm
#
#    routines for setting up ffmpeg
#    Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

#
# Need some kind of hash to deal with the various ffmpeg command version
# differences:
#
# http://svn.mplayerhq.hu/ffmpeg/trunk/libavcodec/utils.c?r1=6252&r2=6257&pathrev=6257
#


package export::ffmpeg;
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

# In case people would rather use yuvdenoise to deinterlace
    add_arg('deint_in_yuvdenoise|deint-in-yuvdenoise!', 'Deinterlace in yuvdenoise instead of ffmpeg');

# Check for ffmpeg
    sub init_ffmpeg {
        my $self      = shift;
        my $audioonly = (shift or 0);
    # Temp var
        my $data;
    # Make sure we have ffmpeg
        my $ffmpeg = find_program('ffmpeg')
            or push @{$self->{'errors'}}, 'You need ffmpeg to use this exporter.';
    # Make sure we have yuvdenoise
        my $yuvdenoise = find_program('yuvdenoise')
            or push @{$self->{'errors'}}, 'You need yuvdenoise (part of mjpegtools) to use this exporter.';
    # Check the yuvdenoise version
        if (!defined $self->{'denoise_vmaj'}) {
            $data = `cat /dev/null | yuvdenoise 2>&1`;
            if ($data =~ m/yuvdenoise version (\d+(?:\.\d+)?)(\.\d+)?/i) {
                $self->{'denoise_vmaj'} = $1;
                $self->{'denoise_vmin'} = ($2 or 0);
            }
            elsif ($data =~ m/version: mjpegtools (\d+(?:\.\d+)?)(\.\d+)?/i) {
                $self->{'denoise_vmaj'} = $1;
                $self->{'denoise_vmin'} = ($2 or 0);
            }
            else {
                push @{$self->{'errors'}}, 'Unrecognizeable yuvdenoise version string.';
                $self->{'noise_reduction'} = 0;
                $self->{'denoise_vmaj'}    = 0;
                $self->{'denoise_vmin'}    = 0;
            }
        # New yuvdenoise can't deinterlace
            if ($self->{'denoise_vmaj'} > 1.6 || ($self->{'denoise_vmaj'} == 1.6 && $self->{'denoise_vmin'} > 2)) {
                $self->{'deint_in_yuvdenoise'} = 0;
            }
        }
    # Check the ffmpeg version
        if (!defined $self->{'ffmpeg_vers'}) {
            $data = `$ffmpeg -version 2>&1`;
            if ($data =~ m/ffmpeg\sversion\s0.5[\-,]/si) {
                $self->{'ffmpeg_vers'} = '0.5';
            }
            elsif ($data =~ m/ffmpeg\sversion\sSVN-r(\d+),/si) {
                $self->{'ffmpeg_vers'} = $1;
            }
            # Disabled unti I need the formatting again to detect wacky ffmpeg
            # versions if they go back to releasing things the old way.
            #elsif ($data =~ m/ffmpeg\sversion\s0.4.9-\d+_r(\d+)\.\w+\.at/si) {
            #    $self->{'ffmpeg_vers'}  = 'svn';
            #    $self->{'ffmpeg_build'} = $1;
            #}
            #elsif ($data =~ m/ffmpeg\sversion\s(.+?),(?:\sbuild\s(\d+))?/si) {
            #    $self->{'ffmpeg_vers'}  = lc($1);
            #    $self->{'ffmpeg_build'} = $2;
            #    if ($self->{'ffmpeg_vers'} =~ /^svn-r(.+?)$/) {
            #        $self->{'ffmpeg_vers'}  = 'svn';
            #        $self->{'ffmpeg_build'} = $1;
            #    }
            #}
            else {
                push @{$self->{'errors'}}, 'Unrecognizeable ffmpeg version string.';
            }
        }
        if ($self->{'ffmpeg_vers'} < 0.5) {
            push @{$self->{'errors'}}, "This version of nuvexport requires ffmpeg 0.5.\n";
        }
    # Audio only?
        $self->{'audioonly'} = $audioonly;
    # Gather the supported codecs
        $data         = `$ffmpeg -formats 2>&1`;
        my ($formats) = $data =~ /(?:^|\n\s*)File\sformats:\s*\n(.+?\n)\s*\n/s;
        my ($codecs)  = $data =~ /(?:^|\n\s*)Codecs:\s*\n(.+?\n)\s*\n/s;
        if ($formats) {
            while ($formats =~ /^\s(..)\s(\S+)\b/mg) {
                $self->{'formats'}{$2} = $1;
            }
        }
        if ($codecs) {
            while ($codecs =~ /^\s(.{6})\s(\S+)\b/mg) {
                $self->{'codecs'}{$2} = $1;
            }
        }
    }

# Returns true or false for the requested codec
    sub can_decode {
        my $self  = shift;
        my $codec = shift;
        return ($self->{'codecs'}{$codec} && $self->{'codecs'}{$codec} =~ /^D/
                || $self->{'formats'}{$codec} && $self->{'formats'}{$codec} =~ /^D/) ? 1 : 0;
    }
    sub can_encode {
        my $self  = shift;
        my $codec = shift;
        return ($self->{'codecs'}{$codec} && $self->{'codecs'}{$codec} =~ /^.E/
                || $self->{'formats'}{$codec} && $self->{'formats'}{$codec} =~ /^.E/) ? 1 : 0;
    }

# ffmpeg keeps changing their parameter names... so we work around it.
    sub param {
        my $self  = shift;
        my $param = lc(shift);
        my $value = shift;
    # No more version checks (until ffmpeg starts doing crazy releases again)
        return param_pair('ab',             $value.'k') if ($param eq 'ab');
        return param_pair('ac',             $value)     if ($param eq 'channels');
        return param_pair('ar',             $value)     if ($param eq 'sample_rate');
        return param_pair('b',              $value.'k') if ($param eq 'bit_rate');
        return param_pair('b_qfactor',      $value)     if ($param eq 'b_quant_factor');
        return param_pair('b_qoffset',      $value)     if ($param eq 'b_quant_offset');
        return param_pair('bf',             $value)     if ($param eq 'max_b_frames');
        return param_pair('bt',             $value.'k') if ($param eq 'bit_rate_tolerance');
        return param_pair('bufsize',        $value.'k') if ($param eq 'rc_buffer_size');
        return param_pair('bug',            $value)     if ($param eq 'bugs');
        return param_pair('error',          $value)     if ($param eq 'error_rate');
        return param_pair('g',              $value)     if ($param eq 'gop_size');
        return param_pair('i_qfactor',      $value)     if ($param eq 'i_quant_factor');
        return param_pair('i_qoffset',      $value)     if ($param eq 'i_quant_offset');
        return param_pair('maxrate',        $value.'k') if ($param eq 'rc_max_rate');
        return param_pair('mblmax',         $value)     if ($param eq 'mb_lmax');
        return param_pair('mblmin',         $value)     if ($param eq 'mb_lmin');
        return param_pair('mepc',           $value)     if ($param eq 'me_penalty_compensation');
        return param_pair('minrate',        $value)     if ($param eq 'rc_min_rate');
        return param_pair('qcomp',          $value)     if ($param eq 'qcompress');
        return param_pair('qdiff',          $value)     if ($param eq 'max_qdiff');
        return param_pair('qsquish',        $value)     if ($param eq 'rc_qsquish');
        return param_pair('rc_init_cplx',   $value)     if ($param eq 'rc_initial_cplx');
        return param_pair('skip_exp',       $value)     if ($param eq 'frame_skip_exp');
        return param_pair('skip_factor',    $value)     if ($param eq 'frame_skip_factor');
        return param_pair('skip_threshold', $value)     if ($param eq 'frame_skip_threshold');
        return param_pair('threads',        $value)     if ($param eq 'thread_count');
    # Unknown version or nothing wacky, just return the parameter
        return param_pair($param, $value);
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
        my $firstpass = (shift or 0);
    # Init the commands
        my $ffmpeg        = '';
        my $mythtranscode = '';

    # Generate a cutlist?
        $self->gen_cutlist($episode);

    # Set up the fifo dirs?
        if (-e "/tmp/fifodir_$$/vidout" || -e "/tmp/fifodir_$$/audout") {
            die "Possibly stale mythtranscode fifo's in /tmp/fifodir_$$/.\nPlease remove them before running nuvexport.\n\n";
        }

    # Here, we have to fork off a copy of mythtranscode (Do not use --fifosync with ffmpeg or it will hang)
        my $mythtranscode_bin = find_program('mythtranscode');
        $mythtranscode = "$NICE $mythtranscode_bin --showprogress"
                        ." -p '$episode->{'transcoder'}'"
                        ." -c '$episode->{'chanid'}'"
                        ." -s '".unix_to_myth_time($episode->{'recstartts'})."'"
                        ." -f \"/tmp/fifodir_$$/\"";
        $mythtranscode .= ' --honorcutlist' if ($self->{'use_cutlist'});
        $mythtranscode .= ' --fifosync'     if ($self->{'audioonly'} || $firstpass);

        my $videofifo = "/tmp/fifodir_$$/vidout";
        my $videotype = 'rawvideo -pix_fmt yuv420p';
        my $crop_w;
        my $crop_h;
        my $pad_w;
        my $pad_h;
        my $height;
        my $width;

    # Standard encodes
        if (!$self->{'audioonly'}) {
        # Do noise reduction -- ffmpeg's -nr flag doesn't seem to do anything other than prevent denoise from working
            if ($self->{'noise_reduction'}) {
                $ffmpeg .= "$NICE ffmpeg -f rawvideo";
                $ffmpeg .= ' -s ' . $episode->{'finfo'}{'width'} . 'x' . $episode->{'finfo'}{'height'};
                $ffmpeg .= ' -r ' . $episode->{'finfo'}{'fps'};
                $ffmpeg .= " -i /tmp/fifodir_$$/vidout -f yuv4mpegpipe -";
                $ffmpeg .= ' 2> /dev/null | ';
                $ffmpeg .= "$NICE yuvdenoise";
                if ($self->{'denoise_vmaj'} < 1.6 || ($self->{'denoise_vmaj'} == 1.6 && $self->{'denoise_vmin'} < 3)) {
                    $ffmpeg .= ' -r 16';
                    if ($self->val('fast_denoise')) {
                        $ffmpeg .= ' -f';
                    }
                # Deinterlace in yuvdenoise
                    if ($self->val('deint_in_yuvdenoise') && $self->val('deinterlace')) {
                        $ffmpeg .= " -F";
                    }
                }
                $ffmpeg   .= ' 2> /dev/null | ';
                $videofifo = '-';
                $videotype = 'yuv4mpegpipe';
            }
        }

    # Start the ffmpeg command
        $ffmpeg .= "$NICE ffmpeg";
        if ($num_cpus > 1) {
            $ffmpeg .= ' -threads '.($num_cpus);
        }
        $ffmpeg .= ' -y -f '.($Config{'byteorder'} == 4321 ? 's16be' : 's16le')
                  .' -ar ' . $episode->{'finfo'}{'audio_sample_rate'}
                  .' -ac ' . $episode->{'finfo'}{'audio_channels'};
        if (!$firstpass) {
            $ffmpeg .= " -i /tmp/fifodir_$$/audout";
        }
        if (!$self->{'audioonly'}) {
            $ffmpeg .= " -f $videotype"
                      .' -s ' . $episode->{'finfo'}{'width'} . 'x' . $episode->{'finfo'}{'height'}
                      .' -aspect ' . $episode->{'finfo'}{'aspect_f'}
                      .' -r '      . $episode->{'finfo'}{'fps'}
                      ." -i $videofifo";

            $self->{'out_aspect'} ||= $episode->{'finfo'}{'aspect_f'};

        # The output is actually a stretched/anamorphic aspect ratio
        # (like 480x480 for SVCD, which is 4:3)
            if ($self->{'aspect_stretched'}) {
                $width  = $self->{'width'};
                $height = $self->{'height'};
                $pad_h  = 0;
                $pad_w  = 0;
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

            $ffmpeg .= ' -aspect ' . $self->{'out_aspect'}
                      .' -r '      . $self->{'out_fps'};

        # Deinterlace in ffmpeg only if the user wants to
            if ($self->val('deinterlace') && !($self->val('noise_reduction') && $self->val('deint_in_yuvdenoise'))) {
                $ffmpeg .= ' -deinterlace';
            }
        # Crop
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
                $ffmpeg .= " -croptop    $t -cropright $r"
                          ." -cropbottom $b -cropleft  $l" if ($t || $r || $b || $l);
            }

        # Letter/Pillarboxing as appropriate
            if ($pad_h) {
                $ffmpeg .= " -padtop $pad_h -padbottom $pad_h";
            }
            if ($pad_w) {
                $ffmpeg .= " -padleft $pad_w -padright $pad_w";
            }
            $ffmpeg .= " -s ${width}x$height";
        }

    # Add any additional settings from the child module
        $ffmpeg .= ' '.$self->{'ffmpeg_xtra'};

    # Output directory set to null means the first pass of a multipass
        if ($firstpass || !$self->{'path'} || $self->{'path'} =~ /^\/dev\/null\b/) {
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
        fifos_wait("/tmp/fifodir_$$/", $mythtrans_pid, $mythtrans_h);
        push @tmpfiles, "/tmp/fifodir_$$", "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";

    # For multipass encodes, we don't need the audio on the first pass
        if ($self->{'audioonly'}) {
            my ($cat_pid, $cat_h) = fork_command("cat /tmp/fifodir_$$/vidout > /dev/null");
            $children{$cat_pid} = 'video dump' if ($cat_pid);
        }
        elsif ($firstpass) {
            my ($cat_pid, $cat_h) = fork_command("cat /tmp/fifodir_$$/audout > /dev/null");
            $children{$cat_pid} = 'audio dump' if ($cat_pid);
        }
    # Execute ffmpeg
        print "Starting ffmpeg.\n" unless ($DEBUG);
        ($ffmpeg_pid, $ffmpeg_h) = fork_command("$ffmpeg 2>&1");
        $children{$ffmpeg_pid} = 'ffmpeg' if ($ffmpeg_pid);

    # Get ready to count the frames that have been processed
        my ($frames, $fps, $start);
        $frames = 0;
        $fps = 0.0;
        $start = time();
        if (!$episode->{'total_frames'} || $episode->{'total_frames'} < 1) {
            $episode->{'total_frames'} = ($episode->{'last_frame'} && $episode->{'last_frame'} > 0)
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
                $fps = sprintf('%.2f', ($frames * 1.0) / (time() - $start));
            }
            else {
                $pct = '~';
            }
            unless (arg('noprogress')) {
                print "\rprocessed:  $frames of $episode->{'total_frames'} frames at $fps fps ($pct\%, eta: ",
                      $self->build_eta($frames, $episode->{'total_frames'}, $fps),
                      ')  ';
            }

        # Read from the ffmpeg handle
            while (has_data($ffmpeg_h) and $l = <$ffmpeg_h>) {
                if ($l =~ /frame=\s*(\d+)/) {
                    $frames = int($1);
                    if ($episode->{'total_frames'} < $frames) {
                        $episode->{'total_frames'} = $frames;
                    }
                }
            # ffmpeg warnings?
                elsif ($l =~ /^Un(known|supported).+?(codec|format)/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            # Segfault?
                elsif ($l =~ /^Segmentation\sfault/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            # Most likely a misconfigured command line
                elsif ($l =~ /^Unable\s(?:to|for)\s(?:find|parse)/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            # Another error?
                elsif ($l =~ /\b(?:Error\swhile|Invalid\svalue)\b/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            # Unrecognized options
                elsif ($l =~ /^ffmpeg:\sunrecognized\soption/m) {
                    $warnings .= $l;
                    die "\n\nffmpeg had critical errors:\n\n$warnings";
                }
            }
        # Read from the mythtranscode handle?
            while (has_data($mythtrans_h) and $l = <$mythtrans_h>) {
                if ($l =~ /Processed:\s*(\d+)\s*of\s*(\d+)\s*frames\s*\((\d+)\s*seconds\)/) {
                # ffmpeg doesn't report fame counts for audio streams, so grab it here.
                    if ($self->{'audioonly'}) {
                        $frames = int($1);
                    }
                # mythtranscode is going to be the most accurate total frame count.
                    my $total = $2 - $episode->{'cutlist_frames'};
                    if ($episode->{'total_frames'} < $total) {
                        $episode->{'total_frames'} = $total;
                    }
                }
            # Unrecognized options
                elsif ($l =~ /^Couldn't\sfind\srecording/m) {
                    $warnings .= $l;
                    die "\n\nmythtranscode had critical errors:\n\n$warnings";
                }
            }
        # Has the deathtimer been started?  Stick around for awhile, but not too long
            if ($death_timer > 0 && time() - $death_timer > 30) {
                $str = "\n\n$last_death died early.";
                if ($warnings) {
                    $str .= "See ffmpeg warnings:\n\n$warnings";
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
    # Remove the fifodir?
        unlink "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";
        rmdir "/tmp/fifodir_$$";
    }


# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
