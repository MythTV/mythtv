#!/usr/bin/perl
## written by greg froese (g_froese@yahoo.com)
## install instructions by Robert Kulagowski (rkulagow@rocketmail.com)
##
## I had trouble maintaining my catalog of recordings when upgrading to
## cvs and from cvs to more recent cvs, so I wrote this.
##
## Here is what this program is supposed to do.
## It first scans through your myth database and displays all shows listed
## in the recorded table.
## It will then traverse your myth directory (set with --dir /YOURMYTHDIR) and
## find all your .nuv files and check them against the database to make sure
## they have an entry.  If no entry exists, you will be prompted for the title
## and subtitle of the recording and a record will be created.
##
## The following options are available to be changed from the command line
## (I've put defaults in if you leave them out, except for $dir)
## --dbhost DBHOSTNAME
## --host HOSTNAME
## --user DBUSERNAME
## --pass DBPASSWORD
## --dir /MYTHFILESARESTOREDHERE
## --database DATABASENAME
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

## To run:
## Ensure that the script is executable
## chmod a+x myth.rebuilddatabase.pl
## ./myth.rebuilddatabase.pl

## Change log:
## 9-19-2003: (awithers@anduin.com)
##  Anduin fights the urge to make code more readable (aka C like).  Battle
##  of urges ends in stalemate: code was reindented but not "changed" (much).
##  To make it a little less useless a contribution also did:
##    - added ability to grab title/subtitle/description from oldrecorded
##    - support for multiple backends (via separation of host and dbhost
##      and bothering to insert the host in the recorded table).
##    - removed dependency on File::Find::Rule stuff
##    - attempt to determine good default host name
##    - provide default for --dir from DB (if not provided)
##    - added --test_mode (for debugging, does everything except INSERT)
##    - added --try_default (good for when you must specify a command
##      line option but don't really need to)
##    - added --quick_run for those occasions where you just don't have
##      the sort of time to be sitting around hitting enter
##    - changed all the DB calls to use parameters (avoids escape issues,
##      and it looks better)

use strict;
use DBI;
use Getopt::Long;
use Sys::Hostname;
use File::Basename;
use Date::Parse;
use Time::Format qw(time_format);

## get command line args

my ($verbose, $dir);

my $show_existing = 0;
my $test_mode = 0;
my $quick_run = 0;
my $try_default = 0;

my $host = hostname;
my $dbhost = $host;
my $database = "mythconverg";
my $user = "mythtv";
my $pass = "mythtv";
my $ext = "nuv";
my $file = "";
my @answers;
my $norename = 0;

my $date_regx = qr/(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)/;
my $db_date_regx = qr/(\d\d\d\d)-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)/;
my $channel_regx = qr/(\d\d\d\d)/;

sub GetAnswer {
    my ($prompt, $default) = @_;
    print $prompt;
    if ($default) {
        print " [", $default, "]";
    }
    print ": ";

    my $answer;
    if ($#answers >= 0) {
        $answer = shift @answers;
        print $answer, "\n";
    } else {
        chomp($answer = <STDIN>);
        $answer = $default if !$answer;
    }

    return $answer;
}

my $script_name = $0;

if ($0 =~ m/([^\/]+)$/) {
	$script_name = $1;
}

my $script_version = "0.0.2";

my $argc=@ARGV;
if ($argc == 0) {
  print "$script_name Version $script_version
    usage: $script_name [options]

    Where [options] is:
      --host          - hostname of this backend (default: \"$host\")
      --dbhost        - hostname or IP address of the mysql server
                        (default: \"$dbhost\")
      --user          - DBUSERNAME (default: \"$user\")
      --pass          - DBPASSWORD (default: \"$pass\")
      --database      - DATABASENAME (default: \"$database\")
      --show_existing - Dumps current recorded table.
      --dir           - path to recordings (default: queried from db)
      --try_default   - Try to just run with the defaults.
      --quick_run     - don't prompt for title/subtitle/description just
                        use the default
      --test_mode     - do everything except update the database
      --ext           - file extensions to scan for
      --file          - specific file to import
      --answer        - command-line response to prompts (give as many
                        answers as you like)
      --norename      - don't rename file to myth convention

    Example 1:
      Assumption: The script is run on DB/backend machine.

        $script_name --try_default

    Example 2:
      Assumption: The script is run on a backend other than the DB machine.
		
        $script_name --dbhost=mydbserver
";
	exit(0);
}

GetOptions('verbose+'=>\$verbose,
		'database=s'=>\$database,
		'dbhost=s'=>\$dbhost,
		'host=s'=>\$host,
		'user=s'=>\$user,
		'pass=s'=>\$pass,
		'dir=s'=>\$dir,
		'show_existing|se'=>\$show_existing,
		'try_default|td'=>\$try_default,
		'quick_run|qr'=>\$quick_run,
		'test_mode|t|tm'=>\$test_mode,
		'ext=s'=>\$ext,
		'file=s'=>\$file,
        'answer=s'=>\@answers,    # =s{,} would be nice but isn't implemented widely
		'norename'=>\$norename
		);

my $dbh = DBI->connect("dbi:mysql:database=$database:host=$dbhost",
		"$user","$pass") or die "Cannot connect to database ($!)\n";

my ($starttime, $endtime, $title, $subtitle, $channel, $description);
my ($syear, $smonth, $sday, $shour, $sminute, $ssecond, $eyear, $emonth, $eday,
		$ehour, $eminute, $esecond);

my $q = "";
my $sth;

if (!$dir) {
	my $dir_query = "select data from settings where value='RecordFilePrefix' and hostname=(?)";
	$sth = $dbh->prepare($dir_query);
	$sth->execute($host) or die "Could not execute ($dir_query)";
	if (my @row = $sth->fetchrow_array) {
		$dir = $row[0];
	}
}

if (!$dir) {
	print("Error: no directory found or specified\n");
	exit 1;
}

# remove trailing slash
$dir =~ s/\/$//;

if ($show_existing) {
	$q = "select title, subtitle, starttime, endtime, chanid from recorded order by starttime";
	$sth = $dbh->prepare($q);
	$sth->execute or die "Could not execute ($q)\n";

	print "\nYour myth database ($database) is reporting the following programs as being recorded:\n\n";

	while (my @row=$sth->fetchrow_array) {
		$title = $row[0];
		$subtitle = $row[1];
		$starttime = $row[2];
		$endtime = $row[3];
		$channel = $row[4];

## get the pieces of the time
		if ($starttime =~ m/$db_date_regx/) {
			($syear, $smonth, $sday, $shour, $sminute, $ssecond) =
				($1, $2, $3, $4, $5, $6);
		}

		if ($endtime =~ m/$db_date_regx/) {
			($eyear, $emonth, $eday, $ehour, $eminute, $esecond) =
				($1, $2, $3, $4, $5, $6);
		}

##		 print "Channel $channel\t$smonth/$sday/$syear $shour:$sminute:$ssecond - $ehour:$eminute:$esecond - $title ($subtitle)\n";
		print "Channel:    $channel\n";
		print "Start time: $smonth/$sday/$syear - $shour:$sminute:$ssecond\n";
		print "End time:   $emonth/$eday/$eyear - $ehour:$eminute:$esecond\n";
		print "Title:      $title\n";
		print "Subtitle:   $subtitle\n\n";
	}
}

print "\nThese are the files stored in ($dir) and will be checked against\n";
print "your database to see if the exist.  If they do not, you will be prompted\n";
print "for a title and subtitle of the entry, and a record will be created.\n\n";

my @files = $file ? ($dir . "/" . $file) : glob("$dir/*.$ext");

foreach my $show (@files) {
    my $showBase = basename($show);

    my $found_title = $dbh->selectrow_array("select title from recorded where hostname=(?) and basename=(?)",
                                            undef, $host, $showBase);

    if ($found_title) {
        print("Found a match between file and database\n");
        print("    File: '$show'\n");
        print("    Title: '$found_title'\n");

        # use this so the stuff below doesn't have to be indented
        next;
    }

    print("Unknown file $show found.\n");
    next unless GetAnswer("Do you want to import?", "y") eq "y";


    # normal case: import file into the database

    my ($channel, $syear, $smonth, $sday, $shour, $sminute, $ssecond,
        $eyear, $emonth, $eday, $ehour, $eminute, $esecond);
    my ($starttime, $duration, $endtime);
    my ($mythfile);

    # filename varies depending on when the recording was
    # created. Gleam as much as possible from the name.

    if ($showBase =~ m/$channel_regx\_/) {
        $channel = $1;
    } else {
        $channel = "0000";
    }

    if ($showBase =~ m/$channel_regx\_$date_regx\./) {
        ($syear, $smonth, $sday, $shour, $sminute, $ssecond) =
            ($2, $3, $4, $5, $6, $7);
    }

    if ($showBase =~ m/$channel_regx\_$date_regx\_$date_regx/) {
        ($eyear, $emonth, $eday, $ehour, $eminute, $esecond) =
            ($8, $9, $10, $11, $12, $13);
    }

    my $guess_title = "Unknown";
    my $guess_subtitle = "";
    my $guess_description = "Recovered file " . $showBase;

    # have enough to look for an past recording?
    if ($ssecond) {
        $starttime = "$syear$smonth$sday$shour$sminute$ssecond";

        my $guess = "select title, subtitle, description from oldrecorded where chanid=(?) and starttime=(?)";
        $sth = $dbh->prepare($guess);
        $sth->execute($channel, $starttime)
            or die "Could not execute ($guess)\n";

        if (my @row = $sth->fetchrow_array) {
            $guess_title = $row[0];
            $guess_subtitle = $row[1];
            $guess_description = $row[2];
        }

        print "Found an orphaned file, initializing database record\n";
        print "Channel:    $channel\n";
        print "Start time: $smonth/$sday/$syear - $shour:$sminute:$ssecond\n";
        print "End time:   $emonth/$eday/$eyear - $ehour:$eminute:$esecond\n";
    }

    my $newtitle = $guess_title;
    my $newsubtitle = $guess_subtitle;
    my $newdescription = $guess_description;

    if ($quick_run) {

        print("QuickRun defaults:\n");
        print("        title: '$newtitle'\n");
        print("     subtitle: '$newsubtitle'\n");
        print("  description: '$newdescription'\n");

    } else {

        $channel = GetAnswer("Enter channel", $channel);
        $newtitle = GetAnswer("... title", $newtitle);
        $newsubtitle = GetAnswer("... subtitle", $newsubtitle);
        $newdescription = GetAnswer("Description", $newdescription);
        $starttime = GetAnswer("... start time (YYYY-MM-DD HH:MM:SS)", $starttime);

        if ($endtime) {
            $duration = (str2time($endtime) - str2time($starttime)) / 60;
        } else {
            $duration = "60";
        }
        $duration = GetAnswer("... duration (in minutes)", $duration);
        $endtime = time_format("yyyy-mm{on}-dd HH:mm{in}:ss", str2time($starttime) + $duration * 60);

    }

    if ($norename) {
        $mythfile = $showBase;
    } else {
        my ($ext) = $showBase =~ /([^\.]*)$/;
        my $time1 = $starttime;
        $time1 =~ s/[ \-:]//g;
        $mythfile = sprintf("%s_%s.%s", $channel, $time1, $ext);
    }

    my $sql = "insert into recorded (chanid, starttime, endtime, title, subtitle, description, hostname, basename, progstart, progend) values ((?), (?), (?), (?), (?), (?), (?), (?), (?), (?))";

    if ($test_mode) {

        $sql =~ s/\(\?\)/"%s"/g;
        my $statement = sprintf($sql, $channel, $starttime, $endtime, $newtitle,
                                $newsubtitle, $newdescription, $host, $mythfile,
                                $starttime, $endtime);
        print("Test mode: insert would have been been:\n");
        print($statement, ";\n");

    } else {

        $sth = $dbh->prepare($sql);
        $sth->execute($channel, $starttime, $endtime, $newtitle,
                      $newsubtitle, $newdescription, $host, $mythfile,
                      $starttime, $endtime)
            or die "Could not execute ($sql)\n";

        if ($mythfile ne $showBase) {
            rename($show, $dir. "/" . $mythfile);
        }
    }

    
    print("Building a seek table should improve FF/RW and JUMP functions when watching this video\n");

    if (GetAnswer("Do you want to build a seek table for this file?", "y") eq "y") {
        # mythcommflag takes --file for myth-originated files and
        # --video for everything else. We assume it came from myth
        # if it's a .nuv or if it's an mpeg where the name has that
        # chanid_startime format
        my $commflag = "mythcommflag --rebuild " .
            ($showBase =~ /[.]nuv$/ || ($showBase =~ /[.]mpg$/ && $ssecond)
             ? "--file" : "--video") .
             " " . $mythfile;
        if (!$test_mode) {
            exec($commflag);
        } else { 
            print("Test mode: exec would have done\n"); 
            print("  Exec: '", $commflag, "'\n");
        }
    }

} ## foreach loop

# vim:sw=4 ts=4 syn=off:
