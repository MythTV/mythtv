#!/usr/bin/perl -w
#Last Updated: 2004.10.02 (xris)
#
#  generic.pm
#
#    generic routines for exporters
#

package export::generic;

    use Time::HiRes qw(usleep);
    use POSIX;

    use nuv_export::shared_utils;
    use nuv_export::ui;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &fork_command &has_data &fifos_wait
                        /;
    }

# Load the following extra parameters from the commandline
    $cli_args{'path:s'}              = 1; # Save path (only used with the noserver option)
    $cli_args{'cutlist|use_cutlist'} = 1; # Use the myth cutlist

# Gather generic export settings
    sub gather_settings {
        my $self = shift;
    # Load the save path, if requested
        $self->{'path'} = query_savepath();
    # Ask the user if he/she wants to use the cutlist
        $self->{'use_cutlist'} = query_text('Enable Myth cutlist?',
                                            'yesno',
                                            'Yes');
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
            # Sleep for 1/100 second so we don't go too fast and annoy the cpu,
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

# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
