#!/usr/bin/perl -w
# ipodexport.pl v1.0
# By Justin Hornsby 27 July 2007
# 
# A MythTV user job script for exporting MythTV recordings to iPod & other xvid/aac formats
#
# Usage info will be output if the script is run with no arguments (or insufficient arguments)
#
# Contains elements of mythtv.pl by Nigel Pearson
# Initial idea from nuvexport
# 
# Requirements: ffmpeg with aac & xvid support enabled, perl and the DBI & DBD::mysql modules
#                Also requires MythTV perl bindings
# 
#
# PERL MODULES WE WILL BE USING
use DBI;
use DBD::mysql;
use MythTV;

# Set default values
my $exportdir = '/home/mythtv/';
my $bitrate = '96';
my $aspect = '4:3';
my $size = '320x240';

$connect = undef;
$debug = 0;

##################################
#                                #
#    Main code starts here !!    #
#                                #
##################################

$usage = "\nHow to use dvbradioexport.pl : \n"
        ."ipodexport.pl exportdir=/foo/bar starttime=%STARTTIME% chanid=%CHANID bitrate=x size=320x240 aspect=4:3 debug\n"
        ."\n%CHANID% = channel ID associated with the recording to export\n"
        ."%STARTTIME% = recording start time in either 'yyyy-mm-dd hh:mm:ss' or 'yyyymmddhhmmss' format\n"
        ."exportdir = dir to export completed MP4 files to (note the user the script runs as must have write permission on that dir\n"
        ."size = frame size of output file.  320x240 is the default value \n"
        ."aspect = aspect ratio of output file.  Valid values are 4:3 (default) and 16:9 \n"
        ."bitrate = audio bitrate in output file in kbps.  Default value is 96 \n"
        ."debug = enable debugging information - outputs which commands would be run etc\n"
        ."\nExample: ipodexport.pl exportdir=/home/juski starttime=20060803205900 chanid=1006 size=320x340 aspect=16:9 bitrate=192 debug \n";

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
    elsif ($_ =~ m/size/) {
        $size = (split(/\=/,$_))[1];
    }
    elsif ($_ =~ m/aspect/) {
        $aspect = (split(/\=/,$_))[1];
    }
    elsif ($_ =~ m/bitrate/) {
        $bitrate = (split(/\=/,$_))[1];
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

#
#
my $myth = new MythTV();
# connect to database
$connect = $myth->{'dbh'};

# PREPARE THE QUERY
$query = "SELECT title, subtitle FROM recorded WHERE chanid=$chanid AND starttime='$starttime'";

$query_handle = $connect->prepare($query);


# EXECUTE THE QUERY
$query_handle->execute() || die "Cannot connect to database \n";

# BIND TABLE COLUMNS TO VARIABLES
$query_handle->bind_columns(undef, \$title, \$subtitle);

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

# replace non-word characters in title with underscores
$title =~ s/\W+/_/g;
# replace non-word characters in subtitle with underscores
$subtitle =~ s/\W+/_/g;

#
# Remove non alphanumeric chars from $starttime & $endtime
#
 
$newstarttime = $starttime;
$newstarttime =~ s/[|^\W|\s|-|]//g;
$filename = $dir."/".$chanid."_".$newstarttime.".mpg";
$newfilename = $exportdir."/".$channame."_".$title."_".$subtitle."_".$newstarttime.".mp4";

if ($debug) {
    print "\n\n Source filename:$filename \nDestination filename:$newfilename\n \n";
}

#
# Now run ffmpeg to get iPod compatible video with a simple ffmpeg line

$command = "nice -n19 ffmpeg -i $filename -vcodec xvid -b 300 -qmin 3 -qmax 5 -bufsize 4096 -g 300 -acodec aac -ab $bitrate -s $size -aspect $aspect '$newfilename' 2>&1";

if ($debug) {
    print "\n\nUSING $command \n\n";
}

if (!$debug) { 
    system "$command";
}
