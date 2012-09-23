#!/usr/bin/perl -w
#
# mythconverg_backup.pl
#
# Creates a backup of the MythTV database.
#
# For details, see:
#   mythconverg_backup.pl --help

# Includes
    use Getopt::Long;
    use File::Temp qw/ tempfile /;

# Script info
    $NAME           = 'MythTV Database Backup Script';
    $VERSION        = '1.0.11';

# Some variables we'll use here
    our ($username, $homedir, $mythconfdir, $database_information_file);
    our ($mysqldump, $compress, $rotate, $rotateglob, $backup_xmltvids);
    our ($usage, $debug, $show_version, $show_version_script, $dbh);
    our ($d_db_name, $d_mysqldump, $d_compress, $d_rotate, $d_rotateglob);
# This script does not accept a database password on the command-line.
# Any packager who enables the functionality should modify the --help output.
#    our ($db_password);
    our ($db_hostname, $db_port, $db_username, $db_name, $db_schema_version);
    our ($backup_directory, $backup_filename);
    our ($verbose_level_always, $verbose_level_debug, $verbose_level_error);

    our %mysql_conf  = ('db_host'       => '',
                        'db_port'       => -1,
                        'db_user'       => '',
                        'db_pass'       => '',
                        'db_name'       => '',
                        'db_schemaver'  => ''
                        );
    our %backup_conf = ('directory'     => '',
                        'filename'      => ''
                       );

# Variables used to untaint data
    our $is_env_tainted = 1;
    our $old_env_path = $ENV{"PATH"};
    our @d_allowed_paths = ("/bin",
                            "/usr/bin",
                            "/usr/local/bin",
                            "/sbin",
                            "/usr/sbin",
                            "/usr/local/sbin"
                           );
    our @allowed_paths;

# Debug levels
    $verbose_level_always = 0;
    $verbose_level_debug = 1;
    $verbose_level_error = 255;

# Defaults
    $d_db_name       = 'mythconverg';
    $d_mysqldump     = 'mysqldump';
    $d_compress      = 'gzip';
    $d_rotate        = 5;
    $d_rotateglob    = $d_db_name.'-????-??????????????.sql*';

# Provide default values for GetOptions
    $mysqldump       = $d_mysqldump;
    $compress        = $d_compress;
    $rotate          = $d_rotate;
    $rotateglob      = $d_rotateglob;
    $debug           = 0;

# Load the cli options
    GetOptions('hostname|DBHostName=s'              => \$db_hostname,
               'port|DBPort=i'                      => \$db_port,
               'username|DBUserName=s'              => \$db_username,
# This script does not accept a database password on the command-line.
#               'password|DBPassword=s'              => \$db_password,
               'name|DBName=s'                      => \$db_name,
               'schemaver|DBSchemaVer=s'            => \$db_schema_version,
               'directory|DBBackupDirectory=s'      => \$backup_directory,
               'filename|DBBackupFilename=s'        => \$backup_filename,
               'mysqldump=s'                        => \$mysqldump,
               'compress=s'                         => \$compress,
               'rotate=i'                           => \$rotate,
               'rotateglob|glob=s'                  => \$rotateglob,
               'backup_xmltvids|backup-xmltvids|'.
               'xmltvids'                           => \$backup_xmltvids,
               'usage|help|h+'                      => \$usage,
               'version'                            => \$show_version,
               'script_version|script-version|v'    => \$show_version_script,
               'verbose|debug|d+'                   => \$debug
              );

# Print version information
    sub print_version_information
    {
        my $script_name = substr $0, rindex($0, '/') + 1;
        print "$NAME\n$script_name\nversion: $VERSION\n";
    }

    if ($show_version_script)
    {
        print "$NAME,$VERSION,,\n";
        exit;
    }
    elsif ($show_version)
    {
        print_version_information;
        exit;
    }


# Print usage
    if ($usage)
    {
        print_version_information;
        print <<EOF;

Usage:
  $0 [options|database_information_file]

Creates a backup of the MythTV database.

QUICK START:

Create a file ~/.mythtv/backuprc with a single line,
"DBBackupDirectory=/home/mythtv" (no quotes), and run this script to create a
database backup. Use the --verbose argument to see what is happening.

# echo "DBBackupDirectory=/home/mythtv" > ~/.mythtv/backuprc
# $0 --verbose

Make sure you keep the backuprc file for next time. Once you have successfully
created a backup, the script may be run without the --verbose argument.

To backup xmltvids:

Ensure you have a ~/.mythtv/backuprc file, as described above, and execute this
script with the --backup_xmltvids argument.

# $0 --backup_xmltvids

EOF

        if ($usage > 1)
        {
            print <<EOF;
DETAILED DESCRIPTION:

This script can be called by MythTV for creating automatic database backups.
In this mode, it is always exected with a single command-line argument
specifying the name of a "database information file" (see DATABASE INFORMATION
FILE, below), which contains sufficient information about the database and the
backup to allow the script to create a backup without needing any additional
configuration files. In this mode, all other MythTV configuration files
(including config.xml, mysql.txt) are ignored, but the backup resource file
(see RESOURCE FILE, below) and the MySQL option files (i.e. /etc/my.cnf or
~/.my.cnf) will be honored.

The script can also be called interactively (i.e. "manually") by the user to
create a database backup on demand. Required information may be passed into
the script using command-line arguments or with a database information file.
If a database information file is specified, all command-line arguments will be
ignored. If no database information file is specified, the script will attempt
to determine the appropriate configuration by using the MythTV configuration
file(s) (preferring config.xml, but falling back to mysql.txt if no config.xml
exists). Once the MythTV configuration file has been parsed, the backup
resource file (see RESOURCE FILE, below) will be parsed, then command-line
arguments will be applied (thus overriding any values determined from the
configuration files).

The only information required by the script is the directory in which the
backup should be created. Therefore, when using a database information file,
the DBBackupDirectory should be specified, or if running manually, the
--directory command-line argument should be specified. The DBBackupDirectory
may be specified in a backup resource file (see RESOURCE FILE, below). Doing
so is especially useful for manual backups. If the specified directory is not
writable, the script will terminate. Likewise, if a file whose name matches
the name to be used for the backup file already exists, the script will
terminate.

If the database name is not specified, the script will attempt to use the
MythTV default database name, $d_db_name. Note that the same is not true for
the database username and database password. These must be explicitly
specified. The password must be specified in a database information file, a
backup resource file, or a MySQL options file. The username may be specified
the same way or may be specified using a command-line argument if not using a
database information file.

While this script may be called while MythTV is running, there is a possibility
of creating a backup with data integrity errors (i.e. if MythTV updates data in
multiple tables between the time the script backs up the first and subsequent
tables). Also, depending on your system configuration, performing a backup
(which may result in locking a table while it is being backed up) while
recording may cause corruption of the recording or inability to properly write
recording data (such as the recording seek table) to the database.
Therefore, if configuring this script to run in a cron job, try to ensure it
runs at a time when recordings are least likely to occur. Alternatively, by
choosing to run the script in a system start/shutdown script (i.e. an init
script), you may call the script before starting mythbackend or after stopping
mythbackend. Note, however, that checking whether to perform the backup is the
responsibility of the init script (not this script)--i.e. in a system with
multiple frontends/backends, the init script should ensure the backup is
created only on the master backend.

DATABASE INFORMATION FILE

The database information file contains information about the database and the
backup. The information within the file is specified as name=value pairs using
the same names as used by the MythTV config.xml and mysql.txt configuration
files. The following variables are recognized:

  DBHostName - The hostname (or IP address) which should be used to find the
               MySQL server.
  DBPort - The TCP/IP port number to use for the connection. This may have a
           value of 0, i.e. if the hostname is localhost or if the server is
           using the default MySQL port or the port specified in a MySQL
           options file.
  DBUserName - The database username to use when connecting to the server.
  DBPassword - The password to use when connecting to the server.
  DBName - The name of the database that contains the MythTV data.
  DBSchemaVer - The MythTV schema version of the database. This value will be
                used to create the backup filename, but only if the filename
                has not been specified using DBBackupFilename or the --filename
                argument.
  DBBackupDirectory - The directory in which the backup file should be
                      created. This directory may have been specially
                      configured by the user as the "DB Backups" storage
                      group. It is recommended that this directory be
                      used--especially in "common-use" scripts such as those
                      provided by distributions.
  DBBackupFilename - The name of the file in which the backup should be
                     created. Additional extensions may be added by this
                     script as required (i.e. adding an appropriate suffix,
                     such as ".gz", to the file if it is compressed). If the
                     filename recommended by mythbackend is used, it will be
                     displayed in the GUI messages provided for the user. If
                     the recommended filename is not used, the user will not be
                     told where to find the backup file. If no value is
                     provided, a filename using the default filename format
                     will be chosen.
  mysqldump        - The path (including filename) of the mysqldump executable.
  compress         - The command (including path, if necessary) to use to
                     compress the backup. Using gzip is significantly less
                     resource intensive on an SQL backup file than using bzip2,
                     at the cost of a slightly (about 33%) larger compressed
                     filesize, a difference which should be irrelevant at the
                     filesizes involved (especially when compared to the size
                     of recording files). If you decide to use another
                     compression algorithm, please ensure you test it
                     appropriately to verify it does not negatively affect
                     operation of your system. If no value is specified for
                     compress or if the value '$d_compress' is specified, the
                     script will first attempt to use the IO::Compress::Gzip
                     module to compress the backup file, but, if not available,
                     will run the command specified. Therefore, if
                     IO::Compress::Gzip is installed and functional, specifying
                     a value for compress is unnecessary. If neither approach
                     works, the backup file will be left uncompressed.
  rotate           - The number of backups to keep when rotating. To disable
                     rotation, specify -1. Backup rotation is performed by
                     identifying all files in DBBackupDirectory whose names
                     match the glob specified by rotateglob. It is critical
                     that the chosen backup filenames can be sorted properly
                     using an alphabetical sort. If using the default filename
                     format--which contains the DBSchemaVer--and you downgrade
                     MythTV and restore a backup from an older DBSchemaVer,
                     make sure you move the backups from the newer DBSchemaVer
                     out of the DBBackupDirectory or they may cause your new
                     backups to be deleted.
  rotateglob       - The sh-like glob used to identify files within
                     DBBackupDirectory to be considered for rotation. Be
                     very careful with the value--especially if using a
                     DBBackupDirectory that contains any files other than
                     backups.

RESOURCE FILE

The backup resource file specifies values using the same format as described
for the database information file, above, but is intended as a "permanent,"
user-created configuration file. The database information file is intended as
a "single-use" configuration file, often created automatically (i.e. by a
program, such as mythbackend, or a script). The backup resource file should be
placed at "~/.mythtv/backuprc" and given appropriate permissions. To be usable
by the script, it must be readable. However, it should be protected as
required--i.e. if the DBPassword is specified, it should be made readable only
by the owner.

When specifying a database information file, the resource file is parsed before
the database information file to prevent the resource file from overriding the
information in the database information file. When no database information
file is specified, the resource file is parsed after the MythTV configuration
files, but before the command-line arguments to allow the resource file to
override values in the configuration files and to allow command-line arguments
to override resource file defaults.

options:

--hostname [database hostname]

    The hostname (or IP address) which should be used to find the MySQL server.
    See DBHostName, above.

--port [database port]

    The TCP/IP port number to use for connection to the MySQL server. See
    DBPort, above.

--username [database username]

    The MySQL username to use for connection to the MySQL server. See
    DBUserName, above.

--name [database name]

    The name of the database containing the MythTV data. See DBName, above.

    Default: $d_db_name

--schemaver [MythTV database schema version]

    The MythTV schema version. See DBSchemaVer, above.

--directory [directory]

    The directory in which the backup file should be stored. See
    DBBackupDirectory, above.

--filename [database backup filename]

    The name to use for the database backup file. If not provided, a filename
    using a default format will be chosen. See DBBackupFilename, above.

--mysqldump [path]

    The path (including filename) of the mysqldump executable. See mysqldump
    in the DATABASE INFORMATION FILE description, above.

    Default: $d_mysqldump

--compress [path]

    The command (including path, if necessary) to use to compress the backup.
    See compress in the DATABASE INFORMATION FILE description, above.

    Default: $d_compress

--rotate [number]
    The number of backups to keep when rotating. To disable rotation, specify
    -1. See rotate in the DATABASE INFORMATION FILE description, above.

    Default: $d_rotate

--rotateglob [glob]
    The sh-like glob used to identify files within DBBackupDirectory to be
    considered for rotation. See rotateglob in the DATABASE INFORMATION FILE
    description, above.

    Default: $d_rotateglob

--backup_xmltvids
    Rather than creating a backup of the entire database, create a backup of
    xmltvids. This is useful when doing a full channel scan. The resulting
    backup is a series of SQL UPDATE statements that can be executed to set
    the xmltvid for channels whose callsign is the same before and after
    the scan. Note that the backup file will contain comments with additional
    channel information, which you can use to identify channels in case the
    callsign changes.

--help

    Show this help text.

--version

    Show version information.

--verbose

    Show what is happening.

--script_version | -v

    Show script version information. This is primarily useful for scripts
    or programs needing to parse the version information.

EOF
        }
        else
        {
            print "For detailed help:\n\n# $0 --help --help\n\n";
        }
        exit;
    }

    sub verbose
    {
        my $level = shift;
        my $error = 0;
        if ($level == $verbose_level_error)
        {
            $error = 1;
        }
        else
        {
            return unless ($debug >= $level);
        }
        print { $error ? STDERR : STDOUT } join("\n", @_), "\n";
    }

    sub print_configuration
    {
        verbose($verbose_level_debug,
                '',
                'Database Information:',
                "         DBHostName: $mysql_conf{'db_host'}",
                "             DBPort: $mysql_conf{'db_port'}",
                "         DBUserName: $mysql_conf{'db_user'}",
                '         DBPassword: ' .
                    ( $mysql_conf{'db_pass'} ? 'XXX' : '' ),
                  #  "$mysql_conf{'db_pass'}",
                "             DBName: $mysql_conf{'db_name'}",
                "        DBSchemaVer: $mysql_conf{'db_schemaver'}",
                "  DBBackupDirectory: $backup_conf{'directory'}",
                "   DBBackupFilename: $backup_conf{'filename'}");
        verbose($verbose_level_debug,
                '',
                'Executables:',
                "          mysqldump: $mysqldump",
                "           compress: $compress");
    }

    sub configure_environment
    {
        verbose($verbose_level_debug,
                '', 'Configuring environment:');

    # Get the user's login and home directory, so we can look for config files
        ($username, $homedir) = (getpwuid $>)[0,7];
        $username = $ENV{'USER'} if ($ENV{'USER'});
        $homedir  = $ENV{'HOME'} if ($ENV{'HOME'});
        if ($username && !$homedir)
        {
            $homedir = "/home/$username";
            if (!-e $homedir && -e "/Users/$username")
            {
                $homedir = "/Users/$username";
            }
        }
        verbose($verbose_level_debug,
                "  -    username: $username",
                "  -        HOME: $homedir");

    # Find the config directory
        $mythconfdir = $ENV{'MYTHCONFDIR'}
            ? $ENV{'MYTHCONFDIR'}
            : "$homedir/.mythtv"
            ;

        verbose($verbose_level_debug,
                "  - MYTHCONFDIR: $mythconfdir");
    }

# Though much of the configuration file parsing could be done by the MythTV
# Perl bindings, using them to retrieve database information is not appropriate
# for a backup script. The Perl bindings require the backend to be running and
# use UPnP for autodiscovery. Also, parsing the files "locally" allows
# supporting even the old MythTV database configuration file, mysql.txt.
    sub parse_database_information
    {
        my $file = shift;
        verbose($verbose_level_debug,
                "  - checking: $file");
        return 0 unless ($file && -e $file);
        verbose($verbose_level_debug,
                "     parsing: $file");
        open(CONF, $file) or die("\nERROR: Unable to read $file: $!".
                                 ', stopped');
        while (my $line = <CONF>)
        {
        # Cleanup
            next if ($line =~ m/^\s*#/);
            $line =~ s/^str //;
            chomp($line);
            $line =~ s/^\s+//;
            $line =~ s/\s+$//;
        # Split off the var=val pairs
            my ($var, $val) = split(/ *[\=\: ] */, $line, 2);
        # Also look for <var>val</var> from config.xml
            if ($line =~ m/<(\w+)>(.+)<\/(\w+)>$/ && $1 eq $3)
            {
                $var = $1; $val = $2;
            }
            next unless ($var && $var =~ m/\w/);
            if (($var eq 'Host') || ($var eq 'DBHostName'))
            {
                $mysql_conf{'db_host'} = $val;
            }
            elsif (($var eq 'Port') || ($var eq 'DBPort'))
            {
                $mysql_conf{'db_port'} = $val;
            }
            elsif (($var eq 'UserName') || ($var eq 'DBUserName'))
            {
                $mysql_conf{'db_user'} = $val;
            }
            elsif (($var eq 'Password') || ($var eq 'DBPassword'))
            {
                $mysql_conf{'db_pass'} = $val;
                $mysql_conf{'db_pass'} =~ s/&amp;/&/sg;
                $mysql_conf{'db_pass'} =~ s/&gt;/>/sg;
                $mysql_conf{'db_pass'} =~ s/&lt;/</sg;
            }
            elsif (($var eq 'DatabaseName') || ($var eq 'DBName'))
            {
                $mysql_conf{'db_name'} = $val;
            }
            elsif ($var eq 'DBSchemaVer')
            {
                $mysql_conf{'db_schemaver'} = $val;
            }
            elsif ($var eq 'DBBackupDirectory')
            {
                $backup_conf{'directory'} = $val;
            }
            elsif ($var eq 'DBBackupFilename')
            {
                $backup_conf{'filename'} = $val;
            }
            elsif ($var eq 'mysqldump')
            {
                $mysqldump = $val;
            }
            elsif ($var eq 'compress')
            {
                $compress = $val;
            }
            elsif ($var eq 'rotate')
            {
                $rotate = $val;
            }
            elsif ($var eq 'rotateglob')
            {
                $rotateglob = $val;
            }
        }
        close CONF;
        return 1;
    }

    sub read_mysql_txt
    {
    # Read the "legacy" mysql.txt file in use by MythTV. It could be in a
    # couple places, so try the usual suspects in the same order that mythtv
    # does in libs/libmyth/mythcontext.cpp
        my $found = 0;
        my $result = 0;
        my @mysql = ('/usr/local/share/mythtv/mysql.txt',
                     '/usr/share/mythtv/mysql.txt',
                     '/usr/local/etc/mythtv/mysql.txt',
                     '/etc/mythtv/mysql.txt',
                     $homedir ? "$homedir/.mythtv/mysql.txt"    : '',
                     'mysql.txt',
                     $mythconfdir ? "$mythconfdir/mysql.txt"    : '',
                    );
        foreach my $file (@mysql)
        {
            $found = parse_database_information($file);
            $result = $result + $found;
        }
        return $result;
    }

    sub read_resource_file
    {
        parse_database_information("$mythconfdir/backuprc");
    }

    sub apply_arguments
    {
        verbose($verbose_level_debug,
                '', 'Applying command-line arguments.');
        if ($db_hostname)
        {
            $mysql_conf{'db_host'} = $db_hostname;
        }
        if ($db_port)
        {
            $mysql_conf{'db_port'} = $db_port;
        }
        if ($db_username)
        {
            $mysql_conf{'db_user'} = $db_username;
        }
    # This script does not accept a database password on the command-line.
#        if ($db_password)
#        {
#            $mysql_conf{'db_pass'} = $db_password;
#        }
        if ($db_name)
        {
            $mysql_conf{'db_name'} = $db_name;
        }
        if ($db_schema_version)
        {
            $mysql_conf{'db_schemaver'} = $db_schema_version;
        }
        if ($backup_directory)
        {
            $backup_conf{'directory'} = $backup_directory;
        }
        if ($backup_filename)
        {
            $backup_conf{'filename'} = $backup_filename;
        }
    }

    sub read_config
    {
        my $result = 0;
    # If specified, use only the database information file
        if ($database_information_file)
        {
            verbose($verbose_level_debug,
                    '', 'Database Information File specified. Ignoring all'.
                    ' command-line arguments');
            verbose($verbose_level_debug,
                    '', 'Database Information File: '.
                    $database_information_file);
            unless (-T "$database_information_file")
            {
                verbose($verbose_level_always,
                        '', 'The argument you supplied for the database'.
                        ' information file is invalid.',
                        'If you were trying to specify a backup filename,'.
                        ' please use the --directory ',
                        'and --filename arguments.');
                die("\nERROR: Invalid database information file, stopped");
            }
        # When using a database information file, parse the resource file first
        # so it cannot override database information file settings
            read_resource_file;
            $result = parse_database_information($database_information_file);
            return $result;
        }

    # No database information file, so try the MythTV configuration files.
        verbose($verbose_level_debug,
                '', 'Parsing configuration files:');
    # Prefer the config.xml file
        my $file = $mythconfdir ? "$mythconfdir/config.xml" : '';
        $result = parse_database_information($file);
        if (!$result)
        {
        # Use the "legacy" mysql.txt file as a fallback
            $result = read_mysql_txt;
        }
    # Read the resource file next to override the config file information, but
    # to allow command-line arguments to override resource file "defaults"
        read_resource_file;
    # Apply the command-line arguments to override the information provided
    # by the config file(s).
        apply_arguments;
        return $result;
    }

    sub check_database_libs
    {
    # Try to load the DBI library if available (but don't require it)
        BEGIN
        {
            our $has_dbi = 1;
            eval 'use DBI;';
            if ($@)
            {
                $has_dbi = 0;
            }
        }
        verbose($verbose_level_debug,
                '', 'DBI is not installed.') if (!$has_dbi);
    # Try to load the DBD::mysql library if available (but don't
    # require it)
        BEGIN
        {
            our $has_dbd = 1;
            eval 'use DBD::mysql;';
            if ($@)
            {
                $has_dbd = 0;
            }
        }
        verbose($verbose_level_debug,
                '', 'DBD::mysql is not installed.') if (!$has_dbd);
        return ($has_dbi + $has_dbd);
    }

    sub check_database
    {
        if (!defined($dbh))
        {
            my $have_database_libs = check_database_libs;
            return 0 if ($have_database_libs < 2);
            $dbh = DBI->connect("dbi:mysql:".
                                "database=$mysql_conf{'db_name'}:".
                                "host=$mysql_conf{'db_host'}",
                                "$mysql_conf{'db_user'}",
                                "$mysql_conf{'db_pass'}",
                                { PrintError => 0 });
        }
        return 1;
    }

    sub create_backup_filename
    {
    # Create a default backup filename
        $backup_conf{'filename'} = $mysql_conf{'db_name'};
        if (!$backup_conf{'filename'})
        {
            $backup_conf{'filename'} = $d_db_name;
        }
        if ((!$mysql_conf{'db_schemaver'}) &&
            ($mysql_conf{'db_host'}) && ($mysql_conf{'db_name'}) &&
            ($mysql_conf{'db_user'}) && ($mysql_conf{'db_pass'}))
        {
        # If DBI is available, query the DB for the schema version
            if (check_database)
            {
                verbose($verbose_level_debug,
                        '', 'No DBSchemaVer specified, querying database.');
                my $query = 'SELECT data FROM settings WHERE value = ?';
                if (defined($dbh))
                {
                    my $sth = $dbh->prepare($query);
                    if ($sth->execute('DBSchemaVer'))
                    {
                        while (my @data = $sth->fetchrow_array)
                        {
                            $mysql_conf{'db_schemaver'} = $data[0];
                            verbose($verbose_level_debug,
                                    "Found DBSchemaVer:".
                                    " $mysql_conf{'db_schemaver'}.");
                        }
                    }
                    else
                    {
                        verbose($verbose_level_debug,
                                "Unable to retrieve DBSchemaVer from".
                                " database. Filename will not contain ",
                                "DBSchemaVer.");
                    }
                }
            }
            else
            {
                verbose($verbose_level_debug,
                        '', 'No DBSchemaVer specified.',
                        'DBI and/or DBD:mysql is not available. Unable'.
                        ' to query database to determine ',
                        'DBSchemaVer. DBSchemaVer will not be included'.
                        ' in backup filename.',
                        'Please ensure DBI and DBD::mysql are'.
                        ' installed.');
            }
        }
        if ($mysql_conf{'db_schemaver'})
        {
            $backup_conf{'filename'} .= '-'.$mysql_conf{'db_schemaver'};
        }
    # Format the time using localtime data so we don't have to bring in
    # another dependency.
        my @timeData = localtime(time);
        $backup_conf{'filename'} .= sprintf('-%04d%02d%02d%02d%02d%02d.sql',
                                            ($timeData[5] + 1900),
                                            ($timeData[4] + 1),
                                            $timeData[3], $timeData[2],
                                            $timeData[1], $timeData[0]);
    }

    sub check_backup_directory
    {
        my $result = 0;
        if ($backup_conf{'directory'})
        {
            $result = 1;
        }
        elsif (check_database)
    # If DBI is available, query the DB for the backup directory
        {
            verbose($verbose_level_debug,
                    '', 'No DBBackupDirectory specified, querying database.');
            my $query = 'SELECT dirname, hostname FROM storagegroup '.
                        ' WHERE groupname = ?';
            if (defined($dbh))
            {
                my $directory;
                my $sth = $dbh->prepare($query);
                if ($sth->execute('DB Backups'))
                {
                # We don't know the hostname associated with this host, and
                # since it's not worth parsing the mysql.txt/config.xml
                # LocalHostName (unique identifier), with fallback to the
                # system hostname, and handling issues along the way, just look
                # for any available DB Backups directory and, if none are
                # usable, look for a Default group directory
                    while (my @data = $sth->fetchrow_array)
                    {
                        $directory = $data[0];
                        if (-d $directory && -w $directory)
                        {
                            $backup_conf{'directory'} = $directory;
                            verbose($verbose_level_debug,
                                    "Found DB Backups directory:".
                                    " $backup_conf{'directory'}.");
                            $result = 1;
                            $sth->finish;
                            last;
                        }
                    }
                }
                if ($result == 0 && $sth->execute('Default'))
                {
                    while (my @data = $sth->fetchrow_array)
                    {
                        $directory = $data[0];
                        if (-d $directory && -w $directory)
                        {
                            $backup_conf{'directory'} = $directory;
                            verbose($verbose_level_debug,
                                    "Found Default directory:".
                                    " $backup_conf{'directory'}.");
                            $result = 1;
                            $sth->finish;
                            last;
                        }
                    }
                }
            }
            if ($result == 0)
            {
                verbose($verbose_level_debug,
                        "Unable to retrieve DBBackupDirectory from".
                        " database.");
            }
        }
        return $result;
    }

    sub check_config
    {
        verbose($verbose_level_debug,
                '', 'Checking configuration.');
    # Check directory/filename
        if (!check_backup_directory)
        {
            print_configuration;
            die("\nERROR: DBBackupDirectory not specified, stopped");
        }
        if ((!-d $backup_conf{'directory'}) ||
            (!-w $backup_conf{'directory'}))
        {
            print_configuration;
            verbose($verbose_level_error,
                    '', 'ERROR: DBBackupDirectory is not a directory or is '.
                    'not writable. Please specify',
                    '       a directory in your database information file'.
                    ' using DBBackupDirectory.',
                    '       If not using a database information file,'.
                    ' please specify the ',
                    '        --directory command-line option.');
            die("\nInvalid backup directory, stopped");
        }
        if (!$backup_conf{'filename'})
        {
            if ($backup_xmltvids)
            {
                my $file = 'mythtv_xmltvid_backup';
            # Format the time using localtime data so we don't have to bring in
            # another dependency.
                my @timeData = localtime(time);
                $file .= sprintf('-%04d%02d%02d%02d%02d%02d.sql',
                                 ($timeData[5] + 1900),
                                 ($timeData[4] + 1),
                                 $timeData[3], $timeData[2],
                                 $timeData[1], $timeData[0]);
                $backup_conf{'filename'} = $file;
            }
            else
            {
                create_backup_filename;
            }
        }
        if ( -e "$backup_conf{'directory'}/$backup_conf{'filename'}")
        {
            verbose($verbose_level_error,
                    '', 'ERROR: The specified file already exists.');
            die("\nInvalid backup filename, stopped");
        }
        if (!$mysql_conf{'db_name'})
        {
            verbose($verbose_level_debug,
                    '', "WARNING: DBName not specified. Using $d_db_name");
            $mysql_conf{'db_name'} = $d_db_name;
        }
    # Though the script will attempt a backup even if no other database
    # information is provided (i.e. using "defaults" from the MySQL options
    # file, warning the user that some "normally-necessary" information is not
    # provided may be nice.
        return if (!$debug);
        if (!$mysql_conf{'db_host'})
        {
            verbose($verbose_level_debug,
                    '', 'WARNING: DBHostName not specified.',
                    '         Assuming it is specified in the MySQL'.
                    ' options file.');
        }
        if (!$mysql_conf{'db_user'})
        {
            verbose($verbose_level_debug,
                    '', 'WARNING: DBUserName not specified.',
                    '         Assuming it is specified in the MySQL'.
                    ' options file.');
        }
        if (!$mysql_conf{'db_pass'})
        {
            verbose($verbose_level_debug,
                    '', 'WARNING: DBPassword not specified.',
                    '         Assuming it is specified in the MySQL'.
                    ' options file.');
        }
    }

    sub create_defaults_extra_file
    {
        return '' if (!$mysql_conf{'db_pass'});
        verbose($verbose_level_debug,
                '', "Attempting to use supplied password for $mysqldump.",
                'Any [client] or [mysqldump] password specified in the MySQL'.
                ' options file will',
                'take precedence.');
    # Let tempfile handle unlinking on exit so we don't have to verify that the
    # file with $filename is the file we created
        my ($fh, $filename) = tempfile(UNLINK => 1);
    # Quote the password if it contains # or whitespace or quotes.
    # Quoting of values in MySQL options files is only supported on MySQL
    # 4.0.16 and above, so only quote if required.
        my $quote = '';
        my $safe_password = $mysql_conf{'db_pass'};
        if ($safe_password =~ /[#'"\s]/)
        {
            $quote = "'";
            $safe_password =~ s/'/\\'/g;
        }
        print $fh "[client]\npassword=${quote}${safe_password}${quote}\n".
                  "[mysqldump]\npassword=${quote}${safe_password}${quote}\n";
        return $filename;
    }

    sub do_xmltvid_backup
    {
        my $exit = 1;
        if (check_database)
        {
            my ($chanid, $channum, $callsign, $name, $xmltvid);
            my $query = "  SELECT chanid, channum, callsign, name, xmltvid".
                        "    FROM channel ".
                        "ORDER BY CAST(channum AS SIGNED),".
                        "         CAST(SUBSTRING(channum".
                        "                        FROM (1 +".
                        "                              LOCATE('_', channum) +".
                        "                              LOCATE('-', channum) +".
                        "                              LOCATE('#', channum) +".
                        "                              LOCATE('.', channum)))".
                        "              AS SIGNED)";
            my $sth = $dbh->prepare($query);
            verbose($verbose_level_debug,
                    '', 'Querying database for xmltvid information.');
            my $file = "$backup_conf{'directory'}/$backup_conf{'filename'}";
            open BACKUP, '>', $file or die("\nERROR: Unable to open".
                                           " $file: $!, stopped");
            for ($section = 0; $section < 2; $section++)
            {
                if ($sth->execute)
                {
                    while (my @data = $sth->fetchrow_array)
                    {
                        $chanid = $data[0];
                        $channum = $data[1];
                        $callsign = $data[2];
                        $name = $data[3];
                        $xmltvid = $data[4];
                        verbose($verbose_level_debug,
                                "Found channel: $chanid, $channum, $callsign,".
                                " $name, $xmltvid.") if ($section == 0);
                        if ($xmltvid && $callsign)
                        {
                            if ($section == 0)
                            {
                                print BACKUP "-- Start Channel Data\n".
                                             "-- ID: '$chanid'\n".
                                             "-- Number: '$channum'\n".
                                             "-- Callsign: '$callsign'\n".
                                             "-- Name: '$name'\n".
                                             "-- XMLTVID: '$xmltvid'\n".
                                             "-- End Channel Data\n";
                                print BACKUP "UPDATE channel".
                                             " SET xmltvid = '$xmltvid'".
                                             " WHERE callsign = '$callsign'".
                                             ";\n";
                            }
                            else
                            {
                                print BACKUP "UPDATE channel".
                                             " SET xmltvid = '$xmltvid'".
                                             " WHERE channum = '$channum'".
                                             " AND name = '$name';\n";
                            }
                        }
                    }
                    if ($section == 0)
                    {
                        verbose($verbose_level_debug,
                                '', 'Successfully backed up xmltvid'.
                                ' information.'.
                                '', '', 'Creating alternate format backup.');
                        print BACKUP "\n\n/* Alternate format */\n".
                                     "/*\n";
                    }
                    else
                    {
                        print BACKUP "*/\n";
                        verbose($verbose_level_debug,
                                'Successfully created alternate format'.
                                ' backup.');
                    }
                    $exit = 0;
                }
                else
                {
                    verbose($verbose_level_error,
                            '', 'ERROR: Unable to retrieve xmltvid information'.
                            ' from database.');
                    die("\nError retrieving xmltvid information, stopped");
                }
            }
            close BACKUP;
        }
        else
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to backup xmltvids without Perl'.
                    ' database libraries.',
                    '       Please ensure the Perl DBI and DBD::mysql'.
                    ' modules are installed.');
            die("\nPerl database libraries missing, stopped");
        }
        return $exit;
    }

# This subroutine performs limited checking of a command and untaints the
# command (and the environment) if the command seems to use an absolute path
# containing no . or .. references or if it's a simple command name referencing
# an executable in a "normal" directory for binaries.  It should only be called
# after careful consideration of the effects of doing so and of whether it
# makes sense to override taint-mode runtime checking of the value.
    sub untaint_command
    {
        my $command = shift;
        my $allow_untaint = 0;
    # Only allow directories from @d_allowed_paths that exist in the PATH
        if (!defined(@allowed_paths))
        {
            foreach my $path (split(/:/, $old_env_path))
            {
                if (grep(/^$path$/, @d_allowed_paths))
                {
                    push(@allowed_paths, $path);
                }
            }
            verbose($verbose_level_debug + 2,
                    '', 'Allowing paths:', @allowed_paths,
                    'From PATH: '.$old_env_path);
        }

        verbose($verbose_level_debug + 2, '', 'Verifying command: '.$command);
        if ($command =~ /^\//)
        {
            verbose($verbose_level_debug + 2, ' - Command starts with /.');
            if (! ($command =~ /\/\.+\//))
            {
                verbose($verbose_level_debug + 2,
                        ' - Command does not contain dir refs.');
                if (-e "$command" && -f "$command" && -x "$command")
                {
                # Seems to be a valid executable specified with a path starting
                # with / and having no current/previous directory references
                    verbose($verbose_level_debug + 2,
                            'Unmodified command meets untaint requirements.',
                            $command);
                    $allow_untaint = 1;
                }
            }
        }
        else
        {
            foreach my $path (@allowed_paths)
            {
                if (-e "$path/$command" && -f "$path/$command" &&
                    -x "$path/$command")
                {
                # Seems to be a valid executable in a "normal" directory for
                # binaries
                    $command = "$path/$command";
                    verbose($verbose_level_debug + 2,
                            'Command seems to be a simple command in a'.
                            ' normal directory for binaries: '.$command);
                    $allow_untaint = 1;
                }
            }
        }
        if ($allow_untaint)
        {
            if ($command =~ /^(.*)$/)
            {
                verbose($verbose_level_debug + 1,
                        'Untainting command: '.$command);
                $command = $1;
                $ENV{'PATH'} = '';
                $is_env_tainted = 0;
            }
        }
        return $command;
    }

# This subroutine performs limited checking of file or directory paths and
# untaints the path if it seems to be an absolute path to a normal file or
# directory and contains no . or .. references.  It should only be called after
# careful consideration of the effects of doing so and of whether it makes
# sense to override taint-mode runtime checking of the value.
    sub untaint_path
    {
        my $path = shift;
        verbose($verbose_level_debug + 2, '', 'Verifying path: '.$path);
        if ($path =~ /^\//)
        {
            verbose($verbose_level_debug + 2, ' - Path starts with /.');
            if (! ($path =~ /\/\.+\//))
            {
                verbose($verbose_level_debug + 2,
                        ' - Path contains no dir refs.');
                if (-e "$path" && (-f "$path" || -d "$path"))
                {
                # Seems to be a file or directory path starting with / and
                # having no current/previous directory references
                    if ($path =~ /^(.*)$/)
                    {
                        verbose($verbose_level_debug + 1,
                                'Untainting path: '.$path);
                        $path = $1;
                    }
                }
            }
        }
        return $path;
    }

# This subroutine does absolutely no data checking.  It blindly accepts a
# possibly-tainted value and "untaints" it.  It should only be called after
# careful consideration of the effects of doing so and of whether it makes
# sense to override taint-mode runtime checking of the value.
    sub untaint_data
    {
        my $value = shift;
        if ($value =~ /^(.*)$/)
        {
            verbose($verbose_level_debug + 1, 'Untainting data: '.$value);
            $value = $1;
        }
        return $value;
    }

    sub reset_environment
    {
        if (!$is_env_tainted)
        {
            $is_env_tainted = 1;
            $ENV{'PATH'} = $old_env_path;
        }
    }

    sub do_backup
    {
        my $defaults_extra_file = create_defaults_extra_file;
        my $host_arg = '';
        my $port_arg = '';
        my $user_arg = '';
        if ($defaults_extra_file)
        {
            $defaults_arg = " --defaults-extra-file='$defaults_extra_file'";
        }
        else
        {
            $defaults_arg = '';
        }
    # For users running in environments where taint mode is activated (i.e.
    # running mythtv-setup or mythbackend as root), executing a command line
    # built with tainted data will fail.  Therefore, try to untaint data if it
    # meets certain basic requirements.
        my $safe_mysqldump = $mysqldump;
        $safe_mysqldump = untaint_command($safe_mysqldump);
        $safe_mysqldump =~ s/'/'\\''/sg;
        $mysql_conf{'db_name'} = untaint_data($mysql_conf{'db_name'});
        $mysql_conf{'db_host'} = untaint_data($mysql_conf{'db_host'});
        $mysql_conf{'db_port'} = untaint_data($mysql_conf{'db_port'});
        $mysql_conf{'db_user'} = untaint_data($mysql_conf{'db_user'});
        $backup_conf{'directory'} = untaint_path($backup_conf{'directory'});
    # Can't use untaint_path because the filename is not a full path and the
    # file doesn't yet exist, anyway
        $backup_conf{'filename'} =~ s/'/'\\''/g;
        $backup_conf{'filename'} = untaint_data($backup_conf{'filename'});
        my $output_file = "$backup_conf{'directory'}/$backup_conf{'filename'}";
        $output_file =~ s/'/'\\''/sg;
    # Create the args for host, port, and user, shell-escaping values, as
    # necessary.
        my $safe_db_name = $mysql_conf{'db_name'};
        $safe_db_name =~ s/'/'\\''/g;
        my $safe_string;
        if ($mysql_conf{'db_host'})
        {
            $safe_string = $mysql_conf{'db_host'};
            $safe_string =~ s/'/'\\''/g;
            $host_arg = " --host='$safe_string'";
        }
        if ($mysql_conf{'db_port'} > 0)
        {
            $safe_string = $mysql_conf{'db_port'};
            $safe_string =~ s/'/'\\''/g;
            $port_arg = " --port='$safe_string'";
        }
        if ($mysql_conf{'db_user'})
        {
            $safe_string = $mysql_conf{'db_user'};
            $safe_string =~ s/'/'\\''/g;
            $user_arg = " --user='$safe_string'";
        }

    # Use redirects to capture stderr (for debug) and send stdout (the backup)
    # to a file
        my $command = "'${safe_mysqldump}'${defaults_arg}${host_arg}".
                      "${port_arg}${user_arg} --add-drop-table --add-locks ".
                      "--allow-keywords --complete-insert --extended-insert ".
                      "--lock-tables --no-create-db --quick --add-drop-table ".
                      "'$safe_db_name' 2>&1 1>'$output_file'";
        verbose($verbose_level_debug,
                '', 'Executing command:', $command);
        my $result = `$command`;
        my $exit = $? >> 8;
        verbose($verbose_level_debug,
                '', "$mysqldump exited with status: $exit");
        verbose($verbose_level_debug,
                "$mysqldump output:", $result) if ($exit);
        reset_environment;
        return $exit;
    }

    sub compress_backup
    {
        if (!-e "$backup_conf{'directory'}/$backup_conf{'filename'}")
        {
            verbose($verbose_level_debug,
                    '', 'Unable to find backup file to compress');
            return 1;
        }
        my $result = 0;
        verbose($verbose_level_debug,
                '', 'Attempting to compress backup file.');
        if ($d_compress eq $compress)
        {
        # Try to load the IO::Compress::Gzip library if available (but don't
        # require it)
            BEGIN
            {
                our $has_compress_gzip = 1;
                # Though this does nothing, it prevents an invalid "only used
                # once" warning that occurs for users without IO::Compress
                # installed.
                undef $GzipError;
                eval 'use IO::Compress::Gzip qw(gzip $GzipError);';
                if ($@)
                {
                    $has_compress_gzip = 0;
                }
            }
            if (!$has_compress_gzip)
            {
                verbose($verbose_level_debug,
                        " - IO::Compress::Gzip is not installed.");
            }
            else
            {
                if (-e "$backup_conf{'directory'}/$backup_conf{'filename'}.gz")
                {
                    verbose($verbose_level_debug,
                            '', 'A file whose name is the backup filename'.
                            ' with the \'.gz\' extension already',
                            'exists. Leaving backup uncompressed.');
                    return 1;
                }
                verbose($verbose_level_debug,
                        " - Compressing backup file with IO::Compress::Gzip.");
                $result = gzip(
                   "$backup_conf{'directory'}/$backup_conf{'filename'}" =>
                   "$backup_conf{'directory'}/$backup_conf{'filename'}.gz");
                if ((defined($result)) &&
                    (-e "$backup_conf{'directory'}/".
                        "$backup_conf{'filename'}.gz"))
                {
                # For users running in environments where taint mode is
                # activated (i.e.  running mythtv-setup or mythbackend as
                # root), unlinking a file whose path is built with tainted data
                # will fail.  Therefore, try to untaint the path if it meets
                # certain basic requirements.
                    my $uncompressed_file = $backup_conf{'directory'}."/".
                                            $backup_conf{'filename'};
                    $uncompressed_file = untaint_path($uncompressed_file);
                    $uncompressed_file =~ s/'/'\\''/sg;
                    verbose($verbose_level_debug + 2,
                            "Unlinking uncompressed file: $uncompressed_file");
                    unlink "$uncompressed_file";
                    $backup_conf{'filename'} = "$backup_conf{'filename'}.gz";
                    verbose($verbose_level_debug,
                            '', 'Successfully compressed backup to file:',
                            "$backup_conf{'directory'}/".
                            "$backup_conf{'filename'}");
                    return 0;
                }
                verbose($verbose_level_debug,
                        "   Error: $GzipError");
            }
        }
    # Try to compress the file with the compress binary.
        verbose($verbose_level_debug,
                " - Compressing backup file with $compress.");
        my $backup_path = "$backup_conf{'directory'}/$backup_conf{'filename'}";
    # For users running in environments where taint mode is activated (i.e.
    # running mythtv-setup or mythbackend as root), executing a command line
    # built with tainted data will fail.  Therefore, try to untaint data if it
    # meets certain basic requirements.
        $compress = untaint_command($compress);
        $compress =~ s/'/'\\''/sg;
        $backup_path = untaint_path($backup_path);
        $backup_path =~ s/'/'\\''/sg;
        my $command = "'$compress' '$backup_path' 2>&1";
        verbose($verbose_level_debug,
                '', 'Executing command:', $command);
        my $output = `$command`;
        my $exit = $? >> 8;
        verbose($verbose_level_debug,
                '', "$compress exited with status: $exit");
        if ($exit)
        {
            verbose($verbose_level_debug,
                    "$compress output:", $output);
        }
        else
        {
            $backup_conf{'filename'} = "$backup_conf{'filename'}.gz";
        }
        reset_environment;
        return $exit;
    }

    sub rotate_backups
    {
        if (($rotate < 1) || (!defined($rotateglob)) || (!$rotateglob))
        {
            verbose($verbose_level_debug,
                    '', 'Backup file rotation disabled.');
            return 0;
        }
        verbose($verbose_level_debug,
                '', 'Rotating backups.');
        verbose($verbose_level_debug,
                '', 'Searching for files matching pattern:',
                "$backup_conf{'directory'}/$rotateglob");
        my @files = <$backup_conf{'directory'}/$rotateglob>;
        my @sorted_files = sort { lc($a) cmp lc($b) } @files;
        my $num_files = @sorted_files;
        verbose($verbose_level_debug,
                " - Found $num_files matching files.");
        $num_files = $num_files - $rotate;
        $num_files = 0 if ($num_files < 0);
        verbose($verbose_level_debug,
                '', "Deleting $num_files and keeping (up to) $rotate backup".
                ' files.');
        my $index = 0;
        foreach my $file (@sorted_files)
        {
            if ($index++ < $num_files)
            {
                if ($file eq
                    "$backup_conf{'directory'}/$backup_conf{'filename'}")
                {
                # This is the just-created backup. Warn the user that older
                # backups with newer schema versions may cause rotation to
                # fail.
                    verbose($verbose_level_debug,
                            '', 'WARNING: You seem to have reverted to an'.
                            ' older database schema version.',
                            'You should move all backups from newer schema'.
                            ' versions to another directory or',
                            'delete them to prevent your new backups from'.
                            ' being deleted on rotation.', '');
                    verbose($verbose_level_debug,
                            " - Keeping backup file: $file");

                }
                else
                {
                    verbose($verbose_level_debug,
                            " - Deleting old backup file: $file");
                # For users running in environments where taint mode is
                # activated (i.e.  running mythtv-setup or mythbackend as
                # root), unlinking a file whose path is built with tainted data
                # will fail.  Therefore, try to untaint the path if it meets
                # certain basic requirements.
                    $file = untaint_path($file);
                    $file =~ s/'/'\\''/sg;
                    unlink "$file";
                }
            }
            else
            {
                verbose($verbose_level_debug,
                        " - Keeping backup file: $file");
            }
        }
        return 1;
    }

##############################################################################
# Main functionality
##############################################################################

# The first argument after option parsing, if it exists, should be a database
# information file.
    $database_information_file = shift;

    configure_environment;
    read_config;
    check_config;

    print_configuration;

    my $status = 1;
    if ($backup_xmltvids)
    {
        $status = do_xmltvid_backup;
    }
    else
    {
        $status = do_backup;
        if (!$status)
        {
            compress_backup;
            rotate_backups;
        }
    }

    $dbh->disconnect if (defined($dbh));

    exit $status;

