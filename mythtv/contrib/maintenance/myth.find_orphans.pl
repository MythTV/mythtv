#!/usr/bin/perl

# check for recording anomolies -
#   based somewhat on greg froese's "myth.rebuilddatabase.pl"
#   -- Lincoln Dale <ltd@interlink.com.au>, September 2006
# 2007-03-11:  Added pretty print of unknown files vs. orphaned thumbnails. (Robert Kulagowski)

#  The intent of this script is to be able to find orphaned rows in the 'recorded' table
#  (entries which don't have matching media files) and orphaned media files (potentially
#  taking up gigabytes of otherwise usable disk space) which have no matching row in
#  the 'recorded' db table.
#
#  By default, running the script will simply return a list of problems it finds.
#  Running with --dodbdelete will remove db recorded rows for which there is no matching
#  media file.  Running with --dodelete will delete media files for which there is no
#  matching db record.
#
#  This script may be useful to fix up some orphaned db entries (causes mythweb to run
#  verrry slow) as well as reclaim some disk space from some orphaned media files.
#  (in an ideal world, neither of these would ever happen, but i've seen both happen in reality).
#  This script makes it easy to keep track of whether it has or hasn't happened, even if you
#  have thousands of recordings and terabytes of stored media.
#
#  no warranties expressed or implied.  if you run this and it deletes all your recordings
#  and sets mythtv to fill up all your disk space with The Home Shopping Network, its entirely
#  your fault.

my $progname = "myth.find_orphans.pl";
my $revision = "0.20";

use DBI;
use Sys::Hostname;
use Getopt::Long;

#
# options
#

my $opt_host =		hostname;
my $opt_dbhost = 	$opt_host;
my $opt_database = 	"mythconverg";
my $opt_user = 		"mythtv";
my $opt_pass = 		"mythtv";
my $opt_ext = 		"{nuv,mpg,mpeg,avi}";
my $opt_dir =		"";
my $opt_dodelete =	0;
my $opt_dodbdelete =	0;
my $debug =		0;
my $opt_help =		0;

GetOptions(
        'host=s'	=> \$opt_host,
        'dbhost=s'	=> \$opt_dbhost,
        'database=s'	=> \$opt_database,
        'user=s'	=> \$opt_user,
        'pass=s'	=> \$opt_pass,
	'dir=s'		=> \$opt_dir,
	'dodelete'	=> \$opt_dodelete,
	'dodbdelete'	=> \$opt_dodbdelete,
        'debug+'	=> \$debug,
        'help'		=> \$opt_help,
        'h'		=> \$opt_help,
        'v'		=> \$opt_help);

if ($opt_help) {
	print<<EOF
$progname (rev $revision)
(checks MythTV recording directories for orphaned files)

options:
	--host=(host)		MythTV backend host ($opt_host)
	--dbhost=(host)		host where MySQL database for backend is ($opt_dbhost)
	--database=(dbname)	MythTV database ($opt_database)
	--user=(user)		MySQL MythTV database user ($opt_user)
	--pass=(pass)		MySQL MythTV database password ($opt_pass)
	--dir=directories	manually specify recording directories (otherwise setting is from database)
	--debug			increase debug level
	--dodbdelete		remove recorded db entries with no matching file (default: don't)
	--dodelete		delete files with no record (default: don't)

EOF
;
	exit(0);
}

#
# go go go!
#

my $valid_recordings = 0;
my $missing_recordings = 0;
my $errors = 0;
my $unknown_files = 0;
my $known_files = 0;
my $unknown_size = 0;
my $known_size = 0;
my $unknown_thumbnail = 0;

if (!($dbh = DBI->connect("dbi:mysql:database=$opt_database:host=$opt_dbhost","$opt_user","$opt_pass"))) {
	die "Cannot connect to database $opt_database on host $opt_dbhost: $!\n";
}

if ($opt_dir eq "") {
	&dir_lookup("SELECT dirname FROM storagegroup WHERE hostname=(?)");
	&dir_lookup("SELECT data FROM settings WHERE value='RecordFilePrefix' AND hostname=(?)");

	printf STDERR "Recording directories ($opt_host): $opt_dir\n" if $debug;
}

if ($opt_dir eq "") {
	printf "ERROR: no directory found or specified\n";
	exit 1;
}

foreach $d (split(/,/,$opt_dir)) {
	$d =~ s/\/$//g; # strip trailing /
	$dirs{$d}++;
}


#
# look in recorded table, make sure we can find every file ..
#

my $q = "SELECT title, subtitle, starttime, endtime, chanid, basename FROM recorded WHERE hostname=(?) ORDER BY starttime";
$sth = $dbh->prepare($q);
$sth->execute($opt_host) || die "Could not execute ($q): $!\n";

while (my @row=$sth->fetchrow_array) {
	($title, $subtitle, $starttime, $endtime, $channel, $basename) = @row;

	# see if we can find it...
	$loc = find_file($basename);
	if ($loc eq "") {
		printf "Missing media: %s (title:%s, start:%s)\n",$basename,$title,$starttime;
		$missing_recordings++;

		if ($opt_dodbdelete) {
			my $sql = sprintf "DELETE FROM recorded WHERE basename LIKE \"%s\" LIMIT 1",$basename;
			printf "performing database delete: %s\n",$sql;
			$dbh->do($sql) || die "Could not execute $sql: $!\n";
		}
	} else {
		$valid_recordings++;
		$seen_basename{$basename}++;
		$seen_basename{$basename.".png"}++; # thumbnail
	}
}

#
# look in recording directories, see if there are extra files not in database
#

foreach my $this_dir (keys %dirs) {
	opendir(DIR, $this_dir) || die "cannot open directory $this_dir: $!\n";
	foreach $this_file (readdir(DIR)) {
		if (-f "$this_dir/$this_file") {

			next if ($this_file eq "nfslockfile.lock");

			my $this_filesize = -s "$this_dir/$this_file";
			if ($seen_basename{$this_file} == 0) {
				$sorted_filesizes{$this_filesize} .= sprintf "unknown file [%s]: %s/%s\n",pretty_filesize($this_filesize),$this_dir,$this_file;
				$unknown_size += $this_filesize;
				if (substr($this_file,-4) eq ".png") {
					$unknown_thumbnail++;
				}
				else {
					$unknown_files++;
				}
				                                                                         
				if ($opt_dodelete) {
					printf STDERR "deleting  [%s]:  %s/%s\n",pretty_filesize($this_filesize),$this_dir,$this_file;
					unlink "$this_dir/$this_file";

					if (-f "$this_dir/$this_file") {
						$errors++;
						printf "ERROR: could not delete $this_dir/$this_file\n";
					}
				}
			} else {
				$known_files++;
				$known_size += $this_filesize;
				printf "KNOWN file [%s]: %s/%s\n",pretty_filesize($this_filesize),$this_dir,$this_file if $debug;
			}
		} else {
			printf "NOT A FILE: %s/%s\n",$this_dir,$this_file if $debug;
		}
	}
	closedir DIR;
}


#
# finished, report results
#

foreach my $key (sort { $a <=> $b } keys %sorted_filesizes) {
	printf $sorted_filesizes{$key};
}

printf "Summary:\n";
printf "  Host: %s, Directories: %s\n", $opt_host, join(" ",keys %dirs);
printf "  %d ERRORS ENCOUNTERED (see above for details)\n",$errors if ($errors > 0);
printf "  %d valid recording%s, %d missing recording%s %s\n",
	$valid_recordings, ($valid_recordings != 1 ? "s" : ""),
	$missing_recordings, ($missing_recordings != 1 ? "s" : ""),
	($missing_recordings > 0 ? ($opt_dodbdelete ? "were fixed" : "not fixed, check above is valid and use --dodbdelete to fix") : "");
printf "  %d known media files using %s\n  %d orphaned thumbnails with no corresponding recording\n  %d unknown files using %s %s\n",
	$known_files, pretty_filesize($known_size), 
	$unknown_thumbnail,$unknown_files, pretty_filesize($unknown_size), 
	($unknown_files > 0 ? ($opt_dodelete ? "were fixed" : "not fixed, check above and use --dodelete to clean up if the above output is accurate") : "");

exit(0);

###########################################################################
# filesize bling

sub pretty_filesize
{
	local($fsize) = @_;
	return sprintf "%0.1fGB",($fsize / 1000000000) if ($fsize >= 1000000000);
	return sprintf "%0.1fMB",($fsize / 1000000) if ($fsize >= 1000000);
	return sprintf "%0.1fKB",($fsize / 1000) if ($fsize >= 1000);
	return sprintf "%0.0fB",$fsize;
}

###########################################################################
# find a file in directories without globbing

sub find_file
{
	local($fname) = @_;

	foreach my $d (keys %dirs) {
		my $f = $d."/".$fname;
		if (-e $f) {
			return $f;
		}
	}
	return;
}

###########################################################################

sub dir_lookup
{
	my $query = shift;

	$sth = $dbh->prepare($query);
	$sth->execute($opt_host) || die "Could not execute ($dir_query)";
	while (my @row = $sth->fetchrow_array) {
		$opt_dir .= "," if ($opt_dir ne "");
		$opt_dir .= $row[0];
	}
}

###########################################################################
