package export_WMV;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'        => 'Export WMV',
					 'enabled'     => 1,
					 'errors'      => undef,
					 'episode'     => undef,
					 'savepath'    => '.',
					 'outfile'     => 'out.wmv',
                     'fifodir'     => "/tmp/fifodir.$$",
					 'use_cutlist' => 0,
					 'a_bitrate'   => 64,
					 'v_bitrate'   => 256,
					 'h_res'       => 320,
					 'v_res'       => 240,
					 'sql_file' => undef,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
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

		$self->{outfile} = $gui->query_filename($default_filename, 'wmv', $self->{savepath});
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
		$self->{a_bitrate} = $a_bitrate;
	# Ask the user what video bitrate he/she wants
		my $v_bitrate = $gui->query_text('Video bitrate?',
										 'int',
										 $self->{v_bitrate});
		$self->{v_bitrate} = $v_bitrate;
	# Ask the user what horiz res he/she wants
		my $h_res = $gui->query_text('Horizontal resolution?', 'int', $self->{h_res});
		$self->{h_res} = $h_res;
	# Ask the user what vert res he/she wants
		my $v_res = $gui->query_text('Vertical resolution?', 'int', $self->{v_res});
		$self->{v_res} = $v_res;
	}

	sub execute {
		my $self = shift;
	# make sure that the fifo dir is clean
		if (-e "$self->{fifodir}/vidout" || -e "$self->{fifodir}/audout") {
			die "Possibly stale mythtranscode fifo's in fifodir.\nPlease remove them before running nuvexport.\n\n";
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
	# Here, we have to fork off a copy of mythtranscode
		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir}";
		$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);
		fifos_wait($self->{fifodir});
	# Now we fork off a process to encode everything
		$safe_outfile = shell_escape($self->{outfile});
		$command = "nice -n 19 ffmpeg -y -f s16le -ar $nuv_info{audio_sample_rate} -ac 2 -i $self->{fifodir}/audout -f rawvideo -s $nuv_info{width}x$nuv_info{height} -r $nuv_info{fps} -i $self->{fifodir}/vidout -b $self->{v_bitrate} -ab $self->{a_bitrate} -s $self->{h_res}x$self->{v_res} $safe_outfile";
		push @{$self->{children}}, fork_command($command);
	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;
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
		foreach my $file ("$self->{fifodir}/audout", "$self->{fifodir}/vidout") {
			unlink $file if (-e $file);
		}
		rmdir $self->{fifodir} if (-e $self->{fifodir});
	}

1;	#return true
