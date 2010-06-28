#!/usr/bin/perl
# 
# Animated satellite map grabber for Environment Canada.
#
# This script downloads satellite map data from the Environment Canada 
# website.  It uses the lists of JPEG images supplied by the page at
# http://www.weatheroffice.gc.ca/satellite/index_e.html.
#
# The bulk of the code in this script was originally authored by 
# Lucien Dunning (ldunning@gmail.com).

use strict;
use warnings;

use English;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)), 
        '/usr/share/mythtv/mythweather/scripts/ca_envcan', 
        '/usr/local/share/mythtv/mythweather/scripts/ca_envcan';

use Getopt::Std;
use LWP::Simple;
use Date::Manip;
use ENVCANMapSearch;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'ENVCAN-Animated-Map';
my $version = 0.3;
my $author = 'Joe Ripley';
my $email = 'vitaminjoe@gmail.com';
my $updateTimeout = 10*60;
my $retrieveTimeout = 30;
my @types = ('amdesc', 'updatetime', 'animatedimage');
my $dir = "./";

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
my $path     = "$dir/$file-";

# Get list of images (at most 10)
foreach my $line (split(/\n/, $response)) {
    if ($line =~ /theImagesComplete\[\d*\] \= \"(.*)\"\;/) {
        push (@image_list, $1);
        if ($#image_list >= 10) { shift @image_list; }
    }
}

# Download map files, if necessary (maps are stale after 15 minutes)
my $i = 0;
foreach my $image (@image_list) {
    my $getImage = 1;
    if (-f $path . $i) {
        my @stats = stat(_);
        if ($stats[9] > (time - 900)) { $i++; next; }
    } 

    getstore($base_url . $image, $path . $i);
    $i++;
}

# Determine image size
if (!$size) {
    use Image::Size;
    my ($x, $y) = imgsize("${path}0");
    $size = "${x}x$y" if ($x && $y);
}

print  "amdesc::$desc\n";
printf "animatedimage::${path}%%1-$i%s\n", ($size && "-$size" || '');
print  "updatetime::Last Updated on " . UnixDate("now", "%b %d, %I:%M %p %Z") . "\n";
