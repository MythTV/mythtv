package MapSearch;
use strict;
require Exporter;
use base qw(XML::SAX::Base);
use XML::Parser;
use Data::Dumper;

our @ISA = qw(Exporter);
our @EXPORT = qw(doSearch AddDescSearch AddURLSearch AddAniSearch);
our $VERSION = 0.1;

my @descsearches;
my @urlsearches;
my @anisearches;
my $searchresults;

sub doSearch {

    my $parser = new XML::Parser( Style => 'Stream' );
    open(XML, 'maps.xml') or die "cannot open file";
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
    } elsif ($name eq 'animated') {
        $expat->{CurrEntry}->{images} = [];
    }
}

sub Text {

    my $expat = shift;
    my $text = $expat->{Text};
    my $search;
    if ($expat->in_element('description')) {
        $expat->{CurrEntry}->{description} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@descsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }

    elsif ($expat->in_element('static_url')) {
        $expat->{CurrEntry}->{url} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@urlsearches) {
                if ($text =~ m/$search/i) {
                    $expat->{MatchFound} = 1;
                    return;
                }
            }
        }
    }
    elsif ($expat->in_element('image')) {
        push @{$expat->{CurrEntry}->{images}}, $text;
    } elsif ($expat->in_element('imgsize')) {
        $text =~ s/,/x/;
        $expat->{CurrEntry}->{imgsize} = $text;
    } elsif ($expat->in_element('base_url')) {
        $expat->{CurrEntry}->{animation} = $text;
        if (!$expat->{MatchFound}) {
            foreach $search (@anisearches) {
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

    if ($name eq 'entry' && $expat->{MatchFound}) {
        push (@$searchresults, $expat->{CurrEntry});
        if ($expat->{finish}) {
            $expat->finish();
            return;
        }
    }

}

sub AddDescSearch {
    push (@descsearches, shift);
}
sub AddURLSearch {
    push (@urlsearches, shift);
}
sub AddAniSearch {
    push (@anisearches, shift);
}
1;
