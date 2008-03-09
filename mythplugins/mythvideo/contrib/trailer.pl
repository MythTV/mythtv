#!/usr/bin/perl -w
# Author: Andrei Rjeousski ( arjeousski at gmail com )
# Version 0.6
# Changes:
# 0.1
#  Initial release
# 0.2
#  Added more handling to linked .mov files
# 0.3
#  Searching trailer pages that dont have "large" or "high" in them
# 0.4
#  If nothing is found based on IMDB title, try looking for movie's short title
# 0.5
#  If apple.com doesnt return anything, try googling it :)
# 0.6
#  Updated and added more recognition in getting the large trailer from a page with choices of small/med/large

use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;



use vars qw($opt_r $opt_d $opt_T);
use Getopt::Std; 


# display usage
sub usage {
   print "usage: $0 \n";
   print "       -T <movieid>  get movie trailer\n";
   print "       -d enable debug";
   exit(-1);
}

# returns text within 'data' between 'beg' and 'end' matching strings (taken from imdb.pl)
sub parseBetween {
   my ($data, $beg, $end)=@_; # grab parameters

   my $ldata = lc($data);
   my $start = index($ldata, lc($beg)) + length($beg);
   my $finish = index($ldata, lc($end), $start);
   if ($start != (length($beg) -1) && $finish != -1) {
      my $result = substr($data, $start, $finish - $start);
      # return w/ decoded numeric character references
      # (see http://www.w3.org/TR/html4/charset.html#h-5.3.1)
      decode_entities($result);
      return $result;
   }
   return "";
}

sub getTrailerURL {
   my ($movieid) = @_;
   
   # get the name of the movie
   my $host = "http://www.imdb.com/title/tt" . $movieid . "/";
   my $response = get $host;

   # get the title
   my $movie_title = parseBetween($response, "<title>", "<\/title>");

   $response =~ /<br>([\w\s\(\)^<^>]+)\S+?\(short title\)/i;
   my $short_movie_title = "";
   $short_movie_title = $1 if ($1);


   # get rid of the year
   $movie_title =~ s/\ *\(.+?\)//xig;
   $movie_title =~ s/\ *,\ *(The|A)$//xgi;
   $movie_title =~ s/\&//ig;

   # get rid of the year
   $short_movie_title =~ s/\ *\(.+?\)//xig;
   $short_movie_title =~ s/\ *,\ *(The|A)$//xgi;
   $short_movie_title =~ s/\&//ig;
   
   if (defined $opt_d) { printf("# looking for movie id: $movieid title: $movie_title short title: $short_movie_title\n"); }
   
   # add pluses
   my $movie_title_plus = $movie_title;
   $movie_title_plus =~ tr/ /+/;
   my $short_movie_title_plus = $short_movie_title;
   $short_movie_title_plus =~ tr/ /+/;    
   
   my $found = 0;
   my $search_string = $movie_title_plus;   
   my $trailer_page_uri = "";    

   while (!$found) {
       if (defined $opt_d) { printf("# looking for: $search_string\n"); }
       # do the search
       $host = "http://searchcgi.apple.com/cgi-bin/sp/nph-searchpre1.pl?q=".$search_string."+site:www.apple.com/trailers&restrict=us_trailers_only&client=www_collection&site=www_collection&lr=lang_en&output=xml&sort=&filter=0&access=p";
       $response = get $host;
       
           # find URL with "large"
           if ($trailer_page_uri eq "") {
                   if ($response =~ m/\"(http:\/\/.*large.*)\"/i) {
                           $trailer_page_uri = $1;
                           if (defined $opt_d) { printf("# found large\n"); }
                   }
           }
   
           # check for high
           if ($trailer_page_uri eq "") {
                   if ($response =~ m/\"(http:\/\/(.*high.*))\"/i) {
                           $trailer_page_uri = $1; 
                           if (defined $opt_d) { printf("# found high\n"); }
                   } 
           }
   
           # must not have "large" or "high"
           if ($trailer_page_uri eq "") {
                   if ($response =~ m/href=\"(http:\/\/[\w.\/]+)\">(Apple - Trailers)[^<]+/i) {
                           $trailer_page_uri = $1;
                           if (defined $opt_d) { printf("# found other\n"); }
                   } 
           }
           
           # check for "large" in the title
           if ($trailer_page_uri eq "") {
                   if ($response =~ m/<a.*?href=\"([^\"]*)\"[^>]*?>.*?large.*?<\/a>/i) {
                           $trailer_page_uri = $1;
                           if (defined $opt_d) { printf("# found title - large\n"); }
                   }
           }	
   
           # we must be at the studios page
           if ($trailer_page_uri =~ m/\/$/) {
                   if (defined $opt_d) { printf("# didnt find the movie, found studio: $trailer_page_uri\n"); }
                   # look for a movie
                   $response =~ /href=\"([^"]+)\"[^>]*>.*?<b>.*?<\/a>/i;
                   $trailer_page_uri = $1;
   
                   # search for a partial match, if the movie is on the studios page, movie must be here
                   if (defined($trailer_page_uri)) {
                           if (defined $opt_d) { printf("# new trailer page: $trailer_page_uri\n"); }
   
                           # get the actual page
                           $response = get $trailer_page_uri;
                           if (
                               $response =~ m/<a.*?href=\"([^\"]*)\"[^>]*?>.*?large/i || # large in image name after A
                               $response =~ m/<a.*?href=\"([^\"]*)\"[^>]*?large[^>]*?>/i || # large within javascript
                               $response =~ m/\"(.*large[^\"]*)\"/i || # large in actual filename
                               $response =~ m/\"([^<>\"]*?high[^\"<>]*?)\"/i || # high in filename
                               $response =~ m/<a.*?href=\"([^\"]*)\"[^>]*?>.*?high/i || # high  after A
                               $response =~ m/\"([^\"]*?lrg[^\"]*?)\"/i ||  #lrg in filename
                               $response =~ m/\"([^\"]*?_lg.[^\"]*?)\"/i || #_lg. in filename
                               $response =~ m/\"([^\"]*?480[^\"]*?)\"/i # 480 in filename
                               ) {
									# m/\"([^\"]*?high[^\"]*?)\"/i - gangs of new york
                                   if (defined $opt_d) { printf("# found large trailer\n"); }
                                   $trailer_page_uri .= $1;
                           } else {
                              $trailer_page_uri = "";
                           }
                   } else {
                           $trailer_page_uri = "";
                   }
           }
           if ($trailer_page_uri eq "" and $short_movie_title ne "" and !$found and $search_string ne $short_movie_title_plus) {
               $search_string = $short_movie_title_plus;
               
           } else {
               $found = 1;
           }
   }

    
   # lets try googling (cant merge with above code yet)
   if ($trailer_page_uri eq "") {
      if (defined $opt_d) { printf("# googling for: $movie_title_plus\n"); }
     
      # google requires the following hack, otherwise it give 403 (try googling a seach page with wget)
      require LWP::UserAgent;
      
      my $ua = LWP::UserAgent->new;
      # gotta trick google     
      $ua->agent('Mozilla/5.0');
      
      $response = $ua->get("http://www.google.com/search?hl=en&ie=UTF-8&q=$movie_title_plus+site:www.apple.com");
      $response = $response->content;
 
      # must not have "large" or "high"
      if ($trailer_page_uri eq "") {
              if ($response =~ m/href=(http:\/\/[\w.\/]+)[^>]*?>(Apple - Trailers)[^<]+/i) {
                      $trailer_page_uri = $1;
                      if (defined $opt_d) { printf("# googled other\n"); }
              } 
      }
            
   }
    
    
    
    my $trailer_uri = "";
    #only proceed, if something was found
    if ($trailer_page_uri ne "") {
        if (defined $opt_d) { printf("# trailer page found: $trailer_page_uri\n"); }
        
	# get the trailer page
	$response = get $trailer_page_uri;
	$response = lc($response);

	if (defined $opt_r) { printf($response); }
        
	# try parsing...
	$trailer_uri = parseBetween($response,"name=\"href\" value=\"","\"") if ($trailer_uri eq "");
	$trailer_uri = parseBetween($response,"controller=\"false\" href=\"","\"") if ($trailer_uri eq "");
	$trailer_uri = parseBetween($response,"target=\"myself\" src=\"","\"") if ($trailer_uri eq "");
	$trailer_uri = parseBetween($response,"controller=\"true\" src=\"","\"") if ($trailer_uri eq "");
	$trailer_uri = parseBetween($response,"param name=\"src\" value=\"","\"") if ($trailer_uri eq "");	

	# now we need to get the filename of the ACTUAL file
	if ($trailer_uri ne "") {
            if (defined $opt_d) { printf("# actual trailer found: $trailer_uri\n"); }
            if (defined $opt_d) { printf("# starting to download\n"); }
	
	    my $file_size = 0; 	
	    while ($file_size < 100000) {
		    my @headers = head $trailer_uri;
		    $file_size = $headers[1];
	    	
            	    if ($file_size < 100000) { # to be save
			    my $file = get $trailer_uri;

	    		    $file =~ /(.*\.mov.*)*url.{5}([\w-]+.mov).*$/is;
		       	    $file = $2;
       
		            if (defined $opt_d) { printf("# actual filename is $file\n"); }
        		    $trailer_uri =~ s/[^\/]+.mov$/$file/gi;
		    }
	    }
            
            if (defined $opt_d) { printf("# final trailer uri is $trailer_uri\n"); }
        }
    }
	return $trailer_uri;
}



#
# Main Program
#

# parse command line arguments 
getopts('drT');

# print out usage if needed
if ($#ARGV<0) { usage(); }

if (defined $opt_T) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -T <movieid>\n";
   my $trailer_uri = getTrailerURL($movieid);
   #if (defined $opt_d) {
      printf("$trailer_uri\n");
   #}
#   if ($trailer_uri ne "") {
#	   system "wget -O $movieid.mov $trailer_uri"; 
#   }   
}



