#!/usr/bin/perl
##
## Script to extract show name and subtitle from DB given the filename.
##
## Hack and Slash done by Rob Snow (rsnow@dympna.com)
##
## 28 Mar 03	1.0	Hack
## 29 Mar 03	1.1	Added --legal to fix filenames for / and \
##			Should probably be fixed for other chars
##			but I'm to lazy.
##
## This is a very nasty hack of myth.rebuilddatabase.pl which was nicely 
## done by Greg Froese and instructions by Robert Kulagowski.  
##
## Those fine gentlemens information may be found below, however, please
## do not confuse them with the author of this hack...they do nice work.
##
## written by greg froese (g_froese@yahoo.com)
## install instructions by Robert Kulagowski (rkulagow@rocketmail.com)
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

##use strict;
use DBI;
use Getopt::Long;
use File::Basename;

## get command line args

my ($database, $host, $user, $pass, $verbose, $dir);

my $argc=@ARGV;
if ($argc == 0) {
   print "usage:  mythname.pl [options] 
/path/to/store/1001_20030401190000_20030401200000.nuv

Where [options] is:
--host          - hostname or IP address of the mysql server (default: 
\"127.0.0.1\")
--user          - DBUSERNAME (default: \"mythtv\")
--pass          - DBPASSWORD (default: \"mythtv\")
--database      - DATABASENAME (default: \"mythconverg\")
--replace	- Replace spaces with this string (--rep=. will return Daily.Show)
--sublen	- Maximum subtitle length (only useful with -s)
-s		- Add subtitle to string after a ':'
--legal		- Make sure the filename is legal (no '/', '\', etc.)
";
exit(0);
}

GetOptions('verbose+'=>\$verbose, 'database=s'=>\$database, 'host=s'=>\$host, 
'user=s'=>\$user, 'pass=s'=>\$pass, 's+'=>\$sub, 'replace=s'=>\$rep, 
'sublen=s'=>\$sublen, 'legal+'=>\$legal);

if (!$host) { $host="127.0.0.1"; }
if (!$database) { $database="mythconverg"; }
if (!$user) { $user="mythtv"; }
if (!$pass) { $pass="mythtv"; }

my $dbh = 
DBI->connect("dbi:mysql:database=$database:host=$host","$user","$pass") or
		 die "Cannot connect to database ($!)\n";

 ($show, $path, $suffix)  = fileparse(@ARGV[0],"\.nuv");
  $channel = substr($show,0,4);
  $syear = substr($show, 5,4);
  $smonth = substr($show, 9,2);
  $sday = substr($show,11,2);
  $shour = substr($show,13,2);
  $sminute = substr($show,15,2);
  $ssecond = substr($show,17,2);
  $eyear = substr($show,20,4);
  $emonth = substr($show,24,2);
  $eday = substr($show,26,2);
  $ehour = substr($show,28,2);
  $eminute = substr($show,30,2);
  $esecond = substr($show,32,2);

 $q = "select title, subtitle, chanid, starttime, endtime from recorded where 
chanid=$channel and starttime='$syear$smonth$sday$shour$sminute$ssecond' and 
endtime='$eyear$emonth$eday$ehour$eminute$esecond'";
 $sth = $dbh->prepare($q);
 $sth->execute or die "Could not execute ($q)\n";

@row=$sth->fetchrow_array;
$title = $row[0];
$subtitle = $row[1];

if ($rep) {
	$subtitle=~s/\ /$rep/gio;
	$title=~s/\ /$rep/gio;
}
if ($sublen) {
	$subtitle=substr($subtitle,0,$sublen);
}

if ($legal) {
	$nogood = " ";
	if ($rep) {
		$nogood = $rep;
	}
	$title =~ s/\\/$nogood/gio;
	$subtitle =~ s/\\/$nogood/gio;
	$title =~ s/\//$nogood/gio;
	$subtitle =~ s/\//$nogood/gio;
}


print "$title";
if ($sub) {
	print ":$subtitle";
}
print "\n";

exit(0);
