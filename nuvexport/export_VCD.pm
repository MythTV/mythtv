package export_VCD;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'            => 'Export to VCD',
					 'enabled'         => 1,
					 'started'         => 0,
					 'fifodir'         => "/tmp/fifodir.$$",
					 'children'        => [],
					 'errors'          => undef,
					 'episode'         => undef,
					 'savepath'        => '.',
					 'outfile'         => 'out.mpg',
					 'tmp_a'           => 'out.mp2',
					 'tmp_v'           => 'out.m2v',
					 'use_cutlist'     => 0,
					 'noise_reduction' => 1,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
	# Make sure that we have an mp2 encoder
		$Prog{mp2_encoder} = find_program('toolame', 'mp2enc');
		push @{$self->{errors}}, 'You need toolame or mp2enc to export an vcd.' unless ($Prog{mp2_encoder});
	# Make sure that we have an mplexer
		$Prog{mplexer} = find_program('tcmplex', 'mplex');
		push @{$self->{errors}}, 'You need tcmplex or mplex to export an vcd.' unless ($Prog{mplexer});
	# Any errors?  disable this function
		$self->{enabled} = 0 if ($self->{errors} && @{$self->{errors}} > 0);
	# Return
		return $self;
	}

	sub gather_data {
		my $self    = shift;
		my $default_filename;
	# Get the save path
		$self->{savepath} = $gui->query_savepath();
	# Ask the user for the filename
		if($self->{episode}->{show_name} ne 'Untitled' and $self->{episode}->{title} ne 'Untitled')
		{
			$default_filename = $self->{episode}->{show_name}.' - '.$self->{episode}->{title};
		}
		elsif($self->{episode}->{show_name} ne 'Untitled')
		{
			$default_filename = $self->{episode}->{show_name};
		}
		elsif($self->{episode}->{title} ne 'Untitled')
		{
			$default_filename = $self->{episode}->{title};
		}

		$self->{outfile} = $gui->query_filename($default_filename, 'mpg', $self->{savepath});
	# Ask the user if he/she wants to use the cutlist
		if ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/) {
			$self->{use_cutlist} = $gui->query_text('Enable Myth cutlist?',
													'yesno',
													$self->{use_cutlist} ? 'Yes' : 'No');
		}
		else {
			$gui->notify('No cutlist found.  Hopefully this means that you already removed the commercials.');
		}
	# Ask the user if he/she wants noise reduction
		$self->{noise_reduction} = $gui->query_text('Enable noise reduction (slower, but better results)?',
													'yesno',
													$self->{noise_reduction} ? 'Yes' : 'No');
	# Do we want bin/cue files, or just an mpeg?
		# nothing, at the moment.
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
		($self->{tmp_v} = $self->{episode}->{filename}) =~ s/\.nuv$/.m1v/;
	# Here, we have to fork off a copy of mythtranscode
		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
		$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);
		fifos_wait($self->{fifodir});
	# Now we fork off a process to encode the audio
		if ($Prog{mp2_encoder} =~ /\btoolame$/) {
			$sample = $nuv_info{audio_sample_rate} / 1000;
			$command = "nice -n 19 toolame -s $sample -m j -b 224 $self->{fifodir}/audout $self->{tmp_a}";
		}
		else {
			$command = "nice -n 19 ffmpeg -f s16le -ar $nuv_info{audio_sample_rate} -ac 2 -i $self->{fifodir}/audout -vn -f wav -"
					  ." | nice -n 19 mp2enc -V -s -o $self->{tmp_a}";
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
			$command .= " | nice -n 19 yuvdenoise -r 16" if ($self->{noise_reduction});
			$command .= " | nice -n 19 yuvscaler -v 0 -n p -M BICUBIC -O VCD";
			$framerate = 3;
		}
	# Other options for NTSC
		else {
			# SOMEDAY I'd like to be able to get 3:2 pulldown working properly....
			#$command .= " | yuvkineco -F 1" if ($pulldown);
			$command .= " | nice -n 19 yuvdenoise -r 16" if ($self->{noise_reduction});
			$command .= " | nice -n 19 yuvscaler -v 0 -n n -O VCD";
			$framerate = 4;
		}
	# Finish building $command, and execute it
		$command .= " | nice -n 19 mpeg2enc --format 1 --quantisation-reduction 2"
					." --frame-rate $framerate -n n"
					#.($pulldown ? ' --frame-rate 1 --3-2-pulldown' : " --frame-rate $framerate")
					." --sequence-length 600"
					." --reduction-4x4 1 --reduction-2x2 1 --keep-hf"
					.($cpus > 1 ? " --multi-thread $cpus" : '')
					." -o $self->{tmp_v}";
		push @{$self->{children}}, fork_command($command);
	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;
	# Multiplex the streams
		my $safe_outfile = shell_escape($self->{outfile});
		if ($Prog{mplexer} =~ /\btcmplex$/) {
			system("nice -n 19 tcmplex -m v -i $self->{tmp_v} -p $self->{tmp_a} -o $safe_outfile");
		}
		else {
			system("nice -n 19 mplex -f 1 $self->{tmp_v} $self->{tmp_a} -o $safe_outfile");
		}
	}

	sub cleanup {
		my $self = shift;
		return unless ($self->{started});
	# Make sure any child processes also go away
		if ($self->{children} && @{$self->{children}}) {
			foreach my $child (@{$self->{children}}) {
				kill('INT', $child);
			}
			1 while (wait > 0);
		}
	# Remove any temporary files
		foreach my $file ("$self->{fifodir}/audout", "$self->{fifodir}/vidout", $self->{tmp_a}, $self->{tmp_v}) {
			unlink $file if (-e $file);
		}
		rmdir $self->{fifodir} if (-e $self->{fifodir});
	}

1;	#return true
