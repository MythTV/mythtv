#!/usr/bin/perl -w
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

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &fork_command &has_data &fifos_wait
                        /;
    }

# Load the following extra parameters from the commandline
    add_arg('path:s',                        'Save path (only used with the noserver option)');
    add_arg('underscores!',                  'Convert spaces to underscores for output filename.');
    add_arg('use_cutlist|cutlist!',          'Use the myth cutlist (or not)');

# These aren't used by all modules, but the routine to define them is here, so here they live
    add_arg('height|v_res|h=s',              'Output height.');
    add_arg('width|h_res|w=s',               'Output width.');
    add_arg('deinterlace!',                  'Deinterlace video.');
    add_arg('noise_reduction|denoise|nr!',   'Enable noise reduction.');
    add_arg('fast_denoise|fast-denoise!',    'Use fast noise reduction instead of standard.');
    add_arg('crop!',                         'Crop out broadcast overscan.');
    add_arg('force_aspect|force-aspect=f',   'Force input aspect ratio rather than detect it.');

# Load defaults
    sub load_defaults {
        my $self = shift;
    # These defaults apply to all modules
        $self->{'defaults'}{'use_cutlist'}     = 1;
        $self->{'defaults'}{'noise_reduction'} = 1;
        $self->{'defaults'}{'deinterlace'}     = 1;
        $self->{'defaults'}{'crop'}            = 1;
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
            $self->{'noise_reduction'} = query_text('Enable noise reduction (slower, but better results)?',
                                                    'yesno',
                                                    $self->val('noise_reduction'));
        # Deinterlace video?
            $self->{'deinterlace'} = query_text('Enable deinterlacing?',
                                                'yesno',
                                                $self->val('deinterlace'));
        # Crop video to get rid of broadcast padding
            $self->{'crop'} = query_text('Crop broadcast overscan (2% border)?',
                                         'yesno',
                                         $self->val('crop'));
        }
    }

# Check for a duplicate filename, and return a full path to the output filename
    sub get_outfile {
        my $self    = shift;
        my $episode = shift;
        my $suffix  = ($self->{'suffix'} or shift);
    # Build a clean filename
        my $outfile;
        if ($episode->{'outfile'}{$suffix}) {
            $outfile = $episode->{'outfile'}{$suffix};
        }
        else {
            if ($episode->{'show_name'} ne 'Untitled' and $episode ne 'Untitled') {
                $outfile = $episode->{'show_name'}.' - '.$episode->{'title'};
            }
            elsif($episode->{'show_name'} ne 'Untitled') {
                $outfile = $episode->{'show_name'};
            }
            elsif($episode ne 'Untitled') {
                $outfile = $episode->{'title'};
            }
            else {
                $outfile = 'Untitled';
            }
            $outfile =~ s/(?:[\/\\\:\*\?\<\>\|\-]+\s*)+(?=[^\s\/\\\:\*\?\<\>\|\-])/- /sg;
            $outfile =~ tr/"/'/s;
        # add underscores?
            if ($self->val('underscores')) {
                $outfile =~ tr/ /_/s;
            }
        # Make sure we don't have a duplicate filename
            if (-e $self->{'path'}.'/'.$outfile.$suffix) {
                my $count = 1;
                my $out   = $outfile;
                while (-e $self->{'path'}.'/'.$out.$suffix) {
                    $count++;
                    $out = "$outfile.$count";
                }
                $outfile = $out;
           }
        # Store it so we don't have to recalculate this next time
            $episode->{'outfile'}{$suffix} = $outfile;
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
        }
        print "Default resolution based on 4:3 aspect ratio.\n";
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
            $self->{'height'} = $self->{'width'} / 4 * 3;
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
        my $fifodir = shift;
        my $overload = 0;
        if (!$DEBUG) {
            while (++$overload < 30 && !(-e "$fifodir/audout" && -e "$fifodir/vidout" )) {
                sleep 1;
                print "Waiting for mythtranscode to set up the fifos.\n";
            }
            unless (-e "$fifodir/audout" && -e "$fifodir/vidout") {
                die "Waited too long for mythtranscode to create its fifos.  Please try again.\n\n";
            }
        }
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
