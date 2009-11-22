#!/usr/bin/perl -w

#
# This script get movie data from www.alpacine.com website, 
# for spanish user.
#
# For more information on MythVideo's external movie look-up mechanism, see
# README file in this directory.
#
# Author: Alvaro Gonzalez Sola
# v1.2
#
# changes:
# - 1.1 - fixed encode error using utf-8.
# - 1.2 - By Robert McNamara, fix broken regex on cast/genres

use LWP::UserAgent;
use HTML::Entities;
use URI::Escape;

use Encode;

eval "use DateTime::Format::Strptime"; my $has_date_format = $@ ? 0 : 1;

use vars qw($opt_h $opt_r $opt_d $opt_l $opt_i $opt_v $opt_D $opt_M);
use Getopt::Long;

$title = "Alpacine Query"; 
$version = "v1.1";
$author = "Alvaro Gonzalez Sola";

binmode(STDOUT, ":utf8");

# display usage
sub usage {
   print "usage: $0 -hdrviMPD [parameters]\n";
   print "       -h           help\n";
   print "       -d           debug\n";
   print "       -r           dump raw query result data only\n";
   print "       -v           display version\n";
   print "       -i           display info\n";
   print "\n";
   print "       -M <query>    get movie list\n";
   print "       -D <movieid>  get movie data\n";
   exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
   print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
   print "Performs queries using the www.alpacine.com website.\n";
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
   my $request = "http://www.alpacine.com/pelicula/" . $movieid . "/";
   my $alpacineurl = $request;
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $ua = LWP::UserAgent->new;
   $ua->timeout(10);
   $ua->env_proxy;
   my $response = $ua->get($request);
   if (!$response->is_success){die $response->status_line;}
   if (defined $opt_r) { print $response->content; }

   # parse title and year
   my $year = "";
   my $title = parseBetween($response->content, "<title>", "</title>");
   if($title =~ m/(.+) \((\d+)\).*/)
   {
      $title = $1;
      $year = $2;
   }else{$title = "";}

   # parse director 
   my $data = parseBetween($response->content, "Direcci","/a>",);
   $data =~ /.*>(.*)</; 
   my $director = $1;

   #www.alpacine.com don't have writer informatation,
   #in future release is possible connect to other website for get writer information
   # parse writer 
   my $writer = "";

   # change ", " for ","
   if($writer){
      $writer =~ s/, /,/g;
   }else{
      $writer = "";
   }

   # parse release date
   my $releasedate = '';
   if ($has_date_format) {
      my $dtp = new DateTime::Format::Strptime(pattern => '%d %b %Y',
         on_error => 'undef');
      my $dt = $dtp->parse_datetime(parseBetween($response->content,
            "a:</div><div class=\"cuerpo\">", "</div>"));
      if (defined($dt)) {
         $releasedate = $dt->strftime("%F");
      }
   }

   # parse plot
   my $plot = parseBetween($response->content, "Sinopsis:</div><div class=\"cuerpo\">", "</div>");

   # parse user rating
   my $userrating = "";
   $data = parseBetween($response->content, "<div class=\"voto\">", "</div>");
   if(length($data)>0){
      $data =~ m/ *(.+) */;
      $userrating = $1;
   }

   # parse MPAA rating
   my $movierating = $userrating;

   # parse movie length
   my $runtime ="";
   $data = parseBetween($response->content,"Durac","/div>");
   $data = parseBetween($data, "</span> "," ");
   if(length($data)>0){$runtime = $data;}

   # parse cast
   my $cast = "";
   $data = parseBetween($response->content, "n:</div><div class=\"cuerpo\"><div>","</div></div></div>"); 
   $data =~ s/<a href="http:\/\/www.alpacine.com\/celebridad\/\d+\/">//g;
   $data =~ s/<\/a>//g;
   $data =~ s/<\/div><div>/,/g;
   $data =~ s/<[^>]*>//gs;
   $cast = $data;

   # parse genres 
   my $lgenres = "";
   $lgenres = parseBetween($response->content, "nero:</span>","</div>"); 
   $lgenres =~ s/<a href="http:\/\/www.alpacine.com\/peliculas\/generos\/\d+\/">//g;
   $lgenres =~ s/<\/a>//g;
   $lgenres =~ s/<[^>]*>//gs;

   # parse countries 
   $data = parseBetween($response->content, "<div class=\"dato\"><span class=\"titulo\">Pa","/a>"); 
   $data =~/">(.*)</;
   my $lcountries = $1;

   # output fields (these field names must match what MythVideo is looking for)
   print "Title:$title\n";
   print "Year:$year\n";
   print "ReleaseDate:$releasedate\n";
   print "Director:$director\n";

   # fix encoding plot because this use ISO-8859-1
	Encode::from_to($plot, "utf8", "ISO-8859-1");

   # Add the coverart to the output
   my $uri = "";
   my $impsite = "";
   $impsite = parseBetween($response->content, "/cartel/", "/");
   if (defined $opt_d) { print "$impsite\n";}

   if($impsite){
      # Make the poster url direction
      if(length($impsite) > 0){
      $impsite = ("http://www.alpacine.com/cartel/".$impsite."/");
      }
      if (defined $opt_d) { printf("%s", $impsite); }

      $response = $ua->get($impsite);
      if (defined $opt_r) { printf("%s", $response->content); }

      # Extract image url from poster page
      $data = parseBetween($response->content,"\"imagen\" src=\"","\"");
      if($data){$uri = $data;}
   }else{
      #get miniature image if don't has big poster image
      $data = parseBetween($response->content,"<div class=\"imagen\">","alt");
      $uri = parseBetween($data,"\"","\"");
   }

   print "Plot:$plot\n";
   print "URL:$alpacineurl\n";
   print "UserRating:$userrating\n";
   print "MovieRating:$movierating\n";
   print "Runtime:$runtime\n";
   print "Writers: $writer\n";
   print "Cast: $cast\n";
   print "Genres: $lgenres\n";
   print "Countries: $lcountries\n";
   print "Coverart: $uri\n";
}

# dump Movie list:  1 entry per line, each line as 'movieid:Movie Title'
sub getMovieList {
   my ($filename)=@_; # grab parameters # adquiere los parametros

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

   # prepare the url
   $query = uri_escape($query);
   if (defined $opt_d) { 
      printf("# query: '%s'\n", $query);
   }

   # get the search results  page
   my $request="http://www.alpacine.com/buscar/?buscar=$query";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }

   my $ua = LWP::UserAgent->new;
   $ua->timeout(10);
   $ua->env_proxy;
   my $response = $ua->get($request);
   if (!$response->is_success){die $response->status_line;}
   if (defined $opt_r) { print $response->content; }

   # parse movie list from matches
   my $beg = "<li>";
   my $end = "</li>";
   my $count = 0;
   my @movies;

   my $data = parseBetween($response->content,"<span class=\"resultados\">","</ul>");
   if ($data eq "") {
      if (defined $opt_d) { printf("# no results\n"); }
      return; 
   }

   my $start = index($data, $beg);
   my $finish = index($data, $end, $start);
   my $year;
   my $title;
   #Extract movie list, with id number, title and year
   while ($start != -1 && $start < length($data)) {
      $start += length($beg);
      my $entry = substr($data, $start, $finish - $start);
      $start = index($data, $beg, $finish + 1);
      $finish = index($data, $end, $start);

      my $title = "";
      my $year = "";
      my $movienum = "";

      if($entry =~ /.*\/pelicula\/(\d+)\/">(.*)<\/a> \((\d+)\)/){
          $movienum = $1;
           $title = $2;
           $year = $3;
      } else {
           if (defined $opt_d) {
               print("Unrecognized entry format ($entry)\n");
           }
           next;
      }

      my $skip = 0;

      # add to array
      my $moviename = $title;
      if ($year ne "") {
         $moviename .= " ($year)";
      }
      $movies[$count++] = $movienum . ":" . $moviename;

   }

   # display array of values
   for $movie (@movies) { print "$movie\n"; }
}

#
# Main Program
#

# parse command line arguments 

    GetOptions( "version" => \$opt_v,
                "info" => \$opt_i,
                "language" => \$opt_l,
                "Data" => \$opt_D,
                "Movie" => \$opt_M,
                );

# print out info 
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }
if (defined $opt_l) {
    my $lang = shift;
}

# print out usage if needed
if (defined $opt_h || $#ARGV<0) { help(); }

# get Movie Data
if (defined $opt_D) {
   # take movieid from cmdline arg
   $movieid = shift || die "Usage : $0 -D <movieid>\n";
   getMovieData($movieid);
}

# get Movie List
elsif (defined $opt_M) {
   # take query from cmdline arg
   $options = shift || die "Usage : $0 -M [options] <query>\n";
   $query = shift;
   if (!$query) {
      $query = $options;
      $options = "";
   }
   getMovieList($query);
}
