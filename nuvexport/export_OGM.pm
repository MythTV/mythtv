package export_OGM;

# Define any command line parameters used by this module
	*Arg_str  = *main::Arg_str;
	push @Arg_str,
			'v_bitr:i',
			'o_fps:i',
			'a_delay:i',
                        'h_resol:i',
			'v_resol:i'; 

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Args = *main::Args;
	*Prog = *main::Prog;
	*gui  = *main::gui;

	sub new {
		my $class = shift;
		my $self  = {
					# The name of this export function - displayed to users in interactive mode
					 'name'        => 'Export OGM (CLI support)',
					# The --mode match (regex) for non-interactive command line interface
					 'cli_mode'    => 'ogm',
					# Set to 0 to disable this export function (eg. if there are errors)
					 'enabled'     => 1,
					# An array of errors - generally displayed to users when they try to use a disabled function					 
					 'errors'      => undef,
					# Misc variables all export functions should keep track of
					 'started'         => 0,
					 'children'        => [],
					 'episode'     => undef,
					 'is_cli'      => 0,
					# Variables specific to export_DivX
					 'savepath'    => '.',
					 'outfile'     => 'out.ogm',
                     'fifodir'     => "fifodir.$$",
					 'use_cutlist' => 0,
					 'v_bitrate'   => 150,
					 'output_fps'   => 15,
                     'tmp_a'           => 'audio.ogg',
					 'tmp_v'           => 'out.avi',
					 'audio_delay'       => 200,
                     'h_res'       => 320,
					 'v_res'       => 240,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
        # Make sure that we have the other necessary programs
		find_program('oggenc')
			or push @{$self->{errors}}, 'You need oggenc.';
                find_program('ogmmerge')
			or push @{$self->{errors}}, 'You need ogmmerge.';
                find_program('mencoder')
			or push @{$self->{errors}}, 'You need mencoder.'; 
        # Any errors?  disable this function
		$self->{enabled} = 0 if ($self->{errors} && @{$self->{errors}} > 0);
	# Return
		return $self;
	}

	sub gather_cli_data {
		my $self = shift;
		return undef unless ($Args{mode} && $Args{mode} =~ /$self->{cli_mode}/i);
	# Get the save path
		($self->{savepath}, $self->{outfile}) = $gui->query_filename($self->{default_outfile}, 'ogm', $self->{savepath});
	
	# Video Bitrate
		$self->{v_bitrate} = $Args{v_bitr} if ($Args{v_bitr});
		
        # Output fps
		$self->{output_fps} = $Args{o_fps} if ($Args{o_fps});
		  
	# Audio delay
		$self->{audio_delay}    = $Args{a_delay} if ($Args{a_delay});

        # Horizontal resolution
		$self->{h_res}    = $Args{h_resol} if ($Args{h_resol});

        # Vertical resolution
		$self->{v_res}    = $Args{v_resol} if ($Args{v_resol});
	
	# Cutlist and noise reduction don't require checking
		$self->{use_cutlist}     = ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/ && $Args{use_cutlist}) ? 1 : 0;
	
	# Return true, so we skip the interactive data gathering
		return 1;
	}


	sub gather_data {
		my $self    = shift;
		my $default_filename;
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
	# grabbing data from the command line?
		return if ($self->gather_cli_data());
	# Get the save path
		$self->{savepath} = $gui->query_savepath();
		$self->{outfile}  = $gui->query_filename($default_filename, 'ogm', $self->{savepath});
	# Ask the user if he/she wants to use the cutlist
		if ($self->{episode}->{cutlist} && $self->{episode}->{cutlist} =~ /\d/) {
			$self->{use_cutlist} = $gui->query_text('Enable Myth cutlist?',
													'yesno',
													$self->{use_cutlist} ? 'Yes' : 'No');
		}
		else {
			$gui->notify('No cutlist found.  Hopefully this means that you already removed the commercials.');
		}


	# Ask the user what video bitrate he/she wants
		my $v_bitrate = $gui->query_text('Video bitrate?',
										 'float',
										 $self->{v_bitrate});
		$self->{v_bitrate} = $v_bitrate;
        # Ask the user what audio delay he/she wants
		my $audio_delay = $gui->query_text('Audio Delay in ms ?',
										 'float',
										 $self->{audio_delay});
		$self->{audio_delay} = $audio_delay;
	# Ask the user what output fps he/she wants
		my $output_fps = $gui->query_text('Output fps?',
										 'float',
										 $self->{output_fps});
		$self->{output_fps} = $output_fps;		

	
        # Ask the user what horizontal resolution he/she wants
		my $h_res = $gui->query_text('Horizontal resolution?',
										 'float',
										 $self->{h_res});				
                $self->{h_res} = $h_res;	

        # Ask the user what vertical resolution he/she wants
		my $v_res = $gui->query_text('Vertical resolution?',
										 'float',
										 $self->{v_res});				
                $self->{v_res} = $v_res;			 
	
        }
	sub execute {
		my $self = shift;
	# make sure that the fifo dir is clean
		if (-e "$self->{fifodir}/vidout" || -e "$self->{fifodir}/audout") {
			die "Possibly stale mythtranscode fifo's in $self->{fifodir}.\nPlease remove them before running nuvexport.\n\n";
		}

        # delete old multipass log files
                 system("rm -f divx2pass.log 2>/dev/null");
	       


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
		($self->{tmp_a} = $self->{episode}->{filename}) =~ s/\.nuv$/.ogg/;
		($self->{tmp_v} = $self->{episode}->{filename}) =~ s/\.nuv$/.avi/;
	# Here, we have to fork off a copy of mythtranscode
		my $command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
		$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);
	# Sleep a bit to let mythtranscode start up
		my $overload = 0;
		while (++$overload < 30 && !(-e "$self->{fifodir}/audout" && -e "$self->{fifodir}/vidout")) {
			sleep 1;
			print "Waiting for mythtranscode to set up the fifos.\n";
		}
		unless (-e "$self->{fifodir}/audout" && -e "$self->{fifodir}/vidout") {
			die "Waited too long for mythtranscode to create its fifos.  Please try again.\n\n";
		}
	
              
                
		
# Null command for audio
		$command = "nice -19 cat < $self->{fifodir}/audout >/dev/null";


		push @{$self->{children}}, fork_command($command);


	# And lastly, we fork off a process to encode the video

                $command="nice -19 mencoder -rawvideo on:w=$nuv_info{width}:h=$nuv_info{height}:fps=$nuv_info{fps} -ovc lavc -lavcopts vcodec=mpeg4:trell:mbd=2:vqmin=2:vqmax=31:v4mv:vmax_b_frames=1:cmp=2:subcmp=2:precmp=2:predia=1:dia=1:vme=4:vqcomp=0.6:vlelim=9:vcelim=-4:idct=7:lumi_mask=0.05:dark_mask=0.01:vstrict=-1:cbp:vfdct=1:vbitrate=$self->{v_bitrate}:keyint=150:vpass=1 -vop pp=hb/vb/dr,eq=15,scale=$self->{h_res}:$self->{v_res},hqdn3d=8:6:12:lavcdeint -ofps $self->{output_fps} -zoom -sws 2 -o /dev/null  $self->{fifodir}/vidout";

                

                push @{$self->{children}}, fork_command($command);


	# Wait for child processes to finish

		1 while (wait > 0);
		$self->{children} = undef;


        # Remove any temporary files but keep the divxpass log file
		foreach my $file ("$self->{fifodir}/audout", "$self->{fifodir}/vidout", $self->{tmp_a}, $self->{tmp_v}) {
			unlink $file if (-e $file);
		}
		rmdir $self->{fifodir} if (-e $self->{fifodir});
	



	# Create a directory for mythtranscode's fifo's
		unless (-d $self->{fifodir}) {
			mkdir($self->{fifodir}, 0755) or die "Can't create $self->{fifodir}:  $!\n\n";
		}



	# Here, we have to fork off another mythtranscode
		$command = "nice -n 19 mythtranscode -p autodetect -c $self->{episode}->{channel} -s $self->{episode}->{start_time_sep} -f $self->{fifodir} --fifosync";
		$command .= ' --honorcutlist' if ($self->{use_cutlist});
		push @{$self->{children}}, fork_command($command);

	# Sleep a bit to let mythtranscode start up
		$overload = 0;
		while (++$overload < 30 && !(-e "$self->{fifodir}/audout" && -e "$self->{fifodir}/vidout")) {
			sleep 1;
			print "Waiting for mythtranscode to set up the fifos.\n";
		}
		unless (-e "$self->{fifodir}/audout" && -e "$self->{fifodir}/vidout") {
			die "Waited too long for mythtranscode to create its fifos.  Please try again.\n\n";
		}

	# Now we fork off a process to encode the audio

                       

		
		$command = "nice -n 19 oggenc -r -C 2 -R $nuv_info{audio_sample_rate} -B 16 -q 2 --resample 22050 -o $self->{tmp_a}  $self->{fifodir}/audout";

		push @{$self->{children}}, fork_command($command);


	# And lastly, we fork off a process to encode the video, keep the video file this time
	

                $command="nice -19 mencoder -rawvideo on:w=$nuv_info{width}:h=$nuv_info{height}:fps=$nuv_info{fps} -ovc lavc -lavcopts vcodec=mpeg4:trell:mbd=2:vqmin=2:vqmax=31:v4mv:vmax_b_frames=1:vb_strategy=0:cmp=2:subcmp=2:precmp=2:predia=1:dia=1:vme=4:vqcomp=0.6:vlelim=9:vcelim=-4:idct=7:lumi_mask=0.05:dark_mask=0.01:vstrict=-1:cbp:vfdct=1:vbitrate=$self->{v_bitrate}:keyint=150:vpass=2 -vop pp=hb/vb/dr,eq=15,scale=$self->{h_res}:$self->{v_res},hqdn3d=8:6:12:lavcdeint -ofps $self->{output_fps} -zoom -sws 2 -o $self->{tmp_v}  $self->{fifodir}/vidout";
                
                push @{$self->{children}}, fork_command($command);



	# Wait for child processes to finish
		1 while (wait > 0);
		$self->{children} = undef;



	# Multiplex the streams
		$safe_outfile = shell_escape($self->{savepath}.'/'.$self->{outfile});
		
		system("nice -n 19 ogmmerge -o  $safe_outfile $self->{tmp_v} --sync $self->{audio_delay}  $self->{tmp_a}");
		
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
		foreach my $file ("$self->{fifodir}/audout", "$self->{fifodir}/vidout",  $self->{tmp_v}, $self->{tmp_a}) {
			unlink $file if (-e $file);
		}
		rmdir $self->{fifodir} if (-e $self->{fifodir});

        # delete old multipass log files
                 system("rm -f divx2pass.log 2>/dev/null");
	}


	


       


   
	
1;	#return true

