#!/usr/bin/perl
##
## Script to extract show name and subtitle from DB given the filename.
##
## Hack and Slash done by Rob Snow (rsnow@dympna.com)
##
## 28 Mar 03	1.0	Hack
## 29 Mar 03	1.1	Added --legal to fix filenames for / and \
##			Should probably be fixed for other chars
##			but I'm too lazy.
## 14 Nov 05    2.0 Make it work with 0.18 and up
## 17 Nov 05    2.1 Fix a bug with the path for --all
##
## This is a very nasty hack of myth.rebuilddatabase.pl which was nicely 
## done by Greg Froese and instructions by Robert Kulagowski.  
##
## Those fine gentlemens information may be found below, however, please
## do not confuse them with the author of this hack...they do nice work.
##
## written by greg froese (g_froese@yahoo.com)
## install instructions by Robert Kulagowski (rkulagow@rocketmail.com)
## Much hacking by Behan Webster <behanw@websterwood.com>
##   Who also added some code from epair_optimize.pl by xris
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

##use strict;
use DBI;
use Getopt::Long;
use File::Basename;
use File::stat qw(:FIELDS);;

## get command line args

sub usage() {
	print <<END;
Usage:  $0 [options] files

Where [options] are:
  --host          - hostname of the mysql server (default: \"127.0.0.1\")
  --user          - DBUSERNAME (default: \"mythtv\")
  --pass          - DBPASSWORD (default: \"mythtv\")
  --database      - DATABASENAME (default: \"mythconverg\")
  --replace       - Replace spaces with this string (--rep=. will return Daily.Show)
  --subtitle      - Add subtitle to string after a ':'
  --sublen        - Maximum subtitle length (only useful with -subtitle)
  --legal         - Make sure the filename is legal (no '/', '\', etc.)
  --all           - Consider all files
  --channel       - Print channel
  --codec         - Print codec
  --description   - Print description
  --extension     - Extension to add to title (defaults to same as filename)
  --file          - Print filename
  --grep          - Search title:subtitle
  --mpeg {2,4}    - Only show files of mpeg2 or mpeg4 encoding
  --quiet         - Don't print title
  --size          - Show size of show (mins, size, size/h)
  --total         - Print total size of listed files
END
	exit 0;
}

my $host="127.0.0.1";
my $database="mythconverg";
my $user="mythtv";
my $pass="mythtv";
my $mythdir = '/var/lib/mythtv/';

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
				$host = $val;
			} elsif ($var eq 'DBUserName') {
				$user = $val;
			} elsif ($var eq 'DBName') {
				$database = $val;
			} elsif ($var eq 'DBPassword') {
				$pass = $val;
			}
		}
		close CONF;
	}
	#die "Unable to locate mysql.txt:  $!\n\n" unless ($found && $dbhost);

usage if !GetOptions('verbose+'=>\$verbose, 'help'=>\$help, 'quiet'=>\$quiet,
'database=s'=>\$database, 'host=s'=>\$host, 'user=s'=>\$user, 'pass=s'=>\$pass,
'subtitle'=>\$sub, 'sublen=s'=>\$sublen, 'replace=s'=>\$rep, 'legal'=>\$legal,
'file'=>\$printfile, 'codec'=>\$printcodec, 'channel'=>\$printchan,
'size'=>\$printsize, 'description'=>\$printdesc, 'mpeg=i'=>\$mpeg,
'all'=>\$all, 'extension:s'=>\$extension, 'grep=s'=>\$grep, 'total'=>\$printtotal,
);
usage if $help;

my @files;
if ($all) {
	opendir(DIR, $mythdir) || die "$mythdir: $!";
	@files = grep {/(mpg|nuv)$/} readdir DIR;
	closedir DIR;
} else {
	@files = @ARGV;
}
usage unless @files;

my $dbh = DBI->connect("dbi:mysql:database=$database:host=$host","$user","$pass")
	|| die "Cannot connect to database ($!)\n";

my $sql = 'SELECT title, subtitle, chanid, starttime, endtime, description FROM recorded WHERE basename=?';
my $sth = $dbh->prepare($sql);
my $chansql = 'SELECT channum, callsign FROM channel WHERE chanid=?';
my $sthchan = $dbh->prepare($chansql);
for my $file (@files) {
	($show, $path, $suffix)  = fileparse($file , '.nuv');

	$sth->execute("$show$suffix") || die "Could not execute ($sql)\n";
	my ($title, $subtitle, $chanid, $start, $end, $description) = $sth->fetchrow_array;
	$sthchan->execute($chanid) || die "Could not execute ($chansql)\n";
	my ($channum, $callsign) = $sthchan->fetchrow_array;

	unless ($title) {
		print "$file is not in the database\n";
		next;
	}

	if ($rep) {
		$subtitle=~s/\ /$rep/gio;
		$title=~s/\ /$rep/gio;
	}

	if ($legal) {
		my $good = " ";
		if ($rep) {
			$good = $rep;
		}
		$title =~ s/[\\\/'"()]/$good/gio;
		$subtitle =~ s/[\\\/'"()]/$good/gio;
	}

	if ($sublen) {
		$subtitle=substr($subtitle,0,$sublen);
	}
	
	my $print = 1;
	if ($mpeg || $printcodec || $printsize || $printtotal) {
		require Time::Piece;
		my $before = Time::Piece->strptime($start, '%Y-%m-%d %H:%M:%S');
		my $after  = Time::Piece->strptime($end,   '%Y-%m-%d %H:%M:%S');
		my $diff = $after - $before;
		stat($file) || stat("$mythdir/$file") || die "Can't find $file: $!";
		$mins = $diff->minutes;
		$gb = $st_size/1024/1024/1024;
		$gbh = $gb * 60 / $mins;

		# This is a bit of a hack, but recording from an ivtv card, and then transcoding it works.
		$codec = $gbh >= 1.1 ? 2 : 4;

		$print = 0 if $mpeg && $mpeg ne $codec;
		#print "MPEG:$mpeg $gbh $print\n";
	}

	if ($grep) {
		$print = 0 unless $title =~ /$grep/ || $subtitle && $subtitle =~ /$grep/;
		#print "Grep: $print $grep $title\n";
	}

	my $ext;
	if (defined $extension) {
		$file =~ /\.(.*?)$/;
		$ext = $extension || $1;
	}

	if ($print) {
		print "$file " if $printfile;
		print "mpeg$codec " if $printcodec;
		print $title unless $quiet;
		print ":$subtitle" if $sub && $subtitle;
		print ".$ext" if $ext;
		print " on $callsign($chanid)" if $printchan;
		printf " (%d mins in %.3f GB %.3f GB/h)", $mins, $gb, $gbh if $printsize;
		print "\n";
		print " $description\n" if $printdesc;

		$tgb += $gb;
		$tt  += $mins;
		$tn  += 1;
	}
}

if ($printtotal) {
	printf "%.3f GB for %.1f hours in %d files\n", $tgb, $tt/60, $tn;
}

exit(0);

# vim: sw=4 ts=4
