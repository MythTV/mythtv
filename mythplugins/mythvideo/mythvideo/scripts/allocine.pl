#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups in french based on 
# the www.allocine.fr website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: Xavier Hervy (maxpower44 AT tiscali DOT fr)
#

# changes:
# 9-10-2006: Anduin Withers
#   Changed output to utf8
#   Made -u option a dummy for this release, it is deprecated and will be
#   removed

use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;


use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P $opt_originaltitle $opt_casting $opt_u_dummy);
use Getopt::Long;

$title = "Allocine Query"; 
$version = "v2.00";
$author = "Xavier Hervy";

binmode(STDOUT, ":utf8");

# display usage
sub usage {
   print "usage: $0 -hviocMPD [parameters]\n";
   print "       -h, --help                       help\n";
   print "       -v, --version                    display version\n";
   print "       -i, --info                       display info\n";
   print "       -o, --originaltitle              concatenate title and original title\n";
   print "       -c, --casting                    with -D option, grap the complete actor list (much slower)\n";
   print "\n";
   print "       -M <query>,   --movie query>     get movie list\n";
   print "       -D <movieid>, --data <movieid>   get movie data\n";
   print "       -P <movieid>, --poster <movieid> get movie poster\n";
   exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
   print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
   print "Performs queries using the www.allocine.fr website.\n";
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
    	# dont use decode entities &npsp; => spécial characters bug in html::entities ?
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
   my $request = "http://www.allocine.fr/film/fichefilm_gen_cfilm=" . $movieid . ".html";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;

   # parse title and year
   my $title = parseBetween($response, "<title>", "</title>");
   my $original_title = parseBetween($response, "<h4>Titre original : ","</h4><br />");
   $original_title = removeTag($original_title);
   if (defined $opt_originaltitle){
      if ($original_title  ne  ""){
        $title = $title . " (" . $original_title . ")";
      }
   }
   
   #print "titre = $title\n";
   $title = removeTag($title);
   my $year = parseBetween($response,"<h4>Année de production : ","</h4>");

   # parse director 
   my $director = parseBetween($response,"<h4>Réalisé par ","</h4>");
   $director = removeTag($director);

   # parse writer 
   # (Note: this takes the 'first' writer, may want to include others)
   my $writer = parseBetween($response, ">Writing credits</b>", "</table>");
   $writer = parseBetween($writer, "/\">", "</");

   # parse plot
   my $plot = parseBetween($response,"<td valign=\"top\" style=\"padding:10 0 0 0\"><div align=\"justify\"><h4>","</h4></div></td>");
   #$plot.replace("<BR>","\n");
   $plot = removeTag($plot);
  
   # parse user rating
   my $userrating;
   my $rating = parseBetween($response,"Presse</a> <img ", " border=\"0\" /></h4></td>");
   my $nbvote = 0;
   my $sommevote = 0;
   $rating = parseBetween($rating,"etoile_",".gif");
   if (!($rating eq "")){
	    $sommevote += $rating - 1;
	    $nbvote ++;
   }
   $rating = parseBetween($response,"Spectateurs</a> <img ", " border=\"0\" /></h4></td>");
   $rating = parseBetween($rating,"etoile_",".gif");
   if (!($rating eq "")){
	$sommevote += $rating - 1;
	$nbvote ++;
  }
   if ($nbvote==0){$userrating=0};
   if ($nbvote==1){$userrating=$sommevote*2;};
   if ($nbvote==2){$userrating=$sommevote;};
	

   # parse rating
   my $movierating = parseBetween($response,"Interdit aux moins de ","ans");
   if (!($movierating eq ""))
   	{ $movierating = "Interdit aux moins de " . $movierating . "ans";}
   else
   	{
    		$movierating = parseBetween($response,"Visible ","enfants");
   		if (!($movierating eq "")){ $movierating = "Visible par des enfants";};
   	}
   

   # parse movie length
   my $runtime = parseBetween($response,"Durée : ",".</h4>&nbsp;");
   my $heure;
   my $minutes;
   ($heure,$minutes)=($runtime=~/[^\d]*(\d+)[^\d]*(\d*)/);
   if (!$heure){ $heure = 0; }
   if (!$minutes){
      $runtime = $heure * 60;
   }else{
       $runtime = $heure * 60 + $minutes;
  }

 


   # parse cast 

   my $cast = parseBetween($response, "<h4>Avec ","</h4><br />");
   $cast = removeTag($cast);
   if (defined $opt_casting){
      my $responsecasting = get "http://www.allocine.fr/film/casting_gen_cfilm=" . $movieid . ".html";
      my $fullcast = parseBetween($responsecasting, "Acteur(s)", "<table");
      my $oneactor;
      $fullcast = parseBetween($fullcast,"style=\"background-color", "</table>");
      my @listactor = split("style=\"background-color", $fullcast);
      my @cleanlistactor ;
      for $oneactor (@listactor ) { 
        $oneactor = parseBetween($oneactor,"<h4>","</h4>");
        $oneactor =  removeTag($oneactor );        
        push(@cleanlistactor,$oneactor); 
	    }
	    my $finalcast = join (", ",@cleanlistactor);
	    if ($finalcast  ne "") {$cast = $finalcast;};
   }
   
   

   #genres
   my $genres = parseBetween($response,"<h4>Genre : ","</h4>");
   $genres = removeTag($genres);
   
   #countries
   my $countries = parseBetween($response,"<h4>Film ",".</h4>&nbsp;");
   $countries = removeTag($countries);

   # output fields (these field names must match what MythVideo is looking for)
   print "Title:$title\n";
   if (!(defined $opt_originaltitle)){
    print "OriginalTitle:$original_title\n";
   }  
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
   
   my $request = "http://www.allocine.fr/film/galerie_gen_cfilm=" . $movieid . ".html";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   my $page=parseBetween($response,"&page=",".html\" class=\"link1\"><span class=\"text2\">>>");
   my @pages = split ("page=",$page);
   $request = "";

   my $uri = "";
   my $furi = "";
   my $first= 1;
   for $page (@pages ) { 
        $request = $page;
  	
	#
	# get only the page number
	# 
	$request = substr($request, 0, index($request, '.'));
  
        if (!($request eq "")) {
	     $request = "http://www.allocine.fr/film/galerie_gen_cfilm=" . $movieid . "&page=" . $request . ".html";
	     $response = get $request;
        
             $uri = parseBetween($response,"<table style=\"padding:0 0 0 0\" border=\"0\" >","Ko\" />");
             $uri = parseBetween($uri ,"<img src=\"","\" border=\"0\" class=\"galerie\" ");
	     if ($first && ! ($uri eq ""))
	     {
		     $furi = $uri;
		     $first = 0;
	     }


        }
	#
	# stop when we have an poster...
	#
	last if (($uri =~ /affiche/) or ($uri =~ /_af/))
   }

   # if $uri =~ affiche or _af then get the first poster if exist

   if (($uri !~ /affiche/) or ($uri !~ /_af/))
   {
	   if ($first == 0)
	   {
		   $uri = $furi;
	   }
   }

   #
   # in case nothing was found fall back to the little poster...
   #
   if ($uri eq "")
   {
	$request = "http://www.allocine.fr/film/fichefilm_gen_cfilm=" . $movieid .".html";
	$response = get $request;
	$response = parseBetween($response, "sousnav_separe_droite2.gif","sortie");
	$uri = parseBetween($response, "<img src=\"","\"");
   
        #
	# in case no little poster was found get the small DVD poster
	# if exists !
        #
        if ($uri =~ /AffichetteAllocine/)
	{
		$request = "http://www.allocine.fr/film/fichefilm_gen_cfilm=" . $movieid .".html";
		$response = get $request;
		$response = parseBetween($response, "Disponible en","Zone");
		$uri = parseBetween($response, "<img src=\"","\"");
		return if ($uri eq "");
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
   # Strip off anything following '-' - people use this for general comments
   if (index($query, '-') != -1) {
      $query = substr($query, 0, index($query, '-')); 
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
   my $typerecherche = 3;
  
   while (($typerecherche <=5) && ($count ==0)){
	   # get the search results  page
	   my $request = "http://www.allocine.fr/recherche/?rub=1&motcle=$query";
	   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
	   my $response = get $request;

	   #
	   # don't try to invent if it doesn't exist
	   #
	   return if $response =~ /Pas de résultats/;
	
	   # extract possible matches
	   #    possible matches are grouped in several catagories:  
	   #        exact, partial, and approximate
	   my $exact_matches = $response;
	   # parse movie list from matches
	   my $beg = "<h4><a href=\"/film/fichefilm_gen_cfilm=";
	   my $end = "</a></h4>";
	   
	   my @movies;
	
	   my $data = $exact_matches;
	   if ($data eq "") {
	      if (defined $opt_d) { printf("# no results\n"); }
	   	$typerecherche = $typerecherche +2 ;
	   }else{
	      my $start = index($data, $beg);
	      my $finish = index($data, $end, $start);
	   
	      my $title;
	      while ($start != -1) {
	         $start += length($beg);
	         my $sub = substr($data, $start, $finish - $start);
	         my ($movienum, $moviename) = split(".html\" class=\"link1\">", $sub);
	         $title = removeTag($moviename);
	         $moviename = removeTag($moviename);
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
	      for $movie (@movies) { 
	        print "$movie\n"; 
	      }
	   }
      }
}

#
# Main Program
#

# parse command line arguments 

    GetOptions( "utf8" => \$opt_u_dummy,
                "version" => \$opt_v,
                "info" => \$opt_i,
                "originaltitle" => \$opt_originaltitle,
                "casting" => \$opt_casting,
                "Data" => \$opt_D,
                "Movie" => \$opt_M,
                "Poster" => \$opt_P
                );       
            

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
   #$options = shift || die "Usage : $0 -M <query>\n";
   my $query;
   my $options = '';
   foreach $key (0 .. $#ARGV) {
   	$query .= $ARGV[$key]. ' ';
   }
   getMovieList($query, $options);
}
