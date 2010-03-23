#
# $Date$
# $Revision$
# $Author$
#
#  mencoder.pm
#
#    routines for setting up mencoder
#

package export::mencoder;
    use base 'export::generic';

    use export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;

    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;
    use MythTV;

# Check for mencoder
    sub init_mencoder {
        my $self = shift;
    # Make sure we have mencoder
        find_program('mencoder')
            or push @{$self->{'errors'}}, 'You need mencoder to use this exporter.';
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
    # Gather generic settings
        $self->SUPER::gather_settings($skip ? $skip - 1 : 0);
        return if ($skip);
    }

    #Fix/build mencoder filter chain
    sub build_vop_line{
        my $cmdline    = shift;
        my $vop        = '';
        while ($cmdline =~ m/.*?-vf\s+([^\s]+)\s/gs){
            $vop .= ",$1";
        }
        $vop =~ s/^,+//;
        $vop =~ s/,+$//;
        $cmdline =~ s/-vf\s+[^\s]+\s/ /g;
        $cmdline .= " -vf $vop ";
        return $cmdline;
    }

    sub export {
        my $self       = shift;
        my $episode    = shift;
        my $suffix     = (shift or '');
        my $skip_audio = shift;
    # Init the commands
        my $mencoder   = '';
        my $mythtranscode = '';

    # Generate a cutlist?
        $self->gen_cutlist($episode);

    # Start the mencoder command
        $mencoder = "$NICE mencoder";
    # Import aspect ratio
        $mencoder .= ' -aspect '.$episode->{'finfo'}{'aspect_f'};
    # Not an mpeg mencoder can not do cutlists (from what I can tell..)
        unless ($episode->{'finfo'}{'is_mpeg'} && !$self->{'use_cutlist'}) {
        # swap red/blue -- used with svcd, need to see if it's needed everywhere
        #   $mencoder .= ' -vf rgb2bgr '; #this is broken in mencoder 1.0preX
        # Set up the fifo dirs?
            if (-e "/tmp/fifodir_$$/vidout" || -e "/tmp/fifodir_$$/audout") {
                die "Possibly stale mythtranscode fifo's in /tmp/fifodir_$$/.\nPlease remove them before running nuvexport.\n\n";
            }
        # Here, we have to fork off a copy of mythtranscode (need to use --fifosync with mencoder? needs testing)
            my $mythtranscode_bin = find_program('mythtranscode');
            $mythtranscode = "$NICE $mythtranscode_bin --showprogress"
                            ." -p '$episode->{'transcoder'}'"
                            ." -c '$episode->{'chanid'}'"
                            ." -s '".unix_to_myth_time($episode->{'recstartts'})."'"
                            ." -f \"/tmp/fifodir_$$/\"";
        # On no-audio encodes, we need to do something to keep mythtranscode's audio buffers from filling up available RAM
        #    $mythtranscode .= ' --fifosync' if ($skip_audio);
        # let mythtranscode handle the cutlist
            $mythtranscode .= ' --honorcutlist' if ($self->{'use_cutlist'});
        }
    # Figure out the input files
        if ($episode->{'finfo'}{'is_mpeg'} && !$self->{'use_cutlist'}) {
            $mencoder .= ' -idx '.shell_escape($episode->{'local_path'});
        }
        else {
            $mencoder .= " -noskip -idx /tmp/fifodir_$$/vidout -audiofile /tmp/fifodir_$$/audout  "
                         .' -demuxer 20 -audio-demuxer 20 -rawaudio'
                         .' rate='.$episode->{'finfo'}{'audio_sample_rate'}.':channels='.$episode->{'finfo'}{'audio_channels'}
                         .' -demuxer 26 -rawvideo'
                         .' w='.$episode->{'finfo'}{'width'}.':h='.$episode->{'finfo'}{'height'}
                         .':fps='.$episode->{'finfo'}{'fps'}
                         ;
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
        # Figure out the new width/height
            my $w = $episode->{'finfo'}{'width'}  - $r - $l;
            my $h = $episode->{'finfo'}{'height'} - $t - $b;
            $mencoder .= " -vf crop=$w:$h:$l:$t " if ($t || $r || $b || $l);
        }
    # Filters
        if ($self->{'deinterlace'}) {
            $mencoder .= " -vf lavcdeint";
        }
        if ($self->{'noise_reduction'}) {
            $mencoder .= " -vf denoise3d";
        }
    # Add any additional settings from the child module.
        $mencoder .= ' '.$self->{'mencoder_xtra'};
    # Output directory set to null means the first pass of a multipass
        if (!$self->{'path'} || $self->{'path'} =~ /^\/dev\/null\b/) {
            $mencoder .= ' -o /dev/null';
        }
    # Add the output filename
        else {
            $mencoder .= ' -o '.shell_escape($self->get_outfile($episode, $suffix));
        }
    # mencoder pids
        my ($mythtrans_pid, $mencoder_pid, $mythtrans_h, $mencoder_h);
    # Set up and run mythtranscode?
        if ($mythtranscode) {
        # Create a directory for mythtranscode's fifo's
            mkdir("/tmp/fifodir_$$/", 0755) or die "Can't create /tmp/fifodir_$$/:  $!\n\n";
            ($mythtrans_pid, $mythtrans_h) = fork_command("$mythtranscode 2>&1");
            $children{$mythtrans_pid} = 'mythtranscode' if ($mythtrans_pid);
            fifos_wait("/tmp/fifodir_$$/");
            push @tmpfiles, "/tmp/fifodir_$$", "/tmp/fifodir_$$/audout", "/tmp/fifodir_$$/vidout";
        }
    #Fix -vf options before we execute mencoder
        $mencoder = build_vop_line($mencoder);
    # Execute mencoder
        print "Starting mencoder.\n" unless ($DEBUG);
        ($mencoder_pid, $mencoder_h) = fork_command("$mencoder 2>&1");
        $children{$mencoder_pid} = 'mencoder' if ($mencoder_pid);
    # Get ready to count the frames that have been processed
        my ($frames, $fps);
        $frames = 0;
        $fps    = 0.0;
        my $total_frames = $episode->{'last_frame'} > 0
                            ? $episode->{'last_frame'} - $episode->{'cutlist_frames'}
                            : 0;
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
            }
            else {
                $pct = "0.00";
            }
            unless (arg('noprogress')) {
                print "\rprocessed:  $frames of $total_frames frames ($pct\%), $fps fps ";
            }
        # Read from the mencoder handle
            while (has_data($mencoder_h) and $l = <$mencoder_h>) {
                if ($l =~ /^Pos:.*?(\d+)f.*?\(.*?(\d+(?:\.\d+)?)fps/) {
                    $frames = int($1);
                    $fps    = $2;
                }
            # Look for error messages
                elsif ($l =~ m/\[mencoder\] warning/) {
                    $warnings .= $l;
                }
                elsif ($l =~ m/\[mencoder\] critical/) {
                    $warnings .= $l;
                    die "\nmencoder had critical errors:\n\n$warnings";
                }
            }
        # Read from the mythtranscode handle?
            if ($mythtranscode && $mythtrans_pid) {
                while (has_data($mythtrans_h) and $l = <$mythtrans_h>) {
                    if ($l =~ /Processed:\s*(\d+)\s*of\s*(\d+)\s*frames\s*\((\d+)\s*seconds\)/) {
                        #$frames       = int($1);
                        $total_frames ||= $2 - $episode->{'cutlist_frames'};
                    }
                }
            }
        # Has the deathtimer been started?  Stick around for awhile, but not too long
            if ($death_timer > 0 && time() - $death_timer > 30) {
                $str = "\n\n$last_death died early.";
                if ($warnings) {
                    $str .= "See mencoder warnings:\n\n$warnings";
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
        # Sleep for 1/100 second so we don't go too fast and annoy the cpu
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
