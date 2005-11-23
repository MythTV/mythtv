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

my $date_regx = qr/(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)/;
my $db_date_regx = qr/(\d\d\d\d)-(\d\d)-(\d\d) (\d\d):(\d\d):(\d\d)/;
my $channel_regx = qr/(\d\d\d\d)/;

my $script_name = $0;

if ($0 =~ m/([^\/]+)$/) {
	$script_name = $1;
}

my $script_version = "0.0.1";

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
        'ext=s'=>\$ext
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

my @files = glob("$dir/*.$ext");

foreach my $show (@files) {
	my ($channel, $syear, $smonth, $sday, $shour, $sminute, $ssecond,
			$eyear, $emonth, $eday, $ehour, $eminute, $esecond);
	my ($starttime, $endtime);

	if ($show =~ m/$channel_regx\_$date_regx\_$date_regx/) {
		$channel = $1;

		($syear, $smonth, $sday, $shour, $sminute, $ssecond) =
			($2, $3, $4, $5, $6, $7);

		($eyear, $emonth, $eday, $ehour, $eminute, $esecond) =
			($8, $9, $10, $11, $12, $13);

		$starttime = "$syear$smonth$sday$shour$sminute$ssecond";
		$endtime = "$eyear$emonth$eday$ehour$eminute$esecond";

## check if this is already in the db
## second hit to the db i know, but performance is not a big
## issue here

		$q = "select title from recorded where chanid=(?) and starttime=(?) and endtime=(?)";
		$sth = $dbh->prepare($q);
		$sth->execute($channel, $starttime, $endtime)
			or die "Could not execute ($q)\n";

		my $exists = 0;
		my $found_title = "Unknown";
		if (my @row = $sth->fetchrow_array) {
			$exists = 1;
			$found_title = $row[0];
		}

		if ($exists) {
			print("Found a match between file and database\n");
			print("    File: '$show'\n");
			print("    Title: '$found_title'\n");
		} else {
			my $basename = $show;
			$basename =~ s/$dir\/(.*)/$1/s;

			my $guess = "select title, subtitle, description from oldrecorded where chanid=(?) and starttime=(?) and endtime=(?)";
			$sth = $dbh->prepare($guess);
			$sth->execute($channel, $starttime, $endtime)
				or die "Could not execute ($guess)\n";

			my $guess_title = "Unknown";
			my $guess_subtitle = "Unknown";
			my $guess_description = "Unknown";

			if (my @row = $sth->fetchrow_array) {
				$guess_title = $row[0];
				$guess_subtitle = $row[1];
				$guess_description = $row[2];
			}

			print "Found an orphaned file, creating database record\n";
			print "Channel:    $channel\n";
			print "Start time: $smonth/$sday/$syear - $shour:$sminute:$ssecond\n";
			print "End time:   $emonth/$eday/$eyear - $ehour:$eminute:$esecond\n";

			my $newtitle = $guess_title;
			my $newsubtitle = $guess_subtitle;
			my $newdescription = $guess_description;

			if (!$quick_run) {
				print "Enter title (default: '$newtitle'): ";
				chomp(my $tmp_title = <STDIN>);
				if ($tmp_title) {$newtitle = $tmp_title;}

				print "Enter subtitle (default: '$newsubtitle'): ";
				chomp(my $tmp_subtitle = <STDIN>);
				if ($tmp_subtitle) {$newsubtitle = $tmp_subtitle;}

				print "Enter description (default: '$newdescription'): ";
				chomp(my $tmp_description = <STDIN>);
				if ($tmp_description) {$newdescription = $tmp_description;}
			} else {
				print("QuickRun defaults:\n");
				print("        title: '$newtitle'\n");
				print("     subtitle: '$newsubtitle'\n");
				print("  description: '$newdescription'\n");
			}

## add records to db
			my $i = "insert into recorded (chanid, starttime, endtime, title, subtitle, description, hostname, basename, progstart, progend) values ((?), (?), (?), (?), (?), (?), (?), (?), (?), (?))";

			$sth = $dbh->prepare($i);
			if (!$test_mode) {
				$sth->execute($channel, $starttime, $endtime, $newtitle,
						$newsubtitle, $newdescription, $host, $basename,
						$starttime, $endtime)
					or die "Could not execute ($i)\n";
			} else {
				print("Test mode: insert would have been done\n");
				print("  Query: '$i'\n");
				print("  Query params: '$channel', '$starttime', '$endtime',");
				print("'$newtitle', '$newsubtitle', '$newdescription',");
				print("'$host', '$basename', '$starttime', '$endtime'\n");
				
			}
		} ## if
	} else {
# file doesn't match
        print("Non-nuv file $show found.\n");
        print("Do you want to import? (y/n): ");
        chomp(my $do_import = <STDIN>);
        if ($do_import eq "y") {
			my $basename = $show;
			$basename =~ s/$dir\/(.*)/$1/s;

            print("Enter channel: ");
            chomp(my $tmp_channel = <STDIN>);
            if ($tmp_channel) {$channel = $tmp_channel;}
            
            print("Enter start time (YYYY-MM-DD HH:MM:SS): ");
            chomp(my $tmp_starttime = <STDIN>);
            if ($tmp_starttime) {$starttime = $tmp_starttime;}

            print("Enter end time (YYYY-MM-DD HH:MM:SS): ");
            chomp(my $tmp_endtime = <STDIN>);
            if ($tmp_endtime) {$endtime = $tmp_endtime;}

			print "Enter title: ";
			chomp(my $tmp_title = <STDIN>);
			if ($tmp_title) {$title = $tmp_title;}

			print "Enter subtitle: ";
			chomp(my $tmp_subtitle = <STDIN>);
			if ($tmp_subtitle) {$subtitle = $tmp_subtitle;}

			print "Enter description: ";
			chomp(my $tmp_description = <STDIN>);
			if ($tmp_description) {$description = $tmp_description;}

## add records to db
			my $i = "insert into recorded (chanid, starttime, endtime, title, subtitle, description, hostname, basename, progstart, progend) values ((?), (?), (?), (?), (?), (?), (?), (?), (?), (?))";

			$sth = $dbh->prepare($i);
			if (!$test_mode) {
				$sth->execute($channel, $starttime, $endtime, $title,
						$subtitle, $description, $host, $basename,
						$starttime, $endtime)
					or die "Could not execute ($i)\n";
			} else {
				print("Test mode: insert would have been done\n");
				print("  Query: '$i'\n");
				print("  Query params: '$channel', '$starttime', '$endtime',");
				print("'$title', '$subtitle', '$description', '$host',");
				print("'$basename', '$starttime', '$endtime'\n");
				
			}

## rename file to proper format
            
		if ($starttime =~ m/$db_date_regx/) {
			($syear, $smonth, $sday, $shour, $sminute, $ssecond) =
				($1, $2, $3, $4, $5, $6);
		}

		if ($endtime =~ m/$db_date_regx/) {
			($eyear, $emonth, $eday, $ehour, $eminute, $esecond) =
				($1, $2, $3, $4, $5, $6);
		}

        my $new_file = "$dir/$channel"."_$syear$smonth$sday$shour$sminute$ssecond"."_$eyear$emonth$eday$ehour$eminute$esecond.nuv";
        if (!$test_mode) {
            rename("$show", "$new_file");
        } else {
            print("Test mode: rename would have done\n");
            print("  From: '$show'\n");
            print("  To: '$new_file'\n");
        } 

## commercial flag
 
        print("Building a seek table should improve FF/RW and JUMP functions when watching this video\n");
        print("Do you want to build a seek table for this file? (y/n): ");
        chomp(my $do_commflag = <STDIN>);
        if ($do_commflag eq "y") {
            if (!$test_mode) {
                exec("mythcommflag --file $new_file --rebuild"); 
            } else { 
                print("Test mode: exec would have done\n"); 
                print("  Exec: 'mythcommflag --file $new_file --rebuild'\n");
            }
        }

        } else { print("Skipping illegal file format: $show\n"); }
	}
} ## foreach loop

# vim:sw=4 ts=4 syn=off:
