#! /usr/bin/perl

#TODO the icons aren't very meaningful, the server gives them to us for 3 or 6
# hr intervals, but since we're parsing for 12 hour, that seem a little useless

use English;
use strict;
use warnings;

use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/us_nws',
        '/usr/local/share/mythtv/mythweather/scripts/us_nws';

use Data::Dumper;
use NDFDParser;
use NWSLocation;
use Date::Manip;
use Getopt::Std;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'NDFD-6_day';
my $version = 0.3;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ('3dlocation', '6dlocation',  'updatetime', 
        'high-0', 'high-1', 'high-2', 'high-3', 'high-4', 'high-5',
        'low-0', 'low-1', 'low-2', 'low-3', 'low-4', 'low-5',
        'icon-0', 'icon-1', 'icon-2', 'icon-3', 'icon-4', 'icon-5',
        'date-0', 'date-1', 'date-2', 'date-3', 'date-4', 'date-5', 'copyright');
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

my $param = { maxt => 1,
    mint =>1,
    temp =>0,
    dew=>0,
    pop12=>0,
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

my $d1 = UnixDate("today at 8:00am", "%O");
my $d2 = UnixDate(DateCalc($d1, "+ 168 hours"), "%O");
my $result;
my $creationdate;
my $nextupdate;
my $getData = 1;
if (open (CACHE, "$dir/ndfd_cache_${latitude}_${longitude}")) {
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
    open(CACHE, ">$dir/ndfd_cache_${latitude}_${longitude}") or 
        die "cannot open cache ($dir/ndfd_cache_${latitude}_${longitude}) for writing";
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

my $lowindex = 0;
my $hiindex = 0;
my $dateindex = 0;
my $iconindex = 0;
my @dates;
my $time;
my $date;

printf "updatetime::Last Updated on %s\n", 
       UnixDate($creationdate, "%b %d, %I:%M %p %Z");

printf "copyright::National Digital Forecast Database\n";

foreach $time (sort(keys(%$result))) {
    my $date;
    if ($time =~ m/,/) {
       ($date) = split /,/, $time;
    } else {
        $date = $time;
    }

    if (Date_Cmp($date, $d1) < 0) {
        next;    
    }

    my $numdate = UnixDate($date, "%Q");
    if (!grep /$numdate/, @dates) {
        push @dates, $numdate;
    }
    my $geticon = 0;
    if ($lowindex <= 5 && $result->{$time}->{temperature_minimum}) { 
        if ($units eq 'SI') {
            $result->{$time}->{temperature_minimum} =
                int ( (5/9) * ($result->{$time}->{temperature_minimum}-32));
        }
        print "low-${lowindex}::$result->{$time}->{temperature_minimum}\n";
        $lowindex++;
    } elsif ($hiindex <= 5 && $result->{$time}->{temperature_maximum}) {
        if ($units eq 'SI') {
            $result->{$time}->{temperature_maximum} =
                int ( (5/9) * ($result->{$time}->{temperature_maximum}-32));
        }
        print "high-${hiindex}::$result->{$time}->{temperature_maximum}\n";
        $hiindex++;
        $geticon = 1;
    } 
    if ($geticon) {
        my $tz = $time;
        $tz =~ s/^.*([+-]\d{4})$/$1/;
        my $iconkey = $date;
        my $i = 0;
        my $icon;
        until ($result->{$iconkey}->{'conditions-icon_forecast-NWS'}
                || $i++ > 8) {
            $iconkey = UnixDate(DateCalc($iconkey, "+ 1 hour"), "%O").$tz;
        }
        if ($i >= 8) {
            $icon = "unknown.png";    
        } else {
            $icon = $result->{$iconkey}->{'conditions-icon_forecast-NWS'};
            $icon =~ s/.*\/([a-z0-9_]+[.][j][p][g])/$1/;
            local *FH;
            open(FH, $icon_file) or die "Cannot open icons";
            while(my $line = <FH>) {
                if ($line =~ /${icon}::/) {
                    $line =~ s/.*:://;
                    print "icon-${iconindex}::$line";
                    $iconindex++;
                    last;
                }
            }
        }
    }
}
print "high-${hiindex}::NA\n" and $hiindex++ while ($hiindex <= 5);
print "low-${lowindex}::NA\n" and $lowindex++ while ($lowindex <= 5);
print "icon-${iconindex}::unknown.png\n" and $iconindex++ while ($iconindex<= 5);

foreach $date (sort(@dates)) {
    print "date-${dateindex}::" . UnixDate($date, "%A") . "\n" 
        if ($dateindex <= 5);
    $dateindex++;
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
        print "location::$lat,$lon\n";
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
    print "3dlocation::$location->{station_name}, $location->{state}\n";
    print "6dlocation::$location->{station_name}, $location->{state}\n";

    return ($lat, $lon);
}
