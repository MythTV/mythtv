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

my $name = 'BBC-Current-XML';
my $version = 0.2;
my $author = 'Stuart Morgan';
my $email = 'stuart@tase.co.uk';
my $updateTimeout = 120*60;
# 2 Hours, BBC updates infrequently ~3 hours
# Given that the option to update in the background now exists
# potentially we could be hitting the server 12 times in a day
my $retrieveTimeout = 30;
my @types = ('cclocation', 'station_id', 'copyright',
        'observation_time', 'weather', 'temp', 'relative_humidity',
        'wind_dir', 'pressure', 'visibility', 'weather_icon',
        'appt', 'wind_spdgst');
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
my $base_xml = '/ObservationsRSS.xml';

if ($locid =~ s/^(\d*)/$1/)
{
    $base_url = $base_url . $1 .$base_xml;
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

# The required elements which aren't provided by this feed
printf "appt::NA\n";

printf "copyright::From bbc.co.uk\n";
printf "station_id::" . $locid . "\n";
my $location = $xml->{channel}->{title};
$location =~ s/.*?Observations for (.*)$/$1/s;
printf "cclocation::" . $location . "\n";

my $item_title = $xml->{channel}->{item}->{title};

my $obs_time = $1 if ($item_title =~ /(^.*)\:.*/);
printf "observation_time::" . $obs_time . "\n";
my $weather_string = $item_title;

$weather_string =~ s/.*\:.*\n(.*)\..*/$1/s;
$weather_string = ucfirst($weather_string);
printf "weather::" . $weather_string . "\n";

if ($weather_string =~ /^cloudy$/i     ||
    $weather_string =~ /^grey cloud$/i ||
    $weather_string =~ /^white cloud$/i) {
    printf "weather_icon::cloudy.png\n";
}
elsif ($weather_string =~ /^fog$/i ||
    $weather_string =~ /^foggy$/i  ||
    $weather_string =~ /^mist$/i   ||
    $weather_string =~ /^misty$/i) {
    printf "weather_icon::fog.png\n";
}
elsif ($weather_string =~ /^sunny$/i) {
    printf "weather_icon::sunny.png\n";
}
elsif ($weather_string =~ /^sunny intervals$/i ||
    $weather_string =~ /^partly cloudy$/i) {
    printf "weather_icon::pcloudy.png\n";
}
elsif ($weather_string =~ /^drizzle$/i ||
    $weather_string =~ /^light rain$/i ||
    $weather_string =~ /^light rain showers?$/i ||
    $weather_string =~ /^light showers?$/i) {
    printf "weather_icon::lshowers.png\n";
}
elsif ($weather_string =~ /^heavy rain$/i  ||
    $weather_string =~ /^heavy showers?$/i ||
    $weather_string =~ /^heavy rain showers?$/i) {
    printf "weather_icon::showers.png\n";
}
elsif ($weather_string =~ /^thundery rain$/i ||
    $weather_string =~ /^thunder storm$/i    ||
    $weather_string =~ /^thundery showers?$/i) {
    printf "weather_icon::thunshowers.png\n";
}
elsif ($weather_string =~ /^heavy snow$/i) {
    printf "weather_icon::snowshow.png\n";
}
elsif ($weather_string =~ /^light snow$/i ||
    $weather_string =~ /^light snow showers?$/i) {
    printf "weather_icon::flurries.png\n";
}
elsif ($weather_string =~ /^sleet$/i ||
    $weather_string =~ /^sleet showers?$/i ||
    $weather_string =~ /^hail showers?$/i) {
    printf "weather_icon::rainsnow.png\n";
}
elsif ($weather_string =~ /^clear$/i ||
       $weather_string =~ /^clear sky$/i) {
    printf "weather_icon::fair.png\n";
}
else {
    printf "weather_icon::unknown.png\n";
}

my @data = split(/, /, $xml->{channel}->{item}->{description});
foreach (@data) {
    my $datalabel;
    my $datavalue;

    ($datalabel, $datavalue) = split(': ', $_);
    if ($datalabel =~ /Temperature/) {
        if ($units =~ /ENG/) {
            $datavalue =~ s/^.*?\((-?\d{1,2}).*/$1/;
        }
        elsif ($units =~ /SI/) {
            $datavalue =~ s/^(-?\d{1,2}).*/$1/;
        }
        $datalabel = "temp";
    }
    elsif ($datalabel =~ /Wind Direction/) {
            $datalabel = "wind_dir";
    }
    elsif ($datalabel =~ /Wind Speed/) {
        $datalabel = "wind_spdgst";
        $datavalue =~ s/^(\d{1,3})mph.*/$1/;

        if ($units =~ /SI/) {
            $datavalue = int($datavalue * 1.609344 + .5);
        }

        $datavalue = $datavalue . " (NA)";
    }
    elsif ($datalabel =~ /Relative Humidity/) {
        $datalabel = "relative_humidity";
        $datavalue =~ s/^(\d{1,3})%.*?/$1/;
    }
    elsif ($datalabel =~ /Pressure/) {
        $datavalue =~ s/^(\d*)mB.*?/$1/;

        if ($units =~ /ENG/) {
            $datavalue = $datavalue * 0.0295301 + .5;
        }

        $datalabel = "pressure";
    }
    elsif ($datalabel =~ /Visibility/) {
        $datalabel = "visibility";
        if ($datavalue =~ /^Very Poor/i) {
            $datavalue = "< 1";
        }
        elsif ($datavalue =~ /^Poor/i) {
            $datavalue = "1-4";
        }
        elsif ($datavalue =~ /^Moderate/i) {
            $datavalue = "4-10";
        }
        elsif ($datavalue =~ /^Good/i) {
            $datavalue = "10-20";
        }
        elsif ($datavalue =~ /^Very Good/i) {
            $datavalue = "20-40";
        }
        elsif ($datavalue =~ /^Excellent/i) {
            $datavalue = "40+";
        }
        else {
            $datavalue = "?";
        }
    }
    else {
        next;
    }

    printf $datalabel . "::" . $datavalue . "\n";
}
