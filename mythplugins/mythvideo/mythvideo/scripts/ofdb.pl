#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups in german based on 
# the www.ofdb.de website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: Xavier Hervy (maxpower44 AT tiscali DOT fr)
#

# changes:
# 9-10-2006: Anduin Withers
#   Updated to work with new " instead of ' in pages
#   Changed output to utf8

use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P);
use Getopt::Std; 

$title = "Ofdb Query"; 
$version = "v1.00";
$author = "Xavier Hervy";

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
   print "       -P <movieid>  get movie poster\n";
   exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
   print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
   print "Performs queries using the www.ofdb.de website.\n";
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
   
   #my $ldata = $data;
   #my $start = index($ldata, $beg) + length($beg);
   #my $finish = index($ldata, $end, $start);
       
   #print "$start $finish\n";
   if ($start != (length($beg) -1) && $finish != -1) {
        my $result = substr($data, $start, $finish - $start);
        # dont use decode entities &npsp; => sp�ial characters bug in html::entities ?
        #decode_entities($result);
        return  removenbsp($result);
   }
   return "";
}

# use to replace &nbsp; by " " (instead of decode_entities)
sub removenbsp {
   my ($data)=@_; # grab parameters

   my $ldata = lc($data);
   my $start = index($ldata, "&nbsp;");
   while ($start != -1){
      $data = substr($data, 0, $start). " " .substr($data, $start+6, length($data));
      $ldata = lc($data);
      $start = index($ldata, "&nbsp;");
   }
   return $data;
}


# returns text within 'data' without tag
sub removeTag {
   my ($data)=@_; # grab parameters

   my $ldata = lc($data);
   my $start = index($ldata, "<");
   my $finish = index($ldata, ">", $start)+1;
   while ($start != -1 && $finish != -1){
      $data = substr($data, 0, $start).substr($data, $finish, length($data));
      $ldata = lc($data);
      $start = index($ldata, "<");
      $finish = index($ldata, ">", $start)+1;
   }
   return $data;
}

# get Movie Data 
sub getMovieData {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.ofdb.de/view.php?page=film&fid=" . $movieid;
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
#print "$response\n";
   if (defined $opt_r) { printf("%s", $response); }

   # parse title and year
   my $title = parseBetween($response, "<font face=\"Arial,Helvetica,sans-serif\" size=\"3\"><b>","</b></font></td>");
   #print "titre = $title\n";
   my $year = parseBetween($response,"<a href=\"view.php?page=blaettern&Kat=Jahr&Text=","\">");
#   $year = parseBetween($year,"<font >(",")");

   # parse director 
   my $director = parseBetween($response,"Regie:","</tr>");
   $director = parseBetween($director,"\">","</a>");
#print "Director $director";
   $director = removeTag($director);

   # parse user rating
   my $userrating = parseBetween($response, "Note: ", "&nbsp;");

   # parse cast 
   my $cast = parseBetween($response,"Darsteller:","</b>");
   #$cast = parseBetween($cast,"Daten\"><b>","</b>...");
   $cast =~  s/<br><a/,<a/g;
   # remove linebreaks and empty space:
   $cast =~  s/\n//g;
   1 while ($cast =~ s/\s\s//g);

   $cast = removeTag($cast);
 
   #genres
   $genres = parseBetween($response,"Genre(s):","></font></td>");
   $genres = parseBetween($genres,"class=\"Daten\"><b>","</b");
   $genres =~  s/<br><a/,<a/g;
   $genres = removeTag($genres);
   
   #countries
   my $countries = parseBetween($response,"Herstellungsland","</tr>");
   $countries = parseBetween($countries,"Daten\"><b>","</td>");
   $countries =~  s/<br><a/,<a/g;
   $countries = removeTag($countries);
   
   # parse plot
   my $plot = parseBetween($response,"Inhalt:","[mehr]");
   $plot = removeTag($plot);
   my $ploturl = parseBetween($response,"view.php?page=inhalt","\"><b>[mehr]");

  
   my $runtime = 0; 
   my $movierating = ""; 
   my $writer = "";
   #runtime provide from german.imdb.com 
   my $urlimdb = parseBetween($response,"http://german.imdb.com/Title?","\" target");
   if ($urlimdb eq ""){
   }else{
       $request = "http://german.imdb.com/Title?".$urlimdb;
       $response = get $request;
       #parse movie length
       $runtime = parseBetween($response,"L&auml;nge:</b>\n"," min ");

       #parse movie rating
       $movierating = parseBetween($response,"Altersfreigabe:</b>\n"," \n<br>");
       $movierating = removeTag($movierating);

       #parse writer (only the first)
       $writer = parseBetween($response,"<b class=\"blackcatheader\">Buch</b>\n\n<br>\n"," <br>");
       $writer = parseBetween($writer,">","</a>");
   }
        
   # parse plot
   if ($ploturl eq ""){
   }
   else{
      $request = "http://www.ofdb.de/view.php?page=inhalt" . $ploturl;
      $response = get $request;
      $response = parseBetween($response,"</a></b><br><br>","</font></p>");
      if ($response eq ""){
      }else{
          $plot=$response;
      }
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
   print "Genres:$genres\n";
   print "Countries:$countries\n";
}

# dump Movie Poster
sub getMoviePoster {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   my $request = "http://www.ofdb.de/view.php?page=film&fid=" . $movieid;
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   my $uriofdb = "";
   $uriofdb = parseBetween($response, "Linke", "Aufgerufen");
   $uriofdb = parseBetween($uriofdb,"src=\"","\" alt"); 
   if ($uriofdb eq "images/film/na.gif") {
       $uriofdb = "";
   }else{
      $uriofdb = "http://www.ofdb.de/$uriofdb\n";
   }

   my $urlimdb = parseBetween($response,"http://german.imdb.com/Title?","\" target");
   my $uri="";
   if ($urlimdb eq ""){
   }else{
         # get the search results  page
        my $request = "http://www.imdb.com/title/tt" . $urlimdb . "/posters";
        if (defined $opt_d) { printf("# request: '%s'\n", $request); }
        my $response = get $request;
        if (defined $opt_r) { printf("%s", $response); }
        
        # look for references to impawards.com posters - they are high quality
        my $site = "http://www.impawards.com";
        my $impsite = parseBetween($response, "<a href=\"".$site, "\">".$site);
       if ($impsite) {
          $impsite = $site . $impsite;
          if (defined $opt_d) 
             { print "# Searching for poster at: ".$impsite."\n"; }
          my $impres = get $impsite;
         if (defined $opt_d) { printf("# got %i bytes\n", length($impres)); }
         if (defined $opt_r) { printf("%s", $impres); }
         $uri = parseBetween($impres, "<img SRC=\"posters/", "\" ALT");
         # uri here is relative... patch it up to make a valid uri
         if (!($uri =~ /http:(.*)/ )) {
            my $path = substr($impsite, 0, rindex($impsite, '/') + 1);
            $uri = $path."posters/".$uri;
         }
         if (defined $opt_d) { print "# found ipmawards poster: $uri\n"; }
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
   }
   if ($uri eq ""){
     print "$uriofdb\n";
   }else{
     print "$uri\n";
   }
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
   my $count = 0;
 #  my $typerecherche = 3;
   
 #  while (($typerecherche <=5) && ($count ==0)){
       # get the search results  page
       my $request = "http://www.ofdb.de/view.php?page=suchergebnis&Kat=DTitel&SText=$query";
       if (defined $opt_d) { printf("# request: '%s'\n", $request); }
       my $response = get $request;
       if (defined $opt_r) {
          print $response;
          exit(0);
       }
    
    
       # extract possible matches
       #    possible matches are grouped in several catagories:  
       #        exact, partial, and approximate
       my $exact_matches = parseBetween($response, "</b><br><br>1.",
                            "<br><br><br></font></p>");
       # print "$exact_matches\n";
       # parse movie list from matches
       my $beg = "<a href=\"view.php?page=film&";
       my $end = "</a>";
       
       my @movies;
    
    
       my $data = $exact_matches;
#      if ($data eq "") {
#         if (defined $opt_d) { printf("# no results\n"); }
#       $typerecherche = $typerecherche +2 ;
#      }else{
          my $start = index($data, $beg);
          my $finish = index($data, $end, $start);
         
          my $title;
          while ($start != -1) {
             $start += length($beg);
             my $sub = substr($data, $start, $finish - $start);
             my $movienum =  parseBetween($sub,"fid=","\">");
             $title =  parseBetween($sub,">","<font size=\"1\">");
             $title = removeTag($title);
             $moviename = removeTag($sub);
             my ($movieyear)= $moviename =~/\((\d+)\)/;
             if ($movieyear){$title = $title." (".$movieyear.")"; }
             $moviename=$title ;
          
             # advance data to next movie
             $data = substr($data, - (length($data) - $finish));
             $start = index($data, $beg);
             $finish = index($data, $end, $start + 1); 
          
             # add to array
             $movies[$count++] = $movienum . ":" . $moviename;
          }
          
          # display array of values
          for $movie (@movies) { print "$movie\n"; }
#      }
#      }
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

