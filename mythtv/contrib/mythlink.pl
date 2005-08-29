#!/usr/bin/perl -w
#
# $Date$
# $Revision$
# $Author$
#
# license: GPL
# author:  Chris Petersen (based on the ideas in mythlink.sh from Dale Gass)
#
# mythlink.pl
#
#   Creates symlinks to more human-readable versions of mythtv recording
#   filenames.  See --help for instructions.
#
#   Automatically detects database settings from mysql.txt, and loads
#   the mythtv recording directory from the database (code from nuvexport).
#

# Includes
    use DBI;
    use Getopt::Long;
    use File::Path;

# Some variables we'll use here
    our ($dest, $format, $usage);
    our ($db_host, $db_user, $db_name, $db_pass, $video_dir);
    our ($hostname, $dbh, $sh);

# Load the destination directory, if one was specified
    GetOptions('dest|destination|path=s' => \$dest,
               'format=s'                => \$format,
               'usage|help|h'            => \$usage,
              );

# Default filename format
    $format ||= '%T - %Y-%m-%d, %g-%i %A - %S';

# Print usage
    if ($usage) {
        print <<EOF;
$0 usage:

options:

--dest

    By default, links will be created in the show_names directory inside of the
    current mythtv data directory on this machine.  eg:

    /var/video/show_names/

--format

    default:  $format

    \%T = title    (aka show name)
    \%S = subtitle (aka episode name)
    \%R = description
    \%y = year, 2 digits
    \%Y = year, 4 digits
    \%n = month
    \%m = month, leading zero
    \%j = day of month
    \%d = day of month, leading zero
    \%g = 12-hour hour
    \%G = 24-hour hour
    \%h = 12-hour hour, with leading zero
    \%H = 24-hour hour, with leading zero
    \%i = minutes
    \%s = seconds
    \%a = am/pm
    \%A = AM/PM

    * Illegal filename characters will be replaced with dashes.
    * A suffix of .mpg or .nuv will be added where appropriate.

--help

    Show this help text.

EOF
        exit;
    }

# Get the hostname of this machine
    $hostname = `hostname`;
    chomp($hostname);

# Read the mysql.txt file in use by MythTV.
# could be in a couple places, so try the usual suspects
    my $found = 0;
    my @mysql = ('/usr/local/share/mythtv/mysql.txt',
                 '/usr/share/mythtv/mysql.txt',
                 '/etc/mythtv/mysql.txt',
                 '/usr/local/etc/mythtv/mysql.txt',
                 "$ENV{HOME}/.mythtv/mysql.txt",
                 'mysql.txt'
                );
    foreach my $file (@mysql) {
        next unless (-e $file);
        $found = 1;
        open(CONF, $file) or die "Unable to open $file:  $!\n\n";
        while (my $line = <CONF>) {
        # Cleanup
            next if ($line =~ /^\s*#/);
            $line =~ s/^str //;
            chomp($line);
        # Split off the var=val pairs
            my ($var, $val) = split(/\=/, $line, 2);
            next unless ($var && $var =~ /\w/);
            if ($var eq 'DBHostName') {
                $db_host = $val;
            }
            elsif ($var eq 'DBUserName') {
                $db_user = $val;
            }
            elsif ($var eq 'DBName') {
                $db_name = $val;
            }
            elsif ($var eq 'DBPassword') {
                $db_pass = $val;
            }
        # Hostname override
            elsif ($var eq 'LocalHostName') {
                $hostname = $val;
            }
        }
        close CONF;
    }
    die "Unable to locate mysql.txt:  $!\n\n" unless ($found && $db_host);

# Connect to the database
    $dbh = DBI->connect("dbi:mysql:database=$db_name:host=$db_host", $db_user, $db_pass)
        or die "Cannot connect to database: $!\n\n";
    END {
        $dbh->disconnect if ($dbh);
    }

# Find the directory where the recordings are located
    $q = "SELECT data FROM settings WHERE value='RecordFilePrefix' AND hostname=?";
    $sh = $dbh->prepare($q);
        $sh->execute($hostname) or die "Could not execute ($q):  $!\n\n";
    ($video_dir) = $sh->fetchrow_array;
    $sh->finish;
    die "This host not configured for myth.\n(No RecordFilePrefix defined for $hostname in the settings table.)\n\n" unless ($video_dir);
    die "Recordings directory $video_dir doesn't exist!\n\n" unless (-d $video_dir);
    $video_dir =~ s/\/+$//;

# Double-check the destination
    $dest ||= "$video_dir/show_names";
    print "Destination directory:  $dest\n";

# Create nonexistent paths
    unless (-e $dest) {
        mkpath($dest, 0, 0755) or die "Failed to create $dest:  $!\n";
    }

# Bad paths
    die "$dest is not a directory.\n" unless (-d $dest);

# Delete any old links
    foreach my $file (<$dest/*>) {
        next unless (-l $file);
        unlink $file or die "Couldn't remove old symlink $file:  $!\n";
    }

# Prepare a database query
    $sh = $dbh->prepare('SELECT title, subtitle, description FROM recorded WHERE chanid=? AND starttime=? AND endtime=?');

# Create symlinks for the files on this machine
    foreach my $file (<$video_dir/*.nuv>) {
        next if ($file =~ /^ringbuf/);
    # Pull out the various parts that make up the filename
        my ($channel,
            $syear, $smonth, $sday, $shour, $sminute, $ssecond,
            $eyear, $emonth, $eday, $ehour, $eminute, $esecond) = $file =~/^$video_dir\/([a-z0-9]+)_(....)(..)(..)(..)(..)(..)_(....)(..)(..)(..)(..)(..)\.nuv$/i;
    # Found a bad filename?
        unless ($channel) {
            print "Unknown filename format:  $file\n";
            next;
        }
    # Query the desired info about this recording
        $sh->execute($channel, "$syear$smonth$sday$shour$sminute$ssecond", "$eyear$emonth$eday$ehour$eminute$esecond")
            or die "Could not execute ($q):  $!\n\n";
        my ($title, $subtitle, $description) = $sh->fetchrow_array;
    # Format some fields we may be parsing below
        my $meridian = ($shour > 12) ? 'AM' : 'PM';
        my $hour = ($shour > 12) ? $shour - 12 : $shour;
        if ($hour < 10) {
            $hour = "0$hour";
        }
        elsif ($hour < 1) {
            $hour = 12;
        }
    # Build a list of name format options
        my %fields;
        ($fields{'T'} = ($title       or '')) =~ s/%/%%/g;
        ($fields{'S'} = ($subtitle    or '')) =~ s/%/%%/g;
        ($fields{'R'} = ($description or '')) =~ s/%/%%/g;
        $fields{'y'} = substr($syear, 2);   # year, 2 digits
        $fields{'Y'} = $syear;              # year, 4 digits
        $fields{'n'} = int($smonth);        # month
        $fields{'m'} = $smonth;             # month, leading zero
        $fields{'j'} = int($sday);          # day of month
        $fields{'d'} = $sday;               # day of month, leading zero
        $fields{'g'} = int($hour);          # 12-hour hour
        $fields{'G'} = int($shour);         # 24-hour hour
        $fields{'h'} = $hour;               # 12-hour hour, with leading zero
        $fields{'H'} = $shour;              # 24-hour hour, with leading zero
        $fields{'i'} = $sminute;            # minutes
        $fields{'s'} = $ssecond;            # seconds
        $fields{'a'} = lc($meridian);       # am/pm
        $fields{'A'} = $meridian;           # AM/PM
    # Make the substitution
        my $keys = join('', sort keys %fields);
        my $name = $format;
        $name =~ s/(?<!%)(?:%([$keys]))/$fields{$1}/g;
        $name =~ s/%%/%/g;
    # Some basic cleanup for illegal characters, etc.
        $name =~ s/(?:[\/\\\:\*\?\<\>\|\-]+\s*)+(?=[^\d\s\/\\\:\*\?\<\>\|\-])/- /sg;
        $name =~ tr/\/\\:*?<>|/-/;
        $name =~ tr/"/'/s;
        $name =~ tr/\s/ /s;
        $name =~ s/^[\-\s]+//s;
        $name =~ s/[\-\s]+$//s;
    # Get a shell-safe version of the filename (yes, I know it's not needed in this case, but I'm anal about such things)
        my $safe_file = $file;
        $safe_file =~ s/'/'\\''/sg;
        $safe_file = "'$safe_file'";
    # Figure out the suffix
        my $out    = `file $safe_file 2>/dev/null`;
        my $suffix = ($out =~ /mpe?g/i) ? '.mpg' : '.nuv';
    # Check for duplicates
        if (-e "$dest/$name$suffix") {
            my $count = 2;
            while (-e "$dest/$name.$count$suffix") {
                $count++;
            }
            $name .= ".$count";
        }
    # Create the link
        symlink $file, "$dest/$name$suffix"
            or die "Can't create symlink $dest/$name$suffix:  $!\n";
        print "$dest/$name$suffix\n";
    }

    $sh->finish;

