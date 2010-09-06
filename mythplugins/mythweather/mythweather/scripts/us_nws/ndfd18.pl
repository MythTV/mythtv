#!/usr/bin/perl
use strict;
use warnings;

use English;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/us_nws',
        '/usr/local/share/mythtv/mythweather/scripts/us_nws';

use NDFDParser;
use NWSLocation;
use Data::Dumper;
use Getopt::Std;
use Date::Manip;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'NDFD-18_Hour';
my $version = 0.2;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ('18hrlocation',  'updatetime', 
        'temp-0', 'temp-1', 'temp-2', 'temp-3', 'temp-4', 'temp-5',
        '18icon-0', '18icon-1', '18icon-2', '18icon-3', '18icon-4', '18icon-5',
        'pop-0', 'pop-1', 'pop-2', 'pop-3', 'pop-4', 'pop-5',
        'time-0', 'time-1', 'time-2', 'time-3', 'time-4', 'time-5', 'copyright');
my $dir = './';
my $icon_file = dirname(abs_path($0 or $PROGRAM_NAME)) . "/icons";

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
            print "$result->{latitude},$result->{longitude}::";
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

my $locstr = shift;
my $units = $opt_u;
my ($latitude, $longitude) = getLocation($locstr);
if (!(defined $opt_u && defined $latitude && defined $longitude
            && $latitude ne "" && $longitude ne "")) {
    die "Invalid Usage";
}

my $param = { maxt => 0,
    mint =>0,
    temp =>1,
    dew=>0,
    pop12=>1,
    qpf=>0,
    sky=>0,
    snow=>0,
    wspd=>0,
    wdir=>0,
    wx=>0, 
    waveh=>0,
    icons=>1,
    rh=>0,
    appt=>0 };

my $d1 = UnixDate("now", "%O");
my $d2 = UnixDate(DateCalc($d1, "+ 18 hours"), "%O");
my $result;
my $creationdate;
my $nextupdate;
my $getData = 1;
    
if (open (CACHE, "$dir/ndfd18_cache_${latitude}_${longitude}")) {
    ($nextupdate, $creationdate) = split / /, <CACHE>;
    # We don't have to check the start/end dates, since we get the same chunk
    # every time, and we update the cache atleast every hour, which is how often the
    # data is updated by the NWS.
    if (Date_Cmp($nextupdate, "now") > 0) { # use cache
        no strict "vars"; # because eval doesn't scope var correctly
        $result = eval <CACHE>;
        if ($result) {
            $getData = 0;
        } else { 
            print STDERR "Error parsing cache $@\n";
        };
    }

} 

if ($getData) {
    ($result, $creationdate) = NDFDParser::doParse($latitude, $longitude, $d1, $d2, $param);
    # output cache
    open(CACHE, ">$dir/ndfd18_cache_${latitude}_${longitude}") or 
        die "cannot open cache ($dir/ndfd18_cache_${latitude}_${longitude}) for writing";
    $Data::Dumper::Purity = 1;
    $Data::Dumper::Sortkeys = 1;
    $Data::Dumper::Indent = 0;
    # NDFD is updated by 45 minutes after the hour, we'll give them until 50 to
    # make sure
    my $min = UnixDate("now", "%M");
    my $newmin;
    if ($min < 50) {
        $newmin = 50-$min;
    } else {
        $newmin = 60-($min-50);
    }
    $nextupdate = DateCalc("now", "+ $newmin minutes");
    print CACHE UnixDate($nextupdate, "%O ") . UnixDate("now", "%O\n");
    print CACHE Dumper($result);
}
my $index = 0;
my $icon;
printf "updatetime::Last Updated on %s\n", 
       UnixDate($creationdate, "%b %d, %I:%M %p %Z");
printf "copyright::National Digital Forecast Database\n";
my $pop12;
foreach my $time (sort keys %$result) {
    if (defined $result->{$time}->{'probability-of-precipitation_12 hour'}) {
        $pop12 = $result->{$time}->{'probability-of-precipitation_12 hour'};
        next;
    }

    print "time-${index}::" . UnixDate($time, "%i %p\n");
    if ($units eq 'SI') {
        $result->{$time}->{temperature_hourly} =
            int( (5/9) * ($result->{$time}->{temperature_hourly}-32));
    }
    print "temp-${index}::$result->{$time}->{temperature_hourly}\n";
    print "pop-${index}::$pop12 %\n";
    $icon = $result->{$time}->{'conditions-icon_forecast-NWS'};
    $icon =~ s/.*\/([a-z0-9_]+[.][j][p][g])/$1/;
    local *FH;
    open(FH, $icon_file) or die "Cannot open icons";
    while(my $line = <FH>) {
        if ($line =~ /${icon}::/) {
            $line =~ s/.*:://;
            print "18icon-${index}::$line";
            last;
        }
    }
    ++$index > 5 && last;

}

# This script will accept locations that are either station ids, or latitude
# longitude.  This is because I haven't decided which to use yet :)
sub getLocation {
    my $str = shift;

    $str =~ tr/[a-z]/[A-Z]/;
    my $lat;
    my $lon;

    if ($str =~ m/[A-Z]{4,4}/) { # station id form
        NWSLocation::AddStationIdSearch($str);

    } else { # hopefully lat/lon 
        ($lat, $lon) = split /,/, $str;
        $lat =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[N]/+$1/ or
            $lat =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[S]/-$1/;
        $lon =~ s/(\d{1,3}[.](\d{1,3})?)([.]\d{1,3})?[E]/+$1/ or
            $lon =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[W]/-$1/;
        NWSLocation::AddLatLonSearch($lat, $lon);
    }

    my $results = NWSLocation::doSearch($str);
    if ($lat && $lon && !$results) {
        # didn't find a matching station
        print "18hrlocation::$lat,$lon\n";
        return ($lat, $lon);
    }

    # Should be one result in array
    my $location = $results->[0];
    $lat = $location->{latitude};
    $lon = $location->{longitude};
    if ($lat eq 'NA' || $lon eq 'NA') {
        # maybe scrape them from website, since they are there, annoying that
        # they aren't all in the XML file, gotta love the U.S. Gov :)
        die "Latitude and Longitude do not exist for $str";
    }
    print "18hrlocation::$location->{station_name}, $location->{state}\n";

    return ($lat, $lon);
}
