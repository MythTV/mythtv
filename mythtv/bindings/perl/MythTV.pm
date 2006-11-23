#
# MythTV bindings for perl.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Version
    $VERSION = '.21svn';

# Load sub libraries
    use MythTV::Program;
    use MythTV::Recording;
    use MythTV::Channel;

package MythTV;

    use IO::Socket;
    use Sys::Hostname;
    use DBI;
    use Date::Manip;

# Export some routines
    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &unix_to_myth_time &myth_to_unix_time
                        /;
    }

# Constants for the recording types
    our $rectype_once       =  1;
    our $rectype_daily      =  2;
    our $rectype_channel    =  3;
    our $rectype_always     =  4;
    our $rectype_weekly     =  5;
    our $rectype_findone    =  6;
    our $rectype_override   =  7;
    our $rectype_dontrec    =  8;
    our $rectype_finddaily  =  9;
    our $rectype_findweekly = 10;

# And the search types
    our $searchtype_power   = 1;
    our $searchtype_title   = 2;
    our $searchtype_keyword = 3;
    our $searchtype_people  = 4;
    our $searchtype_manual  = 5;

# The character string used by the backend to separate records
    our $BACKEND_SEP    = '[]:[]';
    our $BACKEND_SEP_rx = qr/\[\]:\[\]/;

# MYTH_PROTO_VERSION is defined in libmyth in mythtv/libs/libmyth/mythcontext.h
# and should be the current MythTV protocol version.
    our $PROTO_VERSION = 31;

# NUMPROGRAMLINES is defined in mythtv/libs/libmythtv/programinfo.h and is
# the number of items in a ProgramInfo QStringList group used by
# ProgramInfo::ToSringList and ProgramInfo::FromStringList.
    our $NUMPROGRAMLINES = 42;

# Reasons a recording wouldn't be happening (from libs/libmythtv/programinfo.h)
    our %RecStatus_Types = (
                            '-8' => 'TunerBusy',
                            '-7' => 'LowDiskSpace',
                            '-6' => 'Cancelled',
                            '-5' => 'Deleted',
                            '-4' => 'Aborted',
                            '-3' => 'Recorded',
                            '-2' => 'Recording',
                            '-1' => 'WillRecord',
                              0  => 'Unknown',
                              1  => 'DontRecord',
                              2  => 'PreviousRecording',
                              3  => 'CurrentRecording',
                              4  => 'EarlierShowing',
                              5  => 'TooManyRecordings',
                              6  => 'NotListed',
                              7  => 'Conflict',
                              8  => 'LaterShowing',
                              9  => 'Repeat',
                             10  => 'Inactive',
                             11  => 'NeverRecord'
                            );

# The most recent MythTV object created
    our $last;

# Caches so we don't have to query too many hosts/ports
    our %setting_cache;
    our %proto_cache;
    our %fp_cache;

# Get the user's login and home directory, so we can look for config files
    my ($username, $homedir) = (getpwuid $>)[0,7];
    $username = $ENV{'USER'} if ($ENV{'USER'});
    $homedir  = $ENV{'HOME'} if ($ENV{'HOME'});
    if ($username && !$homedir) {
        $homedir = "/home/$username";
    }

# Read the mysql.txt file in use by MythTV.  It could be in a couple places,
# so try the usual suspects in the same order that mythtv does in
# libs/libmyth/mythcontext.cpp
    our %mysql_conf = ('hostname' => hostname);
    my $found = 0;
    my @mysql = ('/usr/local/share/mythtv/mysql.txt',
                 '/usr/share/mythtv/mysql.txt',
                 '/usr/local/etc/mythtv/mysql.txt',
                 '/etc/mythtv/mysql.txt',
                 $homedir            ? "$homedir/.mythtv/mysql.txt"    : '',
                 'mysql.txt',
                 $ENV{'MYTHCONFDIR'} ? "$ENV{'MYTHCONFDIR'}/mysql.txt" : '',
                );
    foreach my $file (@mysql) {
        next unless ($file && -e $file);
        $found = 1;
        open(CONF, $file) or die "Unable to read $file:  $!\n";
        while (my $line = <CONF>) {
        # Cleanup
            next if ($line =~ /^\s*#/);
            $line =~ s/^str //;
            chomp($line);
        # Split off the var=val pairs
            my ($var, $val) = split(/\=/, $line, 2);
            next unless ($var && $var =~ /\w/);
            if ($var eq 'DBHostName') {
                $mysql_conf{'db_host'} = $val;
            }
            elsif ($var eq 'DBUserName') {
                $mysql_conf{'db_user'} = $val;
            }
            elsif ($var eq 'DBPassword') {
                $mysql_conf{'db_pass'} = $val;
            }
            elsif ($var eq 'DBName') {
                $mysql_conf{'db_name'} = $val;
            }
        # Hostname override
            elsif ($var eq 'LocalHostName') {
                $mysql_conf{'hostname'} = $val;
            }
        }
        close CONF;
    }

# Constructor
    sub new {
        my $class = shift;
        my $self  = {
                     'db_host'     => $mysql_conf{'db_host'},
                     'db_user'     => $mysql_conf{'db_user'},
                     'db_pass'     => $mysql_conf{'db_pass'},
                     'db_name'     => $mysql_conf{'db_name'},
                     'hostname'    => $mysql_conf{'hostname'},
                     'master_host' => undef,
                     'master_port' => undef,
                     'dbh'         => undef,

                     'channels'    => {},
                     'scheduled'   => [],
                     'recorded'    => [],

                    };
        bless($self, $class);

    # Passed-in parameters to override
        my $params = shift;
        if (ref $params) {
            $self->{'db_host'}  = $params->{'db_host'}  if ($params->{'db_host'});
            $self->{'db_user'}  = $params->{'db_user'}  if ($params->{'db_user'});
            $self->{'db_pass'}  = $params->{'db_pass'}  if ($params->{'db_pass'});
            $self->{'db_name'}  = $params->{'db_name'}  if ($params->{'db_name'});
            $self->{'hostname'} = $params->{'hostname'} if ($params->{'hostname'});
        }

    # No db config?
        if (!$self->{'db_host'}) {
            die "Unable to locate mysql.txt:  $!\n" unless ($mysql_conf{'db_host'});
            die "No database host was configured.\n";
        }

    # Connect to the database
        $self->{'dbh'} = DBI->connect("dbi:mysql:database=$self->{'db_name'}:host=$self->{'db_host'}",
                                      $self->{'db_user'},
                                      $self->{'db_pass'})
            or die "Cannot connect to database: $!\n\n";

    # Load the master host and port
        $self->{'master_host'} = $self->backend_setting('MasterServerIP');
        $self->{'master_port'} = $self->backend_setting('MasterServerPort');

        if (!$self->{'master_host'} || !$self->{'master_port'}) {
            die "MasterServerIP or MasterServerPort not found!\n"
               ."You may need to check your settings.php file or re-run mythtv-setup.\n";
        }

    # Connect to the backend
        if ($self->backend_command('ANN Monitor '.$self->{'hostname'}.' 0') ne 'OK') {
            die "Unable to connect to mythbackend, is it running?\n";
        }

    # Find the directory where the recordings are located
        $self->{'video_dir'} = $self->backend_setting('RecordFilePrefix', $self->{'hostname'});
        $self->{'video_dir'} =~ s/\/+$//;

    # Cache the database handle
        $MythTV::last = $self;

    # Return
        return $self;
    }

# Get a setting from the database
    sub backend_setting {
        my $self  = shift;
        my $value = shift;
        my $host  = (shift or '.');
    # A key to reference the current database info
        my $db_key = join('|', $self->{'db_host'}, $self->{'db_user'}, $self->{'db_name'});
    # Cached?
        if (defined $setting_cache{$db_key}{$host}{$value}) {
            return $setting_cache{$db_key}{$host}{$value};
        }
    # Database query
        my $query  = 'SELECT data FROM settings WHERE value=?';
        my @params = ($value);
    # Use a host-specific field?
        if ($host ne '.') {
            $query .= ' AND hostname=?';
            push @params, $host;
        }
    # Query
        my $sh = $self->{'dbh'}->prepare($query);
        $sh->execute(@params);
        ($setting_cache{$db_key}{$host}{$value}) = $sh->fetchrow_array;
        $sh->finish;
    # Return
        return $setting_cache{$db_key}{$host}{$value};
    }

# Issue a command to the backend
    sub backend_command {
        my $self    = shift;
        my $command = shift;
        my $host    = (shift or $self->{'master_host'});
        my $port    = (shift or $self->{'master_port'});
    # Execute the command
        return $self->backend_command2($command, \$fp_cache{$host}{$port}, $host, $port);
    }

# A second backend command, so we can allow certain routines to use their own file pointer
    sub backend_command2 {
        my $self    = shift;
        my $command = shift;
        my $fp      = shift;
        my $host    = (shift or $self->{'master_host'});
        my $port    = (shift or $self->{'master_port'});
    # Command is an array -- join it
        if (ref $command) {
            $command = join($BACKEND_SEP, @{$command});
        }
    # $fp should be a reference
        if ($fp) {
            $fp = \$fp unless (ref $fp);
        }
    # Open a connection to the requested backend, unless we've already done so
        if (!$$fp) {
            if (!$fp_cache{$host}{$port}) {
                $fp_cache{$host}{$port}
                    = IO::Socket::INET->new(PeerAddr => $host,
                                            PeerPort => $port,
                                            Proto    => 'tcp',
                                            Timeout  => 25)
                             or die "Couldn't communicate with $host on port $port:  $@\n";
            }
            $fp = \$fp_cache{$host}{$port};
            if ($$fp && $command !~ /^MYTH_PROTO_VERSION\s/) {
                $self->check_proto_version($host, $port) or return;
            }
        }
    # For some reason, the connection still isn't open
        return unless ($$fp);
    # Connection opened, let's do something
        my ($length, $data, $ret);
    # Send the command
    # The format should be <length + whitespace to 8 total bytes><data>
        print $$fp length($command), ' ' x (8 - length(length($command))), $command;
    # Did we send the close command?  Close the socket and set the file pointer to null - don't even bother waiting for a response
        if ($command eq 'DONE') {
            $$fp->close();
            $$fp = undef;
            return;
        }
    # Read the response header to find out how much data we'll be grabbing
        read($$fp, $length, 8);
        $length = int($length);
    # Read and return any data that was returned
        $ret = '';
        while ($length > 0) {
            my $bytes = read($$fp, $data, ($length < 8192 ? $length : 8192));
        # EOF?
            last if ($bytes < 1);
        # On to the next
            $ret .= $data;
            $length -= $bytes;
        }
    # Return
        return $ret;
    }

    sub check_proto_version {
        my $self = shift;
        my $host = (shift or $self->{'master_host'});
        my $port = (shift or $self->{'master_port'});
    # Cached?
        if (defined $proto_cache{$host}{$port}) {
            return $proto_cache{$host}{$port};
        }
    # Query the version
        my $response = $self->backend_command('MYTH_PROTO_VERSION '.$PROTO_VERSION,
                                              $host, $port);
        my ($code, $vers) = split($BACKEND_SEP_rx, $response);
    # Deal with the response
        if ($code eq 'ACCEPT') {
            return $proto_cache{$host}{$port} = 1;
        }
        if ($code eq 'REJECT') {
            warn "Incompatible protocol version ($PROTO_VERSION != $vers)";
            return $proto_cache{$host}{$port} = 0;
        }
        die "Unexpected response to 'MYTH_PROTO_VERSION $PROTO_VERSION':\n    $response\n";
    }

# Performs a mythbackend query and splits the response into the appropriate number of rows.
    sub backend_rows {
        my $self    = shift;
        my $command = shift;
        my $offset  = (shift or 1);
        my $rows    = ();
    # Query the backend, and split the response into an array
        my @recs = split($BACKEND_SEP_rx, $self->backend_command($command));
    # Parse the records, starting at the offset point
        my $row = 0;
        my $col = 0;
        for (my $i = $offset; $i < @recs; $i++) {
            $rows{'rows'}[$row][$col] = $recs[$i];
        # Every $NUMPROGRAMLINES fields (0 through ($NUMPROGRAMLINES-1)) means
        # a new row.  Please note that this changes between myth versions
            if ($col == ($NUMPROGRAMLINES - 1)) {
                $col = 0;
                $row++;
            }
        # Otherwise, just increment the column
            else {
                $col++;
            }
        }
    # Lastly, grab the offset data (if there is any)
        for (my $i = 0; $i < $offset; $i++) {
            $rows{'offset'}[$i] = $recs[$i];
        }
    # Return the data
        return %rows;
    }

# Return a channel object
    sub channel {
        my $self   = shift;
        my $chanid = shift;
        $self->_load_channels() unless ($self->{'channels'}{$chanid});
        return $self->{'channels'}{$chanid};
    }

# Load all of the known channels for this connection
    sub _load_channels {
        my $self = shift;
        return if (%{$self->{'channels'}});
    # Load the channels
        my $sh = $self->{'dbh'}->prepare('SELECT * FROM channel');
        $sh->execute();
        while (my $row = $sh->fetchrow_hashref) {
            $self->{'channels'}{$row->{'chanid'}} = new MythTV::Channel($row);
        }
        $sh->finish;
    }

# Format a unix timestamp into a MythTV timestamp.  This function is exported.
    sub unix_to_myth_time {
        my $self = shift if (ref $_[0]);
        my $time = shift;
    # Already formatted
        return $time if ($time =~ /\D/);
    # Not a unix epoch
        if ($time =~ /^(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
            return "$1-$2-${3}T$4:$5:$6";
        }
    # Otherwise, format it as necessary
        return UnixDate("epoch $time", '%O');
    }

# Format a unix timestamp into a MythTV timestamp.  This function is exported.
    sub myth_to_unix_time {
        my $self = shift if (ref $_[0]);
        my $time = shift;
        return UnixDate($time, '%s');
    }

# Create a new MythTV::Program object
    sub new_program {
        my $self = shift;
    # Two parameters means chanid and starttime
        if ($#_ == 1) {
            my $chanid    = shift;
            my $starttime = unix_to_myth_time(shift);
            my %rows = $self->backend_rows("QUERY_RECORDING TIMESLOT $chanid $starttime", 1);
            return undef unless ($rows{'offset'}[0] eq 'OK');
            return MythTV::Program->new($self, @{$rows{'rows'}[0]});
        }
    # Otherwise, we just expect a standard backend-formatted row
        return MythTV::Program->new($self, @_);
    }

# Create a new MythTV::Recording object
    sub new_recording {
        my $self = shift;
    # Only one parameter passed in -- that means it's a basename
        if ($#_ == 0) {
            my $basename = shift;
            my %rows = $self->backend_rows("QUERY_RECORDING BASENAME $basename", 1);
            return undef unless ($rows{'offset'}[0] eq 'OK');
            return MythTV::Recording->new($self, @{$rows{'rows'}[0]});
        }
    # Two parameters means chanid and starttime
        elsif ($#_ == 1) {
            my $chanid    = shift;
            my $starttime = unix_to_myth_time(shift);
            my %rows = $self->backend_rows("QUERY_RECORDING TIMESLOT $chanid $starttime", 1);
            return undef unless ($rows{'offset'}[0] eq 'OK');
            return MythTV::Recording->new($self, @{$rows{'rows'}[0]});
        }
    # Otherwise, we just expect a standard backend-formatted row
        return MythTV::Recording->new($self, @_);
    }

# Return true
1;

