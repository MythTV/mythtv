package export_DVD;

# Define any command line parameters used by this module
	*Arg_str  = *main::Arg_str;
# These are loaded by export_DVD, so putting them here is redundant
#	push @Arg_str,
#			'a_bitrate|ar:i',
#			'v_bitrate|vr:i',
#			'quantisation:f',
#			'use_cutlist',
#			'noise_reduction|denoise';

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Args  = *main::Args;
	*Prog  = *main::Prog;
	*gui   = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					# The name of this export function - displayed to users in interactive mode
					 'name'            => 'Export to DVD',
					# The --mode match (regex) for non-interactive command line interface
					 'cli_mode'        => 'dvd',
					# Set to 0 to disable this export function (eg. if there are errors)
					 'enabled'         => 1,
					# An array of errors - generally displayed to users when they try to use a disabled function
					 'errors'          => undef,
					# Misc variables all export functions should keep track of
					 'started'         => 0,
					 'children'        => [],
					 'episode'         => undef,
					 'is_cli'          => 0,
					# Variables specific to export_DVD
					 'fifodir'         => "fifodir.$$",
					 'default_outfile' => 'Untitled',
					 'savepath'        => '.',
					 'outfile'         => 'out.mpg',
					 'tmp_a'           => 'out.mp2',
					 'tmp_v'           => 'out.m2v',
					 'use_cutlist'     => 0,
					 'a_bitrate'       => 384,
					 'v_bitrate'       => 3800,
					 'quantisation'    => 5,		# 4 through 6 is probably right...
					 'noise_reduction' => 1,
					 'author_now'      => 1,
					 'burn_now'        => 0,
					 'dvddir'          => "dvddir.$$",
					 'dvdr_speed'      => 4,		# 4x drives are most popular now ...
					 'dvdr_path'       => '/dev/dvd',
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
	# Make sure that we have an mplexer
		$Prog{mplexer} = find_program('tcmplex', 'mplex');
		push @{$self->{errors}}, 'You need tcmplex or mplex to export a dvd.' unless ($Prog{mplexer});
	# Make sure that we have the other necessary programs
		find_program('yuvscaler')
			or push @{$self->{errors}}, 'You need yuvscaler to export a dvd.';
	# Do we have yuvdenoise?
		$Prog{yuvdenoise} = find_program('yuvdenoise');
	# Do we have toolame?
		$Prog{toolame} = find_program('toolame');
	# Do we have dvdauthor?
		$Prog{dvdauthor} = find_program('dvdauthor');
	# Do we have growisofs?
		$Prog{growisofs} = find_program('growisofs');
	# Any errors?  disable this function
		$self->{enabled} = 0 if ($self->{errors} && @{$self->{errors}} > 0);
	# Return
		return $self;
	}

	sub gather_cli_data {
		my $self = shift;
		return undef unless ($Args{mode} && $Args{mode} =~ /$self->{cli_mode}/i);
	# Get the save path
		($self->{savepath}, $self->{outfile}) = $gui->query_filename($self->{default_outfile}, 'mpeg', $self->{savepath});
	# Audio bitrate
		$self->{a_bitrate} = $Args{a_bitrate} if ($Args{a_bitrate});
		die "Audio bitrate is too low; please choose a bitrate >= 64.\n\n"  if ($self->{a_bitrate} < 64);
		die "Audio bitrate is too high; please choose a bitrate <= 384\n\n" if ($self->{a_bitrate} > 384);
	# Video Bitrate
		my $max_v_bitrate = $self->{v_bitrate} = 2742 - $self->{a_bitrate} > 2500 ? 2500 : 2742 - $self->{a_bitrate};
		$self->{v_bitrate} = $Args{v_bitrate} if ($Args{v_bitrate});
		die "Video bitrate is too low; please choose a bitrate >= 1000.\n\n"           if ($self->{v_bitrate} < 1000);
		die "Video bitrate is too high; please choose a bitrate <= $max_v_bitrate\n\n" if ($self->{v_bitrate} > $max_v_bitrate);
	# Quantisation
		$self->{quantisation}    = $Args{quantisation} if ($Args{quantisation});
	# Cutlist and noise reduction don't require checking
		$self->{use_cutlist}     = ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/ && $Args{use_cutlist}) ? 1 : 0;
	# Noise reduction
		$self->{noise_reduction} = $Args{noise_reduction};
	# Return true, so we skip the interactive data gathering
		return 1;
	}

	sub gather_data {
		my $self    = shift;
		my $default_filename = '';
	# Build a default filename
		if ($self->{episode}->{show_name} ne 'Untitled' and $self->{episode}->{title} ne 'Untitled') {
			$self->{default_outfile} = $self->{episode}->{show_name}.' - '.$self->{episode}->{title};
		}
		elsif ($self->{episode}->{show_name} ne 'Untitled') {
			$self->{default_outfile} = $self->{episode}->{show_name};
		}
		elsif ($self->{episode}->{title} ne 'Untitled') {
			$self->{default_outfile} = $self->{episode}->{title};
		}
	# grabbing data from the command line?
		return if ($self->gather_cli_data());
	# Get the save path
		$self->{savepath} = $gui->query_savepath();
		$self->{outfile}  = $gui->query_filename($self->{default_outfile}, 'mpeg', $self->{savepath});
	# Ask the user if he/she wants to use the cutlist
		if ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/) {
			$self->{use_cutlist} = $gui->query_text('Enable Myth cutlist?',
													'yesno',
													$self->{use_cutlist} ? 'Yes' : 'No');
		}
		else {
			$gui->notify('No cutlist found.  Hopefully this means that you already removed the commercials.');
		}
	# Ask the user what audio bitrate he/she wants
		my $a_bitrate = $gui->query_text('Audio bitrate?',
										 'int',
										 $self->{a_bitrate});
		while ($a_bitrate < 64 && $a_bitrate > 384) {
			if ($a_bitrate < 64) {
				$gui->notify('Too low; please choose a bitrate >= 64.');
			}
			elsif ($a_bitrate > 384) {
				$gui->notify('Too high; please choose a bitrate <= 384.');
			}
			$a_bitrate = $gui->query_text('Audio bitrate?',
										  'int',
										  $self->{a_bitrate});
		}
		$self->{a_bitrate} = $a_bitrate;
	# Ask the user what video bitrate he/she wants, or calculate the max bitrate (2756 max, though we round down a bit since some dvd players can't handle the max)
	# Then again, mpeg2enc seems to have trouble with bitrates > 2500
		$self->{v_bitrate} = 2742 - $self->{a_bitrate} > 2500 ? 2500 : 2742 - $self->{a_bitrate};
		my $v_bitrate = $gui->query_text('Maximum video bitrate for VBR?',
										 'int',
										 $self->{v_bitrate});
		while ($v_bitrate < 1000 && $v_bitrate > $self->{v_bitrate}) {
			if ($v_bitrate < 1000) {
				$gui->notify('Too low; please choose a bitrate >= 1000.');
			}
			elsif ($v_bitrate > $self->{v_bitrate}) {
				$gui->notify("Too high; please choose a bitrate <= $self->{v_bitrate}.");
			}
			$v_bitrate = $gui->query_text('Maximum video bitrate for VBR?',
										  'int',
										  $self->{v_bitrate});
		}
		$self->{v_bitrate} = $v_bitrate;
	# Ask the user what vbr quality (quantisation) he/she wants - 2..31
		my $quantisation = $gui->query_text('VBR quality/quantisation (2-31)?', 'float', $self->{quantisation});
		while ($quantisation < 2 && $quantisation > 31) {
			if ($quantisation < 2) {
				print "Too low; please choose a number between 2 and 31.\n";
			}
			elsif ($quantisation > 31) {
				print "Too high; please choose a number between 2 and 31\n";
			}
			$quantisation = $gui->query_text('VBR quality/quantisation (2-31)?',
											 'float',
											 $self->{quantisation});
		}
		$self->{quantisation} = $quantisation;
	# Ask the user what vbr quality (quantisation) he/she wants - 2..31
		if ($Prog{yuvdenoise}) {
			$self->{noise_reduction} = $gui->query_text('Enable noise reduction (slower, but better results)?',
														'yesno',
														$self->{noise_reduction} ? 'Yes' : 'No');
		}
		else {
			$gui->notify('Couldn\'t find yuvdenoise.  Please install it if you want noise reduction.');
		}
	# Do we want dvd/iso files, or just an mpeg?
		if ($Prog{dvdauthor}) {
			$self->{author_now} = $gui->query_text('Author a DVD (versus just outputting an MPEG)?',
														'yesno',
														$self->{author_now} ? 'Yes' : 'No');
		} else {
			$self->{author_now} = 0;
		} 
		if ($self->{author_now} && $Prog{growisofs}) {
			$self->{burn_now} = $gui->query_text('Burn the authored DVD?',
														'yesno',
														$self->{burn_now} ? 'Yes' : 'No');
		} else {
			$self->{burn_now} = 0;
		} 
	}

	sub execute {
		my $self = shift;
	# make sure that the fifo dir is clean
		if (-e "$self->{fifodir}/vidout" || -e "$self->{fifodir}/audout") {
			die "Possibly stale mythtranscode fifo's in $self->{fifodir}.\nPlease remove them before running nuvexport.\n\n";
		}
	# Gather any necessary data
		$self->{episode} = shift;
		$self->gather_data;
	# Load nuv info
		my %nuv_info = nuv_info($self->{episode}->{filename});
	# Set this to true so that the cleanup routine actually runs
		$self->{started} = 1;
	# Create a directory for mythtranscode's fifo's
		unless (-d $self->{fifodir}) {
			mkdir($self->{fifodir}, 0755) or die "Can't create $self->{fifodir}:  $!\n\n";
		}
	# Generate some names for the temporary audio and video files
		($self->{tmp_a} = $self->{episode}->{filename}) =~ s/\.nuv$/.mp2/;
		($self->{tmp_v} = $self->{episode}->{filename}) =~ s/\.nuv$/.m2v/;
	# Here, we have to fork off a copy of mythtranscode
		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
		$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);
		fifos_wait($self->{fifodir});
	# Now we fork off a process to encode the audio
		if ($Prog{toolame}) {
		$command = "nice -n 19 sox -t raw -r $nuv_info{audio_sample_rate} -s -w  -c 2 $self->{fifodir}/audout -t raw -r 48000 -s -w -c 2 - | nice -n 19 toolame -s 48.0 -m j -b $self->{a_bitrate} - $self->{tmp_a}";
		} else {
		$command = "nice -n 19 ffmpeg -f s16le -ar $nuv_info{audio_sample_rate} -ac 2 -i $self->{fifodir}/audout -ac 2 -ar 48000 -ab $self->{a_bitrate} -vn -f mp2 $self->{tmp_a}";
		}

		push @{$self->{children}}, fork_command($command);
	# And lastly, we fork off a process to encode the video
	# Multiple CPU's?  Let's multiprocess
		$cpus = num_cpus();
	# pulldown does NOT work - keeps complaining about unsupport fps even when it's already set to 29.97
		#my $pulldown = 0;
	# Build the command to rescale the image and encode the video
		my $framerate;
		$command = "nice -n 19 ffmpeg -f rawvideo -s $nuv_info{width}x$nuv_info{height} -r $nuv_info{fps} -i $self->{fifodir}/vidout -f yuv4mpegpipe -";
	# Certain options for PAL
		if ($nuv_info{fps} =~ /^2(?:5|4\.9)/) {
			$command .= " | nice -n 19 yuvdenoise -F -r 16" if ($self->{noise_reduction});
			$command .= " | nice -n 19 yuvscaler -v 0 -n p -M BICUBIC -O DVD";
			$framerate = 3;
		}
	# Other options for NTSC
		else {
			# SOMEDAY I'd like to be able to get 3:2 pulldown working properly....
			#$command .= " | yuvkineco -F 1" if ($pulldown);
			$command .= " | nice -n 19 yuvdenoise -F -r 16" if ($self->{noise_reduction});
			$command .= " | nice -n 19 yuvscaler -v 0 -n n -M BICUBIC -O DVD";
			$framerate = 4;
		}
	# Finish building $command, and execute it
		$command .= " | nice -n 19 mpeg2enc --format 8 --quantisation $self->{quantisation} --quantisation-reduction 2"
					." --video-bitrate $self->{v_bitrate} --aspect 2 --frame-rate $framerate"
					." --interlace-mode 0 --video-buffer 230"
					." --nonvideo-bitrate $self->{a_bitrate} --sequence-length 795"
					." --reduction-4x4 2 --reduction-2x2 1 --keep-hf"
					.($cpus > 1 ? " --multi-thread $cpus" : '')
					." -o $self->{tmp_v}";
		push @{$self->{children}}, fork_command($command);
	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;
	# Multiplex the streams
		my $safe_outfile = shell_escape($self->{savepath} eq '.' ? $self->{outfile} : "$self->{savepath}/$self->{outfile}");
		if ($Prog{mplexer} =~ /\btcmplex$/) {
			$command = "nice -n 19 tcmplex -m d -D 0 -i $self->{tmp_v} -p $self->{tmp_a} -o $safe_outfile";
		}
		else {
			$command = "nice -n 19 mplex -f 8 $self->{tmp_v} $self->{tmp_a} -o $safe_outfile";
		}
		system($command);

		if ($self->{author_now}) {
			$command = "nice -n 20 dvdauthor -t -o $self->{dvddir} $safe_outfile";
			system($command);
			$command = "nice -n 20 dvdauthor -T -o $self->{dvddir}";
			system($command);
			if ($self->{burn_now}) {
				$command = "nice -n 20 growisofs -speed=$self->{dvdr_speed} -Z $self->{dvdr_path} -dvd-video $self->{dvddir}";
				system($command);
			} else {
				$command = "nice -n 20 mkisofs -dvd-video -udf -o $safe_outfile.dvd.iso $self->{dvddir}";
				system($command);
			}
		} 
	}

	sub cleanup {
		my $self = shift;
	# Remove any temporary files
		foreach my $file ("$self->{fifodir}/audout", "$self->{fifodir}/vidout", $self->{tmp_a}, $self->{tmp_v}) {
			unlink $file if (-e $file);
		}
		rmdir $self->{fifodir} if (-d $self->{fifodir});
	# Make sure any child processes also go away
		if ($self->{children} && @{$self->{children}}) {
			foreach my $child (@{$self->{children}}) {
				kill('INT', $child);
			}
			1 while (wait > 0);
		}
	}

1;	#return true
