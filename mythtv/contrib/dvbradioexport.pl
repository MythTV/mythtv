#!/usr/bin/perl -w
# dvbradioexport.pl v2.0
# By Justin Hornsby 22 October 2006
# 
# Storage Group Support by Stuart Auchterlonie  28 April 2007
#
# A MythTV user job script for exporting DVB radio recordings to MP3 format
#
# Usage info will be output if the script is run with no arguments (or insufficient arguments)
#
# Contains elements of mythtv.pl by Nigel Pearson
# Initial idea from nuvexport
# 
# requirements: id3tag, ffmpeg with mp3 exporting enabled, PERL and the DBI & DBD::mysql modules
#               MythTV perl bindings.
#

# PERL MODULES WE WILL BE USING
use DBI;
use DBD::mysql;
use MythTV;

my $exportdir = '/home/mythtv/';
my $maxbitrate = '256';
my $bitrate = '192';

$connect = undef;
$debug = 0;

##################################
#                                #
#    Main code starts here !!    #
#                                #
##################################

$usage = "\nHow to use dvbradioexport.pl \n\ndvbradioexport.pl exportdir=/foo/bar starttime=%STARTTIME% 
chanid=%CHANID% maxbitrate=x debug\n"
        ."\n%CHANID% = channel ID associated with the recording to export\n"
        ."%STARTTIME% = recording start time in either 'yyyy-mm-dd hh:mm:ss' or 'yyyymmddhhmmss' format\n"
        ."exportdir = dir to export completed MP3 files to (note the user the script runs as must have write permission on that 
dir)\n"
        ."maxbitrate = maximum bitrate for the export to use.  If more than the original file's bitrate, the original 
bitrate will be used \n"
        ."debug = enable debugging information - outputs which commands would be run etc\n";

# get this script's ARGS
#

$num = $#ARGV + 1;

# if user hasn't passed enough arguments, die and print the usage info

if ($num le "2") {
	die "$usage";
}

#
# Get all the arguments
#

foreach (@ARGV){
    if ($_ =~ m/debug/) {
        $debug = 1;
    }
    elsif ($_ =~ m/maxbitrate/) {
        $maxbitrate = (split(/\=/,$_))[1];
    }
    elsif ($_ =~ m/starttime/) {
        $starttime = (split(/\=/,$_))[1];
    }
    elsif ($_ =~ m/chanid/) {
        $chanid = (split(/\=/,$_))[1];
    }
    elsif ($_ =~ m/exportdir/) {
        $exportdir = (split(/\=/,$_))[1];
    }
}

# connect to backend
my $myth = new MythTV();
# connect to database
$connect = $myth->{'dbh'};

# PREPARE THE QUERY
$query = "SELECT title, subtitle, basename FROM recorded WHERE chanid=$chanid AND starttime='$starttime'";
$query_handle = $connect->prepare($query);
$query_handle->execute() || die "Cannot connect to database \n";

# BIND TABLE COLUMNS TO VARIABLES
$query_handle->bind_columns(undef, \$title, \$subtitle, \$basename);

# LOOP THROUGH RESULTS
$query_handle->fetch();

my $schemaVer = $myth->backend_setting('DBSchemaVer');
# Storage Groups were added in DBSchemaVer 1171
# FIND WHERE THE RECORDINGS LIVE
my $dir = 'UNKNOWN';
if ($schemaVer < 1171)
{
    if ($debug) {
        print ("Using compatibility mode\n");
    }
    $dir = $myth->backend_setting('RecordFilePrefix');
}
else
{
    if ($debug) {
        print ("Going into new mode\n");
    }
    my $storagegroup = new MythTV::StorageGroup();
    $dir = $storagegroup->FindRecordingDir($basename);
}

# FIND OUT THE CHANNEL NAME
$query = "SELECT name FROM channel WHERE chanid=$chanid";
$query_handle = $connect->prepare($query);
$query_handle->execute()  || die "Unable to query settings table";

my ($channame) = $query_handle->fetchrow_array;

# replace whitespace in channame with dashes
$channame =~ s/\s+/-/g;

# replace whitespace in title with underscores
$title =~ s/\W+/_/g;
# replace whitespace in subtitle with underscores
$subtitle =~ s/\W+/_/g;

#
# Remove non alphanumeric chars from $starttime & $endtime
#
 
$newstarttime = $starttime;

$newstarttime =~ s/[|^\W|\s|-|]//g;

$year = substr($newstarttime, 0, 4);

$filename = $dir."/".$basename;

$newfilename = $exportdir."/".$channame."_".$title."_".$subtitle."_".$newstarttime.".mp3";

if ($debug)
{
    print "\n\n Source filename:$filename \nDestination filename:$newfilename\n \n";
}
#
# Now run ffmpeg to find out what bitrate the stream is
#

$origbitrate = -1;

$output = `ffmpeg -i $filename 2>&1`;

$output =~s/^.*?Audio.*?, (\d*) kb\/s.*$/$1/s;

print $output . "\n";

if ($output =~ /^\d+$/) {
    $origbitrate = $output;
}

#
# If maximum bitrate is less than the source bitrate, allow the max bitrate to remain
#

$bitrate = $maxbitrate;

if (($origbitrate != -1) && ($maxbitrate > $origbitrate)) {
    print "maxbitrate is greater than original.  Using source bitrate to save space\n";
    $bitrate = $origbitrate;
}

$bitrate .= "K";

#
# Now run ffmpeg to get mp3 out of the dvb radio recording
#

$command = "nice -n19 ffmpeg -i $filename -ab $bitrate -ac 2 -acodec mp3 -f mp3 '$newfilename' 2>&1";

if ($debug) {
    print "\n\nUSING $command \n\n";
}

system "$command";

# Now tag the finished MP3 file

$command = "id3tag --song='$title' --album='$subtitle' --artist='$channame' --comment='Transcoded by MythTV' --year=$year '$newfilename'";

if ($debug) {
    print "\n\nUsing $command \n\n";
}
system "$command";
