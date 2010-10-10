#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

use English;
use strict;
use warnings;

use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/wunderground',
        '/usr/local/share/mythtv/mythweather/scripts/wunderground';

use utf8;
use encoding 'utf8';
use Getopt::Std;
use POSIX qw(strftime);

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d, $opt_D); 

my $name = 'wunderground-maps';
my $version = 0.2;
my $author = 'Gavin Hurlbut';
my $email = 'gjhurlbu@gmail.com';
my $updateTimeout = 15*60;
my $retrieveTimeout = 30;
my @types = ( 'smdesc', 'updatetime', 'map', 'copyright' );
my $dir = "/tmp/wunderground";
my $logdir = "/tmp/wunderground";
my $config_file = dirname(abs_path($0 or $PROGRAM_NAME)) . "/maps.csv";

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
    my $search = shift;
    $search = qr{(?i)^(.*?),(.*$search.*)$};
    log_print( $logdir, "-l $search\n" ); 

    open my $fh, "<", $config_file or die "Couldn't open config file: $!\n";
    while (<$fh>) {
        if ( /$search/ ) {
            my $code = uc $1;
            print "${code}::$2\n";
        }
    }
    close $fh;

    exit 0;
}

if (defined $opt_t) {
    foreach (@types) {print; print "\n";}
    exit 0;
}

# we get here, we're doing an actual retrieval, everything must be defined
my $loc = uc shift;
if ( not defined $loc or $loc eq "" ) {
    die "Invalid usage";
}

my %attrib;

log_print( $logdir, "-d $dir $loc\n" );

my $search = qr{(?i)^$loc,(.*?)$};
my @names;

open my $fh, "<", $config_file or die "Couldn't open config file: $!\n";
while (<$fh>) {
    push @names, $1 if ( /$search/ );
}
close $fh;

$attrib{"smdesc"} = join( " / ", @names) . " Static Radar Map";

$attrib{"map"} = "http://radblast-mi.wunderground.com/cgi-bin/radar/".
                 "WUNIDS_map?station=$loc&type=N0R&noclutter=0&showlabels=1&".
                 "rainsnow=1&num=1";

$attrib{"copyright"}  = "Weather data courtesy of Weather Underground, Inc.";

my $now = time;
$attrib{"updatetime"} = format_date($now);

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

sub getCachedFile {
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

        my $response = $ua->get($url, ":content_file" => $cachefile);
        if ( !$response->is_success ) {
            die $response->status_line;
        }
    }
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
