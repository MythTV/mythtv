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
## --host HOSTNAME
## --user DBUSERNAME
## --pass DBPASSWORD
## --dir /MYTHFILESARESTOREDHERE  (must be fully qualified dir)
## --database DATABASENAME
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

## unfortunately i used File::Find::Rule which requires a few other modules from
## search.cpan.org
##
## I had to install the following modules, your results may vary:
## File-Find-Rule-0.09
## Number-Compare-0.01
## Text-Glob-0.05

## To install:
## perl -MCPAN -e shell
## cpan>install File::Find::Rule
## cpan>install Number::Compare
## cpan>install Text::Glob
## cpan>exit

## To run:
## Ensure that the script is executable
## chmod a+x myth.rebuilddatabase.pl
## ./myth.rebuilddatabase.pl

use strict;
use DBI;
use Getopt::Long;
use File::Find::Rule;

## get command line args

my ($database, $host, $user, $pass, $verbose, $dir);

my $argc=@ARGV;
if ($argc == 0) {
   print "usage:  myth.rebuilddatabase.pl --dir /path/to/video/ [options]

Where [options] is:
--host          - hostname of the mysql server (default: \"192.168.1.103\")
--user          - DBUSERNAME (default: \"mythtv\")
--pass          - DBPASSWORD (default: \"mythtv\")
--database      - DATABASENAME (default: \"mythconverg\")
";
exit(0);
}

GetOptions('verbose+'=>\$verbose, 'database=s'=>\$database, 'host=s'=>\$host, 'user=s'=>\$user, 'pass=s'=>\$pass, 'dir=s'=>\$dir);


if (!$host) { $host="192.168.1.103"; }
if (!$database) { $database="mythconverg"; }
if (!$user) { $user="mythtv"; }
if (!$pass) { $pass="mythtv"; }
if (substr($dir,0,1) ne "\/") {
		 print "--dir must be a fully qualified path (i.e: must begin with a \"\/\")\n";
		 exit;
}
if (substr($dir,length($dir)-1,1) ne "/") {
		 $dir = $dir."/";
		 print "Added trailing slash to ($dir)\n\n";
}

my $dbh = DBI->connect("dbi:mysql:database=$database:host=$host","$user","$pass") or
		 die "Cannot connect to database ($!)\n";
my $q = "select title, subtitle, starttime, endtime, chanid from recorded order by starttime";
my $sth = $dbh->prepare($q);
$sth->execute or die "Could not execute ($q)\n";

print "\nYour myth database ($database) is reporting the following programs as being recorded:\n\n";

my ($starttime, $endtime, $title, $subtitle, $channel);
my ($syear, $smonth, $sday, $shour, $sminute, $ssecond, $eyear, $emonth, $eday, $ehour, $eminute, $esecond);

while (my @row=$sth->fetchrow_array) {
		 $title = $row[0];
		 $subtitle = $row[1];
		 $starttime = $row[2];
		 $endtime = $row[3];
		 $channel = $row[4];

		 ## get the pieces of the time
		 $syear = substr($starttime,0,4); $smonth = substr($starttime,4,2); $sday = substr($starttime,6,2);
		 $shour = substr($starttime,8,2); $sminute = substr($starttime,10,2); $ssecond = substr($starttime,12,2);
		 
		 $eyear = substr($endtime,0,4); $emonth = substr($endtime,4,2); $eday = substr($endtime,6,2);
		 $ehour = substr($endtime,8,2); $eminute = substr($endtime,10,2); $esecond = substr($endtime,12,2);

##		 print "Channel $channel\t$smonth/$sday/$syear $shour:$sminute:$ssecond - $ehour:$eminute:$esecond - $title ($subtitle)\n";
		 print "Channel:    $channel\n";
		 print "Start time: $smonth/$sday/$syear - $shour:$sminute:$ssecond\n";
		 print "End time:   $emonth/$eday/$eyear - $ehour:$eminute:$esecond\n";
		 print "Title:      $title\n";
		 print "Subtitle:   $subtitle\n\n";
}

print "\nThese are the files stored in ($dir) and will be checked against\n";
print "your database to see if the exist.  If they do not, you will be prompted\n";
print "for a title and subtitle of the entry, and a record will be created.\n\n";

my $count = 0;

## trying to find any .nuv files
my @files = File::Find::Rule->file()
                            ->name( '*.nuv' )
                            ->in( $dir );

foreach my $show (@files) {

		 ## remove the $dir prefix from the filename
		 $show = substr($show, length($dir));

		 ## clear enddate so we can check if the .nuv file is properly named
		 $emonth = ""; $eday = ""; $eyear = "";

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

		 ## make sure it is a valid file name
		 if ($emonth eq "" or $eday eq "" or $eyear eq "") {
		 		 ## file is a bust
		 } else {
		 		 ## check if this is already in the db
		 		 ## second hit to the db i know, but performance is not a big issue here

		 		 $q = "select chanid, starttime, endtime from recorded where chanid=$channel and starttime='$syear$smonth$sday$shour$sminute$ssecond' and endtime='$eyear$emonth$eday$ehour$eminute$esecond'";
		 		 $sth = $dbh->prepare($q);
		 		 $sth->execute or die "Could not execute ($q)\n";

		 		 my $exists=0;
		 		 while (my @row=$sth->fetchrow_array) {
		 		 		 $exists=1;
		 		 }

		 		 if ($exists) {
		 		 		 print "Found a match between file and database ($show)\n\n";
		 		 } else {
		 		 		 print "Found an orphaned file, creating database record\n";
		 		 		 print "Channel:       $channel\n";
		 		 		 print "Start time:    $smonth/$sday/$syear - $shour:$sminute:$ssecond\n";
		 		 		 print "End time:      $emonth/$eday/$eyear - $ehour:$eminute:$esecond\n";

		 		 		 print "Enter title:   ";
		 		 		 chomp (my $newtitle = <STDIN>);
		 
		 		 		 print "Enter subtite: ";
		 		 		 chomp (my $newsubtitle = <STDIN>);
		 
		 		 		 ## add records to db
		 		 		 my $i = "insert into recorded (chanid, starttime, endtime, title, subtitle) values ($channel, '$syear$smonth$sday$shour$sminute$ssecond', '$eyear$emonth$eday$ehour$eminute$esecond', '$newtitle', '$newsubtitle')";
		 		 		 $sth = $dbh->prepare($i);
		 		 		 $sth->execute or die "Could not execute ($i)\n";
		 		 } ## if
		 } ## if
} ## foreach loop
