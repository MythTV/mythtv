# This code is mostly the work of Lucien Dunning (ldunning@gmail.com). It was 
# used to provide search functionality for the Environment Canada weather 
# source scripts.

package ENVCANLocation;
use strict;
require Exporter;
use XML::Parser;

our @ISA = qw(Exporter);
our @EXPORT = qw(doSearch AddStationIdSearch AddRegionIdSearch AddCitySearch AddProvinceSearch);
our $VERSION = 0.1;

my @regionidsearches;
my @stationidsearches;
my @citysearches;
my @provincesearches;
my %currStation;
my $searchresults;

sub doSearch {

    my $parser = new XML::Parser( Style => 'Stream' );
    open(XML, 'ENVCAN-Stations.xml') or die "cannot open file\n";
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

    if ($expat->in_element('region_id')) {
        $expat->{CurrEntry}->{region_id} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@regionidsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    if ($expat->in_element('province')) {
        $expat->{CurrEntry}->{province} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@provincesearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    if ($expat->in_element('city')) {
        $expat->{CurrEntry}->{city} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@citysearches) {
                if ($text =~ m/$search/i) {
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

sub AddCitySearch {
    my $city = shift;
    push (@citysearches, $city);
}

sub AddProvinceSearch {
    my $province = shift;
    push (@provincesearches, $province);
}

sub AddStationIdSearch {
    my $station_id = shift;
    push (@stationidsearches, $station_id);
}

sub AddRegionIdSearch {
    my $region_id = shift;
    push (@regionidsearches, $region_id);
}

1;
