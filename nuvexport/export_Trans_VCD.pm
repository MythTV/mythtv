package export_Trans_VCD;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'            => 'Transcode export to VCD',
					 'enabled'         => 1,
					 'started'         => 0,
					 'children'        => [],
					 'errors'          => undef,
					 'episode'         => undef,
					 'savepath'        => '.',
					 'outfile'         => 'out.mpg',
					 'use_cutlist'     => 1,
					 'noise_reduction' => 0,
					 'deinterlace'     => 1,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
	# Make sure that we have an mp2 encoder
		$Prog{mp2_encoder} = find_program('toolame', 'mp2enc');
		push @{$self->{errors}}, 'You need toolame or mp2enc to export an svcd.' unless ($Prog{mp2_encoder});
	# Make sure that we have an mplexer
		$Prog{mplexer} = find_program('tcmplex', 'mplex');
		push @{$self->{errors}}, 'You need tcmplex or mplex to export an svcd.' unless ($Prog{mplexer});
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
	# Noise reduction
		$self->{noise_reduction} = $gui->query_text('Enable noise reduction (slower, but better results)?',
														'yesno',
														$self->{noise_reduction} ? 'Yes' : 'No');
		$self->{deinterlace} = $gui->query_text('Enable deinterlacing?',
														'yesno',
														$self->{deinterlace} ? 'Yes' : 'No');
	# Do we want bin/cue files, or just an mpeg?
		# nothing, at the moment.
	}

	sub execute {
		my $self = shift;

	# Gather any necessary data
		$self->{episode} = shift;
		$self->gather_data;
	# Load nuv info
		my %nuv_info = nuv_info($self->{episode}->{filename});
	# Set this to true so that the cleanup routine actually runs
		$self->{started} = 1;

	# Generate some names for the temporary audio and video files
                my $safe_outfile = shell_escape($self->{outfile});
                my $safe_outfile_sub = shell_escape(substr($self->{outfile},0,-4));

		my $command = "nice -n 19 transcode -i $main::video_dir/$self->{episode}->{filename} -x mpeg2 -y mpeg2enc,mp2enc -F 1 -E 44100 -b 224 -o $safe_outfile_sub -V";

		if ($nuv_info{fps} =~ /^2(?:5|4\.9)/) {
                     $command .= " -Z 352x288";
		}
	# Other options for NTSC
 		else {
                     $command .= " -Z 352x240";
		}

		if ($self->{use_cutlist}) {
		  @cuts = split("\n",$self->{episode}->{cutlist});
		  my @skiplist;
		  foreach my $cut (@cuts) {
		     push @skiplist, (split(" - ", $cut))[0]."-".(split(" - ", $cut))[1];
	          }
		  $command .= " -J skip=\"".join(" ", @skiplist)."\"";
	        }
		if ($self->{deinterlace}) {
                  $command .= " -J smartdeinter";
	        }
		if ($self->{noise_reduction}) {
                  $command .= " -J yuvdenoise";
	        }
		push @{$self->{children}}, fork_command($command);

	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;

	# Multiplex the streams
		$command = "nice -n 19 mplex -f 1 $safe_outfile_sub.m1v $safe_outfile_sub.mpa -o $safe_outfile";
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
                my $safe_outfile_sub = shell_escape(substr($self->{outfile},0,-4));
		system("rm $safe_outfile_sub.m1v $safe_outfile_sub.mpa -f");
	}

1;	#return true
