#!/usr/bin/perl -w
#Last Updated: 2005.01.17 (xris)
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

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::db;
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
        $self->{'delete'} = query_text("Do you want to removed it from this server when finished?",
                                       'yesno',
                                       $self->{'delete'} ? 'Yes' : 'No');
    # Make EXTRA sure
        if ($self->{'delete'}) {
            $self->{'delete'} = query_text("\nAre you ".colored('sure', 'bold').' you want to remove it from this server?',
                                           'yesno',
                                           'No');
        }
    # Load the save path, if requested
        $self->{'path'} = query_savepath($self->val('path'));
    # Create a
        $self->{'create_dir'} = query_text('Create a directory with the show name?',
                                           'yesno',
                                           $self->{'create_dir'} ? 'Yes' : 'No');
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Make sure we have finfo
        load_finfo($episode);
    # Create a show-name directory?
        if ($self->{'create_dir'}) {
            $self->{'path'} = $self->get_outfile($episode, '');
            mkdir($self->{'path'}, 0755) or die "Can't create $self->{'path'}:  $!\n\n";
        }
    # Load the three files we'll be using
        my $txt_file = basename($episode->{'filename'}, '.nuv') . '.txt';
        my $sql_file = basename($episode->{'filename'}, '.nuv') . '.sql';
        my $nuv_file = basename($episode->{'filename'});
    # Create a txt file with descriptive info in it
        open(DATA, ">$self->{'path'}/$txt_file") or die "Can't create $self->{'path'}/$txt_file:  $!\n\n";
        print DATA '       Show:  ', $episode->{'show_name'},                                        "\n",
                   '    Episode:  ', $episode->{'title'},                                            "\n",
                   '   Recorded:  ', $episode->{'showtime'},                                         "\n",
                   'Description:  ', wrap($episode->{'description'}, 64,'', '', "\n              "), "\n",
                   "\n",
                   ' Orig. File:  ', $episode->{'filename'},                                         "\n",
                   '       Type:  ', $episode->{'finfo'}{'video_type'},                              "\n",
                   ' Dimensions:  ', $episode->{'finfo'}{'width'}.'x'.$episode->{'finfo'}{'height'}, "\n",
                   ;
        close DATA;
    # Start saving the SQL
        open(DATA, ">$self->{'path'}/$sql_file") or die "Can't create $self->{'path'}/$sql_file:  $!\n\n";
    # Define some query-related variables
        my ($q, $sh);
    # Load and save the related database info
        print DATA "USE mythconverg;\n\n";
        foreach $table ('recorded', 'oldrecorded', 'recordedmarkup') {
            $q = "SELECT * FROM $table WHERE chanid=? AND starttime=?";
            $sh = $dbh->prepare($q);
            $sh->execute($episode->{'channel'}, $episode->{'start_time'})
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
    # Rename/move the file
        if ($self->{'delete'}) {
            print "\nMoving $episode->{'filename'} to $self->{'path'}/$nuv_file\n";
            move("$episode->{'filename'}", "$self->{'path'}/$nuv_file")
                or die "Couldn't move specified .nuv file:  $!\n\n";
        # Remove the entry from recordedmarkup
            $q = 'DELETE FROM recordedmarkup WHERE chanid=? AND starttime=?';
            $sh = $dbh->prepare($q);
            $sh->execute($episode->{'channel'}, $episode->{'start_time'})
                or die "Could not execute ($q):  $!\n\n";
        # Remove this entry from the database
            $q = 'DELETE FROM recorded WHERE chanid=? AND starttime=? AND endtime=?';
            $sh = $dbh->prepare($q);
            $sh->execute($episode->{'channel'}, $episode->{'start_time'}, $episode->{'end_time'})
                or die "Could not execute ($q):  $!\n\n";
        # Tell the other nodes that changes have been made
            $q = 'UPDATE settings SET data="yes" WHERE value="RecordChanged"';
            $sh = $dbh->prepare($q);
            $sh->execute()
                or die "Could not execute ($q):  $!\n\n";
        }
    # Copy the file
        else {
            print "\nCopying $episode->{'filename'} to $self->{'path'}/$nuv_file\n";
        # use hard links when copying within a filesystem
            if ((stat($episode->{'filename'}))[0] == (stat($self->{path}))[0]) {
                link("$episode->{'filename'}", "$self->{'path'}/$nuv_file")
                    or die "Couldn't hard link specified .nuv file:  $!\n\n";
            }
            else {
                copy("$episode->{'filename'}", "$self->{'path'}/$nuv_file")
                    or die "Couldn't copy specified .nuv file:  $!\n\n";
            }
        }
    }

    sub cleanup {
        # Nothing to do here
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
