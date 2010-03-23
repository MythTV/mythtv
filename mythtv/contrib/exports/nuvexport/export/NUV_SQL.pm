#
# $Date$
# $Revision$
# $Author$
#
#  export::NUV_SQL
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::NUV_SQL;
    use base 'export::generic';

    use File::Copy;
    use File::Basename;
    use DBI;
    use Term::ANSIColor;
    use MythTV;

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bnuv[\-_]?sql\b/i,
                     'name'            => 'Export to .nuv and .sql',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Delete original files?
                     'delete'          => 0,
                     'create_dir'      => 1,
                    };
        bless($self, $class);

    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

# Load default settings
    sub load_defaults {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();
    # Not really anything to add
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Let the user know what's going on
        print "\nYou have chosen to extract the .nuv.\n"
             ."This will extract it from the MythTV database into .nuv and .sql \n"
             ."files to import into another MythTV installation.\n\n";
    # Make sure the user knows what he/she is doing
        $self->{'delete'} = query_text("Do you want to remove it from this server when finished?",
                                       'yesno',
                                       $self->{'delete'} ? 'Yes' : 'No');
    # Make EXTRA sure
        if ($self->{'delete'}) {
            $self->{'delete'} = query_text("\nAre you ".colored('sure', 'bold').' you want to remove it from this server?',
                                           'yesno',
                                           'No');
        }
    # Create a directory with the show name
        $self->{'create_dir'} = query_text('Store exported files in a directory with the show name?',
                                           'yesno',
                                           $self->{'create_dir'} ? 'Yes' : 'No');
    # Load the save path, if requested
        $self->{'path'} = query_savepath($self->val('path'));
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Create a show-name directory?
        if ($self->{'create_dir'}) {
            $self->{'export_path'} = $self->get_outfile($episode, '');
            mkdir($self->{'export_path'}, 0755) or die "Can't create $self->{'export_path'}:  $!\n\n";
        }
        else {
            $self->{'export_path'} = $self->{'path'};
        }
    # Load the three files we'll be using
        my $txt_file = basename($episode->{'local_path'}, '.nuv') . '.txt';
        my $sql_file = basename($episode->{'local_path'}, '.nuv') . '.sql';
        my $nuv_file = basename($episode->{'local_path'});
    # Create a txt file with descriptive info in it
        open(DATA, ">$self->{'export_path'}/$txt_file") or die "Can't create $self->{'export_path'}/$txt_file:  $!\n\n";
        print DATA '       Show:  ', $episode->{'title'},                                            "\n",
                   '    Episode:  ', $episode->{'subtitle'},                                         "\n",
                   '   Recorded:  ', $episode->{'showtime'},                                         "\n",
                   'Description:  ', wrap($episode->{'description'}, 64,'', '', "\n              "), "\n",
                   "\n",
                   ' Orig. File:  ', $episode->{'local_path'},                                       "\n",
                   '       Type:  ', $episode->{'finfo'}{'video_type'},                              "\n",
                   ' Dimensions:  ', $episode->{'finfo'}{'width'}.'x'.$episode->{'finfo'}{'height'}, "\n",
                   ;
        close DATA;
    # Start saving the SQL
        open(DATA, ">$self->{'export_path'}/$sql_file") or die "Can't create $self->{'export_path'}/$sql_file:  $!\n\n";
    # Define some query-related variables
        my ($q, $sh);
    # Load and save the related database info
        print DATA "USE mythconverg;\n\n";
        foreach $table ('recorded', 'oldrecorded', 'recordedmarkup', 'recordedseek') {
            $q = "SELECT * FROM $table WHERE chanid=? AND starttime=FROM_UNIXTIME(?)";
            $sh = $Main::Myth->{'dbh'}->prepare($q);
            $sh->execute($episode->{'chanid'}, $episode->{'recstartts'})
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
    # Done saving the database info
        close DATA;
    # Copy/link the file into place
        print "\nCopying $episode->{'local_path'} to $self->{'export_path'}/$nuv_file\n";
    # Use hard links when copying within a filesystem
        if ((stat($episode->{'local_path'}))[0] == (stat($self->{'path'}))[0]) {
            link($episode->{'local_path'}, "$self->{'export_path'}/$nuv_file")
                or die "Couldn't hard link specified .nuv file:  $!\n\n";
        }
        else {
            copy($episode->{'local_path'}, "$self->{'export_path'}/$nuv_file")
                or die "Couldn't copy specified .nuv file:  $!\n\n";
        }
    # Delete the record?
        if ($self->{'delete'}) {
            print "Deleting from MythTV\n";
            $Main::Myth->backend_command(['FORCE_DELETE_RECORDING', $episode->to_string()], '0');
        }
    }

    sub cleanup {
        # Nothing to do here
    }

# Escape a string for MySQL
    sub mysql_escape {
        $string = shift;
        return 'NULL' unless (defined $string);
        $string =~ s/'/\\'/sg;
        return "'$string'";
    }


1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
