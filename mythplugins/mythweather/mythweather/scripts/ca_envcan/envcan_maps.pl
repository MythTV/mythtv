#!/usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4
# 
# Static satellite map grabber for Environment Canada.
#
# This script downloads satellite map data from the Environment Canada 
# website.  It uses the lists of JPEG images supplied by the page at
# http://www.weatheroffice.gc.ca/satellite/index_e.html.
#
# The bulk of the code in this script was originally authored by 
# Lucien Dunning (ldunning@gmail.com).
# Based on envcan_animaps.pl by Joe Ripley <vitaminjoe@gmail.com>

use strict;
use warnings;

use English;
use File::Path;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)), 
        '/usr/share/mythtv/mythweather/scripts/ca_envcan', 
        '/usr/local/share/mythtv/mythweather/scripts/ca_envcan';

use Getopt::Std;
use LWP::Simple;
use Date::Manip;
use ENVCANMapSearch;
use Image::Magick;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'ENVCAN-Static-Map';
my $version = 0.1;
my $author = 'Gavin Hurlbut';
my $email = 'gjhurlbu@gmail.com';
my $updateTimeout = 10*60;
my $retrieveTimeout = 30;
my @types = ('smdesc', 'updatetime', 'map', 'copyright');
my $dir = "/tmp/envcan";

getopts('Tvtlu:d:');

if (defined $opt_v) {
    print "$name,$version,$author,$email\n";
    exit 0;
}

if (defined $opt_T) {
    print "$updateTimeout,$retrieveTimeout\n";
    exit 0;
}
if (defined $opt_l) {
    my $search = shift;
    ENVCANMapSearch::AddSatSearch($search);
    ENVCANMapSearch::AddSatClassSearch($search);
    ENVCANMapSearch::AddImageTypeSearch($search);
    foreach my $result (@{ENVCANMapSearch::doSearch()}) {
        print "$result->{entry_id}::($result->{satellite_class}) $result->{satellite} $result->{image_type}\n";
    }
    exit 0;
}

if (defined $opt_t) {
    foreach (@types) {print; print "\n";}
    exit 0;
}

if (defined $opt_d) {
    $dir = $opt_d;
}

if (!-d $dir) {
    mkpath( $dir, {mode => 0755} );
}

my $loc = shift;

if (!defined $loc || $loc eq "") {
    die "Invalid usage";
}

# Get map info
ENVCANMapSearch::AddAniSearch($loc);
my $results = ENVCANMapSearch::doSearch();
my $desc    = $results->[0]->{satellite}; 

# Get HTML and find image list
my $response = get $results->[0]->{animated_url};
die unless defined $response;

my @image_list;
my $size;
my $base_url = "http://www.weatheroffice.gc.ca";
my $file     = $loc;
my $path     = "$dir/$file";

# Get list of images (at most 10)
foreach my $line (split(/\n/, $response)) {
    if ($line =~ /theImagesComplete\[\d*\] \= \"(.*)\"\;/) {
        push (@image_list, $1);
        if ($#image_list >= 1) { shift @image_list; }
    }
}

# Download map file, if necessary (maps are stale after 15 minutes)
my $image = shift @image_list;
my $ext = $image;
$ext =~ s/.*\.(.*?)$/$1/;

my $cached = 0;
if (-f "$path.$ext" ) {
    my @stats = stat(_);
    if ($stats[9] > (time - 900)) { 
        $cached = 1;
    }
} 

if (!$cached) {
    getstore($base_url . $image, "$path.$ext");
}

print "smdesc::$desc\n";
print "map::$path.$ext\n";
print "updatetime::Last Updated on " . 
      UnixDate("now", "%b %d, %I:%M %p %Z") . "\n";
print "copyright::Environment Canada\n";
