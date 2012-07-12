#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

use English;
use strict;
use warnings;

use File::Path;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/wunderground',
        '/usr/local/share/mythtv/mythweather/scripts/wunderground';

use utf8;
use encoding 'utf8';
use LWP::UserAgent;
use Getopt::Std;
use URI::Escape;
use POSIX qw(strftime);
use JSON;
use Env qw(HOME MYTHCONFDIR);

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d, $opt_D); 

my $name = 'wunderground-animaps';
my $version = 0.2;
my $author = 'Gavin Hurlbut';
my $email = 'gjhurlbu@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ( 'amdesc', 'updatetime', 'animatedimage', 'copyright', 
              'copyrightlogo' );
my $dir = "/tmp/wunderground";
my $logdir = "/tmp/wunderground";

my $conffile;

if (defined $MYTHCONFDIR) {
    $conffile = "$MYTHCONFDIR/MythWeather/wunderground.key";
} 

if ((!defined $MYTHCONFDIR) || ($conffile && !-f $conffile)) {
    $conffile = "$HOME/.mythtv/MythWeather/wunderground.key";
}

if (!-f $conffile) {
    print STDERR "You need to sign up for an API key from wunderground and " .
                 "put it into\n" .
                 $conffile . "\n\n" .
                 "Visit: http://www.wunderground.com/weather/api/\n\n";
    exit 1;
}

open IF, "<", $conffile or die "Couldn't read $conffile: $!\n";
my $apikey = <IF>;
close IF;
chomp $apikey;

binmode(STDOUT, ":utf8");

if (!-d $logdir) {
    mkpath( $logdir, {mode => 0755} );
}

getopts('Tvtlu:d:D');

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
    my $base_url = "http://api.wunderground.com/api/$apikey/geolookup/q/";

    my $hash = getCachedJSON($base_url . $search . ".json", $dir,
                             $search . ".json", $updateTimeout, $logdir);

    if (exists $hash->{'location'}) {
        # Single match
        my $loc = $hash->{'location'};
        print_location($loc);
    } elsif (exists $hash->{'response'}->{'results'}) {
        for my $loc (@{$hash->{'response'}->{'results'}}) {
            print_location($loc);
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
if ( not defined $rawloc or $rawloc eq "" ) {
    die "Invalid usage";
}
my $loc = uri_escape($rawloc);

my %attrib;

log_print( $logdir, "-d $dir $loc\n" );

my $base_url = "http://api.wunderground.com/api/$apikey/geolookup/q/";

my $hash = getCachedJSON($base_url . $loc . ".json", $dir,
                         $loc . ".json", $updateTimeout, $logdir);
my $location = location($hash->{'location'});

my $url = "http://api.wunderground.com/api/$apikey/animatedradar/q/$loc.gif?" .
          "width=1024&height=768&newmaps=1&num=6&timelabel=1&timelabel.x=10&" .
          "timelabel.y=758";
$attrib{"amdesc"} = "$location Animated Radar Map";

$attrib{"animatedimage"} = $url;
$attrib{"copyright"}  = "Weather data courtesy of Weather Underground, Inc.";
$attrib{"copyrightlogo"} = "http://icons.wxug.com/logos/images/wundergroundLogo_4c.jpg";

my $now = time;
$attrib{"updatetime"} = format_date($now);

for my $attr ( sort keys %attrib ) {
    print $attr . "::" . $attrib{$attr} . "\n";
}
exit 0;

#
# Subroutines
#
sub getCachedJSON {
    my ($url, $dir, $file, $timeout, $logdir) = @_;

    my $cachefile = "$dir/$file";

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

        open OF, ">", $cachefile or die "Can't open $cachefile: $!\n";
        print OF $response->content;
        close OF;
    }

    open OF, "<", $cachefile or die "Can't open $cachefile: $!\n";
    my $json = do { local $/; <OF> };
    my $hash = from_json($json);

    return $hash;
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

sub location {
    my $loc = shift;
    my $location = $loc->{'city'};
    $location .= ", " . $loc->{'state'} if $loc->{'state'};
    $location .= ", " . $loc->{'country_name'} if $loc->{'country_name'};

    return $location;
}

sub print_location {
    my $loc = shift;

    my $ident = $loc->{'l'};
    $ident =~ s/^\/q\///;
    my $location = location($loc);
    print $ident."::$location\n";
}
