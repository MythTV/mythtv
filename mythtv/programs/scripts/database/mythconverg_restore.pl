#!/usr/bin/perl -w
#
# mythconverg_restore.pl
#
# Restores a backup of the MythTV database.
#
# For details, see:
#   mythconverg_restore.pl --help

# Includes
    use Getopt::Long;
    use File::Temp qw/ tempfile /;

# Script info
    $NAME           = 'MythTV Database Restore Script';
    $VERSION        = '1.0.18';

# Some variables we'll use here
    our ($username, $homedir, $mythconfdir, $database_information_file);
    our ($partial_restore, $with_plugin_data, $restore_xmltvids);
    our ($mysql_client, $uncompress, $drop_database, $create_database);
    our ($change_hostname, $old_hostname, $new_hostname);
    our ($usage, $debug, $show_version, $show_version_script, $dbh);
    our ($d_mysql_client, $d_db_name, $d_uncompress);
    our ($db_hostname, $db_port, $db_username, $db_name, $db_schema_version);
# This script does not accept a database password on the command-line.
# Any packager who enables the functionality should modify the --help output.
#    our ($db_password);
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

# Debug levels
    $verbose_level_always = 0;
    $verbose_level_debug = 1;
    $verbose_level_error = 255;

# Defaults
    $d_db_name       = 'mythconverg';
    $d_mysql_client  = 'mysql';
    $d_uncompress    = 'gzip -d';

# Provide default values for GetOptions
    $mysql_client    = $d_mysql_client;
    $uncompress      = $d_uncompress;
    $debug           = 0;
    $new_hostname    = '';
    $old_hostname    = '';

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
               'partial_restore|new_hardware|'.
               'partial-restore|new-hardware'       => \$partial_restore,
               'with_plugin_data|plugin_data|'.
               'with-plugin-data|plugin-data'       => \$with_plugin_data,
               'restore_xmltvids|'.
               'restore-xmltvids|xmltvids'          => \$restore_xmltvids,
               'mysql_client|mysql-client|client=s' => \$mysql_client,
               'uncompress=s'                       => \$uncompress,
               'drop_database|drop_db|'.
               'drop-database|drop-db'              => \$drop_database,
               'create_database|create_db|mc_sql|'.
               'create-database|create-db|mc-sql'   => \$create_database,
               'change_hostname|change-hostname'    => \$change_hostname,
               'new_hostname|new-hostname=s'        => \$new_hostname,
               'old_hostname|old-hostname=s'        => \$old_hostname,
               'usage|help|h+'                      => \$usage,
               'version'                            => \$show_version,
               'script_version|script-version|v'    => \$show_version_script,
               'verbose|debug|d+'                   => \$debug
              );

    $partial_restore ||= $restore_xmltvids;

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

Restores a backup of the MythTV database.

QUICK START:

Create a file ~/.mythtv/backuprc with a single line,
"DBBackupDirectory=/home/mythtv" (no quotes). For example:

# echo "DBBackupDirectory=/home/mythtv" > ~/.mythtv/backuprc

To do a full restore:
Ensure you have an empty database. If you are replacing an existing database,
you must first drop the old database. You may do this using the mysql client
executable by issuing the command:

# mysql -umythtv -p mythconverg -e "DROP DATABASE IF EXISTS mythconverg;"

(fix the database name and username, as required). Then, execute the mc.sql
script as described in the MythTV HOWTO ( http://www.mythtv.org/docs/ ) to
prepare a new (empty) database. Alternatively, you may specify the
--drop_database and/or --create_database arguments to automatically drop and/or
create the database for you (see the command-line argument descriptions in the
detailed help for more information).

Then, run this script to restore the most-recent backup in the directory
specified in ~/.mythtv/backuprc . Use the --verbose argument to see what is
happening:

# $0 --verbose

or specify a backup file with:

# $0 --directory=/path/to/backups/ --filename=backup_file.sql.gz --verbose

(You may leave out the --directory argument if you've specified the directory
in the ~/.mythtv/backuprc .)

Once the restore completes successfully, you may start mythtv-setup or
mythbackend. If you restored a backup from an older version of MythTV,
mythtv-setup will upgrade the database for you.

To change the hostname of a MythTV frontend or backend:

Ensure that the database exists (restore an old database, as above, if
necessary) and execute the following command, replacing "XXXX" and "YYYY"
with appropriate values for the old and new hostnames, respectively:

# $0 --change_hostname --old_hostname="XXXX" --new_hostname="YYYY"

To restore xmltvids:

Ensure you have a ~/.mythtv/backuprc file, as described above, and execute this
script with the --restore_xmltvids argument.

# $0 --restore_xmltvids

EOF

        if ($usage > 1)
        {
            print <<EOF;
DETAILED DESCRIPTION:

This script is used to restore a backup of the MythTV database (as created by
the mythconverg_backup.pl script). It can be called with a single command-line
argument specifying the name of a "database information file" (see DATABASE
INFORMATION FILE, below), which contains sufficient information about the
database and the backup to allow the script to restore a backup without needing
any additional configuration files. In this mode, all other MythTV
configuration files (including config.xml, mysql.txt) are ignored, but the
backup resource file (see RESOURCE FILE, below) and the MySQL option files
(i.e. /etc/my.cnf or ~/.my.cnf) will be honored.

The script can also be called using command-line arguments to specify the
required information. If no database information file is specified, the script
will attempt to determine the appropriate configuration by using the MythTV
configuration file(s) (preferring config.xml, but falling back to mysql.txt if
no config.xml exists). Once the MythTV configuration file has been parsed, the
backup resource file (see RESOURCE FILE, below) will be parsed, then
command-line arguments will be applied (thus overriding any values determined
from the configuration files).

The only information required by the script is the directory in which the
backup exists (the script will attempt to find the most current backup file,
based on the filename). Therefore, when using a database information file, the
DBBackupDirectory should be specified, or if running manually, the --directory
command-line argument should be specified. The DBBackupDirectory may be
specified in a backup resource file (see RESOURCE FILE, below). If the
specified directory is not readable, the script will terminate. Likewise, if
the backup file cannot be read, the script will terminate.

If the database name is not specified, the script will attempt to use the
MythTV default database name, $d_db_name. Note that the same is not true for
the database username and database password. These must be explicitly
specified. The password must be specified in a database information file, a
backup resource file, or a MySQL options file. The username may be specified
the same way or may be specified using a command-line argument if not using a
database information file.

If attempting to perform a full restore, the database must be empty (no
tables). To automatically drop any existing database and create an empty
database, specify the --drop_database and the --create_database arguments.

If you have a corrupt database, you may be able to recover some information
using a partial restore. To do a partial restore, you must have a
fully-populated database schema (but without the data you wish to import) from
the version of MythTV used to create the backup. You may create and populate
the database by running the mc.sql script (see the description of the
--create_database argument) to create the database. Then, start and exit
mythtv-setup to populate the database. And, finally, do the partial restore
with:

# $0 --partial_restore

Include the --with_plugin_data argument if you would like to keep the data used
by MythTV plugins.  Note that this approach cannot be used to "merge"
databases from different MythTV databases nor to import recordings from other
MythTV databases.

If you would like to do a partial/new-hardware restore and have upgraded
MythTV, you must first do a full restore, then start and exit mythtv-setup (to
upgrade the database), then create a backup, then drop the database, then
follow the instructions for doing a partial restore with the new (upgraded)
backup file.

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
  partial_restore  - Do a partial restore (as would be required when setting
                     up MythTV on new hardware) of only the MythTV recordings
                     and recording rules.
  with_plugin_data - When doing a partial restore, include plugin data.
                     Ignored, unless the --partial_restore argument is given.i
                     Note that you will still need to configure all plugins
                     after the restore completes.
  mysql_client     - The path (including filename) of the mysql client
                     executable.
  uncompress       - The command (including path, if necessary) to use to
                     uncompress the backup. If you specify an uncompress
                     program, the backup file will be assumed to be compressed,
                     so the command will be run on the file regardless.
                     If no value is specified for uncompress or if the value
                     '$d_uncompress' or 'gunzip' is specified, the script will
                     check to see if the file is actually a gzip-compressed
                     file, and if so, will first attempt to use the
                     IO::Uncompress::Gunzip module to uncompress the backup
                     file, but, if not available, will run the command
                     specified. Therefore, if IO::Uncompress::Gunzip is
                     installed and functional, specifying a value for
                     uncompress is unnecessary.

RESOURCE FILE

The backup resource file specifies values using the same format as described
for the database information file, above, but is intended as a "permanent,"
user-created configuration file. The database information file is intended as a
"single-use" configuration file, often created automatically (i.e. by a
program, such as a script). The backup resource file should be placed at
"~/.mythtv/backuprc" and given appropriate permissions. To be usable by the
script, it must be readable. However, it should be protected as required--i.e.
if the DBPassword is specified, it should be made readable only by the owner.

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

--partial_restore

    Do a partial restore (as would be required when setting up MythTV on new
    hardware) of only the MythTV recordings and recording rules.

--with_plugin_data

    When doing a partial restore, include plugin data. Ignored, unless the
    --partial_restore argument is given. Note that you will still need to
    configure all plugins after the restore completes.

--restore_xmltvids

    Restore channel xmltvids from a backup created with
    mythconverg_backup.pl --backup_xmltvids

--mysql_client [path]

    The path (including filename) of the mysql client executable. See
    mysql_client in the DATABASE INFORMATION FILE description, above.

    Default: $d_mysql_client

--uncompress [path]

    The command (including path, if necessary) to use to uncompress the
    backup. See uncompress in the DATABASE INFORMATION FILE description, above.

    Default: $d_uncompress

--drop_database

    If specified, and if the database already exists, the script will attempt
    to drop the database. This argument may only be used when the
    --create_database argument is also specified (see below).

--create_database

    If specified, and if the database does not exist or the --drop_database
    argument is specified, the script will attempt to create the initial
    database. Note that database creation requires a properly configured MySQL
    user and permissions.  See, also, the MythTV HOWTO (
    http://www.mythtv.org/docs/ ) for details on "Setting up the initial
    database."

--change_hostname

    Specifies that the script should change the hostname of a MythTV frontend
    or backend in the database rather than restore a database backup. It is
    critical that no MythTV frontends or backends are running when a hostname
    is changed.

--new_hostname

    Specifies the new hostname. The new_hostname is only used when the
    --change_hostname argument is specified.

--old_hostname

    Specifies the old hostname. The old_hostname is only used when the
    --change_hostname argument is specified.

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
                "   DBBackupFilename: $backup_conf{'filename'}",
                '      drop_database: '.($drop_database ? 'yes' : 'no'),
                '    create_database: '.($create_database ? 'yes' : 'no'));
        verbose($verbose_level_debug,
                '',
                'Executables:',
                "       mysql_client: $mysql_client",
                "         uncompress: $uncompress");
        verbose($verbose_level_debug,
                '',
                'Miscellaneous:',
                '    partial_restore: '.($partial_restore ? 'yes' : 'no'));
        if ($partial_restore)
        {
            verbose($verbose_level_debug,
                    '   with_plugin_data: '.($with_plugin_data ?
                                                'yes' : 'no'));
        }
        verbose($verbose_level_debug,
                '   restore_xmltvids: '.($restore_xmltvids ? 'yes' : 'no'),
                '    change_hostname: '.($change_hostname ? 'yes' : 'no'));
        if ($change_hostname)
        {
            verbose($verbose_level_debug,
                    '     - old_hostname: '.$old_hostname,
                    '     - new_hostname: '.$new_hostname);
        }
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
            elsif ($var eq 'partial_restore')
            {
                $partial_restore = $val;
            }
            elsif ($var eq 'with_plugin_data')
            {
                $with_plugin_data = $val;
            }
            elsif ($var eq 'mysql_client')
            {
                $mysql_client = $val;
            }
            elsif ($var eq 'uncompress')
            {
                $uncompress = $val;
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

    sub create_defaults_extra_file
    {
        return '' if (!$mysql_conf{'db_pass'});
        verbose($verbose_level_debug,
                '', "Attempting to use supplied password for $mysql_client".
                ' command-line client.',
                'Any [client] or [mysql] password specified in the MySQL'.
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

    sub check_file_config
    {
        if (!$backup_conf{'directory'})
        {
            if (!$backup_conf{'filename'} || (!-r "/$backup_conf{'filename'}"))
            {
                print_configuration;
                die("\nERROR: DBBackupDirectory not specified, stopped");
            }
        # The user must have specified an absolute path for the
        # DBBackupFilename. Though this is not how the script is meant to be
        # used, allow it.
            $backup_conf{'directory'} = '';
        }
        elsif (!-d $backup_conf{'directory'})
        {
            print_configuration;
            verbose($verbose_level_error,
                    '', 'ERROR: DBBackupDirectory is not a directory. Please'.
                    ' specify a directory in',
                    '       your database information file using'.
                    ' DBBackupDirectory.',
                    '       If not using a database information file,' .
                    ' please specify the ',
                    '       --directory command-line option.');
            die("\nInvalid backup directory, stopped");
        }
        if (!$backup_conf{'filename'})
        {
        # Look for most current backup file
            verbose($verbose_level_debug,
                    '', 'No filename specified. Attempting to find the newest'.
                    ' database backup.');
            if ($restore_xmltvids)
            {
                $backup_conf{'filename'} = 'mythtv_xmltvid_backup';
            }
            else
            {
                $backup_conf{'filename'} = $mysql_conf{'db_name'};
                if (!$backup_conf{'filename'})
                {
                    $backup_conf{'filename'} = $d_db_name;
                }
            }
            my @files = <$backup_conf{'directory'}/$backup_conf{'filename'}*>;
            @files = grep(!/.*mythconverg_(backup|restore).*\.pl$/, @files);
            my $num_files = @files;
            if ($num_files < 1)
            {
                verbose($verbose_level_error,
                        'ERROR: Unable to find any backup files in'.
                        ' DBBackupDir and none specified.');
            }
            else
            {
                my @sorted_files = sort { lc($b) cmp lc($a) } @files;
                $backup_conf{'filename'} = $sorted_files[0];
                $backup_conf{'filename'} =~ s#^$backup_conf{'directory'}/?##;
                verbose($verbose_level_debug,
                        'Using database backup file:',
                        "$backup_conf{'directory'}/$backup_conf{'filename'}");
            }
        }
        if (!-e "$backup_conf{'directory'}/$backup_conf{'filename'}")
        {
            my $temp_filename = $backup_conf{'filename'};
        # Perhaps the user specified some unnecessary path information in the
        # filename (i.e. using the shell's filename completion)
            $temp_filename =~ s#^.*/##;
            if (-e "$backup_conf{'directory'}/$temp_filename")
            {
                $backup_conf{'filename'} = $temp_filename;
            }
            else
            {
                verbose($verbose_level_error,
                        '', 'ERROR: The specified backup file does not exist.',
                        "$backup_conf{'directory'}/$backup_conf{'filename'}");
                die("\nInvalid backup filename, stopped");
            }
        }
        if ((-d "$backup_conf{'directory'}/$backup_conf{'filename'}") ||
            (-p "$backup_conf{'directory'}/$backup_conf{'filename'}") ||
            (-S "$backup_conf{'directory'}/$backup_conf{'filename'}") ||
            (-b "$backup_conf{'directory'}/$backup_conf{'filename'}") ||
            (-c "$backup_conf{'directory'}/$backup_conf{'filename'}") ||
            (!-s "$backup_conf{'directory'}/$backup_conf{'filename'}"))
        {
                verbose($verbose_level_error,
                        '', 'ERROR: The specified backup file is empty or is'.
                        ' not a file.',
                        "$backup_conf{'directory'}/$backup_conf{'filename'}");
                die("\nInvalid backup filename, stopped");
        }
        if (!-r "$backup_conf{'directory'}/$backup_conf{'filename'}")
        {
            verbose($verbose_level_error,
                    '', 'ERROR: The specified backup file cannot be read.');
            die("\nInvalid backup filename, stopped");
        }
        if (!$mysql_conf{'db_name'})
        {
            verbose($verbose_level_debug,
                    '', "WARNING: DBName not specified. Using $d_db_name");
            $mysql_conf{'db_name'} = $d_db_name;
        }
    }

    sub check_config
    {
        verbose($verbose_level_debug,
                '', 'Checking configuration.');

        if (!defined($change_hostname))
        {
        # Check directory/filename
            check_file_config;
        }
    # Though the script will attempt a restore even if no other database
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

    sub connect_to_database
    {
        my $use_db = shift;
        my $show_errors = shift;
        my $result = 1;
        my $connect_string = 'dbi:mysql:database=';
        if ($use_db)
        {
            $connect_string .= $mysql_conf{'db_name'};
        }
        $connect_string .= ":host=$mysql_conf{'db_host'}";
        $dbh->disconnect if (defined($dbh));
        $dbh = DBI->connect($connect_string,
                            "$mysql_conf{'db_user'}",
                            "$mysql_conf{'db_pass'}",
                            { PrintError => 0 });
        $result = 0 if (!defined($dbh));
        if ($show_errors && !defined($dbh))
        {
            verbose($verbose_level_always,
                    '', 'Unable to connect to database.',
                    "           database: $mysql_conf{'db_name'}",
                    "               host: $mysql_conf{'db_host'}",
                    "           username: $mysql_conf{'db_user'}"
                   );
            if ($debug < $verbose_level_debug)
            {
                verbose($verbose_level_always,
                        'To see the password used, please re-run the script '.
                        'with the --verbose',
                        'argument.');
            }
        # Connection issues will only occur with improper user configuration
        # Because they should be rare, output the password with --verbose
            verbose($verbose_level_debug,
                    "           password: $mysql_conf{'db_pass'}");
            verbose($verbose_level_always,
                    '', 'Please check your configuration files to verify the'.
                    ' database connection',
                    'information is correct.  The files that are used to'.
                    ' retrieve connection',
                    'information are prefixed with "parsing" in the "Parsing'.
                    ' configuration files"',
                    'section of the --verbose output.');
            verbose($verbose_level_always,
                    '', 'Also note that any [client] or [mysql] password'.
                    ' specified in the MySQL options',
                    'file (/etc/my.cnf or /etc/mysql/my.cnf or ~/.my.cnf)'.
                    ' will take precedence over',
                    'the password specified in the MythTV configuration'.
                    ' files.');
        }
        return $result;
    }

    sub is_database_empty
    {
        my $result = 1;
        connect_to_database(1, 1);
        if (!defined($dbh))
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to connect to database.');
            return -1;
        }

        if (defined($dbh))
        {
            my $sth = $dbh->table_info('', '', '', 'TABLE');
            my $num_tables = keys %{$sth->fetchall_hashref('TABLE_NAME')};
            verbose($verbose_level_debug,
                    '', "Found $num_tables tables in the database.");
            if ($num_tables > 0)
            {
                if (!defined($change_hostname) && !defined($partial_restore))
                {
                    verbose($verbose_level_debug,
                            'WARNING: Database not empty.');
                }
                $result = 0;
            }
        }
        return $result;
    }

    sub create_initial_database
    {
        return 0 if (!$create_database && !$drop_database);

        my $database_exists = (connect_to_database(1, 0) && defined($dbh));
        if ($database_exists)
        {
            if ($drop_database && !$create_database)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Refusing to drop the database without'.
                        ' the --create_database argument.',
                        'If you really want to drop the database, please '.
                        're-run the script and specify',
                        'the --create_database argument, too.');
                return 2;
            }
        }
        else
        {
            if (!$create_database)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: The database does not exist.');
                return 1;
            }
        }

        verbose($verbose_level_debug,
                '', 'Preparing initial database.');

        my ($query, $sth);

        if ($database_exists && $drop_database)
        {
            verbose($verbose_level_debug, 'Dropping database.');
            connect_to_database(0, 1);
            if (!defined($dbh))
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to connect to database.');
                return -1;
            }

            $query = qq{DROP DATABASE $mysql_conf{'db_name'};};
            $sth = $dbh->prepare($query);
            if (! $sth->execute())
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to drop database.',
                        $sth->errstr);
                return -2;
            }
        }

        connect_to_database(0, 1) if (!defined($dbh));

        if (!defined($dbh))
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to connect to database.');
            return -1;
        }

        verbose($verbose_level_debug, 'Creating database.');
        $query = qq{CREATE DATABASE $mysql_conf{'db_name'};};
        $sth = $dbh->prepare($query);
        if (! $sth->execute())
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to create database.',
                    $sth->errstr);
            return -4;
        }

        verbose($verbose_level_debug, 'Setting database character set.');
        $query = qq{ALTER DATABASE $mysql_conf{'db_name'}
                    DEFAULT CHARACTER SET latin1
                    COLLATE latin1_swedish_ci;};
        $sth = $dbh->prepare($query);
        if (! $sth->execute())
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to create database.',
                    $sth->errstr);
            return -8;
        }

        return 0;
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
    # Try to load the DBD::mysql library if available (but don't # require it)
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
        my $have_database_libs = check_database_libs;
        if ($have_database_libs < 2)
        {
            if ($create_database || $drop_database)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to drop or create the initial '.
                        'database without Perl database',
                        '       libraries.',
                        'Please ensure the Perl DBI and DBD::mysql modules'.
                        ' are installed.');
                die("\nPerl database libraries missing, stopped");
            }
            if ($change_hostname)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to change hostname without Perl'.
                        ' database libraries.',
                        'Please ensure the Perl DBI and DBD::mysql modules'.
                        ' are installed.');
                die("\nPerl database libraries missing, stopped");
            }
            else
            {
                verbose($verbose_level_debug,
                        'Blindly assuming your database is prepared for a'.
                        ' restore. For better checking,',
                        'please ensure the Perl DBI and DBD::mysql modules'.
                        ' are installed.');
                return 1;
            }
        }
    # DBI/DBD::mysql are available; check the DB status
        verbose($verbose_level_debug,
                '', 'Checking database.');
        my $initial_database = create_initial_database;
        if ($initial_database)
        {
            return 0;
        }
        my $database_empty = is_database_empty;
        if ($database_empty == -1)
        {
        # Unable to connect to database
            return 0;
        }
        if ($change_hostname)
        {
            if ($database_empty)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to change hostname. The database'.
                        ' is empty.',
                        '       Please restore a backup, first, then re-run'.
                        ' this script.');
                return 0;
            }
        }
        elsif ($partial_restore)
        {
            if ($database_empty)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to do a partial restore. The'.
                        ' database is empty.',
                        '       Please run mythtv-setup, first, then re-run'.
                        ' this script.');
                return 0;
            }
        }
        else
        {
            if (!$database_empty)
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to do a full restore. The'.
                        ' database contains data.');
                return 0;
            }
        }
        return 1;
    }

    sub is_gzipped
    {
    # Simple magic number verification.
    # This naive approach works without requiring File::MMagic or any other
    # modules.
        my $result = 0;
        my $magic_number;
        my $gzip_magic_number = pack("C*", 0x1f, 0x8b);
        open(BACKUPFILE, "$backup_conf{'directory'}/$backup_conf{'filename'}")
          or return $result;
        binmode(BACKUPFILE);
        read(BACKUPFILE, $magic_number, 2);
        close(BACKUPFILE);
        return ($gzip_magic_number eq $magic_number);
    }

# Though it's possible to uncompress the file without writing the uncompressed
# data to a file, doing so is complicated by supporting the use of
# IO::Uncompress::Gunzip /and/ external uncompress programs. Also,
# uncompressing the file separately allows for easier and more precise error
# reporting.
    sub uncompress_backup_file
    {
        if (($d_uncompress eq $uncompress) || ('gunzip' eq $uncompress))
        {
            if (!is_gzipped)
            {
                verbose($verbose_level_debug,
                        '', 'Backup file is uncompressed.');
                return 0;
            }
            verbose($verbose_level_debug,
                    '', 'Backup file is compressed.');
        # Try to load the IO::Uncompress::Gunzip library if available (but
        # don't require it)
            BEGIN
            {
                our $has_uncompress_gunzip = 1;
                # Though this does nothing, it prevents an invalid "only used
                # once" warning that occurs for users without IO::Uncompress
                # installed.
                undef $GunzipError;
                eval 'use IO::Uncompress::Gunzip qw(gunzip $GunzipError);';
                if ($@)
                {
                    $has_uncompress_gunzip = 0;
                }
            }
            if (!$has_uncompress_gunzip)
            {
                verbose($verbose_level_debug,
                        ' - IO::Uncompress::Gunzip is not installed.');
            }
            else
            {
                verbose($verbose_level_debug,
                        ' - Uncompressing backup file with'.
                        ' IO::Uncompress::Gunzip.');
                my ($bfh, $temp_backup_filename) = tempfile(UNLINK => 1);
                my $result = gunzip(
                    "$backup_conf{'directory'}/$backup_conf{'filename'}" =>
                    $bfh);
                if ((defined($result)) &&
                    (-f "$temp_backup_filename") &&
                    (-r "$temp_backup_filename") &&
                    (-s "$temp_backup_filename"))
                {
                    $backup_conf{'directory'} = '';
                    $backup_conf{'filename'} = "$temp_backup_filename";
                    return 0;
                }
                verbose($verbose_level_error,
                        "   ERROR: $GunzipError");
            }
        }
        else
        {
            verbose($verbose_level_debug,
                    '', 'Unrecognized uncompress program.'.
                    ' Assuming backup file is compressed.',
                    ' - If the file is not compressed, please do not specify'.
                    ' a custom uncompress',
                    '   program name.');
        }
    # Try to uncompress the file with the uncompress binary.
    # With the approach, the original backup file will be uncompressed and
    # left uncompressed.
        my $safe_uncompress = $uncompress;
        $safe_uncompress =~ s/'/'\\''/sg;
        verbose($verbose_level_debug,
                " - Uncompressing backup file with $uncompress.",
                '   The original backup file will be left uncompressed.'.
                ' Please recompress,',
                '   if desired.');
        my $backup_path = "$backup_conf{'directory'}/$backup_conf{'filename'}";
        $backup_path =~ s/'/'\\''/sg;
        my $output = `'$safe_uncompress' '$backup_path' 2>&1`;
        my $exit = $? >> 8;
        verbose($verbose_level_debug,
                '', "$uncompress exited with status: $exit");
        if ($exit)
        {
            verbose($verbose_level_debug,
                    "$uncompress output:", $output);
        }
        else
        {
            if (!-r "$backup_conf{'directory'}/$backup_conf{'filename'}")
            {
        # Assume the final extension was removed by uncompressing.
                $backup_conf{'filename'} =~ s/\.\w+$//;
                if (!-r "$backup_conf{'directory'}/$backup_conf{'filename'}")
                {
                    verbose($verbose_level_error,
                            '',
                            'ERROR: Unable to find uncompressed backup file.');
                    die("\nInvalid backup filename, stopped");
                }
            }
        }
        return $exit;
    }

    sub do_hostname_change
    {
        my $exit = 0;
        if (!$new_hostname)
        {
            $exit++;
            verbose($verbose_level_error,
                    '', 'ERROR: Cannot change hostname without --new_hostname'.
                    ' value.');
        }
        if (!$old_hostname)
        {
            $exit++;
            verbose($verbose_level_error,
                    '', 'ERROR: Cannot change hostname without --old_hostname'.
                    ' value.');
        }
        if ($exit > 0)
        {
            die("\nInvalid --old/--new_hostname value(s) for".
                ' --change_hostname, stopped');
        }
    # Get a list of all tables in the DB.
        if (defined($dbh))
        {
            my $num_tables = 0;
            my $sth_tables;
            my $table_cat;
            my $table_schema;
            my $table_name;
            my $sth_columns;
            my $column_name;
            my $query;
            my $sth_update;
            my $result;

            $sth_tables = $dbh->table_info('', '', '', 'TABLE');
            while (my $table = $sth_tables->fetchrow_hashref)
            {
            # Loop over all tables in the DB, checking for a hostname column.
                $num_tables++;
                $table_cat = $table->{'TABLE_CAT'};
                $table_schema = $table->{'TABLE_SCHEM'};
                $table_name = $table->{'TABLE_NAME'};
                $sth_columns = $dbh->column_info($table_cat, $table_schema,
                                                 $table_name, '%');
                while (my $column = $sth_columns->fetchrow_hashref)
                {
                # If a hostname column exists, change its value.
                    $column_name = $column->{'COLUMN_NAME'};
                    if (($column_name eq 'hostname') ||
                        ($column_name eq 'host'))
                    {
                        verbose($verbose_level_debug,
                                "Found '$column_name' column in $table_name.");
                        $query = "UPDATE $table_name SET $column_name = ?".
                                 " WHERE $column_name = ?";
                        $sth_update = $dbh->prepare($query);
                        $sth_update->bind_param(1, $new_hostname);
                        $sth_update->bind_param(2, $old_hostname);
                        $result = $sth_update->execute;
                        if (!defined($result))
                        {
                            verbose($verbose_level_always,
                                    "Unable to update $column_name in table: ".
                                    $table_name,
                                    $sth_update->errstr);
                            $exit++;
                        }
                        else
                        {
                            verbose($verbose_level_debug,
                                    'Updated '.
                                    (($result == 0E0) ? '0' : $result)
                                    ." rows in table: $table_name");
                        }
                        last;
                    }
                }
            }
            if ($num_tables == 0)
            {
                verbose($verbose_level_always,
                        'Database is empty. Cannot change hostname.');
                return 1;
            }
        # delete (orphaned) rows with hostname coded into chainid in tvchain
        # live-<hostname>-2008-06-26T18:43:18
            $table_name = 'tvchain';
            $query = "DELETE FROM $table_name WHERE chainid LIKE ?";
            $sth_update = $dbh->prepare($query);
            $sth_update->bind_param(1, '%'.$old_hostname.'%');
            $result = $sth_update->execute;
            if (!defined($result))
            {
                verbose($verbose_level_debug,
                        "Unable to remove orphaned $table_name rows.",
                        $sth_update->errstr);
            }
            else
            {
                verbose($verbose_level_debug,
                        'Removed '.
                        (($result == 0E0) ? '0' : $result)
                        ." orphaned entries in table: $table_name");
            }
        # hostname coded into SGweightPerDir setting in settings (modify)
        # SGweightPerDir:<hostname>:<directory>
            $table_name = 'settings';
            $query = "UPDATE $table_name SET value = REPLACE(value, ?, ?)".
                     " WHERE value LIKE ?";
            $sth_update = $dbh->prepare($query);
            $sth_update->bind_param(1, 'SGweightPerDir:'.$old_hostname.':');
            $sth_update->bind_param(2, 'SGweightPerDir:'.$new_hostname.':');
            $sth_update->bind_param(3, 'SGweightPerDir:'.$old_hostname.':%');
            $result = $sth_update->execute;
            if (!defined($result))
            {
                verbose($verbose_level_always,
                        'Unable to update SGweightPerDir setting for host.',
                        $sth_update->errstr);
            }
            else
            {
                verbose($verbose_level_debug,
                        'Updated '.
                        (($result == 0E0) ? '0' : $result)
                        .' SGweightPerDir settings.');
            }
        }
        return $exit;
    }

    sub get_db_schema_ver
    {
        connect_to_database(1, 1) if (!defined($dbh));
        if (!defined($dbh))
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to connect to database.');
            return -1;
        }
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
                            '', 'Found DBSchemaVer:'.
                            " $mysql_conf{'db_schemaver'}.");
                }
            }
            else
            {
                verbose($verbose_level_debug,
                        "Unable to retrieve DBSchemaVer from".
                        " database.");
            }
        }

        return 0;
    }

    sub set_database_charset
    {
        return 0 if (!$create_database && !$drop_database);

        if (get_db_schema_ver && ! $mysql_conf{'db_schemaver'})
        {
            verbose($verbose_level_error,
                    "Unknown database schema version.  Assuming current.");
            $mysql_conf{'db_schemaver'} = '1216';
        }

        if ($mysql_conf{'db_schemaver'} > 1215)
        {
            connect_to_database(0, 1) if (!defined($dbh));
            if (!defined($dbh))
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to connect to database.');
                return -1;
            }

            verbose($verbose_level_debug, 'Setting database character set.');

            my ($query, $sth);
            $query = qq{ALTER DATABASE $mysql_conf{'db_name'}
                        DEFAULT CHARACTER SET utf8
                        COLLATE utf8_general_ci;};
            $sth = $dbh->prepare($query);
            if (! $sth->execute())
            {
                verbose($verbose_level_error,
                        '', 'ERROR: Unable to set database character set.',
                        $sth->errstr);
                return -16;
            }
        }

        return 0;
    }

    sub restore_backup
    {
        my $exit = 0;
        my $defaults_extra_file = create_defaults_extra_file;
        my $host_arg = '';
        my $port_arg = '';
        my $user_arg = '';
        my $filter = '';
        if ($defaults_extra_file)
        {
            $defaults_arg = " --defaults-extra-file='$defaults_extra_file'";
        }
        else
        {
            $defaults_arg = '';
        }
        my $safe_mysql_client = $mysql_client;
        $safe_mysql_client =~ s/'/'\\''/g;
    # Create the args for host, port, and user, shell-escaping values, as
    # necessary.
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
    # Configure a filter for a partial/new-host restore
        if ($partial_restore)
        {
            my @partial_restore_tables;
            if (defined($with_plugin_data))
            {
            # Blacklist the MythTV tables we don't want to keep
            # This may result in keeping old tables that were dropped in
            # previous DB schema updates if the user is running a restore
            # script from an older version of MythTV, but the extra data will
            # only take a bit of hard drive space.
                @partial_restore_tables = ('callsignnetworkmap',
                                           'capturecard',
                                           'cardinput',
                                           'channel',
                                           'channelgroup',
                                           'channelgroupnames',
                                           'channelscan',
                                           'channelscan_channel',
                                           'channelscan_dtv_multiplex',
                                           'codecparams',
                                           'conflictresolutionany', # historic
                                           'conflictresolutionoverride', # hst
                                           'conflictresolutionsingle',   # hst
                                           'credits',
                                           'customexample',
                                           'diseqc_config',
                                           'diseqc_tree',
                                           'displayprofilegroups',
                                           'displayprofiles',
                                           'dtv_multiplex',
                                           'dtv_privatetypes',
                                           'dvb_channel',           # historic
                                           'dvb_pids',              # historic
                                           'dvb_sat',               # historic
                                           'dvb_signal_quality',    # historic
                                           'dvb_transport',         # historic
                                           'eit_cache',
                                           'favorites',
                                           'filemarkup',
                                           'housekeeping',
                                           'inputgroup',
                                           'inuseprograms',
                                           'jobqueue',
                                           'jumppoints',
                                           'keybindings',
                                           'keyword',
                                           'mythlog',
                                           'networkiconmap',
                                           'oldfind',
                                           'oldprogram',
                                           'people',
                                           'pidcache',
                                           'playgroup',
                                           'powerpriority',
                                           'profilegroups',
                                           'program',
                                           'programgenres',
                                           'programrating',
                                           'recgrouppassword',
                                           'recordedcredits',
                                           'recordedfile',
                                           'recordedprogram',
                                           'recordingprofiles',
                                           'recordmatch',
                                           'recordoverride',        # historic
                                           'record_tmp',
                                           'schemalock',
                                           'settings',
                                           'storagegroup',
                                           'transcoding',           # historic
                                           'tvchain',
                                           'tvosdmenu',
                                           'upnpmedia',
                                           'videobookmarks',        # historic
                                           'videosource',
                                           'xvmc_buffer_settings'   # historic
                                          );
            }
            else
            {
            # Whitelist the tables we want to keep
                @partial_restore_tables = ('oldrecorded',
                                           'record',
                                           'recorded',
                                           'recordedmarkup',
                                           'recordedprogram',
                                           'recordedrating',
                                           'recordedseek');
            }
            if (!defined($restore_xmltvids))
            {
                $filter = '^INSERT INTO \`?(' .
                          join('|', @partial_restore_tables).')\`? ';
                # If doing a whitelist restore, ensure we keep the character
                # set info to prevent data corruption
                if (!defined($with_plugin_data))
                {
                    $filter = '(40101 SET NAMES |'.$filter.')';
                }
                verbose($verbose_level_debug,
                        '', 'Restoring partial backup with filter:', $filter);
            }
        }
        my $safe_db_name = $mysql_conf{'db_name'};
        $safe_db_name =~ s/'/'\\''/g;
        my $command = "'${safe_mysql_client}'${defaults_arg}${host_arg}".
                      "${port_arg}${user_arg} '$safe_db_name'";
        verbose($verbose_level_debug,
                '', 'Executing command:', $command);
        my $read_status = open(BACKUP,
            "<$backup_conf{'directory'}/$backup_conf{'filename'}");
        if (!defined($read_status))
        {
            verbose($verbose_level_error,
                    '', 'ERROR: Unable to read backup file.');
            return 255;
        }
        my $write_status = open(COMMAND, "| $command");
        if (!defined($write_status))
        {
            verbose($verbose_level_error,
                    '', "ERROR: Unable to execute $mysql_client.");
            return 254;
        }
        my $lines_total = 0;
        my $lines_restored = 0;
        while (<BACKUP>)
        {
            $lines_total++;
            if ($partial_restore)
            {
                if ($restore_xmltvids)
                {
                # Send all lines through
                }
                if ($with_plugin_data)
                {
                # Skip tables in the blacklist
                    next if /$filter/;
                }
                else
                {
                # Skip tables not in the whitelist
                    next if !/$filter/;
                }
            }
            $lines_restored++;
            print COMMAND or die("\nERROR: Cannot write to ".
                                 "$mysql_client, stopped");
        }
        close(COMMAND);
        close(BACKUP);
        $exit = $?;
        verbose($verbose_level_debug,
                '', "$mysql_client exited with status: $exit",
                '', "Restored $lines_restored of $lines_total lines.");
        return $exit;
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
    if (check_database)
    {
        if ($change_hostname)
        {
            $status = do_hostname_change;
            verbose($verbose_level_always,
                    '', 'Successfully changed hostname.') if (!$status);
        }
        elsif (!uncompress_backup_file)
        {
            $status = restore_backup;
            if (!$status)
            {
                verbose($verbose_level_always,
                        '', 'Successfully restored backup.');
                $status = set_database_charset;
            }
        }
    }

    $dbh->disconnect if (defined($dbh));

    exit $status;

