#!/usr/bin/perl -w
#
# Renames mythtv recordings to more human-readable filenames.
# See --help for instructions.
#
# Automatically detects database settings from mysql.txt, and loads
# the mythtv recording directory from the database (code from nuvexport).
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Includes
    use DBI;
    use Getopt::Long;
    use File::Path;
    use File::Basename;
    use File::Find;
    use MythTV;

# Some variables we'll use here
    our ($dest, $format, $usage, $underscores, $live);
    our ($dformat, $dseparator, $dreplacement, $separator, $replacement);
    our ($db_host, $db_user, $db_name, $db_pass, $video_dir, $verbose);
    our ($hostname, $dbh, $sh, $q, $count, $base_dir);

# Default filename format
    $dformat = '%T %- %Y-%m-%d, %g-%i %A %- %S';
# Default separator character
    $dseparator = '-';
# Default replacement character
    $dreplacement = '-';

# Provide default values for GetOptions
    $format      = $dformat;
    $separator   = $dseparator;
    $replacement = $dreplacement;

# Load the cli options
    GetOptions('link|dest|destination|path:s' => \$dest,
               'format=s'                     => \$format,
               'live'                         => \$live,
               'separator=s'                  => \$separator,
               'replacement=s'                => \$replacement,
               'usage|help|h'                 => \$usage,
               'underscores'                  => \$underscores,
               'verbose'                      => \$verbose
              );

# Print usage
    if ($usage) {
        print <<EOF;
$0 usage:

options:

--link [destination directory]

    If you would like mythrename.pl to work like the old mythlink.pl, specify
    --link and an optional pathname. If no pathname is given, links will be
    created in the show_names directory inside of the current mythtv data
    directory on this machine.  eg:

    /var/video/show_names/

    WARNING: ALL symlinks within the destination directory and its
    subdirectories (recursive) will be removed when using the --link option.

--live

    Include live tv recordings, affects both linking and renaming.

    default: do not link/rename live tv recordings

--format

    default:  $dformat

    \%T   = Title (show name)
    \%S   = Subtitle (episode name)
    \%R   = Description
    \%C   = Category
    \%U   = RecGroup
    \%hn  = Hostname of the machine where the file resides
    \%c   = Channel:  MythTV chanid
    \%cn  = Channel:  channum
    \%cc  = Channel:  callsign
    \%cN  = Channel:  channel name
    \%y   = Recording start time:  year, 2 digits
    \%Y   = Recording start time:  year, 4 digits
    \%n   = Recording start time:  month
    \%m   = Recording start time:  month, leading zero
    \%j   = Recording start time:  day of month
    \%d   = Recording start time:  day of month, leading zero
    \%g   = Recording start time:  12-hour hour
    \%G   = Recording start time:  24-hour hour
    \%h   = Recording start time:  12-hour hour, with leading zero
    \%H   = Recording start time:  24-hour hour, with leading zero
    \%i   = Recording start time:  minutes
    \%s   = Recording start time:  seconds
    \%a   = Recording start time:  am/pm
    \%A   = Recording start time:  AM/PM
    \%ey  = Recording end time:  year, 2 digits
    \%eY  = Recording end time:  year, 4 digits
    \%en  = Recording end time:  month
    \%em  = Recording end time:  month, leading zero
    \%ej  = Recording end time:  day of month
    \%ed  = Recording end time:  day of month, leading zero
    \%eg  = Recording end time:  12-hour hour
    \%eG  = Recording end time:  24-hour hour
    \%eh  = Recording end time:  12-hour hour, with leading zero
    \%eH  = Recording end time:  24-hour hour, with leading zero
    \%ei  = Recording end time:  minutes
    \%es  = Recording end time:  seconds
    \%ea  = Recording end time:  am/pm
    \%eA  = Recording end time:  AM/PM
    \%py  = Program start time:  year, 2 digits
    \%pY  = Program start time:  year, 4 digits
    \%pn  = Program start time:  month
    \%pm  = Program start time:  month, leading zero
    \%pj  = Program start time:  day of month
    \%pd  = Program start time:  day of month, leading zero
    \%pg  = Program start time:  12-hour hour
    \%pG  = Program start time:  24-hour hour
    \%ph  = Program start time:  12-hour hour, with leading zero
    \%pH  = Program start time:  24-hour hour, with leading zero
    \%pi  = Program start time:  minutes
    \%ps  = Program start time:  seconds
    \%pa  = Program start time:  am/pm
    \%pA  = Program start time:  AM/PM
    \%pey = Program end time:  year, 2 digits
    \%peY = Program end time:  year, 4 digits
    \%pen = Program end time:  month
    \%pem = Program end time:  month, leading zero
    \%pej = Program end time:  day of month
    \%ped = Program end time:  day of month, leading zero
    \%peg = Program end time:  12-hour hour
    \%peG = Program end time:  24-hour hour
    \%peh = Program end time:  12-hour hour, with leading zero
    \%peH = Program end time:  24-hour hour, with leading zero
    \%pei = Program end time:  minutes
    \%pes = Program end time:  seconds
    \%pea = Program end time:  am/pm
    \%peA = Program end time:  AM/PM
    \%oy  = Original Airdate:  year, 2 digits
    \%oY  = Original Airdate:  year, 4 digits
    \%on  = Original Airdate:  month
    \%om  = Original Airdate:  month, leading zero
    \%oj  = Original Airdate:  day of month
    \%od  = Original Airdate:  day of month, leading zero#    %f -> full path to the filename
    \%%   = a literal % character

    * The program start time is the time from the listings data and is not
      affected by when the recording started.  Therefore, using program start
      (or end) times may result in duplicate names.  In that case, the script
      will append a "counter" value to the name.

    * A suffix of .mpg or .nuv will be added where appropriate.

    * To separate links into subdirectories, include the / format specifier
      between the appropriate fields.  For example, "\%T/\%S" would create
      a directory for each title containing links for each recording named
      by subtitle.  You may use any number of subdirectories in your format
      specifier.  If used without the --link option, "/" will be replaced
      with the "\%-" separator character.

--separator

    The string used to separate sections of the link name.  Specifying the
    separator allows trailing separators to be removed from the link name and
    multiple separators caused by missing data to be consolidated. Indicate the
    separator character in the format string using either a literal character
    or the \%- specifier.

    default:  '$dseparator'

--replacement

    Characters in the link name which are not legal on some filesystems will
    be replaced with the given character

    illegal characters:  \\ : * ? < > | "

    default:  '$dreplacement'

--underscores

    Replace whitespace in filenames with underscore characters.

    default:  No underscores

--verbose

    Print debug info.

    default:  No info printed to console

--help

    Show this help text.

EOF
        exit;
    }

# Check the separator and replacement characters for illegal characters
    if ($separator =~ /(?:[\/\\:*?<>|"])/) {
        die "The separator cannot contain any of the following characters:  /\\:*?<>|\"\n";
    }
    elsif ($replacement =~ /(?:[\/\\:*?<>|"])/) {
        die "The replacement cannot contain any of the following characters:  /\\:*?<>|\"\n";
    }

# Escape where necessary
    our $safe_sep = $separator;
        $safe_sep =~ s/([^\w\s])/\\$1/sg;
    our $safe_rep = $replacement;
        $safe_rep =~ s/([^\w\s])/\\$1/sg;

# Get the hostname of this machine
    $hostname = `hostname`;
    chomp($hostname);

# Connect to mythbackend
    my $Myth = new MythTV();

# Connect to the database
    $dbh = $Myth->{'dbh'};
    END {
        $sh->finish  if ($sh);
    }

    my $sgroup = new MythTV::StorageGroup();

# Get our base location
    $base_dir = $sgroup->FindRecordingDir('show_names');
    if ($base_dir eq '') {
        $base_dir = $sgroup->GetFirstStorageDir();
    }

# Link destination
    if (defined($dest)) {
    # Double-check the destination
        $dest ||= "$base_dir/show_names";
    # Alert the user
        vprint("Link destination directory:  $dest");
    # Create nonexistent paths
        unless (-e $dest) {
            mkpath($dest, 0, 0755) or die "Failed to create $dest:  $!\n";
        }
    # Bad path
        die "$dest is not a directory.\n" unless (-d $dest);
    # Delete any old links
        find sub { if (-l $_) {
                       unlink $_ or die "Couldn't remove old symlink $_: $!\n";
                   }
                 }, $dest;
    # Delete empty directories (should this be an option?)
    # Let this fail silently for non-empty directories
        finddepth sub { rmdir $_; }, $dest;
    }

# Only if we're renaming files
    unless ($dest) {
        $q  = 'UPDATE recorded SET basename=? WHERE chanid=? AND starttime=FROM_UNIXTIME(?)';
        $sh = $dbh->prepare($q);
    }

# Create symlinks for the files on this machine
    my %rows = $Myth->backend_rows('QUERY_RECORDINGS Delete');
    foreach my $row (@{$rows{'rows'}}) {
        my $show = new MythTV::Recording(@$row);
    # Skip LiveTV recordings?
        next unless (defined($live) || $show->{'recgroup'} ne 'LiveTV');
    # File doesn't exist locally
        next unless (-e $show->{'local_path'});
    # Load info about the file so we can determine the file type
        $show->load_file_info();
    # Format the name
        my $name = $show->format_name($format,$separator,$replacement,$dest,$underscores);
    # Get a shell-safe version of the filename (yes, I know it's not needed in this case, but I'm anal about such things)
        my $safe_file = $show->{'local_path'};
        $safe_file =~ s/'/'\\''/sg;
        $safe_file = "'$safe_file'";
    # Figure out the suffix
        my $suffix = ($show->{'finfo'}->{'is_mpeg'}) ? '.mpg' : '.nuv';
    # Link destination
        if ($dest) {
        # Check for duplicates
            if (-e "$dest/$name$suffix") {
                $count = 2;
                while (-e "$dest/$name.$count$suffix") {
                    $count++;
                }
                $name .= ".$count";
            }
            $name .= $suffix;
        # Create the link
            my $directory = dirname("$dest/$name");
            unless (-e $directory) {
                mkpath($directory, 0, 0755)
                    or die "Failed to create $directory:  $!\n";
            }
            symlink $show->{'local_path'}, "$dest/$name"
                or die "Can't create symlink $dest/$name:  $!\n";
            vprint("$dest/$name");
        }
    # Rename the file, but only if it's a real file
        elsif (-f $show->{'local_path'}) {
            if ($show->{'basename'} ne $name.$suffix) {
            # Check for duplicates
                $video_dir = $sgroup->FindRecordingDir($show->{'basename'});
                if (-e "$video_dir/$name$suffix") {
                    $count = 2;
                    while (-e "$video_dir/$name.$count$suffix") {
                        $count++;
                    }
                    $name .= ".$count";
                }
                $name .= $suffix;
            # Update the database
                my $rows = $sh->execute($name, $show->{'chanid'}, $show->{'recstartts'});
                die "Couldn't update basename in database for ".$show->{'basename'}.":  ($q)\n" unless ($rows == 1);
                my $ret = rename $show->{'local_path'}, "$video_dir/$name";
            # Rename failed -- Move the database back to how it was (man, do I miss transactions)
                if (!$ret) {
                    $rows = $sh->execute($show->{'basename'}, $show->{'chanid'}, $show->{'recstartts'});
                    die "Couldn't restore original basename in database for ".$show->{'basename'}.":  ($q)\n" unless ($rows == 1);
                }
                vprint($show->{'basename'}."\t-> $name");
            }
        }
    }

    $sh->finish if ($sh);

# Print the message, but only if verbosity is enabled
    sub vprint {
        return unless (defined($verbose));
        print join("\n", @_), "\n";
    }

