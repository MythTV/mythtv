#!/usr/bin/perl
use strict;
use warnings;

use English;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/us_nws',
        '/usr/local/share/mythtv/mythweather/scripts/us_nws';

use Getopt::Std;
use LWP::Simple;
use Date::Manip;
use MapSearch;
use Data::Dumper;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'Map-Download';
my $version = 0.2;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 10*60;
my $retrieveTimeout = 30;
my @types = ('smdesc', 'updatetime', 'map', 'copyright');
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
    MapSearch::AddDescSearch(shift);
    foreach my $result (@{MapSearch::doSearch()}) {
        print "$result->{url}::$result->{description}\n";
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

#should only get one location result since its by url, assuming things is fun :)
MapSearch::AddURLSearch($loc);
my $results = MapSearch::doSearch();
my $url = $results->[0]->{url};
my $desc = $results->[0]->{description};
my $size = $results->[0]->{imgsize};
chomp $url;
chomp $desc;
my $file = $desc;
$file =~ s/[^a-zA-Z0-9]//g;
my $path = "$dir/$file";
getstore($url, $path) or $path = 'unknown.png';
if (!$size) {
    use Image::Size;
    my ($x, $y) = imgsize($path);
    $size = "${x}x$y" if ($x && $y);
}
print "smdesc::$desc\n";
printf "map::$path%s\n", ($size && "-$size" || '');
print "updatetime::Last Updated on " . UnixDate("now", "%b %d, %I:%M %p %Z") . "\n";
print "copyright::The Weather Channel Interactive, Inc.\n";
