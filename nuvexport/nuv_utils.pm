#!/usr/bin/perl -w
#Last Updated: 2004.08.22 (xris)
#
#  nuv_utils
#
#   Utility routines for nuvexport and nuvinfo
#

package nuv_utils;

    use Time::HiRes qw(usleep);

    use Exporter;
    our @ISA = qw/ Exporter /;
    our @EXPORT = qw/ generate_showtime find_program nuv_info num_cpus
                      fork_command system fifos_wait has_data
                      shell_escape mysql_escape Quit
                    /;

    *DEBUG = *main::DEBUG;

    $is_child = 0;

# Returns a nicely-formatted timestamp from a specified time
    sub generate_showtime {
        $showtime = '';
    # Get the
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

# This searches the path for the specified programs, and returns the lowest-index-value program found
    sub find_program {
    # Load the programs, and get a count of the priorities
        my(%programs, $num_programs);
        foreach my $program (@_) {
            $programs{$program} = ++$num_programs;
        }
    # No programs requested?
        return undef unless ($num_programs > 0);
    # Search for the program(s)
        my %found;
        foreach my $path (split(/:/, $ENV{PATH}), '.') {
            foreach my $program (keys %programs) {
                if (-e "$path/$program" && (!$found{name} || $programs{$program} < $programs{$found{name}})) {
                    $found{name} = $program;
                    $found{path} = $path;
                }
            # Leave early if we found the highest priority program
                last if ($found{name} && $programs{$found{name}} == 1);
            }
        }
    # Return
        return undef unless ($found{path} && $found{name});
        return $found{path}.'/'.$found{name};
    }

# Opens a .nuv file and returns information about it
    sub nuv_info {
        my $file = shift;
        my(%info, $buffer);
    # open the file
        if ($main::video_dir and -e $main::video_dir and -e "$main::video_dir/$file") {
            open(DATA, "$main::video_dir/$file") or die "Can't open $file:  $!\n\n";
        }
        else {
            open(DATA, $file) or die "Can't open $file:  $!\n\n";
        }
    # Read the file info header
        read(DATA, $buffer, 72);
    # Unpack the data structure
        ($info{finfo},          # "NuppelVideo" + \0
         $info{version},        # "0.05" + \0
         $info{width},
         $info{height},
         $info{desiredheight},  # 0 .. as it is
         $info{desiredwidth},   # 0 .. as it is
         $info{pimode},         # P .. progressive, I .. interlaced  (2 half pics) [NI]
         $info{aspect},         # 1.0 .. square pixel (1.5 .. e.g. width=480: width*1.5=720 for capturing for svcd material
         $info{fps},
         $info{videoblocks},    # count of video-blocks -1 .. unknown   0 .. no video
         $info{audioblocks},    # count of audio-blocks -1 .. unknown   0 .. no audio
         $info{textsblocks},    # count of text-blocks  -1 .. unknown   0 .. no text
         $info{keyframedist}
            ) = unpack('Z12 Z5 xxx i i i i a xxx d d i i i i', $buffer);
    # Is this even a NUV file?
        return mpeg_info($file) unless ($info{finfo} =~ /\w/);
    # Perl occasionally over-reads on the previous read()
        seek(DATA, 72, 0);
    # Read and parse the first frame header
        read(DATA, $buffer, 12);
        my ($frametype,
            $comptype,
            $keyframe,
            $filters,
            $timecode,
            $packetlength) = unpack('a a a a i i', $buffer);
    # Parse the frame
        die "Illegal nuv file format:  $file\n\n" unless ($frametype eq 'D');
    # Read some more stuff if we have to
        read(DATA, $buffer, $packetlength) if ($packetlength);
    # Read the remaining frame headers
        while (12 == read(DATA, $buffer, 12)) {
        # Parse the frame header
            ($frametype,
             $comptype,
             $keyframe,
             $filters,
             $timecode,
             $packetlength) = unpack('a a a a i i', $buffer);
        # Read some more stuff if we have to
            read(DATA, $buffer, $packetlength) if ($packetlength);
        # Look for the audio frame
            if ($frametype eq 'X') {
                my $frame_version;
                ($frame_version,
                 $info{video_type},
                 $info{audio_type},
                 $info{audio_sample_rate},
                 $info{audio_bits_per_sample},
                 $info{audio_channels},
                 $info{audio_compression_ratio},
                 $info{audio_quality},
                 $info{rtjpeg_quality},
                 $info{rtjpeg_luma_filter},
                 $info{rtjpeg_chroma_filter},
                 $info{lavc_bitrate},
                 $info{lavc_qmin},
                 $info{lavc_qmax},
                 $info{lavc_maxqdiff},
                 $info{seektable_offset},
                 $info{keyframeadjust_offset}
                 ) = unpack('ia4a4iiiiiiiiiiiill', $buffer);
            # Found the audio data we want - time to leave
                 last;
            }
        # Done reading frames - let's leave
            else {
                last;
            }
        }
    # Close the file
        close DATA;
    # Make sure some things are actually numbers
        $info{width}  += 0;
        $info{height} += 0;
    # Return
        return %info;
    }

# Uses one of two mpeg info programs to load data about mpeg-based nuv files
    sub mpeg_info {
        my $file = "$main::video_dir/".shift;
        $file =~ s/'/\\'/sg;
        my %info;
    # First, we check for the existence of  an mpeg info program
        my $program = find_program('tcprobe', 'mpgtx');
    # Nothing found?  Die
        die "You need tcprobe (transcode) or mpgtx to use this script on mpeg-based nuv files.\n\n" unless ($program);
    # Set the is_mpeg flag
        $info{is_mpeg}    = 1;
        $info{video_type} = 'MPEG';
    # Grab tcprobe info
        if ($program =~ /tcprobe$/) {
            my $data = `$program -i '$file'`;
            ($info{width}, $info{height}) = $data =~ /frame\s+size:\s+-g\s+(\d+)x(\d+)\b/m;
            ($info{fps})                  = $data =~ /frame\s+rate:\s+-f\s+(\d+(?:\.\d+)?)\b/m;
            ($info{audio_sample_rate})    = $data =~ /audio\s+track:.+?-e\s+(\d+)\b/m;
            if ($data =~ m/MPEG\s+(system|program)/i) {
                $info{mpeg_stream_type}  = "\L$1";
            }
        }
    # Grab tcmplex info
        elsif ($program =~ /mpgtx$/) {
            my $data = `$program -i '$file'`;
            ($info{width}, $info{height}, $info{fps}) = $data =~ /\bSize\s+\[(\d+)\s*x\s*(\d+)\]\s+(\d+(?:\.\d+)?)\s*fps/m;
            ($info{audio_sample_rate})                = $data =~ /\b(\d+)\s*Hz/m;
            if ($data =~ m/Mpeg\s+(\d)\s+(system|program)/i) {
                $info{video_type}       .= $1;
                $info{mpeg_stream_type}  = "\L$2";
            }
        }
    # Return
        return %info;
    }

# Queries /proc/cpuinfo to find out how many cpu's are available on this machine
    sub num_cpus {
        my $cpuinfo = `cat /proc/cpuinfo`;
        $num = 0;
        while ($cpuinfo =~ /^processor\s*:\s*\d+/mg) {
            $num++;
        }
        return $num;
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
            $nuv_utils::is_child = 1;
            close $read;
        # Autoflush $write
            select((select($write), $|=1)[0]);
        # Run the requested command
            my ($data, $buffer) = ('', '');
            open(COM, "$command |") or die "couldn't run command:  $!\n$command\n";
            while (read(COM, $data, 100)) {
                next unless (length $data > 0);
            # Convert CR's to linefeeds so the data will flush properly
                $data =~ s/\r\n?/\n/sg;
            # Some magic so that we only send whole lines (which helps us do nonblocking reads on the other end)
                substr($data, 0, 0) = $buffer;
                $buffer  = '';
                if ($data !~ /\n$/) {
                    ($data, $buffer) = $data =~ /(.+\n)?([^\n]+)$/;
                }
            # We have a line to print?
                if ($data && length $data > 0) {
                    print $write $data;
                }
            # Sleep for 1/20 second so we don't go too fast and annoy the cpu
                usleep(50000);
            }
            close COM;
        # Print the return status of the child
            my $status = $? >> 8;
            print $write "\n!!! process $$ complete:  $status !!!\n";
        # Close the write handle
            close $write;
        # Exit using something that won't set off the END block
            use POSIX;
            POSIX::_exit($status);
        }
    # Couldn't fork, guess we have to quit
        die "Couldn't fork: $!\n\n$command\n\n";
    }

    sub shell_escape {
        $file = shift;
        $file =~ s/(["\$])/\\$1/sg;
        return "\"$file\"";
    }

    sub mysql_escape {
        $string = shift;
        return 'NULL' unless (defined $string);
        $string =~ s/'/\\'/sg;
        return "'$string'";
    }

    sub Quit {
    # If this is the main script, print a nice goodbye message
        print "\nThanks for using nuvexport!\n\n" unless ($nuv_utils::is_child);
    # Time to leave
        exit;
    }

    # Allow the functions to clean up after themselves - and make sure that they can, but not if they are marked as child processes
    END {
        if (@main::Functions) {
            foreach $function (@main::Functions) {
                $function->cleanup;
            }
        }
    }

    sub system {
        my $command = shift;
        if ($DEBUG) {
            $command =~ s#\ 2>/dev/null##sg;
            print "\nsystem call:\n$command\n";
        } else {
            CORE::system($command);
        }
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

1;  #return true
