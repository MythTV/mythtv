#!/usr/bin/perl -w
# MythWeather-revamp script to retreive weather information from Environment 
# Canada.
#
# Most of this code was taken directly from Lucien Dunning's 
# (ldunning@gmail.com) PERL scripts.  Kudos to Lucien for doing all of the
# hard work that I shamelessly stole.
#
# TODO Code clean up and organization

use strict;
use LWP::Simple;
use Date::Manip;
use Getopt::Std;
use ENVCANLocation;
use ENVCANParser;
use Data::Dumper;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d);

my $name = 'ENVCAN';
my $version = 0.4;
my $author  = 'Joe Ripley';
my $email   = 'vitaminjoe@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ('cclocation', 'station_id', 'copyright',
            'observation_time', 'observation_time_rfc822', 'weather',
            'temp', 'relative_humidity', 
            'wind_dir', 'wind_degrees', 'wind_speed', 'wind_gust',
            'pressure', 'dewpoint', 'heat_index', 'windchill',
            'visibility', 'weather_icon', 'appt', 'wind_spdgst',
            '3dlocation', '6dlocation', 'date-0', 'icon-0', 'low-0', 'high-0',
            'date-1', 'icon-1', 'low-1', 'high-1',
            'date-2', 'icon-2', 'low-2', 'high-2', 'updatetime',
            'date-3', 'icon-3', 'low-3', 'high-3',
            'date-4', 'icon-4', 'low-4', 'high-4',
            'date-5', 'icon-5', 'low-5', 'high-5' );

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
    ENVCANLocation::AddStationIdSearch($search);
    ENVCANLocation::AddRegionIdSearch($search);
    ENVCANLocation::AddCitySearch($search);
    ENVCANLocation::AddProvinceSearch($search);
    my $results = doSearch();
    my $result;
    while($result = shift @$results) {
        if ($result->{station_id} ne "NA" ) {
            print "$result->{station_id}::";
            print "$result->{city}, $result->{region_id}\n";
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

# check variables for defined status 
my $loc = shift;
if (!(defined $opt_u && defined $loc && !$loc eq "")) {
    die "Invalid usage";
}

my $units = $opt_u;

# check for cached data
my $creationdate;
my $nextupdate;
my %results;
my $getData = 1;
if (open(CACHE, "$dir/envcan_$loc")) {
    ($nextupdate, $creationdate) = split / /, <CACHE>;
    if (Date_Cmp($nextupdate, "now") > 0) { # use cache
        no strict "vars";
        %results = eval <CACHE>;

        if (%results) { $getData = 0; }
        else { print STDERR "Error parsing cache $@\n"; }
    }
}
close(CACHE);

# no cache, grab from the web
if ($getData) {
    my $base_url = 'http://www.weatheroffice.gc.ca/rss/city/';
    my $response = get $base_url . $loc .'_e.xml';
    die unless defined $response;

    %results = ENVCANParser::doParse($response, @types);
    $results{'station_id'} = $loc;

    # output cache
    open (CACHE, ">$dir/envcan_$loc") or 
        die ("Cannot open cache ($dir/envcan_$loc) for writing.");
    $Data::Dumper::Purity   = 1;
    $Data::Dumper::Indent   = 0;

    # cache is good for 15 minutes
    my $newmin = 15;

    $nextupdate = DateCalc("now", "+ $newmin minutes");
    print CACHE UnixDate($nextupdate, "%O ") . UnixDate("now", "%O\n");
    print CACHE Data::Dumper->Dump([\%results], ['*results']);
}

# do some quick conversions
if ($units eq "ENG") { 
    $results{'temp'}       = int(((9/5) * $results{'temp'}) + 32);
    $results{'dewpoint'}   = int(((9/5) * $results{'dewpoint'}) + 32);
    $results{'windchill'}  = int(((9/5) * $results{'windchill'}) + 32);
    $results{'appt'}       = int(((9/5) * $results{'appt'}) + 32);
    $results{'visibility'} = sprintf("%.1f", ($results{'visibility'} * 0.621371192));
    $results{'pressure'}   = sprintf("%.2f", $results{'pressure'} * 0.0295301);
    $results{'wind_gust'}  = sprintf("%.2f", $results{'wind_gust'} * 0.621371192);
    $results{'wind_speed'} = sprintf("%.2f", $results{'wind_speed'} * 0.621371192);
    $results{'wind_spdgst'} = sprintf("%.2f (%.2f)", $results{'wind_speed'}, $results{'wind_gust'});
    
    for (my $i=0;$i<6;$i++) {
        if ($results{"high-$i"} =~ /\d*/) {
            $results{"high-$i"} = int(((9/5) * $results{"high-$i"}) + 32);
        }
        if ($results{"low-$i"} =~ /\d*/) {
            $results{"low-$i"} = int(((9/5) * $results{"low-$i"}) + 32);
        }
    }
} else {
    $results{'wind_spdgst'} = sprintf("%.2f (%.2f)", $results{'wind_speed'}, $results{'wind_gust'});
}
    

foreach my $key (sort (keys %results)) {
    print "$key". "::";
    if (length($results{$key}) == 0) {
        print "NA\n";
    } else {
        print $results{$key} ."\n";
    }
}

