package BBCLocation;
use strict;
require Exporter;
use LWP::Simple;

our @EXPORT = qw(Search);
our $VERSION = 0.2;

my @searchresults;
my @resulturl;
my $resultcount = -1;

sub Search {

    my $base_url       = 'http://news.bbc.co.uk/weather/util/search/Search.xhtml?';
    my $world_base_url = $base_url . 'lowgraphics=true&region=world&search=';
    my $local_base_url = $base_url . 'lowgraphics=true&region=uk&search=';

    my $search_string = shift;

    my $world_response = get $world_base_url . $search_string;
    my $local_response = get $local_base_url . $search_string;

    &parseResults($world_response) if defined($world_response);
    &parseResults($local_response) if defined($local_response);

    if ( ($resultcount > 0 ) && ($#searchresults < 0) ) {
        foreach my $url (@resulturl) {
            my $url_response = get $base_url . $url;
            die unless defined $url_response;
            &parseResults($url_response);
        }
    }

    return @searchresults;
}

sub parseResults {
    my $response = shift;
    my $isresults = 0;
    my $resultline = "";

    foreach (split("\n", $response)) {
        if (/<p class=\"response\">/) {
            $isresults = 1;
        }

        my $locname;
        my $locid; 
        my $url;

        if ($isresults) {
            last if (/There are no forecasts matching/);

            $resultcount = $1 if (/<strong>There \w{2,3} (\d*) forecasts? matching/);

            # Collect result URLs
            if (/<a id=\"result_\d*\" .* href \=\"?.*search\=.*/) {
                $url = $_;
                $url =~ s/.*href \=\"(.*)\".*/$1/s;
                push (@resulturl, $url);
            }

            # Collect location IDs and location names
            elsif (/<a id=\"result_\d*\" .* href \=\"\/weather\/forecast\//) {
                $locid = $_;
                $locid =~ s/.*\/weather\/forecast\/(\d{0,5})\?.*/$1/s;
        
                $locname = $_;
                $locname =~ s/.*<a id=\"result_\d*\".*>(.*)<\/a>.*/$1/s;

                $resultline = $locid . "::" . $locname;
                if (! grep(/^$locid/, @searchresults)) {
                    push (@searchresults, $resultline);
                }
            }
        }
    }

    return @searchresults;
}

1;
