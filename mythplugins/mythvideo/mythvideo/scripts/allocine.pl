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

use LWP::Simple;      # libwww-perl providing simple HTML get actions
use HTML::Entities;
use URI::Escape;
#use utf8;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P);
use Getopt::Std; 

$title = "Allocine Query"; 
$version = "v1.00";
$author = "Xavier Hervy";

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
   
   #my $ldata = $data;
   #my $start = index($ldata, $beg) + length($beg);
   #my $finish = index($ldata, $end, $start);
       
   #print "$start $finish\n";
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
   if (defined $opt_r) { printf("%s", $response); }

   # parse title and year
   my $title = parseBetween($response, "<FONT Class=\"titrePage\">", "</FONT>");
   #print "titre = $title\n";
   $title = removeTag($title);
   #print "titre ap = $title\n";
   my $year = parseBetween($response,"<TD><font >","</font>.");
   $year = parseBetween($year,"<font >(",")");

   # parse director 
   my $director = parseBetween($response," par&nbsp;</FONT>","</TD></TR>");
   $director = removeTag($director);

   # parse writer 
   # (Note: this takes the 'first' writer, may want to include others)
   my $writer = parseBetween($response, ">Writing credits</b>", "</table>");
   $writer = parseBetween($writer, "/\">", "</");

   # parse plot
   my $plot = parseBetween($response,"<DIV Align='Justify'><FONT class=\"size2\">","</FONT></DIV>");
   #$plot.replace("<BR>","\n");
   $plot = removeTag($plot);
   

   # parse user rating
   #my $userrating = parseBetween($response, ">User Rating:</b>", "> (");
   #$userrating = parseBetween($userrating, "<b>", "/");
   my $userrating;
   my $rating = parseBetween($response,"Presse","</TABLE>");
   my $nbvote = 0;
   my $sommevote = 0;
   $rating = parseBetween($rating,"etoile_",".gif");
   if (!($rating eq "")){
	    $sommevote += $rating - 1;
	    $nbvote ++;
   }
   $rating = parseBetween($response,"Spectateurs","</TABLE>");
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
   my $runtime = parseBetween($response,">Dur","</FONT>");
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

   my $cast = parseBetween($response, "<FONT Class=\"titreDescription\">Avec&nbsp;", "</TD></TR>");

#   $cast = parseBetween($response, "<FONT Class=\"titreDescription\">Avec&nbsp;", "Plus");
   $cast = removeTag($cast);
   if (parseBetween($cast,"", "Plus") eq ""){
  }else{
   	$cast = parseBetween($cast,"", "Plus");
   }

   #genres
   my $genres = parseBetween($response,")</font>. ","</FONT>.");#. <FONT Class=\"size2\">Dur");
   $genres = removeTag($genres);
   
   #countries
   my $countries = parseBetween($response,"</font>&nbsp;"," <font >(");
   $countries = removeTag($countries);
   
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
   my $request = "http://www.allocine.fr/film/galerie_gen_cfilm=" . $movieid . "&big=1&page=1.html";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my $response = get $request;
   if (defined $opt_r) { printf("%s", $response); }

   my $uri = parseBetween($response, "Ko'></A><BR><CENTER>", "<BR></CENTER></TD></TR></TABLE>");
   if ($uri eq ""){
   	$uri = parseBetween($response, "<A HREF=\"/film/galerie_gen_cfilm=" . $movieid . ".html\" target=\"\"><IMG border=0 src=\"", "\" Zalt");
   	if ($uri eq "") {
   		my $request = "http://www.allocine.fr/film/fichefilm_gen_cfilm=" . $movieid . ".html";
   		if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   		my $response = get $request;
   		if (defined $opt_r) { printf("%s", $response); }
   		$uri = parseBetween($response, "<TABLE Border=0 CellPadding=0 CellSpacing=0><TR><TD><IMG Src='", "' Border=0></TD><TD><IMG Border=0 Src='");
   		
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
   my $count = 0;
   my $typerecherche = 3;
   
   while (($typerecherche <=5) && ($count ==0)){
	   # get the search results  page
	   my $request = "http://www.allocine.fr/recherche/rubrique.html?typerecherche=$typerecherche&motcle=$query";
	   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
	   my $response = get $request;
	   if (defined $opt_r) {
	      print $response;
	      exit(0);
	   }
	
	
	   # extract possible matches
	   #    possible matches are grouped in several catagories:  
	   #        exact, partial, and approximate
	   my $exact_matches = parseBetween($response, "<TABLE width=96% cellPadding=0 cellSpacing=2 border=0>",
	   						"</TABLE>");
	   #print "$exact_matches\n";
	   # parse movie list from matches
	   my $beg = "<A HREF=\"/film/fichefilm_gen_cfilm=";
	   my $end = "</TD></TR>";
	   
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
	         my ($movienum, $moviename) = split(".html\">", $sub);
	         $title = parseBetween($moviename,"<FONT color=#003399>","</FONT></A>");
	         $title = removeTag($title);
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
	      for $movie (@movies) { print "$movie\n"; }
	   }
      }
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
