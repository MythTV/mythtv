package export_MP3;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'            => 'Export to MP3',
					 'enabled'         => 1,
					 'started'         => 0,
					 'fifodir'         => "fifodir.$$",
					 'children'        => [],
					 'errors'          => undef,
					 'episode'         => undef,
					 'savepath'        => '.',
					 'outfile'         => 'out.mpg',
					 'tmp_a'           => 'out.mp2',
					 'tmp_v'           => 'out.m2v',
					 'bitrate'	   => '224',
					 'use_cutlist'     => 0,
					 'noise_reduction' => 1,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
	# Make sure that we have an mp2 encoder
		$Prog{mp2_encoder} = find_program('toolame', 'mp2enc');
		push @{$self->{errors}}, 'You need toolame or mp2enc to export an mp3.' unless ($Prog{mp2_encoder});
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
		if ($self->{episode}->{show_name} ne 'Untitled' and $self->{episode}->{title} ne 'Untitled') {
			$default_filename = $self->{episode}->{show_name}.' - '.$self->{episode}->{title};
		}
		elsif ($self->{episode}->{show_name} ne 'Untitled') {
			$default_filename = $self->{episode}->{show_name}.' - '.$self->{episode}->{start_time_sep};
		}
		elsif ($self->{episode}->{title} ne 'Untitled') {
			$default_filename = $self->{episode}->{title}.' - '.$self->{episode}->{start_time_sep};
		}

	# Ask the user what audio bitrate he/she wants
		my $bitrate = $gui->query_text('Audio bitrate?', 'int', $self->{bitrate});
		$self->{bitrate} = $bitrate;
		$self->{outfile} = $gui->query_filename($default_filename, 'mp3', $self->{savepath});
	# Ask the user if he/she wants to use the cutlist
		if ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/) {
			$self->{use_cutlist} = $gui->query_text('Enable Myth cutlist?',
													'yesno',
													$self->{use_cutlist} ? 'Yes' : 'No');
		}
		else {
			$gui->notify('No cutlist found.  Hopefully this means that you already removed the commercials.');
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
		($self->{tmp_v} = $self->{episode}->{filename}) =~ s/\.nuv$/.m1v/;
	# Here, we have to fork off a copy of mythtranscode
#		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
	#	$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);
		fifos_wait($self->{fifodir});
	# Now we fork off a process to extract  the audio
	$command = "nice -19 cat < $self->{fifodir}/audout > $self->{tmp_a}" ;
		push @{$self->{children}}, fork_command($command);

	# Null command for video
		$command = "nice -19 cat < $self->{fifodir}/vidout >/dev/null";

		push @{$self->{children}}, fork_command($command);
	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;

        # Now encode the raw file to mp3
			$sample = $nuv_info{audio_sample_rate} / 1000;
		         my $safe_outfile = shell_escape($self->{outfile});
			$command = "nice -n 19 toolame -t1 -s $sample -m j -b $self->{bitrate} $self->{tmp_a} $safe_outfile";
		system($command);

	# Now tag it
	$command = "id3tag -A\"$self->{episode}->{title}\" -a\"$self->{episode}->{channel}\" -c\"$self->{episode}->{description}\" -s\"$self->{episode}->{show_name}\" $safe_outfile";
		system($command);
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
