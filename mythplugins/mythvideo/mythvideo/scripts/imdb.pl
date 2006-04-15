#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups based on 
# the popular www.imdb.com website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: Tim Harvey (tharvey AT alumni.calpoly DOT edu)
# Modified: Andrei Rjeousski
# v1.1
# - Added amazon.com covers and improved handling for imdb posters
# v1.2
#     - when searching amazon, try searching for main movie name and if nothing
#       is found, search for informal name
#     - better handling for amazon posters, see if movie title is a substring
#       in the search results returned by amazon
#     - fixed redirects for some movies on impawards
# v1.3
#     - fixed search for low res images (imdb changed the page layout)
#     - added cinemablend poster search
#     - added nexbase poster search
#     - removed amazon.com searching for now

use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;
use XML::Simple;


use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P);
use Getopt::Std; 

$title = "IMDB Query"; 
$version = "v1.3";
$author = "Tim Harvey, Andrei Rjeousski";

# display usage
sub usage {
   print "usage: $0 -hdrviMPD [parameters]\n";
   print "       -h           help\n";
   print "       -d           debug\n";
   print "       -r           dump raw query result data only\n";
   print "       -v           display version\n";
   print "       -i           display info\n";
   print "\n";
   print "       -M [options] <query>    get movie list\n";
   print "               some known options are:\n";
   print "                  type=[fuzy]         looser search\n";
   print "                  from_year=[int]     limit matches to year\n";
   print "                  to_year=[int]       limit matches to year\n";
   print "                  sort=[smart]        ??\n";
   print "                  tv=[no|both|only]   limits between tv and movies\n";
   print "               Note: multiple options must be separated by ';'\n";
   print "       -P <movieid>  get movie poster\n";
   print "       -D <movieid>  get movie data\n";
   exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
   print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
   print "Performs queries using the www.imdb.com website.\n";
}

# display detailed help 
sub help {
   version();
   info();
   usage();
}

# returns text within 'data' between 'beg' and 'end' matching strings
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

# get Movie Data 
sub getMovieData {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.imdb.com/title/tt" . $movieid . "/";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   # parse title and year
   my $title = parseBetween($response, "<title>", "</title>");
   $title =~ m#(.+) \((\d+).*\)#;  # Note some years have a /II after them?
   $title = $1;
   my $year = $2;

   # parse director 
   my $director = parseBetween($response, ">Directed by</b>", "/a><br>");
   $director = parseBetween($director, "/\">", "<");

   # parse writer 
   # (Note: this takes the 'first' writer, may want to include others)
   my $writer = parseBetween($response, ">Writing credits</b>", "</table>");
   $writer = parseBetween($writer, "/\">", "</");

   # parse plot
   my $plot = parseBetween($response, ">Plot Outline:</b> ", "<a href=\"");
   if (!$plot) {
      $plot = parseBetween($response, ">Plot Summary:</b> ", "<a href=\"");
   }

   # parse user rating
   my $userrating = parseBetween($response, ">User Rating:</b>", "> (");
   $userrating = parseBetween($userrating, "<b>", "/");

   # parse MPAA rating
   my $ratingcountry = "USA";
   my $movierating = parseBetween($response, ">MPAA</a>:</b> ", "<br>");
   if (!$movierating) {
       $movierating = parseBetween($response, ">Certification:</b>", "<br>");
       $movierating = parseBetween($movierating, "certificates=$ratingcountry",
                                   "/a>");
       $movierating = parseBetween($movierating, ">", "<");
   }

   # parse movie length
   my $runtime = parseBetween($response, ">Runtime:</b>\n", " min");

   # parse cast 
   #  Note: full cast would be from url: 
   #    www.imdb.com/title/<movieid>/fullcredits
   my @actors;
   my $cast = "";
   my $count = 0;
   my $data = parseBetween($response, "Cast overview, first billed only:",
                               "/table>"); 
   if ($data) {
      my $beg = "/\">"; 
      my $end = "</a>";
      my $start = index($data, $beg);
      my $finish = index($data, $end, $start);
      my $actor;
      while ($start != -1) {
         $start += length($beg);
         $actor = substr($data, $start, $finish - $start);
         # add to array
         $actors[$count++] = $actor;

         # advance data to next movie
         $data = substr($data, - (length($data) - $finish));
         $start = index($data, $beg);
         $finish = index($data, $end, $start + 1); 
      }
      $cast = join(',', @actors);
   }
   
   
   # parse genres 
   my @genres;
   my $lgenres = "";
   $count = 0;
   $data = parseBetween($response, "<b class=\"ch\">Genre:</b>","<b class=\"ch\">User Comments:</b>"); 
   if ($data) {
      my $beg = "/\">"; 
      my $end = "</a>";
      my $start = index($data, $beg);
      my $finish = index($data, $end, $start);
      my $genre;
      while ($start != -1) {
         $start += length($beg);
         $genre = substr($data, $start, $finish - $start);
         # add to array
         $genres[$count++] = $genre;

         # advance data to next movie
         $data = substr($data, - (length($data) - $finish));
         $start = index($data, $beg);
         $finish = index($data, $end, $start + 1); 
      }
      $lgenres = join(',', @genres);
   }
   
   # parse countries 
   my @countries;
   my $lcountries = "";
   $count = 0;
   $data = parseBetween($response, "<b class=\"ch\">Country:</b>","<br>"); 
   if ($data) {
      my $beg = "/\">"; 
      my $end = "</a>";
      my $start = index($data, $beg);
      my $finish = index($data, $end, $start);
      my $country;
      while ($start != -1) {
         $start += length($beg);
         $country = substr($data, $start, $finish - $start);
         # add to array
         $countries[$count++] = $country;

         # advance data to next movie
         $data = substr($data, - (length($data) - $finish));
         $start = index($data, $beg);
         $finish = index($data, $end, $start + 1); 
      }
      $lcountries = join(',', @countries);
   }

   # output fields (these field names must match what MythVideo is looking for)
   print "Title:$title\n";
   print "Year:$year\n";
   print "Director:$director\n";
   print "Plot:$plot\n";
   print "UserRating:$userrating\n";
   print "MovieRating:$movierating\n";
   print "Runtime:$runtime\n";
   print "Writers: $writer\n";
   print "Cast: $cast\n";
   print "Genres: $lgenres\n";
   print "Countries: $lcountries\n";
}

# dump Movie Poster
sub getMoviePoster {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.imdb.com/title/tt" . $movieid . "/posters";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   my $uri = "";

   # look for references to impawards.com posters - they are high quality
   my $site = "http://www.impawards.com";
   my $impsite = parseBetween($response, "<a href=\"".$site, "\">".$site);

   # jersey girl fix
   $impsite = parseBetween($response, "<a href=\"http://impawards.com","\">http://impawards.com") if ($impsite eq "");

   if ($impsite) {
      $impsite = $site . $impsite;

      if (defined $opt_d) { print "# Searching for poster at: ".$impsite."\n"; }
      my $impres = get $impsite;
      if (defined $opt_d) { printf("# got %i bytes\n", length($impres)); }
      if (defined $opt_r) { printf("%s", $impres); }      

	# making sure it isnt redirect
	$uri = parseBetween($impres, "0;URL=..", "\">");
	if ($uri ne "") {
		if (defined $opt_d) { printf("# processing redirect to %s\n",$uri); }
		# this was redirect
                $impsite = $site . $uri;
                $impres = get $impsite;
	}

      
      # do stuff normally	
      $uri = parseBetween($impres, "<img SRC=\"posters/", "\" ALT");
      # uri here is relative... patch it up to make a valid uri
      if (!($uri =~ /http:(.*)/ )) {
         my $path = substr($impsite, 0, rindex($impsite, '/') + 1);
         $uri = $path."posters/".$uri;
      }
      if (defined $opt_d) { print "# found ipmawards poster: $uri\n"; }
   }

   # try looking on nexbase
   if ($uri eq "" && $response =~ m/<a href="([^"]*)">([^"]*?)nexbase/i) {
      if ($1 ne "") {
         if (defined $opt_d) { print "# found nexbase poster page: $1 \n"; }
         my $cinres = get $1;
         if (defined $opt_d) { printf("# got %i bytes\n", length($cinres)); }
         if (defined $opt_r) { printf("%s", $cinres); }

         if ($cinres =~ m/<a id="photo_url" href="([^"]*?)" ><\/a>/i) {
            if (defined $opt_d) { print "# nexbase url retreived\n"; }
            $uri = $1;
         }
      }
   }

   # try looking on cinemablend
   if ($uri eq "" && $response =~ m/<a href="([^"]*)">([^"]*?)cinemablend/i) {
      if ($1 ne "") {
         if (defined $opt_d) { print "# found cinemablend poster page: $1 \n"; }
         my $cinres = get $1;
         if (defined $opt_d) { printf("# got %i bytes\n", length($cinres)); }
         if (defined $opt_r) { printf("%s", $cinres); }

         if ($cinres =~ m/<td align=center><img src="([^"]*?)" border=1><\/td>/i) {
            if (defined $opt_d) { print "# cinemablend url retreived\n"; }
            $uri = "http://www.cinemablend.com/".$1;   
         }
      }
   }

   # if the impawards site attempt didn't give a filename grab it from imdb
   if ($uri eq "") {
       if (defined $opt_d) { print "# looking for imdb posters\n"; }
       my $host = "http://posters.imdb.com/posters/";

       $uri = parseBetween($response, $host, "\"><td><td><a href=\"");
       if ($uri ne "") {
           $uri = $host.$uri;
       } else {
          if (defined $opt_d) { print "# no poster found\n"; }
       }
   }



   my @movie_titles;
   my $found_low_res = 0;
   my $k = 0;
   
   # no poster found, take lowres image from imdb
   if ($uri eq "") {
      if (defined $opt_d) { print "# looking for lowres imdb posters\n"; }
      my $host = "http://www.imdb.com/title/tt" . $movieid . "/";
      $response = get $host;

      # Better handling for low resolution posters
      # 
      if ($response =~ m/<a name="poster".*<img.*src="([^"]*).*<\/a>/ig) {
         if (defined $opt_d) { print "# found low res poster at: $1\n"; }
         $uri = $1;
         $found_low_res = 1;
      } else {
         if (defined $opt_d) { print "# no low res poster found\n"; }
         $uri = "";
      }

      if (defined $opt_d) { print "# starting to look for movie title\n"; }
      
      # get main title
      if (defined $opt_d) { print "# Getting possible movie titles:\n"; }
      $movie_titles[$k++] = parseBetween($response, "<title>", "<\/title>");
      if (defined $opt_d) { print "# Title: ".$movie_titles[$k-1]."\n"; }

      # now we get all other possible movie titles and store them in the titles array
      while($response =~ m/>([^>^\(]*)([ ]{0,1}\([^\)]*\)[^\(^\)]*[ ]{0,1}){0,1}\(informal title\)/g) {
         $movie_titles[$k++] = $1;
         chomp($movie_titles[$k-1]);
         $movie_titles[$k-1] =~ s/^\s+//;
         $movie_titles[$k-1] =~ s/\s+$//;
         if (defined $opt_d) { print "# Title: ".$movie_titles[$k-1]."\n"; }
      }
       
   }
   
   print "$uri\n";
}

# dump Movie list:  1 entry per line, each line as 'movieid:Movie Title'
sub getMovieList {
   my ($filename, $options)=@_; # grab parameters

   # If we wanted to inspect the file for any reason we can do that now

   #
   # Convert filename into a query string 
   # (use same rules that Metadata::guesTitle does)
   my $query = $filename;
   $query = uri_unescape($query);  # in case it was escaped
   # Strip off the file extension
   if (rindex($query, '.') != -1) {
      $query = substr($query, 0, rindex($query, '.'));
   }
   # Strip off anything following '(' - people use this for general comments
   if (rindex($query, '(') != -1) {
      $query = substr($query, 0, rindex($query, '(')); 
   }
   # Strip off anything following '[' - people use this for general comments
   if (rindex($query, '[') != -1) {
      $query = substr($query, 0, rindex($query, '[')); 
   }

   # IMDB searches do better if any trailing ,The is left off
   $query =~ /(.*), The$/i;
   if ($1) { $query = $1; }
   
   # prepare the url 
   $query = uri_escape($query);
   if (!$options) { $options = "" ;}
   if (defined $opt_d) { 
      printf("# query: '%s', options: '%s'\n", $query, $options);
   }
   
   # get the search results  page
   #    some known IMDB options are:  
   #         type=[fuzy]         looser search
   #         from_year=[int]     limit matches to year (broken at imdb)
   #         to_year=[int]       limit matches to year (broken at imdb)
   #         sort=[smart]        ??
   #         tv=[no|both|only]   limits between tv and movies (broken at imdb)
   #$options = "tt=on;nm=on;mx=20";  # not exactly clear what these options do
   my $request = "http://www.imdb.com/find?q=$query;$options";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) {
      print $response;
      exit(0);
   }
   
   # check to see if we got a results page or a movie page
   #    looking for 'add=<movieid>" target=' which only exists
   #    in a movie description page 
   my $movienum = parseBetween($response, "add=", "\" target=");
   if ($movienum) {
       if (defined $opt_d) { printf("# redirected to movie page\n"); }
       my $movietitle = parseBetween($response, "<title>", "</title>"); 
       $movietitle =~ m#(.+) \((\d+)\)#;
       $movietitle = $1;
       print "$movienum:$movietitle\n";
       exit(0);
   }

   # extract possible matches
   #    possible matches are grouped in several catagories:  
   #        exact, partial, and approximate
   my $popular_results = parseBetween($response, "<b>Popular Titles</b>",
                                              "</ol>");
   my $exact_matches = parseBetween($response, "<b>Titles (Exact Matches)</b>",
                                              "</ol>");
   my $partial_matches = parseBetween($response, "<b>Titles (Partial Matches)</b>", 
                                              "</ol>");
#   my $approx_matches = parseBetween($response, "<b>Approximate Matches</b>", 
#                                               "</ol>");
   # parse movie list from matches
   my $beg = "<li>";
   my $end = "</li>";
   my $count = 0;
   my @movies;

#   my $data = $exact_matches.$partial_matches;
   my $data = $popular_results.$exact_matches;
   # resort to partial matches if no exact
   if ($data eq "") { $data = $partial_matches; }
   # resort to approximate matches if no exact or partial
#   if ($data eq "") { $data = $approx_matches; }
   if ($data eq "") {
      if (defined $opt_d) { printf("# no results\n"); }
      return; 
   }
   my $start = index($data, $beg);
   my $finish = index($data, $end, $start);
   my $year;
   my $type;
   my $title;
   while ($start != -1 && $start < length($data)) {
      $start += length($beg);
      my $entry = substr($data, $start, $finish - $start);
      $start = index($data, $beg, $finish + 1);
      $finish = index($data, $end, $start);

      my $title = "";
      my $year = "";
      my $type = "";
      my $movienum = "";

      my $link_end = "</a>";
      $fl_end = index($entry, $link_end);
      $fl_end += length($link_end);
      my $lhs = substr($entry, 0, $fl_end);
      my $rhs = substr($entry, $fl_end);

      if ($lhs =~ m/<a href="\/title\/tt(\d+)\/.*\">(.+)<\/a>/i) {
          $movienum = $1;
          $title = $2;
      } else {
           if (defined $opt_d) {
               print("Unrecognized entry format\n");
           }
           next;
      }

      if ($rhs =~ m/\((\d+)\) \((.+)\)/) {
          $year = $1;
          $type = $2;
      } elsif ($rhs =~ m/\((\d+)\)/) {
          $year = $1;
      }

      my $skip = 0;

      # fix broken 'tv=no' option
      if ($options =~ /tv=no/) {
         if ($type eq "TV") {
            if (defined $opt_d) {printf("# skipping TV program: %s\n", $title);}
            $skip = 1; 
         }
      }
      if ($options =~ /tv=only/) {
         if ($type eq "") {
            if (defined $opt_d) {printf("# skipping Movie: %s\n", $title);}
            $skip = 1; 
         }
      }
      # fix broken 'from_year=' option
      if ($options =~ /from_year=(\d+)/) {
         if ($year < $1) {
            if (defined $opt_d) {printf("# skipping b/c of yr: %s\n", $title);}
            $skip = 1; 
         }
      }
      # fix broken 'to_year=' option
      if ($options =~ /to_year=(\d+)/) {
         if ($year > $1) {
            if (defined $opt_d) {printf("# skipping b/c of yr: %s\n", $title);}
            $skip = 1; 
         }
      }

      # option to strip out videos (I think that's what '(V)' means anyway?)
      if ($options =~ /video=no/) {
         if ($type eq "V") {
            if (defined $opt_d) {
                printf("# skipping Video program: %s\n", $title);
            }
            $skip = 1; 
         }
      }
   
      # (always) strip out video game's (why does IMDB give these anyway?)
      if ($type eq "VG") {
         if (defined $opt_d) {printf("# skipping videogame: %s\n", $title);}
         $skip = 1; 
      }

      # add to array
      if (!$skip) {
          my $moviename = $title;
          if ($year ne "") {
              $moviename .= " ($year)";
          }

#         $movies[$count++] = $movienum . ":" . $title;
         $movies[$count++] = $movienum . ":" . $moviename;
      }
   }

   # display array of values
   for $movie (@movies) { print "$movie\n"; }
}

#
# Main Program
#

# parse command line arguments 
getopts('ohrdivDMP');

# print out info 
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }

# print out usage if needed
if (defined $opt_h || $#ARGV<0) { help(); }

if (defined $opt_D) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -D <movieid>\n";
   getMovieData($movieid);
}

elsif (defined $opt_P) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -P <movieid>\n";
   getMoviePoster($movieid);
}

elsif (defined $opt_M) {
   # take query from cmdline arg
   $options = shift || die "Usage : $0 -M [options] <query>\n";
   $query = shift;
   if (!$query) {
      $query = $options;
      $options = "";
   }
   getMovieList($query, $options);
}
# vim: set expandtab ts=3 sw=3 :
