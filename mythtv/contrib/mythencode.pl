#!/usr/bin/perl
##
## Script to automate some of the encoding tasks associated with
## Getting MythTV files into a nice DIVX .AVI file.
##
## Hack and Slash done by Rob Snow (rsnow@dympna.com)
##
## 15 Apr 03  1.0  	Hacked
## 16 Apr 03  1.0.1  	Remove .log file when we start (may not be needed)
## 16 Apr 03  1.1  	Added nice support and autoscale/fps support
## 18 Jun 03  1.1.1 	Added patch by Andrew Albinger to fix ' in strings
## 24 Jun 03  1.2	Large patch by Mike Nugent to make things right
##			Now runs under strict and added debug option
##			Also allows a hostname to be passed to mythname
## 2003-07-16 1.3       Added quality options to one pass
##
## TODO:
## 1) Fine tune the --autoscale function
##
## use at your own risk, i am not responsible for anything this program may
## or may not do.

use strict;
use Getopt::Long;
use File::Basename;

#
# Constant
# These should be changed to point to your values
#
use constant mythname => '/usr/local/bin/mythname.pl -s -rep=. ';
use constant mencoder => '/usr/local/bin/mencoder';

#
# get command line args and printout some usage if no args 
#
my $argc=@ARGV;
if ($argc == 0) {
   print qq{usage:  mythencode.pl [options] /path/to/store/1001_20030401190000_20030401200000.nuv

Where [options] is:
--hours [3.0]\t\t- Hours per 700MB CD.  Can be a float 
\t\t\t(i.e. --hours 2.25 for 2:15) (default: 3 hours)
--nice [10]\t\t- Nice level to run mencoder at (default: 10)
--cdsize [700]\t\t- Size of CD to create for 
\t\t\t(185, 650, 700 are obvious choices) (default: 700)
--ratio [0.8]\t\t- Ratio of Video to total bitrate (default: 0.8)
\t\t\t(lower to add more audio and less video)
--1\t\t\t- 1 pass (default: 2 pass)
--test\t\t\t- Show values that would be used and exit
--getname\t\t- Get name with mythname.pl script (default: no)
--namehost\t\t- db where mythname.pl should look
--scale [320:240]\t- Set output scale by hand (default: 320:240)
--autoscale\t\t- Set to autopick scale/fps based on bitrate
--fps [30]\t\t- Set output fps (default: 30)
--debug\t\t\t- Dump some debugging output
};
exit(0);
}

my ($hours, $nice, $cdsize, $ratio, $passes, $test, $getname, $namehost);
my ($scale, $auto, $fps, $debug);

GetOptions('hours=f'      =>\$hours,
           'nice:i'       =>\$nice,
           'cdsize=f'     =>\$cdsize, 
           'ratio=f'      =>\$ratio,
           '1+'           =>\$passes,
           'test+'        =>\$test,
           'getname+'     =>\$getname,
           'namehost=s'   =>\$namehost, 
           'scale=s'      =>\$scale,
           'autoscale+'   =>\$auto,
           'fps=s'        =>\$fps,
           'debug'        =>\$debug,
          );

#
# Set some default values if not set via commandline
#
if ($nice==0) { $nice=10; }

if (!$cdsize) { $cdsize=700.0; }
if (!$ratio)  { $ratio=0.8; }
if (!$passes) { $passes=2; }
if (!$hours)  { $hours=3; }
if (!$scale)  { $scale="320:240"; }
if (!$fps)    { $fps=30; }

#
#Remove the divx2pass.log file, probably doesn't need to be done.
#
unlink "divx2pass.log";

#
# Parse out the filename
#
my ($show, $path, $suffix) = fileparse(@ARGV[0],"\.nuv");

#
# Do some computations and print out the debugging output
# This is messy
#

my $max_bits=$cdsize*2.285;
my $total_br=($max_bits/$hours);
my $vid_br=int($total_br*$ratio);
my $aud_br=int($total_br-$vid_br);

if ($debug) {
  warn "cdsize:\t$cdsize\n";
  warn "hours:\t$hours\n";
  warn "nice:\t$nice\n";
  warn "max_br:\t$max_bits\n";
  warn "total_br:\t$total_br\n";
  warn "video_br:\t$vid_br\n";
  warn "audio_br:\t$aud_br\n";
  warn "passes:\t$passes\n";
  warn "file:\t$show\n";
  warn "path:\t$path\n";
  warn "suffix:\t$suffix";
}

# 
# Set the output filename to the input filename first, then modify it
# if we are using the external call to the mythname.pl script.
# 
my $outname=$show;
if ($getname) {
  my $mythname = mythname;
  if ($namehost) {
    $mythname .= ' -h ' . $namehost;
  }

  my $getnamestr= $mythname . ' ' . $path . $show . $suffix;

  if ($debug) {
    warn "string to execute:\t$getnamestr\n";
  }
  $outname=`$getnamestr`;
  $outname = quotemeta($outname);
  chomp $outname;
}
if ($debug) {
  warn "outname:\t$outname\n";
}

#
# Setup the nice string so we can put it into the rest of the command line
# without having to add additional cases
#
my $nicestr;
if ($nice) {
  $nicestr="/bin/nice -$nice ";
}
else {
  $nicestr="/bin/nice";
}

#
# Set a scale if autoscale has been chosen
#
if ($auto) {
  if ($vid_br > 600)    { $scale="400:300"; }
  elsif ($vid_br > 400) { $scale="320:240"; }
  elsif ($vid_br > 250) { $scale="256:192"; }
  elsif ($vid_br > 175) { $scale="192:144"; }
  elsif ($vid_br > 150) { $scale="192:144"; $fps=20; }
  elsif ($vid_br > 100) { $scale="160:120"; $fps=20; }
  else                  { $scale="160:120"; $fps=15; }

  if ($debug) {
    warn "scale:\t$scale\n";
  }
}

#
# Print out some bppixel info
#

my @res=split(/:/, $scale);
my $pps=($res[0]*$res[1])*$fps;
my $bps=($vid_br*1024);
my $bpp=$bps/$pps;

if ($debug) {
  warn "xs:\t$res[0] ys:\t$res[1]\n";
  warn "pixels/sec:\t$pps\n";
  warn "bits/sec:\t$bps\n";
  warn "bitsec/pixel:\t$bpp\n";
}


#
# A case based on number of passes
#
if ($passes==1) {
  print "Doing a one pass encode.\n";
  my $pass1="$nicestr " . mencoder . " -idx $path$show.nuv -fps $fps " .
  "-oac mp3lame -lameopts vbr=3:br=$aud_br -ovc lavc " .
  "-lavcopts  vcodec=mpeg4:vbitrate=$vid_br:vhq:v4mv:vpass=1 " .
  "-vop lavcdeint,scale=$scale -aspect 4:3 -o $outname.avi";
  print "\tPASS ONE=$pass1\n";

  if(!$test) {
    system $pass1;
  }
}
else {
  print "Doing a two pass encode.\n";
  my $pass1="$nicestr " . mencoder . " -idx $path$show.nuv -fps $fps " .
  "-oac mp3lame -lameopts vbr=3:br=$aud_br -ovc lavc -lavcopts " .
  "vcodec=mpeg4:vbitrate=$vid_br:vpass=1 -vop lavcdeint,scale=$scale " .
  "-aspect 4:3 -o $outname.avi";

  my $pass2="$nicestr " . mencoder . " -idx $path$show.nuv -fps $fps " .
  "-oac mp3lame -lameopts vbr=3:br=$aud_br -ovc lavc -lavcopts " .
  "vcodec=mpeg4:vbitrate=$vid_br:vhq:v4mv:vpass=2 -vop lavcdeint," .
  "scale=$scale -aspect 4:3 -o $outname.avi";
  print "\tPASS ONE=$pass1\n";
  print "\tPASS TWO=$pass2\n";

  if(!$test) {
    exec $pass1;
    exec $pass2;
  }
}

exit(0);

