package nuv_utils;

	use Exporter;
	our @ISA = qw/ Exporter /;
	our @EXPORT = qw/ generate_showtime find_program nuv_info num_cpus fork_command shell_escape mysql_escape Quit /;

# This is used to tell child processes apart from the parent, so that cleanup routines do NOT get run by children.
	our $is_child = 0;

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
		open(DATA, "$main::video_dir/$file") or die "Can't open $file:  $!\n\n";
	# Read the file info header
		read(DATA, $buffer, 72);
	# Unpack the data structure
		($info{finfo},			# "NuppelVideo" + \0
		 $info{version},        # "0.05" + \0
		 $info{width},
		 $info{height},
		 $info{desiredheight},  # 0 .. as it is
		 $info{desiredwidth},   # 0 .. as it is
		 $info{pimode},         # P .. progressive, I .. interlaced  (2 half pics) [NI]
		 $info{aspect},         # 1.0 .. square pixel (1.5 .. e.g. width=480: width*1.5=720 for capturing for svcd material
		 $info{fps},
		 $info{videoblocks},	# count of video-blocks -1 .. unknown   0 .. no video
		 $info{audioblocks},	# count of audio-blocks -1 .. unknown   0 .. no audio
		 $info{textsblocks},	# count of text-blocks  -1 .. unknown   0 .. no text
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
				 $info{video_fourcc},
				 $info{audio_fourcc},
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
				 ) = unpack('iiiiiiiiiiiiiiill', $buffer);
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
		$info{is_mpeg} = 1;
	# Grab tcprobe info
		if ($program =~ /tcprobe$/) {
			my $data = `$program -i '$file'`;
			($info{width}, $info{height}) = $data =~ /frame\s+size:\s+-g\s+(\d+)x(\d+)\b/m;
			($info{fps})                  = $data =~ /frame\s+rate:\s+-f\s+(\d+(?:\.\s+)?)\b/m;
			($info{audio_sample_rate})    = $data =~ /audio\s+track:.+?-e\s+(\d+)\b/m;
		}
	# Grab tcmplex info
		elsif ($program =~ /mpgtx$/) {
			my $data = `$program -i '$file'`;
			($info{width}, $info{height}, $info{fps}) = $data =~ /\bSize\s+\[(\d+)\s*x\s*(\d+)\]\s+(\d+(?:\.\d+)?)\s*fps/m;
			($info{audio_sample_rate})    = $data =~ /\b(\d+)\s*Hz/m;
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
	# Fork and return the child's pid
		my $pid = undef;
		if ($pid = fork) {
			return $pid
		}
	# $pid defined means that this is now the forked child
		elsif (defined $pid) {
			$is_child = 1;
			system($command);
		# Don't forget to exit, or we'll keep going back into places that the child shouldn't play
			exit(0);
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
		print "\nThanks for using nuvexport!\n\n" unless ($is_child);
	# Time to leave
		exit;
	}

	# Allow the functions to clean up after themselves - and make sure that they can, but not if they are marked as child processes
	END {
		if (!$is_child && @main::Functions) {
			foreach $function (@main::Functions) {
				$function->cleanup;
			}
		}
	}

1;	#return true
