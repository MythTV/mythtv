# This code is mostly the work of Lucien Dunning (ldunning@gmail.com). It was
# used to provide search functionality for the Environment Canada animated and
# static maps.

package ENVCANMapSearch;
use strict;
require Exporter;
use base qw(XML::SAX::Base);
use XML::Parser;

our @ISA = qw(Exporter);
our @EXPORT = qw(doSearch AddSatSearch AddImageTypeSearch AddSatClassSearch);
our $VERSION = 0.1;

my @satclasssearches;
my @satsearches;
my @imagetypesearches;
my @anisearches;
my $searchresults;

sub doSearch {

    my $parser = new XML::Parser( Style => 'Stream' );
    open(XML, 'ENVCAN-Maps.xml') or die "cannot open file";
    $parser->parse(*XML); 
    close(XML);
    return $searchresults;
}

sub StartDocument {

    my $expat = shift;
    $expat->{finish} = 0;
    $searchresults = [];
}

sub StartTag {

    my ($expat, $name, %atts) = @_;
    if ($name eq 'entry') {
       $expat->{CurrEntry} = {};
       $expat->{MatchFound} = 0;
    }
}

sub Text {

    my $expat = shift;
    my $text = $expat->{Text};
    my $search;
    if ($expat->in_element('satellite_class')) {
        $expat->{CurrEntry}->{satellite_class} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@satclasssearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    elsif ($expat->in_element('satellite')) {
        $expat->{CurrEntry}->{satellite} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@satsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    elsif ($expat->in_element('image_type')) {
        $expat->{CurrEntry}->{image_type} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@imagetypesearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    elsif ($expat->in_element('entry_id')) {
        $expat->{CurrEntry}->{entry_id} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@anisearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }


    elsif ($expat->in_element('animated_url')) {
        $expat->{CurrEntry}->{animated_url} = $text;
    }
}

sub EndTag {

    my ($expat, $name) = @_;
    if ($name eq 'entry' && $expat->{MatchFound}) {
        push (@$searchresults, $expat->{CurrEntry});
        if ($expat->{finish}) {
            $expat->finish();
            return;
        }
    }

}

sub AddSatClassSearch {
    push (@satclasssearches, shift);
}
sub AddSatSearch {
    push (@satsearches, shift);
}
sub AddImageTypeSearch {
    push (@imagetypesearches, shift);
}

sub AddAniSearch {
    push (@anisearches, shift);
}
1;
