
#TODO In its infinite wisdom, the XML the NWS gives us (NWS-Stations.xml) is
# not consistent wrt to lat lon.  Some are in a +/- format, others in [NSEW]
# format, also, some aren't even listed.  It seems unlikely that the NWS doesn't
# know that location of some of its weather stations and in fact they are on
# their website, so maybe write a script to grab missing ones and massage them
# all to a consistent format.

package NWSLocation;
use English;
use strict;
use warnings;

require Exporter;
use base qw(XML::SAX::Base);
use XML::Parser;
use Data::Dumper;
use File::Basename;
use Cwd 'abs_path';

our @ISA = qw(Exporter);
our @EXPORT = qw(doSearch AddLatLonSearch AddStationIdSearch AddLocSearch);
our $VERSION = 0.1;

my @latlonsearches;
my @stationidsearches;
my @locstrsearches;
my @statesearches;
my %currStation;
my $searchresults;

sub doSearch {
    my $xml_file = dirname(abs_path($0 or $PROGRAM_NAME)) . "/NWS-Stations.xml";

    my $parser = new XML::Parser( Style => 'Stream' );
    open(XML, $xml_file) or die "cannot open NWS-Stations.xml file\n";
    $parser->parse(*XML); 
    close(XML);
    return $searchresults;
}

sub StartDocument {
    my $expat = shift;

    $expat->{finish} = 0;

}

sub StartTag {
    my ($expat, $name, %atts) = @_;

    if ($name eq 'station') {
       $expat->{CurrEntry} = {};
       $expat->{MatchFound} = 0;
    }
}

sub Text {

    my $expat = shift;
    my $text = $expat->{Text};
    my $search;

    if ($expat->in_element('station_id')) {
        $expat->{CurrEntry}->{station_id} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@stationidsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    if ($expat->in_element('state')) {
        $expat->{CurrEntry}->{state} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@statesearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    if ($expat->in_element('station_name')) {
        $expat->{CurrEntry}->{station_name} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@locstrsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                } 
            }
        }
    }

    # annoyingly, the lat/lon format is not consistent in the XML file,
    # sometimes its in +/- format, other times N/S E/W, so we convert it right
    # off to be unifrom in +/-
    if ($expat->in_element('latitude')){
        $text =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[N]/+$1/ or
            $text =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[S]/-$1/;
        $expat->{CurrEntry}->{latitude} = $text;
        $expat->{currLat} = $text;
        return;
    }

    if ($expat->in_element('longitude')) {
        $text =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[E]/+$1/ or
            $text =~ s/(\d{1,3}([.]\d{1,3})?)([.]\d{1,3})?[W]/-$1/;
        $expat->{CurrEntry}->{longitude} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@latlonsearches) {
                if ($search->[0] eq $expat->{currLat} && $search->[1] eq $text) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }
}

sub EndTag {
    my ($expat, $name) = @_;

    if ($name eq 'station' && $expat->{MatchFound}) {
        push (@$searchresults, $expat->{CurrEntry});
        if ($expat->{finish}) {
            $expat->finish();
            return;
        }
    }

}

sub AddLatLonSearch {
    my ($lat, $lon) = @_;
    push (@latlonsearches, [$lat, $lon]);
}

sub AddStationIdSearch {
    my $id = shift;
    push (@stationidsearches, $id);
}

sub AddLocSearch {
    my $loc = shift;
    push (@locstrsearches, $loc);
}

sub AddStateSearch {
    my $state = shift;
    push (@statesearches, $state);
}

1;
