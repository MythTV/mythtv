#! /usr/bin/perl -w

use strict;
use XML::Simple;
use LWP::Simple;
use Data::Dumper;
use Getopt::Std;
use NWSLocation;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'NWS-XML';
my $version = 0.3;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ('cclocation', 'station_id', 'latitude', 'longitude',
        'observation_time', 'observation_time_rfc822', 'weather',
        'temperature_string', 'temp', 'relative_humidity', 'wind_string',
        'wind_dir', 'wind_degrees', 'wind_speed', 'wind_gust',
        'pressure_string', 'pressure', 'dewpoint_string', 'dewpoint',
        'heat_index_string', 'heat_index', 'windchill_string', 'windchill',
        'visibility', 'weather_icon', 'appt', 'wind_spdgst', 'copyright');
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
    NWSLocation::AddLocSearch($search);
    NWSLocation::AddStateSearch($search);
    NWSLocation::AddStationIdSearch($search);
    my $results = doSearch();
    my $result;
    while($result = shift @$results) {
        if ($result->{latitude} ne "NA" && $result->{longitude} ne "NA") {
            print "$result->{station_id}::";
            print "$result->{station_name}, $result->{state}\n";
        }
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
my $loc = shift;
if (!(defined $opt_u && defined $loc && !$loc eq "")) {
    die "Invalid usage";
}

my $units = $opt_u;

my $base_url = 'http://www.weather.gov/data/current_obs/';

my $response = get $base_url . $loc . '.xml';
die unless defined $response;

my $xml = XMLin($response);
foreach (@types) {
    my $label;
    my $key;

    $label = $_;

    if (/temp$/ || /dewpoint$/ || /heat_index$/ || /windchill$/) {
        $key = $_ . '_f' if $units =~ /ENG/;
        $key = $_ . '_c' if $units =~ /SI/;
    }
    elsif (/pressure$/) {
        $key = $_ . '_in' if $units =~ /ENG/;
        $key = $_ . '_mb' if $units =~ /SI/;
    }
    elsif (/wind_speed/) {
        if ($units =~ /ENG/) {
            $key = 'wind_mph';
        } else {
            $key = 'wind_kph';
            $xml->{$key} = int($xml->{'wind_mph'} * 1.609344 + .5);
        }
    } elsif (/wind_gust/) {
        if (defined($xml->{'wind_gust_mph'})) {
            if ($units =~ /ENG/ || $xml->{'wind_gust_mph'} eq 'NA') {
                $key = 'wind_gust_mph';
            } else {
                $key = 'wind_gust_kph';
                $xml->{$key} = int($xml->{'wind_gust_mph'} * 1.609344 + .5);
            }
        } else {
            $xml->{'wind_gust_mph'} = 'NA';
            $xml->{'wind_gust_kph'} = 'NA';
            $key = 'wind_gust';
        }
    } elsif (/visibility/) {
        if ($units =~ /ENG/) {
            $key = 'visibility_mi';
        } else {
            $key = 'visibility_km';
            $xml->{$key} = int($xml->{'visibility_mi'} * 1.609344 + .5);
        }
    } elsif (/weather_icon/) {
        $key = 'weather_icon';
        $xml->{$key} = 'unknown.png';
        local *FH;
        open(FH, "icons") or die "Cannot open icons";
        while(my $line = <FH>) {
            chomp $line;
            if ($line =~ /$xml->{'icon_url_name'}::/) {
                $line =~ s/.*:://;
                $xml->{$key} = $line;
                last;
            }
        }
    } elsif (/cclocation/) {
        $key = 'location';   
    } elsif (/appt$/) {
        if (defined($xml->{windchill_f})) {
            if ($xml->{windchill_f} eq 'NA') {
                $key = 'heat_index_f' if ($units =~ /ENG/); 
                $key = 'heat_index_c' if ($units =~ /SI/);
            } else { 
                $key = 'windchill_f' if ($units =~ /ENG/); 
                $key = 'windchill_c' if ($units =~ /SI/);
            };
        } else {
            $key = 'appt';
        }
    } elsif (/wind_spdgst/) {
        # relying on this being after speed and gust
        $key = "wind_spdgst";
        if ($units =~ /ENG/ ) {
            $xml->{$key} = "$xml->{wind_mph} ($xml->{wind_gust_mph})";
        } else {
            $xml->{$key} = "$xml->{wind_kph} ($xml->{wind_gust_kph})";
        }
    } elsif (/copyright/) {
        $key = "copyright";
        $xml->{$key} = $xml->{credit};
    } else {
        $key = $label;
    }

    print $label . "::";
    if (defined($xml->{$key})) {
        print $xml->{$key};
    } else {
        print "NA";
    }
    print "\n";
}
