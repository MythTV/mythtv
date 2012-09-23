#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

use strict;
use warnings;

use utf8;
use encoding 'utf8';
use English;

use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/uk_metoffice',
        '/usr/local/share/mythtv/mythweather/scripts/uk_metoffice';

use XML::Simple;
use LWP::Simple;
use Getopt::Std;
use File::Path;
use Switch;

use DateTime;
use Date::Parse;
use Date::Calc qw(Day_of_Week);
use File::Basename;
use lib dirname($0);
use MetOffCommon qw(:DEFAULT $apikey);

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d, $opt_D);

my $name = 'MetOffice-Forecast-API';
my $version = 0.3;
my $author = 'Stuart Morgan';
my $email = 'smorgan@mythtv.org';

my $updateInterval = 3*60*60; # 3 Hours
my $retrieveTimeout = 30;
my @types = ('3dlocation', '6dlocation', 'station_id', 'copyright',
             'copyrightlogo', 'weather_icon', 'updatetime',
             'date-0', 'icon-0', 'low-0', 'high-0',
             'date-1', 'icon-1', 'low-1', 'high-1',
             'date-2', 'icon-2', 'low-2', 'high-2',
             'date-3', 'icon-3', 'low-3', 'high-3',
             'date-4', 'icon-4', 'low-4', 'high-4',
             'date-5', 'icon-5', 'low-5', 'high-5');
my $dir = "/tmp/uk_metoffice";
my $logdir = "/tmp/uk_metoffice";

binmode(STDOUT, ":utf8");

if (!-d $logdir) {
    mkpath( $logdir, {mode => 0755} );
}

getopts('Tvtlu:d:');

if (defined $opt_v) {
    print "$name,$version,$author,$email\n";
    log_print( $logdir, "-v\n" );
    exit 0;
}

if (defined $opt_T) {
    print "$updateInterval,$retrieveTimeout\n";
    log_print( $logdir, "-t\n" );
    exit 0;
}

if (defined $opt_d) {
    $dir = $opt_d;
}

if (!-d $dir) {
    mkpath( $dir, {mode => 0755} );
}

if (defined $opt_l) {
     my $search = shift;
     log_print( $logdir, "-l $search\n" );
     my @results = MetOffCommon::Search($search, $dir, $updateInterval, $logdir);
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

my $units = "SI";
if (defined $opt_u) {
    $units = $opt_u;
}

my $locid = shift;
# we get here, we're doing an actual retrieval, everything must be defined
#my $locid = MetOffLocation::FindLoc(shift, $dir, $updateInterval, $logdir);
if (!(defined $locid && !$locid eq "")) {
    die "Invalid usage";
}

my $base_args = '?res=daily&key=' . $MetOffCommon::api_key;

# BEGIN workaround for API bug, may be removed when fixed, although it does no
# harm and possibly some benefits to it such as 6 days instead of the default
# 5
my $start_date = DateTime->now()->set_hour(0)->set_minute(0)->set_second(0);
if (DateTime->now()->hour() >= 18) {
 $start_date->add( days => 1 );
}
my $end_date = $start_date->clone();
$end_date->add( days => 5 );
$base_args = $base_args . '&valid-from=' . $start_date . '&valid-to=' . $end_date;
# END workaround

my $url = "";

if ($locid =~ s/^(\d*)/$1/)
{
    $url = $MetOffCommon::forecast_url . $1 . $base_args;
}
else
{
    die "Invalid Location ID";
}


my $response = get $url;
die unless defined $response;

my $xml = XMLin($response);

if (!$xml) {
    die "Not xml";
}

printf "copyright::" . $MetOffCommon::copyright_str . "\n";
#printf "copyrightlogo::" . $MetOffCommon::copyright_logo . "\n";
printf "station_id::" . $locid . "\n";
my $location = $xml->{DV}->{Location}->{name};
printf "3dlocation::" . $location . "\n";
printf "6dlocation::" . $location . "\n";
printf "updatetime::" . localtime() . "\n";

my $i = 0;
my $period;

foreach $period (@{$xml->{DV}->{Location}->{Period}}) {

    if (ref($period->{Rep}) ne 'ARRAY') {
        next;
    }

    my ($ss,$mm,$hh,$day,$month,$year,$zone) = strptime($period->{value});
    $year += 1900; # Returns year as offset from 1900
    $month += 1; # Returns month starting at zero
    my $dow = Day_of_Week($year,$month,$day);
    $dow = $dow == 7 ? 0 : $dow;
    printf "date-" . $i . "::" . $dow . "\n";

    my $daytime = $period->{Rep}[0];
    my $nighttime = $period->{Rep}[1];
    
    my $iconname = "unknown";
    switch ($daytime->{W}) { # Weather Type
        case  0  { $iconname = "fair"; }  # Clear sky
        case  1  { $iconname = "sunny"; }  # Sunny
        case  2  { $iconname = "pcloudy"; }  # Partly cloudy (night)
        case  3  { $iconname = "pcloudy"; }  # Sunny intervals
        case  4  { $iconname = "dusthaze"; }  # Dust
        case  5  { $iconname = "fog"; }  # Mist
        case  6  { $iconname = "fog"; }  # Fog
        case  7  { $iconname = "mcloudy"; }  # Medium-level cloud
        case  8  { $iconname = "cloudy"; }  # Low-level cloud
        case  9  { $iconname = "lshowers"; }  # Light rain shower (night)
        case  10  { $iconname = "lshowers"; }  # Light rain shower (day)
        case  11  { $iconname = "lshowers"; }  # Drizzle
        case  12  { $iconname = "lshowers"; }  # Light rain
        case  13  { $iconname = "showers"; }  # Heavy rain shower (night)
        case  14  { $iconname = "showers"; }  # Heavy rain shower (day)
        case  15  { $iconname = "showers"; }  # Heavy rain
        case  16  { $iconname = "rainsnow"; }  # Sleet shower (night)
        case  17  { $iconname = "rainsnow"; }  # Sleet shower (day)
        case  18  { $iconname = "rainsnow"; }  # Sleet
        case  19  { $iconname = "rainsnow"; }  # Hail shower (night)
        case  20  { $iconname = "rainsnow"; }  # Hail shower (day)
        case  21  { $iconname = "rainsnow"; }  # Hail
        case  22  { $iconname = "flurries"; }  # Light snow shower (night)
        case  23  { $iconname = "flurries"; }  # Light snow shower (day)
        case  24  { $iconname = "flurries"; }  # Light snow
        case  25  { $iconname = "snowshow"; }  # Heavy snow shower (night)
        case  26  { $iconname = "snowshow"; }  # Heavy snow shower (day)
        case  27  { $iconname = "snowshow"; }  # Heavy snow
        case  28  { $iconname = "thunshowers"; }  # Thundery shower (night)
        case  29  { $iconname = "thunshowers"; }  # Thundery shower (day)
        case  30  { $iconname = "thunshowers"; }  # Thunder storm
        case  31  { $iconname = "thunshowers"; }  # Tropical storm
        case  33  { $iconname = "dusthaze"; }  # Haze
    }

    printf "icon-" . $i . "::" . $iconname . ".png\n";

    printf "low-" . $i . "::" . MetOffCommon::convert_temp($nighttime->{Nm}, $units) . "\n";
    printf "high-" . $i . "::" .  MetOffCommon::convert_temp($daytime->{Dm}, $units) . "\n";

    $i++;
}

sub log_print {
    return if not defined $::opt_D;
    my $dir = shift;

    open OF, ">>$dir/uk_metoffice.log";
    print OF @_;
    close OF;
}
