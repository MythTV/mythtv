#! /usr/bin/perl

#
# Based on nwsxml.pl by Lucien Dunning
#

use strict;
use warnings;

use English;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/uk_bbc',
        '/usr/local/share/mythtv/mythweather/scripts/uk_bbc';

use XML::Simple;
use LWP::Simple;
# Ideally we would use the If-Modified-Since header
# to reduce server load, but they ignore it
#use HTTP::Cache::Transparent;
use Getopt::Std;
use File::Basename;
use lib dirname($0);
use BBCLocation;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d);

my $name = 'BBC-3day-XML';
my $version = 0.2;
my $author = 'Stuart Morgan';
my $email = 'stuart@tase.co.uk';
my $updateTimeout = 360*60; # 6 Hours
my $retrieveTimeout = 30;
my @types = ('3dlocation', 'station_id', 'copyright', 'weather_icon',
        'date-0', 'icon-0', 'low-0', 'high-0',
        'date-1', 'icon-1', 'low-1', 'high-1',
        'date-2', 'icon-2', 'low-2', 'high-2', 'updatetime');
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
    my @results = BBCLocation::Search($search);
    my $result;

    foreach (@results) {
        print $_ . "\n";
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


# we get here, we're doing an actual retrieval, everything must be defined
my $locid = shift;
if (!(defined $opt_u && defined $locid && !$locid eq "")) {
    die "Invalid usage";
}

my $units = $opt_u;
my $base_url = 'http://newsrss.bbc.co.uk/weather/forecast/';
my $base_xml = '/Next3DaysRSS.xml';

if ($locid =~ s/^(\d*)/$1/)
{
    $base_url = $base_url . $1 . $base_xml;
}
else
{
    die "Invalid Location ID";
}


my $response = get $base_url;
die unless defined $response;

my $xml = XMLin($response);

if (!$xml) {
    die "Not xml";
}

printf "copyright::From bbc.co.uk\n";
printf "station_id::" . $locid . "\n";
my $location = $xml->{channel}->{title};
$location =~ s/.*?Forecast for (.*)$/$1/s;
printf "3dlocation::" . $location . "\n";
printf "updatetime::Updated " . localtime() . "\n";

my $i = 0;
my $item;

foreach $item (@{$xml->{channel}->{item}}) {

    my $item_title = $item->{title};
    $item_title =~ s/\n//;

    my $day = $item_title;
    $day =~ s/^(.*?):.*/$1/;

    if ($day eq 'Sunday') {
        $day = '0';
    }
    elsif ($day eq 'Monday') {
        $day = '1';
    }
    elsif ($day eq 'Tuesday') {
        $day = '2';
    }
    elsif ($day eq 'Wednesday') {
        $day = '3';
    }
    elsif ($day eq 'Thursday') {
        $day = '4';
    }
    elsif ($day eq 'Friday') {
        $day = '5';
    }
    elsif ($day eq 'Saturday') {
        $day = '6';
    }

    printf "date-" . $i . "::" . $day . "\n";

    my $weather_string = $item_title;
    $weather_string =~ s/.*?\: (.*?),.*/$1/s;
    $weather_string = ucfirst($weather_string);

    if ($weather_string =~ /^cloudy$/i     ||
        $weather_string =~ /^grey cloud$/i ||
        $weather_string =~ /^white cloud$/i) {
        printf "icon-" . $i . "::cloudy.png\n";
    }
    elsif ($weather_string =~ /^fog$/i ||
        $weather_string =~ /^foggy$/i  ||
        $weather_string =~ /^mist$/i   ||
        $weather_string =~ /^misty$/i) {
        printf "icon-" . $i . "::fog.png\n";
    }
    elsif ($weather_string =~ /^sunny$/i) {
        printf "icon-" . $i . "::sunny.png\n";
    }
    elsif ($weather_string =~ /^sunny intervals$/i ||
        $weather_string =~ /^partly cloudy$/i) {
        printf "icon-" . $i . "::pcloudy.png\n";
    }
    elsif ($weather_string =~ /^drizzle$/i ||
        $weather_string =~ /^light rain$/i ||
        $weather_string =~ /^light rain showers?$/i ||
        $weather_string =~ /^light showers?$/i) {
        printf "icon-" . $i . "::lshowers.png\n";
    }
    elsif ($weather_string =~ /^heavy rain$/i  ||
        $weather_string =~ /^heavy showers?$/i ||
        $weather_string =~ /^heavy rain showers?$/i) {
        printf "icon-" . $i . "::showers.png\n";
    }
    elsif ($weather_string =~ /^thundery rain$/i ||
        $weather_string =~ /^thunder storm$/i    ||
        $weather_string =~ /^thundery showers?$/i) {
        printf "icon-" . $i . "::thunshowers.png\n";
    }
    elsif ($weather_string =~ /^heavy snow$/i) {
        printf "icon-" . $i . "::snowshow.png\n";
    }
    elsif ($weather_string =~ /^light snow$/i ||
        $weather_string =~ /^light snow showers?$/i) {
        printf "icon-" . $i . "::flurries.png\n";
    }
    elsif ($weather_string =~ /^sleet$/i ||
        $weather_string =~ /^sleet showers?$/i ||
        $weather_string =~ /^hail showers?$/i) {
        printf "icon-" . $i . "::rainsnow.png\n";
    }
    elsif ($weather_string =~ /^clear$/i ||
           $weather_string =~ /^clear sky$/i) {
        printf "icon-" . $i . "::fair.png\n";
    }
    else {
        printf "icon-" . $i . "::unknown.png\n";
    }

    my @data = split(/, /, $item->{description});
    foreach (@data) {
        my $datalabel;
        my $datavalue;

        ($datalabel, $datavalue) = split(': ', $_);
        if ($datalabel =~ /.*Temp$/) {
            if ($units =~ /ENG/) {
                $datavalue =~ s/^.*?\((-?\d{1,2}).*/$1/;
            }
            elsif ($units =~ /SI/) {
                $datavalue =~ s/^(-?\d{1,2}).*/$1/;
            }
            if ($datalabel =~ /^Max.*/) {
                $datalabel = "high-" . $i;
            }
            elsif ($datalabel =~ /^Min.*/) {
                $datalabel = "low-" . $i;
            }

        }
        else {
            next;
        }

        printf $datalabel . "::" . $datavalue . "\n";
    }

    $i++;
}
