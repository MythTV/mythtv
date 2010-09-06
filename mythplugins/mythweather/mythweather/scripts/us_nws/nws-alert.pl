#!/usr/bin/perl
use strict;
use warnings;

use English;
use File::Basename;
use Cwd 'abs_path';
use lib dirname(abs_path($0 or $PROGRAM_NAME)),
        '/usr/share/mythtv/mythweather/scripts/us_nws',
        '/usr/local/share/mythtv/mythweather/scripts/us_nws';

use XML::Parser;
use base qw(XML::SAX::Base);
use Date::Manip;
use Date::Manip::TZ;
use Data::Dumper;
use Getopt::Std;
use LWP::Simple;

my $alerts;
my $currAlert;
my $currInfo;
my $loc_file = dirname(abs_path($0 or $PROGRAM_NAME)) . "/bp16mr06.dbx";

sub StartDocument {
    $alerts = [];
}

sub StartTag {
    my ($expat, $name, %atts) = @_;
    if ($name eq "cap:alert"){
        $currAlert = {};
    }

    if ($name eq "cap:info") {
        $currInfo = {};
    }

}


sub EndTag {
    my ($expat, $name, %atts) = @_;

    if ($name eq "cap:alert") {
        push @$alerts, $currAlert;
    }
    if ($name eq "cap:info") {
        push (@{$currAlert->{'cap:info'}}, $currInfo);
    }
}

sub Text {
    my ($expat, $text) = @_;

    if ($expat->within_element('cap:info')) {
        $currInfo->{$expat->current_element} = $expat->{Text} if ($expat->{Text}
                =~ /\w+/);

    } elsif ($expat->within_element('cap:alert')) {
        $currAlert->{$expat->current_element} = $expat->{Text} if ($expat->{Text} =~
                /\w+/);
    }

}

############################################
sub getWarnings {
    
    my $state = shift;
    $state =~ tr/[A-Z]/[a-z]/;
    my $parser = new XML::Parser(Style => 'Stream');
    my $capfile = get "http://www.weather.gov/alerts/$state.cap" or
        die "cannot retrieve alert data";
    $parser->parse($capfile);
    return $alerts;
}

sub getEffectiveWarnings {
    my $date = shift;
    my $state = shift;
    my $geo = shift;
    my @results;
    if (!$alerts) {
        getWarnings($state);
    }
    my $alert;
    my $info;

    $date = ParseDate($date);
    my $tz = new Date::Manip::TZ;
    $date = Date_ConvTZ($date, $tz->curr_zone(), "UTC");
    $date = UnixDate($date, "%O");

    my @dates;
    while ($alert = shift @$alerts) {
        push @dates, $alert->{'cap:sent'};
        while ($info = shift @{$alert->{'cap:info'}}) {
            if ($info->{'cap:effective'} && 
                Date_Cmp($date, "$info->{'cap:effective'}") >= 0 &&
                Date_Cmp($date, "$info->{'cap:expires'}") < 0 &&
                (!$geo || $info->{'cap:geocode'} == $geo)) {
                push @results, $info;
            }
        }
    }
    if (scalar(@dates) > 1) {
        return @dates, @results;
    } else {
        return $dates[0], @results;
    }
    return @results;        
}

our ($opt_v, $opt_t, $opt_T, $opt_l, $opt_u, $opt_d); 

my $name = 'NWS-Alerts';
my $version = 0.3;
my $author = 'Lucien Dunning';
my $email = 'ldunning@gmail.com';
my $updateTimeout = 10*60;
my $retrieveTimeout = 30;
my @types = ('swlocation', 'updatetime', 'alerts', 'copyright');
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
    open(LOCS, $loc_file) or die "couldn't open bp16mr06.dbx";
    my $search = shift;
    while(<LOCS>) {
        if (m/$search/i) {
            my @entry = split /[|]/;
            print "$entry[6]::$entry[3], $entry[0]\n";
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

my $loc = shift;

if (!(defined $loc && !$loc eq "")) {
    die "Invalid usage";
}

my $state;
my $locstr;
# its a big file that we have to search linearly, so we keep a simple cache
if (open(CACHE, "$dir/NWSAlert_$loc")) {
    ($state, $locstr) = split /::/, <CACHE>;
    chomp $locstr;
    close(CACHE);
} 

if (!$state || !$locstr) {
    ($state, $locstr) = doLocation($loc);
    if ($state && $locstr) {
        my $file = "$dir/NWSAlert_$loc";
        open(CACHE, ">$file") and
            print CACHE "${state}::${locstr}\n";
    } else { die "cannot find location"; }
}

my ($updatetime, @warnings) = getEffectiveWarnings("now", $state, $loc);

foreach my $warning (@warnings) {
    my $txt = $warning->{'cap:description'};
    for my $line (split /\n/, $txt) {
        print "alerts::$line\n" if ($line =~ m/\w+/);
    }
} 
if (!@warnings) {
    print "alerts::No Warnings\n";
}

print "swlocation::$locstr,$state\n";

$updatetime = ParseDate($updatetime);
$updatetime = UnixDate($updatetime, "%b %d, %I:%M %p %Z");

print "updatetime::Last Updated at $updatetime\n";
print "copyright::NOAA,National Weather Service\n";

sub doLocation {
    my $code = shift;
    open(LOCS, $loc_file) or die "couldn't open bp16mr06.dbx";
    while(<LOCS>) {
        if (m/$code/) {
            my @entry = split /[|]/;
            return ($entry[0], $entry[3]);
        }
    }
}
