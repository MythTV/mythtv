package export_Trans_XviD;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'        => 'Transcode export to XviD',
					 'enabled'     => 1,
					 'errors'      => undef,
					 'episode'     => undef,
					 'savepath'    => '.',
					 'outfile'     => 'out.avi',
					 'use_cutlist' => 1,
					 'a_bitrate'   => 64,
					 'v_bitrate'   => 512,
					 'h_res'       => 320,
					 'v_res'       => 240,
					 'sql_file' => undef,
					 'deinterlace' => 1,
					 'noise_reduction'     => 0,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
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

		$self->{outfile} = $gui->query_filename($default_filename, 'avi', $self->{savepath});
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
	# Noise reduction
		$self->{noise_reduction} = $gui->query_text('Enable noise reduction (slower, but better results)?',
														'yesno',
														$self->{noise_reduction} ? 'Yes' : 'No');
		$self->{deinterlace} = $gui->query_text('Enable deinterlacing?',
														'yesno',
														$self->{deinterlace} ? 'Yes' : 'No');

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

		$safe_outfile = shell_escape($self->{outfile});
		my $command = "nice -n 19 transcode -x mpeg2 -i $main::video_dir/$self->{episode}->{filename} -y xvid -Z $self->{h_res}x$self->{v_res} -o $safe_outfile -w $self->{v_bitrate} -b $self->{a_bitrate}";

		if ($self->{use_cutlist}) {
		  @cuts = split("\n",$self->{episode}->{cutlist});
		  my @skiplist;
		  foreach $cut (@cuts) {
		     push @skiplist, (split(" - ", $cut))[0]."-".(split(" - ", $cut))[1];
	          }
		  $command .= " -J skip=\"".join(" ", @skiplist)."\"";
	        }

		if ($self->{deinterlace}) {
                  $command .= " -V -J smartdeinter";
	        }
		if ($self->{noise_reduction}) {
                  $command .= " -V -J yuvdenoise";
	        }

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
	}

1;	#return true
