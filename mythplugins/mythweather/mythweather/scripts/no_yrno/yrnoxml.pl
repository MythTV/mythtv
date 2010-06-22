#! /usr/bin/perl

use strict;
use warnings;

use utf8;
use LWP::UserAgent;
use Getopt::Std;
use URI::Escape;
use XML::XPath;
use XML::XPath::XMLParser;
use DateTime::Format::ISO8601;
use POSIX qw(strftime);
use File::Path;

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'yrno-XML';
my $version = 0.1;
my $author = 'Gavin Hurlbut';
my $email = 'gjhurlbu@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ( '3dlocation',
              '6dlocation', 'altitude', 'cclocation', 'copyright', 'date-0',
              'date-1', 'date-2', 'date-3', 'date-4', 'date-5', 'geobaseid',
              'high-0', 'high-1', 'high-2', 'high-3', 'high-4', 'high-5',
              'low-0', 'low-1', 'low-2', 'low-3', 'low-4', 'low-5',
              'icon-0', 'icon-1', 'icon-2', 'icon-3', 'icon-4', 'icon-5',
              'latitude', 'longitude', 'observation_time', 
              '18hrlocation', 
              '18icon-0', '18icon-1', '18icon-2', 
              '18icon-3', '18icon-4', '18icon-5', 
              'temp-0', 'temp-1', 'temp-2', 'temp-3', 'temp-4', 'temp-5', 
              'time-0', 'time-1', 'time-2', 'time-3', 'time-4', 'time-5', 
              'pop-0', 'pop-1', 'pop-2', 'pop-3', 'pop-4', 'pop-5',
              'updatetime', 'station_id' );
my $dir = "/tmp/yrnoxml";
my $logdir = "/tmp/yrnoxml";
my %images = ( "partly cloudy" => "pcloudy.png", "cloudy" => "cloudy.png",
               "sleet" => "rainsnow.png", "fair" => "fair.png", 
               "snow" => "flurries.png", "rain" => "showers.png",
               "sunny" => "sunny.png", "fog" => "fog.png",
               "mostly cloudy" => "mcloudy.png", 
               "rain showers" => "lshowers.png", "heavy rain" => "showers.png",
               "thunder showers" => "thunshowers.png",
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
    my $base_url = 'http://www.yr.no/soek.aspx?sted=';

    my $xp = getCachedXML($base_url . $search, $dir, $search . ".html",
                          $updateTimeout, $logdir);

    my $nodeset = $xp->find('//td[@class="place"]/a');
    foreach my $node ($nodeset->get_nodelist) {
        my $loc = $node->getAttribute("href");
	$loc =~ s/^\/place\///;
	$loc =~ s/\/$//;
        print $loc . "::" . $node->string_value . "\n";
    }
    exit 0;
}

if (defined $opt_t) {
    foreach (@types) {print; print "\n";}
    exit 0;
}

# we get here, we're doing an actual retrieval, everything must be defined
my $loc = shift;
if (!(defined $opt_u && defined $loc && !$loc eq "")) {
    die "Invalid usage";
}

my %attrib;
my $units = $opt_u;
log_print( $logdir, "-u $units -d $dir $loc\n" );


my $base_url = 'http://www.yr.no/place/';
my $file = $loc;
$file =~ s/\//-/g;

my $xp = getCachedXML($base_url . $loc . "/forecast.xml", $dir, $file . ".xml",
                      $updateTimeout, $logdir);

$attrib{"station_id"} = $loc;

my $nodeset;
my $node;

$name  = $xp->getNodeText('/weatherdata/location/name');
$name .= ", " . $xp->getNodeText('/weatherdata/location/country');

$attrib{"cclocation"}   = $name;
$attrib{"3dlocation"}   = $name;
$attrib{"6dlocation"}   = $name;
$attrib{"18hrlocation"} = $name;

$nodeset = $xp->find('/weatherdata/location/location');
foreach $node ($nodeset->get_nodelist) {
    $attrib{"altitude"}  = convert_alt($node->getAttribute("altitude"), $units);
    $attrib{"latitude"}  = $node->getAttribute("latitude");
    $attrib{"longitude"} = $node->getAttribute("longitude");
    $attrib{"geobaseid"} = $node->getAttribute("geobaseid");
}

$nodeset = $xp->find('/weatherdata/credit/link');
foreach $node ($nodeset->get_nodelist) {
    $attrib{"copyright"}  = $node->getAttribute("text");
}

my $tzoffset;
$nodeset = $xp->find('/weatherdata/location/timezone');
foreach $node ($nodeset->get_nodelist) {
    $tzoffset = $node->getAttribute("utcoffsetMinutes");
}
$tzoffset *= 60;
my $now = time;
$attrib{"updatetime"} = format_date($now);

$attrib{"observation_time"}  = format_date(
       parse_date($xp->getNodeText('/weatherdata/meta/lastupdate'), $tzoffset));

my $lastperiod = undef;
my @forecast;
$nodeset = $xp->find('/weatherdata/forecast/tabular/time');
foreach $node ($nodeset->get_nodelist) {
    my $hashref = {};

    nodeToHash( $node, $hashref );
    push @forecast, $hashref;
    $lastperiod = $hashref->{"time::period"};
}
  
my $day = 0;
my $time = 0;
foreach my $hashref (@forecast) {
#    foreach my $key ( sort keys %$hashref ) {
#        print $key . "::" . $hashref->{$key} . "\n";
#    }
    my $fromtime = parse_date($hashref->{"time::from"}, $tzoffset);
    if( $day < 6 and $hashref->{"time::period"} == $lastperiod ) {
        $attrib{"date-$day"} = format_date($fromtime);
        my $img = $images{lc $hashref->{"symbol::name"}};
        if (not defined $img) {
            log_print( $dir, "Unknown image mapping: " . 
                             $hashref->{"symbol::name"} . "\n" );
            $img = $images{"unknown"};
        }
        $attrib{"icon-$day"} = $img;
        $attrib{"high-$day"} = convert_temp( $hashref->{"temperature::value"},
                                             $units );
        $attrib{"low-$day"} = "N/A";
        $day++;
    }
    if ($time < 6 and $fromtime > $now) {
        $attrib{"time-$time"} = format_date($fromtime);
        my $img = $images{lc $hashref->{"symbol::name"}};
        if (not defined $img) {
            log_print( $dir, "Unknown image mapping: " . 
                             $hashref->{"symbol::name"} . "\n" );
            $img = $images{"unknown"};
        }
        $attrib{"18icon-$time"} = $img;
        $attrib{"temp-$time"} = convert_temp( $hashref->{"temperature::value"},
                                              $units );
        $attrib{"pop-$time"} = "N/A";
        $time++;
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
    my ($node, $hashref) = @_;

    my $nodename = $node->getName;

    foreach my $attr ( $node->getAttributes ) {
        $hashref->{$nodename."::".$attr->getName} = $attr->getData;
    }
    
    foreach my $subnode ( $node->getChildNodes ) {
        nodeToHash( $subnode, $hashref );
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

sub convert_temp {
    my ( $degC, $units ) = @_;
    my $deg;

    if( $units ne "SI" ) {
        $deg = int(($degC * 1.8) + 32.5);
    } else {
        $deg = $degC;
    }
    return $deg;
}

sub parse_date {
    my ( $date, $tzoffset ) = @_;
    my $time = DateTime::Format::ISO8601->parse_datetime( $date );

    $time = $time->epoch - $tzoffset;
    return $time;
}

sub format_date {
    my ($time) = @_;

    return strftime '%a %b %e, %Y %H:%M:%S', localtime($time);
}

sub convert_alt {
    my ( $altm, $units ) = @_;
    my $alt;

    if( $units ne "SI" ) {
        $alt = int(($altm * (100 / 2.54 / 12)) + 0.5);
    } else {
        $alt = $altm;
    }
    return $alt;
}

sub log_print {
    my $dir = shift;

    open OF, ">>$dir/yrnoxml.log";
    print OF @_;
    close OF;
}
