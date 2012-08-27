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

use Date::Parse;
use Date::Calc qw(Day_of_Week);
use DateTime;
use DateTime::Format::ISO8601;
use File::Basename;
use lib dirname($0);
use MetOffCommon qw(:DEFAULT $apikey);

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d, $opt_D);

my $name = 'MetOffice-3Hourly-API';
my $version = 0.3;
my $author = 'Stuart Morgan';
my $email = 'smorgan@mythtv.org';

my $updateInterval = 3*60*60; # 3 Hours
my $retrieveTimeout = 30;
my @types = ('18hrlocation',
            'time-0', 'time-1', 'time-2', 'time-3', 'time-4', 'time-5',
            '18icon-0', '18icon-1', '18icon-2', '18icon-3', '18icon-4', '18icon-5',
            'temp-0', 'temp-1', 'temp-2', 'temp-3', 'temp-4', 'temp-5',
            'pop-0', 'pop-1', 'pop-2', 'pop-3', 'pop-4', 'pop-5',
            'updatetime', 'copyright', 'copyrightlogo');
             
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

my $base_args = '?res=3hourly&key=' . $MetOffCommon::api_key;
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
printf "18hrlocation::" . $location . "\n";
printf "updatetime::" . localtime() . "\n";

my $period;
my $i = 0;

foreach $period (@{$xml->{DV}->{Location}->{Period}}) {

    if (ref($period->{Rep}) ne 'ARRAY') {
        next;
    }

    foreach my $rep (@{$period->{Rep}}) {

        my $datetime = DateTime::Format::ISO8601->parse_datetime(substr($period->{value}, 0, -1));
        $datetime->add( minutes => $rep->{content} );
        
        if ($datetime <= DateTime->now()->subtract( hours => 3 )) # Ignore periods in past
        {
            next;
        }
        
        printf "time-" . $i . "::" .  MetOffCommon::format_date($datetime->epoch) . "\n";
        my $iconname = "unknown";
        switch ($rep->{W}) { # Weather Type
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

        printf "weather_type" . $i . "::" . $rep->{W} . "\n";
        printf "18icon-" . $i . "::" . $iconname . ".png\n";
        printf "temp-" . $i . "::" . MetOffCommon::convert_temp($rep->{T}, $units) . "\n";
        printf "appt-" . $i . "::" . MetOffCommon::convert_temp($rep->{F}, $units) . "\n";
        printf "pop-" . $i . "::" .  $rep->{Pp} . "\n";
        printf "wind_speed-" . $i . "::" .  MetOffCommon::convert_speed($rep->{S}, $units) . "\n";
        printf "wind_gust-" . $i . "::" .  MetOffCommon::convert_speed($rep->{G}, $units) . "\n";
        printf "wind_dir-" . $i . "::" .  $rep->{D} . "\n";
        printf "relative_humidity-" . $i . "::" .  $rep->{H} . "\n";
        printf "uv-" . $i . "::" .  $rep->{U} . "\n";
        # printf "visibility-" . $i . "::" .  $rep->{V} . "\n";
        
        $i++;
    }

}

sub log_print {
    return if not defined $::opt_D;
    my $dir = shift;

    open OF, ">>$dir/uk_metoffice.log";
    print OF @_;
    close OF;
}
