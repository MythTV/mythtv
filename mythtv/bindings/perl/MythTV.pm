#
# MythTV bindings for perl.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Version
    $VERSION = '.26git';

# Load sub libraries
    use IO::Socket::INET::MythTV;
    use MythTV::Program;
    use MythTV::Recording;
    use MythTV::Channel;
    use MythTV::StorageGroup;

package MythTV;

    use Sys::Hostname;
    use DBI;
    use HTTP::Request;
    use LWP::UserAgent;
    use POSIX;

# Load the UPNP libraries if we have them, but die nicely if we don't.
    BEGIN {
        our $has_upnp = 1;
        eval 'use Net::UPnP::QueryResponse;';
        if ($@) {
            $has_upnp = 0;
            print STDERR "Net::UPnP::QueryResponse is not installed!\n";
        }
        else
        {
            eval 'use Net::UPnP::ControlPoint;';
            if ($@) {
                $has_upnp = 0;
                print STDERR "Net::UPnP::ControlPoint is not installed!\n";
            }
        }
    }

# Necessary constants for sysopen
    use Fcntl;

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

# And the recstatus types
    our $recstatus_tunerbusy         = '-8';
    our $recstatus_lowdiskspace      = '-7';
    our $recstatus_cancelled         = '-6';
    our $recstatus_deleted           = '-5';
    our $recstatus_aborted           = '-4';
    our $recstatus_recorded          = '-3';
    our $recstatus_recording         = '-2';
    our $recstatus_willrecord        = '-1';
    our $recstatus_unknown           =   0 ;
    our $recstatus_dontrecord        =   1 ;
    our $recstatus_previousrecording =   2 ;
    our $recstatus_currentrecording  =   3 ;
    our $recstatus_earliershowing    =   4 ;
    our $recstatus_toomanyrecordings =   5 ;
    our $recstatus_notlisted         =   6 ;
    our $recstatus_conflict          =   7 ;
    our $recstatus_latershowing      =   8 ;
    our $recstatus_repeat            =   9 ;
    our $recstatus_inactive          =  10 ;
    our $recstatus_neverrecord       =  11 ;

# The character string used by the backend to separate records
    our $BACKEND_SEP    = '[]:[]';
    our $BACKEND_SEP_rx = qr/\[\]:\[\]/;

# MYTH_PROTO_VERSION is defined in libmythbase in 
# mythtv/libs/libmythbase/mythversion.h and should be the current MythTV 
# protocol version.
#
# Note: as of July 21, 2010, this is actually a string, to account for proto
# versions of the form "58a".  This will get used if protocol versions are 
# changed on a fixes branch ongoing.
    our $PROTO_VERSION = "77";
    our $PROTO_TOKEN = "WindMark";

# currentDatabaseVersion is defined in libmythtv in
# mythtv/libs/libmythtv/dbcheck.cpp and should be the current MythTV core
# schema version supported in the main code.  We need to check that the schema
# version in the database is as expected by the bindings, which are expected
# to be kept in sync with the main code.
    our $SCHEMA_VERSION = "1314";

# NUMPROGRAMLINES is defined in mythtv/libs/libmythtv/programinfo.h and is
# the number of items in a ProgramInfo QStringList group used by
# ProgramInfo::ToSringList and ProgramInfo::FromStringList.
    our $NUMPROGRAMLINES = 47;

# Reasons a recording wouldn't be happening (from libs/libmythtv/programinfo.h)
    our %RecStatus_Types = (
                            $recstatus_tunerbusy         => 'TunerBusy',
                            $recstatus_lowdiskspace      => 'LowDiskSpace',
                            $recstatus_cancelled         => 'Cancelled',
                            $recstatus_deleted           => 'Deleted',
                            $recstatus_aborted           => 'Aborted',
                            $recstatus_recorded          => 'Recorded',
                            $recstatus_recording         => 'Recording',
                            $recstatus_willrecord        => 'WillRecord',
                            $recstatus_unknown           => 'Unknown',
                            $recstatus_dontrecord        => 'DontRecord',
                            $recstatus_previousrecording => 'PreviousRecording',
                            $recstatus_currentrecording  => 'CurrentRecording',
                            $recstatus_earliershowing    => 'EarlierShowing',
                            $recstatus_toomanyrecordings => 'TooManyRecordings',
                            $recstatus_notlisted         => 'NotListed',
                            $recstatus_conflict          => 'Conflict',
                            $recstatus_latershowing      => 'LaterShowing',
                            $recstatus_repeat            => 'Repeat',
                            $recstatus_inactive          => 'Inactive',
                            $recstatus_neverrecord       => 'NeverRecord'
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
        if (!-e $homedir && -e "/Users/$username") {
            $homedir = "/Users/$username";
        }
    }

# Config file?
    my $conf = $ENV{'MYTHCONFDIR'}
        ? $ENV{'MYTHCONFDIR'}
        : "$homedir/.mythtv"
        ;
    unless (-T "$conf/config.xml") {
        unless ($has_upnp) {
            die "Please install Net::UPnP or copy ~/.mythtv/config.xml\n"
               ."from a working MythTV installation to $conf/config.xml.\n";
        }
    # Try to create the config directory first, so we can die sooner if it fails.
        unless (-d $conf) {
            mkdir $conf or die "Can't create config directory $conf:  $!\n";
        }
    # @todo:  prompt for a security pin
        my $pin = '';
    # Try to detect things via upnp
        my (%seen, @devices);
        my $obj = Net::UPnP::ControlPoint->new();
        my @dev_list = $obj->search(
            st => 'urn:schemas-mythtv-org:device:MasterMediaServer:1',
            mx => 2
            );
        foreach $dev (@dev_list) {
            $device_type = $dev->getdevicetype();
            if  ($device_type ne 'urn:schemas-upnp-org:device:MediaServer:1') {
                next;
            }
        # Skip non-mythtv
            my $mfg = $dev->getdescription(name=>'manufacturer');
            next unless ($mfg eq 'MythTV');
        # Skip duplicate scan entries
            my $urlbase = $dev->getdescription(name=>'URLBase');
            my $hash = join(';',
                $dev->getudn(),
                $urlbase,
                );
            next if ($seen{$hash});
            $seen{$hash} = 1;
        # Try to connect to the host to get
            my $req = HTTP::Request->new('GET', "${urlbase}Myth/GetConnectionInfo?Pin=$pin");
            my $ua  = LWP::UserAgent->new(keep_alive => 1);
            my $response = $ua->request($req);
        # Return the results
            next unless ($response->is_success);
            next unless ($response->content =~ m#<Database>(.+?)</Database>#s);
            my $info = $1;
            ($dev->{db_host}) = ($info =~ m#<Host>(.+?)</Host>#);
            ($dev->{db_port}) = ($info =~ m#<Port>(\d*?)</Port>#);
            ($dev->{db_user}) = ($info =~ m#<UserName>(.+?)</UserName>#);
            ($dev->{db_pass}) = ($info =~ m#<Password>(.+?)</Password>#);
            ($dev->{db_name}) = ($info =~ m#<Name>(.+?)</Name>#);
            $dev->{db_port} ||= 3306;
        # Skip duplicate db connections, too
            $hash = "mysql:$dev->{db_user}:$dev->{db_pass}\@$dev->{db_host}:$dev->{db_port}::$dev->{db_name}";
            next if ($seen{$hash});
            $seen{$hash} = 1;
        # Add some more data to the device, while we're here
            $dev->{udn} = $dev->getudn();
            $dev->{usn} = join('::',
                $dev->{udn},
                $dev->getdevicetype(),
                );
        # Success
            push @devices, $dev;
        }
        if (@devices < 1) {
            die "No backends found.  Please copy config.xml from a "
               ."working MythTV installation to $conf.\n";
        }
        my $upnp = $devices[0];
        if (@devices > 1) {
            die "Multiple devices found via UPnP.  This configuration not yet\n"
               ."supported.  Please copy config.xml from a working "
               ."MythTV installation to $conf.\n";
            #print $dev->getfriendlyname(), "\n";
        }
    # Save the config.xml
        open CONF, ">$conf/config.xml" or die "Can't write to $conf/config.xml:  $!\n";
        print CONF <<EOF;
<Configuration>
  <UPnP>
    <UDN>
      <MediaRenderer>$upnp->{usn}</MediaRenderer>
    </UDN>
    <MythFrontend>
      <DefaultBackend>
        <USN>$upnp->{usn}</USN>
        <SecurityPin>$pin</SecurityPin>
      </DefaultBackend>
    </MythFrontend>
  </UPnP>
  <LocalHostName>my-unique-identifier-goes-here</LocalHostName>
  <Database>
    <PingHost>1</PingHost>
    <Host>$upnp->{db_host}</Host>
    <UserName>$upnp->{db_user}</UserName>
    <Password>$upnp->{db_pass}</Password>
    <DatabaseName>$upnp->{db_name}</DatabaseName>
    <Port>$upnp->{db_port}</Port>
  </Database>
  <WakeOnLAN>
    <Enabled>0</Enabled>
    <SQLReconnectWaitTime>0</SQLReconnectWaitTime>
    <SQLConnectRetry>5</SQLConnectRetry>
    <Command>echo 'WOLsqlServerCommand not set'</Command>
  </WakeOnLAN>
</Configuration>
EOF
        close CONF;
    }

# Read the config.xml file in use by MythTV.
    our %mysql_conf = ('hostname' => hostname,
                       'db_host'  => 'localhost',
                       'db_port'  => '',
                       'db_user'  => 'mythtv',
                       'db_pass'  => 'mythtv',
                       'db_name'  => 'mythconverg',
                       'upnp_pin' => '',
                      );
    open(CONF, "$conf/config.xml") or die "Unable to read $conf/config.xml:  $!\n";
    while (my $line = <CONF>) {
    # Search through the XML data
        if ($line =~ m#<SecurityPin>(.*?)</SecurityPin>#) {
            $mysql_conf{'upnp_pin'} = $1;
        }
        elsif (($line =~ m#<Host>(.*?)</Host>#) ||
               ($line =~ m#<DBHostName>(.*?)</DBHostName>#)) {
            $mysql_conf{'db_host'}  = $1;
        }
        elsif (($line =~ m#<UserName>(.*?)</UserName>#) ||
               ($line =~ m#<DBUserName>(.*?)</DBUserName>#)) {
            $mysql_conf{'db_user'}  = $1;
        }
        elsif (($line =~ m#<Password>(.*?)</Password>#) ||
               ($line =~ m#<DBPassword>(.*?)</DBPassword>#)) {
            $mysql_conf{'db_pass'}  = $1;
            $mysql_conf{'db_pass'} =~ s/&amp;/&/sg;
            $mysql_conf{'db_pass'} =~ s/&gt;/>/sg;
            $mysql_conf{'db_pass'} =~ s/&lt;/</sg;
        }
        elsif (($line =~ m#<DatabaseName>(.*?)</DatabaseName>#) ||
               ($line =~ m#<DBName>(.*?)</DBName>#)) {
            $mysql_conf{'db_name'}  = $1;
        }
        elsif (($line =~ m#<Port>(\d*?)</Port>#) ||
               ($line =~ m#<DBPort>(.*?)</DBPort>#)) {
            $mysql_conf{'db_port'}  = $1;
        }
    # Hostname override.  Not sure if this is still valid or not
        elsif ($line =~ m#<LocalHostName>(.*?)</LocalHostName>#) {
            $profileoverride = $1;
            if ($profileoverride ne "my-unique-identifier-goes-here") {
                $mysql_conf{'hostname'} = $profileoverride;
            }
        }
    }
    close CONF;

# Cleanup
    $mysql_conf{'db_port'} ||= 3306;

# Constructor
    sub new {
        my $class = shift;
        my $self  = {
                     'master_host' => undef,
                     'master_port' => undef,
                     'dbh'         => undef,

                     'channels'    => {},
                     'callsigns'   => {},
                     'scheduled'   => [],
                     'recorded'    => [],

                    # The following options can be overridden via $opts
                     'db_host'     => $mysql_conf{'db_host'},
                     'db_port'     => $mysql_conf{'db_port'},
                     'db_user'     => $mysql_conf{'db_user'},
                     'db_pass'     => $mysql_conf{'db_pass'},
                     'db_name'     => $mysql_conf{'db_name'},
                     'hostname'    => $mysql_conf{'hostname'},
                     'connect'     => 1,

                    };
        bless($self, $class);

    # Passed-in parameters to override
        my $params = shift;
        if (ref $params eq 'HASH') {
            $self->{'db_host'}  = $params->{'db_host'}  if ($params->{'db_host'});
            $self->{'db_port'}  = $params->{'db_port'}  if ($params->{'db_port'});
            $self->{'db_user'}  = $params->{'db_user'}  if ($params->{'db_user'});
            $self->{'db_pass'}  = $params->{'db_pass'}  if ($params->{'db_pass'});
            $self->{'db_name'}  = $params->{'db_name'}  if ($params->{'db_name'});
            $self->{'hostname'} = $params->{'hostname'} if ($params->{'hostname'});
            if (defined $params->{'connect'}) {
                $self->{'connect'} = $params->{'connect'};
            }
        }

    # Connect to the database
        $self->{'dbh'} = DBI->connect("dbi:mysql:database=$self->{'db_name'}:host=$self->{'db_host'};port=$self->{'db_port'}",
                                      $self->{'db_user'},
                                      $self->{'db_pass'})
            or die "Cannot connect to database: $!\n\n";
        $self->{'dbh'}->do("SET time_zone = 'UTC'")
            or die "Can't set timezone: $!\n\n";

    # Check for supported schema version
        $self->{'schema_version'} = $self->backend_setting('DBSchemaVer');
        if( $self->{'schema_version'} != $SCHEMA_VERSION ) {
            die "Database schema $self->{'schema_version'} not supported.\n" .
                "Bindings support schema version $SCHEMA_VERSION\n";
        }

    # Load the master host and port
        $self->{'master_host'} = $self->backend_setting('MasterServerIP');
        $self->{'master_port'} = $self->backend_setting('MasterServerPort');

        if (!$self->{'master_host'} || !$self->{'master_port'}) {
            die "MasterServerIP or MasterServerPort not found!\n"
               ."You may need to check your settings.php file or re-run mythtv-setup.\n";
        }

    # Cache the database handle
        $MythTV::last = $self;

    # Connect to the backend?
        $self->connect() if ($self->{'connect'});

    # Find the directory where the recordings are located
        $self->{'video_dirs'} = $self->get_recording_dirs();

    # Return
        return $self;
    }

# Connect to the running backend
    sub connect {
        my $self = shift;
        my $wantsEvents = shift;
        $wantsEvents = 0 if (!defined($wantsEvents));
        if ($self->backend_command('ANN Monitor '.$self->{'hostname'}." $wantsEvents") ne 'OK') {
            die "Unable to connect to mythbackend, is it running?\n";
        }
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
    # Command is an array -- join it
        if (ref $command) {
            $command = join($BACKEND_SEP, @{$command});
        }
    # Smaller name for the socket pointer
        my $fp = \$fp_cache{$host}{$port};
    # Open a connection to the requested backend, unless we've already done so
        if (!$$fp) {
            $$fp = $self->new_backend_socket($host, $port);
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
        $$fp->send_data($command);
    # Did we send the close command?  Close the socket and set the file pointer
    # to null - don't even bother waiting for a response.
        if ($command eq 'DONE') {
            $$fp->close();
            $$fp = undef;
            return;
        }
    # Return the response
        return $$fp->read_data();
    }

# Connect to the status port and execute a command
    sub status_command {
        my $self   = shift;
        my $path   = (shift or '');
        my $params = (shift or {});
        my $host   = (shift or $self->{'master_host'});
    # This is a total kludge, since BackendStatusPort can be different for
    # different hosts, but since the hostname field in the db is a NAME, and
    # MasterServerIP can be either name or IP, there's not much else we can do
    # here for now.
        my $port   = (shift or $self->backend_setting('BackendStatusPort'));
    # Generate the query string
        my $query = '';
        if (ref $params eq 'HASH') {
            foreach my $var (keys %{$params}) {
                my $svar = $var;
                my $sval = ($params->{$var} or '');
                $query .= '&' if ($query);
                $svar =~ s/([^\w*\.\-])/sprintf '%%%02x', ord $1/sge;
                $sval =~ s/([^\w*\.\-])/sprintf '%%%02x', ord $1/sge;
                $query .= "$svar=$sval";
            }
        }
    # Set up the HTTP::Request object
        my $req = HTTP::Request->new('POST', "http://$host:$port/$path");
        $req->header('Content-Type'   => 'application/x-www-form-urlencoded',
                     'Content-Length' => length($query));
        $req->content($query);
    #Make the request
        my $ua = LWP::UserAgent->new(keep_alive => 1);
        my $response = $ua->request($req);
    # Return the results
        return undef unless ($response->is_success);
        return $response->content;
    }

# Create a new socket connection to the backend
    sub new_backend_socket {
        my $self    = shift;
        my $host    = (shift or $self->{'master_host'});
        my $port    = (shift or $self->{'master_port'});
        my $data    = shift;
        my $sock    = IO::Socket::INET::MythTV->new(PeerAddr => $host,
                                                    PeerPort => $port,
                                                    Proto    => 'tcp',
                                                    Reuse    => 1,
                                                    Timeout  => 25)
                                       or die "Couldn't communicate with $host on port $port:  $@\n";
    # Send any data that we received
        if ($data) {
            if (ref $data) {
                $data = join($BACKEND_SEP, @{$data});
            }
            $sock->send_data($data);
        }
    # Return the socket we just created
        return $sock
    }

# Download/stream a file from a backend.  Return values are:
#
# undef:  Error
# 1:      File copied into place
# 2:      File retrieved from the backend
    sub stream_backend_file {
        my $self = shift;
    # The basename of the file to be downloaded
        my $basename = shift;
    # This parameter can be a file handle or a string.  We'll determine what to
    # do with it below.
        my $fh = shift;
        my $target_path = ref($fh) ? undef : $fh;
    # Local path is optional, but if it is provided, this routine will attempt
    # to access it directly rather than streaming it over the network interface.
        my $local_path = shift;
    # Host and port are optional, but *really* should be provided, since it's
    # not guaranteed that the master backend will hold all files.
        my $host = (shift or $self->{'master_host'});
        my $port = (shift or $self->{'master_port'});
    # $seek is optional, and is the amount to seek from the start of the file
    # (for resuming downloads, etc.)
        my $seek = (shift or 0);
    # Check to see if we were passed a URL using the myth protocol - if so,
    # override the values set above accordingly
        if (substr($basename, 0,7) eq "myth://") {
            $basename = substr($basename, 7);
            my $index = index($basename, ":");
            $host = substr($basename, 0, $index);
            my $endindex = index($basename, "/", $index);
            $port = substr($basename, $index+1, $endindex-$index-1);
            $basename = substr($basename, $endindex);
        }
    # We need to figure out if we were passed a file handle or a filename.  If
    # it was a pathname, we should open the file for writing.
        if ($target_path) {
            sysopen($fh, $target_path, O_CREAT|O_TRUNC|O_WRONLY) or die "Can't create $target_path:  $!\n";
        }
        elsif (ref($fh) ne 'GLOB') {
            die "Developer passed an unrecognized filehandle/path value to MythTV::stream_backend_file().\n";
        }
    # Make sure the output file handle is in binary mode
        binmode($fh);
    # Local path to the file that we can just copy from?
        if ($local_path && -e $local_path) {
            my $infh;
            if (sysopen($infh, $local_path, O_RDONLY)) {
                binmode($infh);
            # Seek?
                if ($seek > 0) {
                    sysseek($infh, $seek, 0);
                }
            # Read the file
                my $buffer;
                while (sysread($infh, $buffer, 2097152)) {
                    $fh->print($buffer);
                }
                close $infh;
            # Close any file handles we created
                $infh->close();
                $fh->close() if ($target_path);
                return 1;
            }
            else {
                print STDERR "Can't read $local_path ($!).  Trying to stream from mythbackend instead.\n";
            }
        }
    # Otherwise, we have to stream the file from the backend.
    # First, tell the backend with the file that we're about to request some
    # data.
        my $csock = $self->new_backend_socket($host,
                                              $port,
                                              'ANN Playback '.hostname.' 0');
        $csock->read_data();
    # Announce the file transfer request to the backend, and prepare a new
    # socket to hold the data from the backend.
        my $sock = $self->new_backend_socket($host,
                                             $port,
                                             join($MythTV::BACKEND_SEP,
                                                  'ANN FileTransfer '.hostname.
                                                  ' 0 1 2000', $basename, '...'));
        my @recs = split($MythTV::BACKEND_SEP_rx,
                         $sock->read_data());
    # Error?
        if ($recs[0] ne 'OK') {
            print STDERR "Unknown error starting transfer of $basename\n";
            return undef;
        }
    # Seek to the requested position?
        if ($seek > 0) {
            $csock->send_data(join($MythTV::BACKEND_SEP,
                                   'QUERY_FILETRANSFER '.$recs[1],
                                   'SEEK',
                                   $seek,
                                   0,
                                   0));
            $csock->read_data();
        }
    # Transfer the data.
        # File size is longlong but is sent as 2 signed ints
        my $total = $recs[2];
        while ($sock && $total > 0) {
        # Attempt to read in 2M chunks, or the remaining total, whichever is
        # smaller.
            my $length = $total > 2097152 ? 2097152 : $total;
        # Request a block of data from the backend.  In the case of a preview image,
        # it should be small enough that we can request the whole thing at once.
            $csock->send_data(join($MythTV::BACKEND_SEP,
                                   'QUERY_FILETRANSFER '.$recs[1],
                                   'REQUEST_BLOCK',
                                   $length));
        # Read the data from the socket and save it into the requested file.
        # We have to loop here because sometimes the backend can't send data fast
        # enough, even if we're dealing with small files.
            my $buffer;
            while ($length > 0) {
                my $bytes = $sock->sysread($buffer, $length);
            # Error?
                last unless (defined $bytes);
            # EOF?
                last if ($bytes < 1);
            # On to the next
                $fh->print($buffer);
                $length -= $bytes;
                $total  -= $bytes;
            }
        # Get the response from the control socket
            my $size = $csock->read_data();
            last if ($size < 1);
        }
    # Make sure we close any files we created
        if ($target_path) {
            close($fh);
        }
    # Return
        return 2;
    }

# Check the MythProto version between the backend and this script
    sub check_proto_version {
        my $self = shift;
        my $host = (shift or $self->{'master_host'});
        my $port = (shift or $self->{'master_port'});
    # Cached?
        if (defined $proto_cache{$host}{$port}) {
            return $proto_cache{$host}{$port};
        }
    # Query the version
        my $response = $self->backend_command('MYTH_PROTO_VERSION '.$PROTO_VERSION.' '.$PROTO_TOKEN,
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

# Return a channel object, by chanid
    sub channel {
        my $self   = shift;
        my $chanid = shift;
        $self->load_channels() unless (%{$self->{'channels'}});
        return $self->{'channels'}{$chanid};
    }

# Return a channel object, by callsign
    sub callsign {
        my $self     = shift;
        my $callsign = shift;
        $self->load_channels() unless (%{$self->{'channels'}});
        return $self->{'callsigns'}{$callsign};
    }

# Load all of the known channels for this connection
    sub load_channels {
        my $self = shift;
        return if (%{$self->{'channels'}});
    # Load the channels
        my $sh = $self->{'dbh'}->prepare('SELECT channel.*,
                                                 dtv_multiplex.bandwidth         AS dtv_bandwidth,
                                                 dtv_multiplex.constellation     AS dtv_constellation,
                                                 dtv_multiplex.default_authority AS dtv_default_authority,
                                                 dtv_multiplex.fec               AS dtv_fec,
                                                 dtv_multiplex.frequency         AS dtv_frequency,
                                                 dtv_multiplex.guard_interval    AS dtv_guard_interval,
                                                 dtv_multiplex.hierarchy         AS dtv_hierarchy,
                                                 dtv_multiplex.hp_code_rate      AS dtv_hp_code_rate,
                                                 dtv_multiplex.inversion         AS dtv_inversion,
                                                 dtv_multiplex.lp_code_rate      AS dtv_lp_code_rate,
                                                 dtv_multiplex.modulation        AS dtv_modulation,
                                                 dtv_multiplex.mod_sys           AS dtv_mod_sys,
                                                 dtv_multiplex.networkid         AS dtv_networkid,
                                                 dtv_multiplex.polarity          AS dtv_polarity,
                                                 dtv_multiplex.rolloff           AS dtv_rolloff,
                                                 dtv_multiplex.serviceversion    AS dtv_serviceversion,
                                                 dtv_multiplex.sistandard        AS dtv_sistandard,
                                                 dtv_multiplex.sourceid          AS dtv_sourceid,
                                                 dtv_multiplex.symbolrate        AS dtv_symbolrate,
                                                 dtv_multiplex.transmission_mode AS dtv_transmission_mode,
                                                 dtv_multiplex.transportid       AS dtv_transportid,
                                                 dtv_multiplex.updatetimestamp   AS dtv_updatetimestamp,
                                                 dtv_multiplex.visible           AS dtv_visible
                                            FROM channel
                                                 LEFT JOIN dtv_multiplex
                                                        ON channel.mplexid = dtv_multiplex.mplexid
                                        ORDER BY chanid
                                         ');
        $sh->execute();
        while (my $row = $sh->fetchrow_hashref) {
            $self->{'channels'}{$row->{'chanid'}} = new MythTV::Channel($row);
            $self->{'callsigns'}{$row->{'callsign'}} ||= \$self->{'channels'}{$row->{'chanid'}};
        }
        $sh->finish;
    }

# Format a unix timestamp into a MythTV timestamp.  This function is exported.
    sub unix_to_myth_time {
        my $self = (ref $_[0] ? shift : $MythTV::last);
        my $time = shift;
    # Already formatted
        return $time if ($time =~ /\D/);
    # Not a unix epoch
        if ($time =~ /^(\d{4})(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
            return "$1-$2-${3}T$4:$5:$6";
        }
    # Otherwise, format it as necessary.
        return POSIX::strftime("%Y-%m-%dT%H:%M:%S", gmtime($time));
    }

# Format a MythTV timestamp into a unix timestamp.  This function is exported.
# We have to use MySQL here because it was used historically, and so the
# formats that MySQL supports is effectively the API...
    sub myth_to_unix_time {
        my $self = (ref $_[0] ? shift : $MythTV::last);
        my $time = shift;
        my $sh = $self->{'dbh'}->prepare('SELECT UNIX_TIMESTAMP(?)');
        $sh->execute($time);
        ($time) = $sh->fetchrow_array();
        return $time;
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

# Get the list of all recording directories
    sub get_recording_dirs {
        my $self = shift;

        my @recdirs;
        my $sh = $self->{'dbh'}->prepare('SELECT DISTINCT dirname
                                            FROM storagegroup');

        $sh->execute();
        while ((my $dir) = $sh->fetchrow_array) {
            $dir =~ s/\/+$//;
            push(@recdirs, $dir);
        }
        $sh->finish;

        return \@recdirs;
    }

# Delete the specified file in the specified Storage Group
    sub delete_file {
        my $self = shift;
        my $filename = shift;
        my $sgroup = shift;

        return $self->backend_command(join($MythTV::BACKEND_SEP, 'DELETE_FILE', $filename, $sgroup));
    }

# Return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
