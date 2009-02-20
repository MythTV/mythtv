#!/usr/bin/perl -w
# ============================================================================
# = NAME
# copy_and_transcode.pl
#
# = PURPOSE
# Copy a recording before transcoding, so the user can re-cut the original
#
# = USAGE
my $usage = 'Usage:
copy_and_transcode.pl -f %FILE% -p autodetect
copy_and_transcode.pl -j %JOBID% -p autodetect
copy_and_transcode.pl --file file [--debug] [--noaction]  [transcode-arguments]
';
# The first two forms would invoke this as a MythTV User Job
#
# = DESCRIPTION
# The exsisting mythtranscode system is designed for replacing a recording
# with a transcoded copy. There is a setting which causes the backend to
# keep the original file, but both the seektable and the cutlist are deleted.
# If you are using transcode to extract multiple clips from a recording,
# this is very inconvenient.
#
# This script transcodes a recording into a new recording,
# with a trivially different starttime.
# If the original recording had a cutlist, that should be honoured in the copy.
#
# = INSTALLATION
# Copy this script to a known location, and then add the command
# in mythtv-setup as a User Job (pg. 7 & 9 of General button) e.g.
#
# cp copy_and_transcode.pl /usr/local/bin
# UserJob1     /usr/local/bin/copy_and_transcode.pl -f %FILE% -p autodetect
# UserJobDesc1 Copy and Transcode
#
# = KNOWN BUGS
#
# = REVISION
# $Id$
#
# = AUTHORS
# Nigel Pearson
# ============================================================================

use strict;
use MythTV;

# What file are we copying/transcoding?
my $file  = '';
my $jobid = -1;

# do nothing?
my $noexec = 0;

# extra console output?
my $DEBUG = 1;

# some globals
my ($chanid, $command, $query, $ref, $starttime);
my ($newfilename, $newstarttime);

my $mt = new MythTV();
my $db = $mt->{'dbh'};


# ============================================================================
sub Die($)
{
   print STDERR "@_\n";
   exit -1;
}
# ============================================================================
# Parse command-line arguments, check there is something to do:
#
if ( ! @ARGV )
{   Die "$usage"  }

while ( @ARGV && $ARGV[0] =~ m/^-/ )
{
    my $arg = shift @ARGV;

    if ( $arg eq '-d' || $arg eq '--debug' )
    {   $DEBUG = 1  }
    elsif ( $arg eq '-n' || $arg eq '--noaction' )
    {   $noexec = 1  }
    elsif ( $arg eq '-j' || $arg eq '--jobid' )
    {   $jobid = shift @ARGV  }
    elsif ( $arg eq '-f' || $arg eq '--file' )
    {   $file = shift @ARGV  }
    else
    {
        unshift @ARGV, $arg;
        last;
    }
}

if ( ! $file && $jobid == -1 )
{
    print "No file or job specified. $usage";
    exit;
}

if ( $noexec )
{   print "NoExecute mode. No transcoding or SQL database changes.\n"  }

# ============================================================================
# If we were supplied a jobid, lookup chanid
# and starttime so that we can find the filename
#
if ( $jobid != -1 )
{
    $query = $db->prepare("SELECT chanid, starttime " .
                          "FROM jobqueue WHERE id=$jobid;");
    $query->execute || Die "Unable to query jobqueue table";
    $ref       = $query->fetchrow_hashref;
    $chanid    = $ref->{'chanid'};
    $starttime = $ref->{'starttime'};
    $query->finish;

    if ( ! $chanid || ! $starttime )
    {   Die "Cannot find details for job $jobid"  }

    $query = $db->prepare("SELECT basename FROM recorded " .
                          "WHERE chanid=$chanid AND starttime='$starttime';");
    $query->execute || Die "Unable to query recorded table";
    ($file) = $query->fetchrow_array; 
    $query->finish; 

    if ( ! $file )
    {   Die "Cannot find recording for chan $chanid, starttime $starttime"  }

    if ( $DEBUG )
    {
        print "Job $jobid refers to recording chanid=$chanid,",
              " starttime=$starttime\n"
    }
}
else
{
    if ( $file =~ m/(\d+)_(\d\d\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)/ )
    {   $chanid = $1, $starttime = "$2-$3-$4 $5:$6:$7"  }
    else
    {
        print "File $file has a strange name. Searching in recorded table\n";
        $query = $db->prepare("SELECT chanid, starttime " .
                              "FROM recorded WHERE basename='$file';");
        $query->execute || Die "Unable to query recorded table";
        ($chanid,$starttime) = $query->fetchrow_array;
        $query->finish;

        if ( ! $chanid || ! $starttime )
        {   Die "Cannot find details for filename $file"  }
    }
}


# A commonly used SQL row selector:
my $whereChanAndStarttime = "WHERE chanid=$chanid AND starttime='$starttime'";


# ============================================================================
# Find the directory that contains the recordings, check the file exists
#

my $dir  = undef;
my $dirs = $mt->{'video_dirs'};

foreach my $d ( @$dirs )
{
    if ( ! -e $d )
    {   Die "Cannot find directory $dir that contains recordings"  }

    if ( -e "$d/$file" )
    {
        $dir = $d;
        #print "$d/$file exists\n";
        last
    }
    else
    {   print "$d/$file does not exist\n"   }
}

if ( ! $dir )
{   Die "Cannot find recording"  }

# ============================================================================
# First, generate a new filename,
# by adding one second to the starttime until we get a unique one
#
my ($year,$month,$day,$hour,$mins,$secs) = split m/[- :]/, $starttime;
my $oldShortTime = sprintf "%04d%02d%02d%02d%02d%02d",
                   $year, $month, $day, $hour, $mins, $secs;
do
{
    $secs ++;
    if ( $secs > 59 )
    {   $secs -= 60, $mins ++  }
    if ( $mins > 59 )
    {   $mins -= 60, $hour ++  }
    if ( $hour > 23 )
    {   Die "Cannot generate a new file"  }

    $newstarttime = sprintf "%04d-%02d-%02d %02d:%02d:%02d",
                    $year, $month, $day, $hour, $mins, $secs;
    my $newShrtTm = sprintf "%04d%02d%02d%02d%02d%02d",
                    $year, $month, $day, $hour, $mins, $secs;
    $newfilename  = $file;
    $newfilename  =~ s/_$oldShortTime/_$newShrtTm/;
} while ( -e "$dir/$newfilename" );

$DEBUG && print "$dir/$newfilename seems unique\n";

# ============================================================================
# Second, do the transcode, obeying the cutlist if there is one
#
$query = $db->prepare("SELECT cutlist FROM recorded $whereChanAndStarttime;");
$query->execute || Die "Unable to query recorded table";
my ($cutlist) = $query->fetchrow_array;
$ref = $query->fetchrow_hashref;
$query->finish;

$command = "mythtranscode -c $chanid -s '$starttime' --outfile $newfilename";
if ($cutlist)
{   $command .= ' --honorcutlist'   }

if ( @ARGV )
{   $command .= ' ' . join(' ', @ARGV)  }

if ( $DEBUG || $noexec )
{   print "# $command\n"  }

if ( ! $noexec )
{
    chdir $dir;
    system $command;

    if ( ! -e "$dir/$newfilename" )
    {   Die "Transcode failed\n"  }


    # Insert the generated position map into the recorded markup table.
    # File has a structure like this:
    #
    #Type: 9
    #0 14
    #12 380942
    #
    open(MAP, "$newfilename.map") || Die "Cannot find position map file";

    my $type;
    if ( <MAP> =~ m/^Type: (\d+)$/ )
    {   $type = $1  }
    else
    {   Die "Position map file is incomplete"  }

    while ( <MAP> )
    {
        if ( m/^(\d+) (\d+)$/ )
        {
            $command = "INSERT INTO recordedseek" .
                       "        (chanid,starttime,mark,offset,type)" .
                       " VALUES ($chanid,'$newstarttime',$1,$2,$type);";

	    # This outputs too many lines for normal debugging
            #if ( $DEBUG || $noexec )
            #{   print "# $command\n"  }

            if ( ! $noexec )
            {
                $db->do($command) ||
                    Die "Couldn't insert data into recordedmarkup"
            }
        }
        else
        {   Die "Position map file has bad data line"  }
    }
    close MAP;

    unlink "$newfilename.map" || Die "Cannot delete position map file";
}

# ============================================================================
# Last, copy the existing recorded details with the new file name.
#
$query = $db->prepare("SELECT * FROM recorded $whereChanAndStarttime;");
$query->execute || Die "Unable to query recorded table";
$ref = $query->fetchrow_hashref;
$query->finish;

$ref->{'starttime'} = $newstarttime;
$ref->{'basename'}  = $newfilename;
if ( $DEBUG && ! $noexec )
{
    print 'Old file size = ' . (-s "$dir/$file")        . "\n";
    print 'New file size = ' . (-s "$dir/$newfilename") . "\n";
}
$ref->{'filesize'}  = -s "$dir/$newfilename";

my $extra = 'Copy';
if ( $cutlist )
{   $extra = 'Cut'   }

if ( $ref->{'subtitle'} !~ m/$extra$/ )
{
    if ( $ref->{'subtitle'} )
    {   $ref->{'subtitle'} .= " - $extra"  }
    else
    {   $ref->{'subtitle'} = $extra  }
}

#
# The new recording file has no cutlist, so we don't insert that field
#
my @recKeys = grep(!/^cutlist$/, keys %$ref);

#
# Build up the SQL insert command:
#
$command = 'INSERT INTO recorded (' . join(',', @recKeys) . ') VALUES ("';
foreach my $key ( @recKeys )
{
    if (defined $ref->{$key})
    {   $command .= quotemeta($ref->{$key}) . '","'   }
    else
    {   chop $command; $command .= 'NULL,"'   }
}

chop $command; chop $command;  # remove trailing comma quote

$command .= ');';

if ( $DEBUG || $noexec )
{   print "# $command\n"  }

if ( ! $noexec )
{   $db->do($command)  || Die "Couldn't create new recording's record"   }

# ============================================================================

$db->disconnect;

1;
