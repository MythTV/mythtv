#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

use strict;
use warnings;

use utf8;
use encoding 'utf8';
use LWP::UserAgent;
use Getopt::Std;
use URI::Escape;
use XML::XPath;
use XML::XPath::XMLParser;
use POSIX qw(strftime);
use File::Path;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d, $opt_D); 

my $name = 'wunderground';
my $version = 0.1;
my $author = 'Gavin Hurlbut';
my $email = 'gjhurlbu@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ( '3dlocation', '6dlocation', 'cclocation', 'copyright', 
              'date-0', 'date-1', 'date-2', 'date-3', 'date-4', 'date-5', 
              'high-0', 'high-1', 'high-2', 'high-3', 'high-4', 'high-5',
              'low-0', 'low-1', 'low-2', 'low-3', 'low-4', 'low-5',
              'icon-0', 'icon-1', 'icon-2', 'icon-3', 'icon-4', 'icon-5',
              'observation_time', 'updatetime', 'station_id' );
my $dir = "/tmp/wunderground";
my $logdir = "/tmp/wunderground";
my %images = ( "clear" => "fair.png", "cloudy" => "cloudy.png",
               "flurries" => "flurries.png", "fog" => "fog.png",
               "hazy" => "fog.png", "mostlycloudy" => "mcloudy.png",
               "mostlysunny" => "pcloudy.png", "partlycloudy" => "pcloudy.png",
               "partlysunny" => "mcloudy.png", "rain" => "showers.png",
               "sleet" => "rainsnow.png", "snow" => "flurries.png",
               "sunny" => "sunny.png", "tstorms" => "thunshowers.png",
               "unknown" => "unknown.png" );

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
    print "$updateTimeout,$retrieveTimeout\n";
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
    my $search = uri_escape(shift);
    log_print( $logdir, "-l $search\n" ); 
    my $base_url = 
      'http://api.wunderground.com/auto/wui/geo/GeoLookupXML/index.xml?query=';

    my $xp = getCachedXML($base_url . $search, $dir, $search . ".html",
                          $updateTimeout, $logdir);

    # This can return two different types of responses.  One where there is a
    # list of locations, and one where it's unique.
    my $nodeset = $xp->find('/locations/location');
    my $city;
    my $state;
    if( $nodeset->size == 0 ) {
        # Single location
        $city  = $xp->getNodeText('/location/city');
        $state = $xp->getNodeText('/location/state');
        if( not defined $state ) {
            $state = "";
        } else {
            $state = ", $state";
        }
        print $city . "::$city$state, " . 
              $xp->getNodeText('/location/country') . "\n";
    } else {
        # Multiple locations
        foreach my $node ($nodeset->get_nodelist) {
            my $type = $node->getAttribute('type');
            next unless $type eq "CITY" or $type eq "INTLCITY";
            $city = $xp->find("name", $node);
            print "$city" . "::$city\n";
        }
    }

    exit 0;
}

if (defined $opt_t) {
    foreach (@types) {print; print "\n";}
    exit 0;
}

# we get here, we're doing an actual retrieval, everything must be defined
my $rawloc = shift;
my $loc = uri_escape($rawloc);
if (!(defined $opt_u && defined $loc && !$loc eq "")) {
    die "Invalid usage";
}

my %attrib;
my $units = $opt_u;
log_print( $logdir, "-u $units -d $dir $loc\n" );


my $base_url = 
     'http://api.wunderground.com/auto/wui/geo/ForecastXML/index.xml?query=';
my $file = $loc;
$file =~ s/\//-/g;

my $xp = getCachedXML($base_url . $loc, $dir, $file . ".xml",
                      $updateTimeout, $logdir);

$attrib{"station_id"} = $rawloc;

my $nodeset;
my $node;

$attrib{"cclocation"}   = $rawloc;
$attrib{"3dlocation"}   = $rawloc;
$attrib{"6dlocation"}   = $rawloc;

$attrib{"copyright"}  = "Weather data courtesy of Weather Underground, Inc.";

my $now = time;
$attrib{"updatetime"} = format_date($now);

$attrib{"observation_time"}  = $xp->getNodeText('/forecast/txt_forecast/date');

my @forecast;
$nodeset = $xp->find('/forecast/simpleforecast/forecastday');
foreach $node ($nodeset->get_nodelist) {
    my $hashref = {};

    nodeToHash( $node, "", $hashref );
    push @forecast, $hashref;
}
  
my $day = 0;
my $time = 0;
#foreach my $hashref (@forecast) {
#    print "---------------\n";
#    foreach my $key ( sort keys %$hashref ) {
#        print $key . "::" . $hashref->{$key} . "\n";
#    }
#}

foreach my $hashref (@forecast) {
    my $fromtime = $hashref->{"forecastday::date::epoch"};
    if( $day < 6 ) {
        $attrib{"date-$day"} = format_date($fromtime);
        my $icon = lc $hashref->{"forecastday::icon"};
        $icon =~ s/^chance//;
        my $img = $images{$icon};
        if (not defined $img) {
            log_print( $dir, "Unknown image mapping: " . 
                             $hashref->{"forecastday::icon"} . "\n" );
            $img = $images{"unknown"};
        }
        $attrib{"icon-$day"} = $img;
        if ($units eq "SI") {
            $attrib{"high-$day"} = $hashref->{"forecastday::high::celsius"};
            $attrib{"low-$day"} = $hashref->{"forecastday::low::celsius"};
        } else {
            $attrib{"high-$day"} = $hashref->{"forecastday::high::fahrenheit"};
            $attrib{"low-$day"} = $hashref->{"forecastday::low::fahrenheit"};
        }
        $day++;
    }
}

for my $attr ( sort keys %attrib ) {
    print $attr . "::" . $attrib{$attr} . "\n";
}
exit 0;

#
# Subroutines
#
sub nodeToHash {
    my ($node, $prefix, $hashref) = @_;

    my $nodename = $node->getName;
    my @subnodelist = $node->getChildNodes;

    if ( not defined $prefix or $prefix eq "" ) {
        $prefix = $nodename;
    } elsif ( defined $nodename ) {
        $prefix = $prefix . "::" . $nodename;
    }

    foreach my $attr ( $node->getAttributes ) {
        $prefix .= "::".$attr->getName."=".$attr->getData;
    }

    if ( $#subnodelist == 0 ) {
        $hashref->{$prefix} = $node->string_value;
    } else {
        foreach my $subnode ( @subnodelist ) {
            nodeToHash( $subnode, $prefix, $hashref );
        }
    }
}

sub getCachedXML {
    my ($url, $dir, $file, $timeout, $logdir) = @_;

    my $cachefile = "$dir/$file";
    my $xp;

    my $now = time();

    if( (-e $cachefile) and ((stat($cachefile))[9] >= ($now - $timeout)) ) {
        # File cache is still recent.
        log_print( $logdir, "cached in $cachefile\n" );
    } else {
        log_print( $logdir, "$url\ncaching to $cachefile\n" );
        my $ua = LWP::UserAgent->new;
        $ua->timeout(30);
        $ua->env_proxy;
        $ua->default_header('Accept-Language' => "en");

        my $response = $ua->get($url);
        if ( !$response->is_success ) {
            die $response->status_line;
        }

        open OF, ">$cachefile" or die "Can't open $cachefile: $!\n";
        print OF $response->content;
        close OF;
    }

    $xp = XML::XPath->new(filename => $cachefile);

    return $xp;
}

sub format_date {
    my ($time) = @_;

    return strftime '%a %b %e, %Y %H:%M:%S', localtime($time);
}

sub log_print {
    return if not defined $opt_D;
    my $dir = shift;

    open OF, ">>$dir/wunderground.log";
    print OF @_;
    close OF;
}
