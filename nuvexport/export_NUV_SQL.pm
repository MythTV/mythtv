package export_NUV_SQL;

	use File::Copy;
	use DBI;

# Load the nuv utilities
	use nuv_utils;

# Make sure we have pointers to the main:: namespace for certain variables
	*Prog      = *main::Prog;
	*gui       = *main::gui;
	*dbh       = *main::dbh;
	*video_dir = *main::video_dir;

	sub new {
		my $class = shift;
		my $self  = {
					 'name'     => 'Extract .nuv and .sql',
					 'enabled'  => 1,
					 'errors'   => undef,
					 'episode'  => undef,
					 'savepath' => '.',
					 'sql_file' => undef,
					 'copy'     => undef,
					 @_		#allows user-specified attributes to override the defaults
					};
		bless($self, $class);
	# Return
		return $self;
	}

	sub gather_data {
		my $self = shift;
	# Make sure the user knows what he/she is doing
		my $copy = 0;

		if($gui->query_text("\nYou have chosen to extract the .nuv.\n"
			."This will extract it from the MythTV database into .nuv and .sql \n"
			."files to import into another MythTV installation.\n"
			# Make sure the user made the correct choice
			."Do you want to removed it from this server when finished?",
			'yesno',
			'Yes'))
		{
			if($gui->query_text("\nAre you sure you want to remove it from this server?",
				'yesno',
				'Yes'))
			{
				$self->{copy}=0;
			}
			else
			{
				$self->{copy}=1;
			}
		}
		else
		{
			$self->{copy}=1;
		}

	# Get the savepath
		$self->{savepath} = $gui->query_savepath();
	}

	sub execute {
		my $self = shift;
	# Gather any necessary data
		$self->{episode} = shift;
		$self->gather_data;
	# Start saving
		($self->{sql_file} = $self->{episode}->{filename}) =~ s/\.nuv$/.sql/si;
		open(DATA, ">$self->{savepath}/$self->{sql_file}") or die "Can't create $self->{savepath}/$self->{sql_file}:  $!\n\n";
	# Define some query-related variables
		my ($q, $sh);
	# Load and save the related database info
		print DATA "USE mythconverg;\n\n";
		foreach $table ('recorded', 'oldrecorded', 'recordedmarkup') {
			$q = "SELECT * FROM $table WHERE chanid=? AND starttime=?";
			$sh = $dbh->prepare($q);
			$sh->execute($self->{episode}->{channel}, $self->{episode}->{start_time})
				or die "Count not execute ($q):  $!\n\n";
			my $count = 0;
			my @keys = undef;
			while (my $row = $sh->fetchrow_hashref) {
			# First row - let's add the insert statement;
				if ($count++ == 0) {
					@keys = keys(%$row);
					print DATA "INSERT INTO $table (", join(', ', @keys), ") VALUES\n\t(";
				}
				else {
					print DATA ",\n\t(";
				}
			# Print the data
				my $count2 = 0;
				foreach $key (@keys) {
					print DATA ', ' if ($count2++);
					print DATA mysql_escape($row->{$key});
				}
				print DATA ')';
			}
			print DATA ";\n\n";
		}
	# Done savig the database info
		close DATA;
	# Rename/move the file
		if ($self->{copy} == 1) {
			$gui->notify("\nCopying $video_dir/$self->{episode}->{filename} to $self->{savepath}/$self->{episode}->{filename}\n");
			copy("$video_dir/$self->{episode}->{filename}", "$self->{savepath}/$self->{episode}->{filename}")
				or die "Couldn't copy specified .nuv file:  $!\n\n";
		}
		else {
			$gui->notify("\nMoving $video_dir/$self->{episode}->{filename} to $self->{savepath}/$self->{episode}->{filename}\n");
			move("$video_dir/$self->{episode}->{filename}", "$self->{savepath}/$self->{episode}->{filename}")
				or die "Couldn't move specified .nuv file:  $!\n\n";
			# Remove the entry from recordedmarkup
			$q = 'DELETE FROM recordedmarkup WHERE chanid=? AND starttime=?';
			$sh = $dbh->prepare($q);
			$sh->execute($self->{episode}->{channel}, $self->{episode}->{start_time})
				or die "Could not execute ($q):  $!\n\n";
			# Remove this entry from the database
			$q = 'DELETE FROM recorded WHERE chanid=? AND starttime=? AND endtime=?';
			$sh = $dbh->prepare($q);
			$sh->execute($self->{episode}->{channel}, $self->{episode}->{start_time}, $self->{episode}->{end_time})
				or die "Could not execute ($q):  $!\n\n";
			# Tell the other nodes that changes have been made
			$q = 'UPDATE settings SET data="yes" WHERE value="RecordChanged"';
			$sh = $dbh->prepare($q);
			$sh->execute()
				or die "Could not execute ($q):  $!\n\n";
		}
	}

	sub cleanup {
		# Nothing to do here
	}

1;	#return true
