package gui_cli;

	use File::Path;

# Make sure we have pointers to the main:: namespace for certain variables
	*Args      = *main::Args;
	*Shows     = *main::Shows;
	*Functions = *main::Functions;
	*num_shows = *main::num_shows;

	sub new {
		my $class = shift;
		my $self  = {
					 @_		#allows user-specified attributes to override the defaults
					};
		return bless($self, $class);
	}

	sub main_loop {
		my $self = shift;
	# Make sure all of the required parameters were requested
		die "Please specify the show starttime as: --starttime=YYYYMMDDhhmmss\n\n" if (!$Args{starttime} || $Args{starttime} =~ /\D/ || length($Args{starttime}) != 14);
		die "Please specify the chanid with --chanid\n\n" unless ($Args{chanid} > 0);
	# Figure out which show the user requested, or die with an error
		my $episode_choice = undef;
		foreach my $show (sort keys %Shows) {
			foreach my $episode (@{$Shows{$show}}) {
				next unless ($episode->{channel} == $Args{chanid}
								&& $episode->{start_time} == $Args{starttime});
				$episode_choice = $episode;
				last;
			}
			last if ($episode_choice);
		}
		die "No matching recordings were found.\n\n" unless ($episode_choice);
	# Figure out which export function the user requested
		my $function_choice = undef;
		if ($Args{mode}) {
			foreach my $function (@Functions) {
				next unless ($function->{cli_mode} and $Args{mode} =~ /$function->{cli_mode}/i);
				$function_choice = $function;
				last;
			}
		}
		unless ($function_choice) {
			my $str = ($Args{mode} ? 'No matching export function was found' : 'No export function was specified')
						.".\nPlease see the following regular expressions:\n\n";
			foreach my $function (@Functions) {
				$str .= ' ' x (26 - length $function->{name});
				$str .= "$function->{name}:  ";
				$str .= $function->{cli_mode} ? $function->{cli_mode} : '[No command line interface available]';
				$str .= "\n";
			}
			die "$str\n";
		}
	# Make sure that this function is enabled
		if (!$function_choice->{enabled}) {
			if ($function_choice->{errors} && @{$function_choice->{errors}}) {
				die join("\n", @{$function_choice->{errors}})."\n";
			}
			else {
				die 'Function "'.$function_choice->{name}."\" is disabled.\n";
			}
		}
	# Keep track of when the encode started
		my $start_time = time();
	# Encode
		$function_choice->execute($episode_choice);
	# Report the duration
		my $seconds = time() - $start_time;
		my $timestr = '';
	# How many hours?
		my $hours = int($seconds / 3600);
		$timestr .= $hours.'h ' if ($hours > 0);
		$seconds  = $seconds % 3600;
	# Minutes
		my $minutes = int($seconds / 60);
		$timestr .= $minutes.'m ' if ($minutes > 0);
		$seconds  = $seconds % 60;
	# Generate a nice
		$timestr .= $seconds.'s' if ($seconds > 0);
	# Notify the user
		print "Encode lasted: $timestr";
	# Quit gracefully
		main::Quit();
	}

	sub query_filename {
		my $self = shift;
		my $default  = shift;
		my $suffix   = shift;
		my $savepath = (shift or '.');
		my $outfile  = undef;
	# User-specified path?
		if ($Args{outfile} && $Args{outfile} =~ /\w/) {
			($savepath, $outfile) = $Args{outfile} =~ /^(?:(.*?)\/)?([^\/]+)$/;
			$savepath ||= '.';
		}
	# Otherwise, use the default
		else {
			$outfile = "$default.$suffix";
		}
	# Make sure we get a suffix
		$outfile =~ s/(?:\.$suffix)?$/.$suffix/si;
	# Save path doesn't exist
		die "Directory $savepath doesn't exist.  Please create it first.\n\n" unless (-e $savepath);
	# Make sure the save path is a valid directory
		die "$savepath exists, but is not a directory.\n\n" if (-e $savepath && !-d $savepath);
	# Make sure the file doesn't already exist
		die "$savepath/$outfile already exists; please delete it or choose another name.\n\n" if (-e "$savepath/$outfile");
	# Return
		return ($savepath, $outfile);
	}

	sub query_savepath {
	# Return the nothing - query_filename() will take care of everything
		return;
	}

1;	#return true
