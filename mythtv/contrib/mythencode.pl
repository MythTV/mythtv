#!/usr/bin/perl
##
## Script to automate some of the encoding tasks associated with
## Getting MythTV files into a nice DIVX .AVI file.
##
## Hack and Slash done by Rob Snow (rsnow@dympna.com)
##
## 15 Apr 03	1.0	Hacked
## 16 Apr 03	1.0.1	Remove .log file when we start (may not be needed)
## 16 Apr 03	1.1	Added nice support and autoscale/fps support
##
## TODO:
## 1) Fine tune the --autoscale function
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

##use strict;
use Getopt::Long;
use File::Basename;


#
# Constant
# These should be changed to point to your values
#
$mythname="/usr/local/bin/mythname.pl -host localhost -s -rep=.";
$mencoder="./mencoder-cvs";

#
# get command line args and printout some usage if no args 
#
my $argc=@ARGV;
if ($argc == 0) {
   print "usage:  mythencode.pl [options] 
/path/to/store/1001_20030401190000_20030401200000.nuv

Where [options] is:
--hours [3.0]		- Hours per 700MB CD.  Can be a float 
			  (i.e. --hours 2.25 for 2:15) (default: 3 hours)
--nice [10]		- Nice level to run mencoder at (default: 10)
--cdsize [700]		- Size of CD to create for 
			  (185, 650, 700 are obvious choices) (default: 700)
--ratio [0.8]		- Ratio of Video to total bitrate (default: 0.8)
			  (lower to add more audio and less video)
--1			- 1 pass (default: 2 pass)
--test			- Show values that would be used and exit
--getname		- Get name with mythname.pl script (default: no)
--scale [320:240]	- Set output scale by hand (default: 320:240)
--autoscale		- Set to autopick scale/fps based on bitrate
--fps [30]		- Set output fps (default: 30)
";
exit(0);
}
GetOptions('hours=f'=>\$hours, 'nice:i'=>\$nice, 'cdsize=f'=>\$cdsize, 
'ratio=f'=>\$ratio, '1+'=>\$passes, 'test+'=>\$test, 'getname+'=>\$getname, 
'scale=s'=>\$scale, 'autoscale+'=>\$auto, 'fps=s'=>\$fps);

#
# Set some default values if not set via commandline
#
if (!$cdsize) { $cdsize=700.0; }
if (!$ratio) { $ratio=0.8; }
if (!$passes) { $passes=2; }
if (!$hours) { $hours=3; }
if ($nice==0) { $nice=10; }
if (!$scale) { $scale="320:240"; }
if (!$fps) { $fps=30; }

#
#Remove the divx2pass.log file, probably doesn't need to be done.
#
unlink "divx2pass.log";

#
# Parse out the filename
#
($show, $path, $suffix) = fileparse(@ARGV[0],"\.nuv");

#
# Do some computations and print out the debugging output
# This is messy
#
print "cdsize=$cdsize\n";
$max_bits=$cdsize*2.285;
print "hours=$hours\n";
print "nice=$nice\n";
print "max_br=$max_bits\n";
$total_br=($max_bits/$hours);
print "total_br=$total_br\n";
$vid_br=int($total_br*$ratio);
$aud_br=int($total_br-$vid_br);
print "video_br=$vid_br\n";
print "audio_br=$aud_br\n";
print "passes=$passes\n";
print "file=$show\n";
print "path=$path\n";

# 
# Set the output filename to the input filename first, then modify it
# if we are using the external call to the mythname.pl script.
# 
$outname=$show;
if ($getname) {
		  $getnamestr="$mythname $path$show$suffix";
		  print "string to execute=$getnamestr\n";
		  $outname=`$getnamestr`;
		  chomp $outname;
}
print "outname=$outname\n";

#
# Setup the nice string so we can put it into the rest of the command line
# without having to add additional cases
#
$nicestr="";
if ($nice) {
		  $nicestr="nice $nice ";
}

#
# Set a scale if autoscale has been chosen
#
if ($auto) {
		  if ($vid_br > 600) { $scale="400:300"; }
		  elsif ($vid_br > 400) { $scale="320:240"; }
		  elsif ($vid_br > 250) { $scale="256:192"; }
		  elsif ($vid_br > 175) { $scale="192:144"; }
		  elsif ($vid_br > 150) { $scale="192:144"; $fps=20; }
		  elsif ($vid_br > 100) { $scale="160:120"; $fps=20; }
		  else { $scale="160:120"; $fps=15; }
		  print "scale=$scale\n";
}

#
# Print out some bppixel info
#
@res=split(/:/, $scale);
print "xs=$res[0] ys=$res[1]\n";
$pps=($res[0]*$res[1])*$fps;
print "pixels/sec=$pps\n";
$bps=($vid_br*1024);
print "bits/sec=$bps\n";
$bpp=$bps/$pps;
print "bitsec/pixel=$bpp\n";


#
# A case based on number of passes
#
if ($passes==1) {

		  print "Doing a one pass encode.\n";
		  $pass1="$nicestr $mencoder -idx $path$show.nuv -fps $fps -oac mp3lame 
-lameopts vbr=3:br=$aud_br -ovc lavc -lavcopts 
vcodec=mpeg4:vbitrate=$vid_br:vhq:v4mv -vop lavcdeint,scale=$scale -aspect 4:3 -o $outname.avi";
		  print "\tPASS ONE=$pass1\n";

		  if(!$test) {
			exec $pass1;
		  }

} else {

		  print "Doing a two pass encode.\n";
		  $pass1="$nicestr $mencoder -idx $path$show.nuv -fps $fps -oac mp3lame 
-lameopts vbr=3:br=$aud_br -ovc lavc -lavcopts 
vcodec=mpeg4:vbitrate=$vid_br:vpass=1 -vop lavcdeint,scale=$scale -aspect 4:3 -o $outname.avi";
		  $pass2="$nicestr $mencoder -idx $path$show.nuv -fps $fps -oac mp3lame 
-lameopts vbr=3:br=$aud_br -ovc lavc -lavcopts 
vcodec=mpeg4:vbitrate=$vid_br:vhq:v4mv:vpass=1 -vop lavcdeint,scale=$scale -aspect 4:3 -o $outname.avi";
		  print "\tPASS ONE=$pass1\n";
		  print "\tPASS TWO=$pass2\n";

		  if(!$test) {
			exec $pass1;
			exec $pass2;
		  }
}

exit(0);
