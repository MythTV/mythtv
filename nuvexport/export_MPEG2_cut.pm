package export_MPEG2_cut;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
				 'name'            => 'MPEG2->MPEG2 cut only',
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
	# Make sure we have tcprobe
		$Prog{tcprobe} = find_program('tcprobe');
		push @{$self->{errors}}, 'You need tcprobe to use this.' unless ($Prog{tcprobe});
	# Make sure that we have lvemux
		$Prog{mplexer} = find_program('lvemux');
		push @{$self->{errors}}, 'You need lvemux to use this.' unless ($Prog{mplexer});
	# Make sure we have avidemux2
		$Prog{avidemux} = find_program('avidemux2');
		push @{$self->{errors}}, 'You need avidemux2 to use this.' unless ($Prog{avidemux});
	# Make sure we have mpeg2cut
		$Prog{mpeg2cut} = find_program('mpeg2cut');
		push @{$self->{errors}}, 'You need mpeg2cut to use this.' unless ($Prog{mpeg2cut});
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

		if (!($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/)) {
			$gui->notify('No cutlist found.  This won\'t do!');
			return 0;
		}

		my $command = "mpeg2cut $main::video_dir/$self->{episode}->{filename} $safe_outfile ";

		@cuts = split("\n",$self->{episode}->{cutlist});
		my @skiplist;
		foreach my $cut (@cuts) {
		   push @skiplist, (split(" - ", $cut))[0];
		   push @skiplist, (split(" - ", $cut))[1];
	        }

		my $cutnum = 0;
		if ($skiplist[0] ne 0) {
		   $command .= "-";
		   $cutnum = 1;
	        }

		foreach my $cut (@skiplist) {
		   if ($cutnum eq 0) {
		      if( $cut ne 0 ) {
		         $cutnum = 1;
		         $cut++;
		         $command .= "$cut-";
		      }
		   } else {
		      $cutnum = 0;
		      $cut--;
		      $command .= "$cut ";
		   }
		}

		system($command);
	}

	sub cleanup {
		my $self = shift;
		return unless ($self->{started});
	}

1;	#return true
