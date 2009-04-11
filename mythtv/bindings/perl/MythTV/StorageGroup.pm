#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV Storage Group.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Make sure that the main MythTV package is loaded
    use MythTV;

package MythTV::StorageGroup;

# Constructor
    sub new {
        my $class = shift;
        my $self  = { };
        bless($self, $class);

    # We need MythTV
        die "Please create a MythTV object before creating a $class object.\n" unless ($MythTV::last);
    # Figure out how the data was passed in
        if (ref $_[0]) {
            $self->{'_mythtv'} = shift;
        }
        $self->{'_mythtv'} ||= $MythTV::last;

    # The information passed in will be a hashref
        my $data = shift;

        $self->{'groupname'}     = $data->{'groupname'};
        $self->{'hostname'}      = $data->{'hostname'};
        $self->{'dirs'}          = $self->GetStorageDirs();

    # Return
        return $self;
    }

# Find the full pathname of the given file
    sub FindRecordingFile {
        my $self = shift;
        my $basename = shift;

        my $dir = $self->FindRecordingDir($basename);
        return $dir ? "$dir/$basename" : '';
    }

# Find the directory which contains the given file
    sub FindRecordingDir {
        my $self = shift;
        my $basename = shift;

        foreach my $dir ( @{$self->{'dirs'}} ) {
            next unless (-e "$dir/$basename");
            return $dir;
        }

        return '';
    }

# Get the first directory in the list
    sub GetFirstStorageDir {
        my $self = shift;
        return ($self->{'dirs'}->[0] or '');
    }

# Get the list of all recording directories
    sub GetStorageDirs {
        my $self = shift;

        my @recdirs;
        my @params;
        my $query = 'SELECT DISTINCT dirname FROM storagegroup';

        if ($self->{'groupname'}) {
            $query .= ' WHERE groupname = ?';
            push @params, $self->{'groupname'};
            if ($self->{'hostname'}) {
                $query .= ' AND hostname = ?';
                push @params, $self->{'hostname'};
            }
        }

        my $sh = $self->{'_mythtv'}{'dbh'}->prepare($query);

        $sh->execute(@params);
        while ((my $dir) = $sh->fetchrow_array) {
            $dir =~ s/\/+$//;
            push(@recdirs, $dir);
        }
        $sh->finish;

        return \@recdirs;
    }

# Return true
    1;
