package gui_text;

	use File::Path;

# Make sure we have pointers to the main:: namespace for certain variables
	*Shows     = *main::Shows;
	*Functions = *main::Functions;
	*num_shows = *main::num_shows;
	*DEBUG = *main::DEBUG;

	sub new {
		my $class = shift;
		my $self  = {
					 'query_stage'    => 'show',
					 'show_choice'    => '',
					 'episode_choice' => undef,
					 'start_time'     => 0,
					 'end_time'       => 0,
					 @_		#allows user-specified attributes to override the defaults
					};
		return bless($self, $class);
	}

	sub main_loop {
		my $self = shift;
	# Display the show list
		while (1) {
		# Clear the screen
			system('clear') unless ($DEBUG);
		# Stage "quit" means, well, quit...
			if ($self->stage eq 'quit') {
			# Report the duration
				if ($self->{start_time} && $self->{end_time}) {
					my $seconds = $self->{end_time} - $self->{start_time};
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
					$self->notify("Encode lasted: $timestr");
				}
			# Leave the loop so we can quit
				last;
			}
		# Are we asking the user which show to encode?
			if (!$self->{show_choice} || $self->stage eq 'show') {
				$self->query_shows;
			}
		# Nope.  What about the episode choice?
			elsif (!$self->{episode_choice} || $self->stage eq 'episode') {
				$self->query_episodes;
			}
		# Time to decide what we want to do?
			elsif ($self->stage eq 'function') {
				$self->query_functions;
			}
		}
	}

	sub query_shows {
		my $self = shift;
	# Build the query
		my $query = "\nYou have recorded the following shows:\n\n";
		my ($count, @show_choices);
		foreach $show (sort keys %Shows) {
			$count++;
		# Print out this choice, adjusting space where necessary
			$query .= '  ';
			$query .= ' ' if ($num_shows > 10 && $count < 10);
			$query .= ' ' if ($num_shows > 100 && $count < 100);
			$query .= "$count. ";
		# print out the name of this show, and an episode count
			my $num_episodes = @{$Shows{$show}};
			$query .= "$show ($num_episodes episode".($num_episodes == 1 ? '' : 's').")\n";
			$show_choices[$count-1] = $show;
		}
		$query .= "\n  q. Quit\n\nChoose a show: ";
	# Query the user
		my $choice = $self->query_text($query, 'string', '');
	# Quit?
		return $self->stage('quit') if ($choice =~ /^\W*q/i);
	# Move on to the next stage if the user chose a valid show
		$choice =~ s/^\D*/0/s;	# suppress warnings
		if ($choice > 0 && $show_choices[$choice-1]) {
			$self->{show_choice} = $show_choices[$choice-1];
			$self->stage('episode');
		}
	}

	sub query_episodes {
		my $self = shift;
		my $num_episodes = @{$Shows{$self->{show_choice}}};
	# Define a newline + whitespace so we can tab out extra lines of episode description
		my $newline = "\n" . ' ' x (4 + length $num_episodes);
	# Build the query
		my $query = "\nYou have recorded the following episodes of $self->{show_choice}:\n\n";
		my ($count, @episode_choices);
		foreach $episode (@{$Shows{$self->{show_choice}}}) {
			$count++;
		# Print out this choice, adjusting space where necessary
			$query .= '  ';
			$query .= ' ' if ($num_episodes > 10 && $count < 10);
			$query .= ' ' if ($num_episodes > 100 && $count < 100);
			$query .= "$count. ";
		# print out the name of this show, and an episode count
			$query .= join($newline, "$episode->{title} ($episode->{showtime})",
									 $episode->{description})."\n";
			$episode_choices[$count-1] = $episode;
		}
		$query .= "\n  r. Return to shows menu\n  q. Quit\n\nChoose an episode: ";
	# Query the user
		my $choice = $self->query_text($query, 'string', '');
	# Quit?
		return $self->stage('quit') if ($choice =~ /^\W*q/i);
	# Backing up a stage?
		return $self->stage('show') if ($choice =~ /^\W*[rb]/i);
	# Move on to the next stage if the user chose a valid episode
		$choice =~ s/^\D*/0/s;	# suppress warnings
		if ($choice > 0 && $episode_choices[$choice-1]) {
			$self->{episode_choice} = $episode_choices[$choice-1];
			$self->stage('function');
		}
	}

	sub query_functions {
		my $self = shift;
	# Build the query
		my $query = "What would you like to do with your recording?\n\n"
				   ."         Show:  $self->{show_choice}\n"
				   ."      Episode:  $self->{episode_choice}->{title}\n"
				   ."      Airtime:  $self->{episode_choice}->{showtime}\n\n";
	# What are our function options?
		my ($count);
		foreach my $function (@Functions) {
			$count++;
			$query .= (' ' x (3 - length($count)))."$count. ".$function->{name};
			$query .= ' (disabled)' unless ($function->{enabled});
			$query .= "\n";
		}
		$query .= "\n  r. Return to episode menu\n  q. Quit\n\nChoose a function: ";
	# Query the user
		my $choice = $self->query_text($query, 'string', '');
	# Quit?
		return $self->stage('quit') if ($choice =~ /^\W*q/i);
	# Backing up a stage?
		return $self->stage('episode') if ($choice =~ /^\W*[rb]/i);
	# Execute the chosen function, and then quit
		$choice =~ s/^\D*/0/s;	# suppress warnings
	# No choice given?
		if ($choice < 1) {
			next;
		}
	# Make sure that this function is enabled
		elsif ($choice < 1 || !$Functions[$choice-1]->{enabled}) {
			if ($Functions[$choice-1]->{errors} && @{$Functions[$choice-1]->{errors}}) {
				$self->notify("\n".join("\n", @{$Functions[$choice-1]->{errors}})."\n");
			}
			else {
				$self->notify('Function "'.$Functions[$choice-1]->{name}."\" is disabled.\n");
			}
			$self->notify("Press ENTER to continue.\n");
			<STDIN>;
		}
		elsif ($Functions[$choice-1]->{enabled}) {
			$self->{start_time} = time();
			$Functions[$choice-1]->execute($self->{episode_choice});
			$self->{end_time}   = time();
			$self->stage('quit');
		}
	}

	sub query_filename {
		my $self = shift;
		my $default  = shift;
		my $suffix   = shift;
		my $savepath = shift;
		my $outfile  = undef;
		until ($outfile) {
			$outfile = $self->query_text('Output filename? ', 'string', "$default.$suffix");
			$outfile =~ s/(?:\.$suffix)?$/.$suffix/si;
			if (-e "$savepath/$outfile") {
				if (-f "$savepath/$outfile") {
					unless ($self->query_text("$savepath/$outfile exists.  Overwrite?", 'yesno', 'No')) {
						$outfile = undef;
					}
				}
				else {
					$self->notify("$savepath/$outfile exists and is not a regular file; please choose another.");
					$outfile = undef;
				}
			}
		}
		return $outfile;
	}

	sub query_savepath {
		$self = shift;
	# Where are we saving the files to?
		my $savepath = undef;
		until ($savepath) {
			$savepath = $self->query_text('Where would you like to export the files to?', 'string', '.');;
			$savepath =~ s/\/+$//s;
			unless ($savepath eq '.') {
			# Make sure this is a valid directory
				if (-e $savepath && !-d $savepath) {
					$savepath = undef;
					$self->notify("$savepath exists, but is not a directory.");
				}
			# Doesn't exist - query the user to create it
				elsif (!-e $savepath) {
					my $create = $self->query_text("$savepath doesn't exist.  Create it?", 'yesno', 'Yes');
					if ($create) {
			           mkpath($savepath, 1, 0711) or die "Couldn't create $savepath:  $!\n\n";
					}
					else {
						$savepath = undef;
					}
				}
			}
		}
		return $savepath;
	}

	sub query_text {
		my $self          = shift;
		my $text          = shift;
		my $expect        = shift;
		my $default       = shift;
		my $default_extra = shift;
		my $return = undef;
	# Loop until we get a valid response
		while (1) {
		# Ask the question, get the answer
			print $text,
				  ($default ? " [$default]".($default_extra ? $default_extra : '').' '
				  			: ' ');
			chomp($return = <STDIN>);
		# Nothing typed, is there a default value?
			unless ($return =~ /\w/) {
				next unless (defined $default);
				$return = $default;
			}
		# Looking for a boolean/yesno response?
			if ($expect =~ /yes.*no|bool/i) {
				return $return =~ /^\W*[nf0]/i ? 0 : 1;
			}
		# Looking for an integer?
			elsif ($expect =~ /int/i) {
				$return =~ s/^\D*/0/;
				if ($return != int($return)) {
					print "Whole numbers only, please.\n";
					next;
				}
				return $return;
			}
		# Looking for a float?
			elsif ($expect =~ /float/i) {
				$return =~ s/^\D*/0/;
				return $return + 0;
			}
		# Well, then we must be looking for a string
			else {
				return $return;
			}
		}
	}

	sub notify {
		my $self = shift;
		print shift, "\n";
	}

	sub stage {
		my $self  = shift;
		my $stage = shift;
		$self->{query_stage} = $stage if (defined $stage);
		$self->{query_stage} = 'show' unless ($self->{query_stage});
		return $self->{query_stage};
	}

1;	#return true
