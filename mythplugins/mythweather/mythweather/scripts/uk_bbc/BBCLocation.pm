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
    
    # Initialise a hash for the $locid & $locname results.
     
    # Use of a hash indexed by $locid ensures that more informative results 
    # (e.g. "Sale, Australia" vs. "Sale") coming from <p class="response"> 
    # section will overwrite less informative results coming from 
    # <span id="printbutton_Forecast"> section
    
    my %loc_hash = ();

    foreach (split("\n", $response)) {
    
    	# Declare a result if either the '<p class="response"> OR <span id="printbutton_Forecast"> strings are found
	# This ensures that single and multiple matches are caught
    
        if (/<p class=\"response\">/ || /<span id=\"printbutton_Forecast\">/) {
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
		
		$loc_hash{$locid} = $locname;
            }
	    
	    # Extract location ID and name from "Print <location>" link
	    
	    # This string is always present (provided valid search result - invalid results are caught above)
	    # irresepective of whether single or multiple matches are returned
	    
	    elsif (/<span id=\"printbutton_Forecast\"><a title=\"Print (.+)\" href=\"\/weather\/forecast\/(\d{0,5})?/o)
	    {
	    	$locid   = $2;
		$locname = $1;
	  	
		$loc_hash{$locid} = $locname;		    
	    }
        }
    }
    
    # Loop through contents of %loc_hash, check for existence within @searchresults, and add as necessary
    
    foreach my $key (keys %loc_hash)
    {
    	my $resultline = $key."::".$loc_hash{$key};
    
   	if (! grep(/^$key/, @searchresults)) 
	{
		push (@searchresults, $resultline);
	}
    }

    return @searchresults;
}

1;
