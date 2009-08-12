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

#   18-06-2009: Geoffroy Geerseau ( http://www.soslinux.net : jamdess AT soslinux DOT net )
#   Add regex to new html code
   
use File::Basename;
use lib dirname($0);
use Encode;

use MythTV::MythVideoCommon;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P $opt_originaltitle $opt_casting $opt_u_dummy);
use Getopt::Long;

$title = "Allocine Query"; 
$version = "v2.04";
$author = "Xavier Hervy";
push(@MythTV::MythVideoCommon::URL_get_extras, ($title, $version));

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

#print encode('iso-8859-1','Durée');
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
   my ($rc, $response) = myth_url_get($request);

   # parse title and year
   my $title = parseBetween($response, "<title>", "</title>");
   $title =~ s/\s*-\s*AlloCin.*//;
   my $original_title = parseBetween($response, ">Titre original : <i>","</i></h3></div>");
   $original_title = removeTag($original_title);
   if (defined $opt_originaltitle){
      if ($original_title ne  ""){
        $title = $title . " (" . $original_title . ")";
      }
   }
   
   #print "titre = $title\n";
   $title = removeTag($title);
   my $year = parseBetween($response,"e de production : ","</h");

   # parse director 
   my $tempresponse = $response;
   $tempresponse =~ s/>R.alis.\spar/>Ralispar/gm;
   my $director = parseBetween($tempresponse,">Ralispar ","</h");
   $director = removeTag($director);

   # parse plot
   my $plot = parseBetween($response,"<td valign=\"top\" style=\"padding:10 0 0 0\"><div align=\"justify\"><h4>","</h4></div></td>");
   $plot =~ s/\n//g;
   $plot = removeTag($plot);
  
   # parse user rating
   my $userrating=0;
   my $tmpratings = parseBetween($response,"Critiques&nbsp;:</b></h5>", "</table>");
   my $rating_pat = qr'class="etoile_(\d+)"';

   # ratings are from one to four stars
   my @ratings = ($tmpratings =~ m/$rating_pat/g);
   $userrating += $_ foreach @ratings;
   if (@ratings) { $userrating /= @ratings; }
	
   if ($userrating) { $userrating = int($userrating * 2.5); }

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

   $tempresponse = $response;
   $tempresponse =~ s/>Dur.e\s:/>Durae\ :/gm;
   my $runtime = parseBetween($tempresponse,"Durae : ",".&nbsp;</h");
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

   my $castchunk;

   my $name_link_pat = qr'<a .*?href="/personne/.*?".*?>(.*?)</a>';
   if (defined $opt_casting){
	  my ($tmprc, $responsecasting) = myth_url_get("http://www.allocine.fr/film/casting_gen_cfilm=" . $movieid . ".html");
      $castchunk = parseBetween($responsecasting, "Acteurs", "</table");
   }
   
   if (!$castchunk) {
      $castchunk = parseBetween($response, ">Avec ","</h");
   }
   
   my $cast = "";
   if (defined $castchunk) {
      $cast = trim(join(',', ($castchunk =~ m/$name_link_pat/g)));
   }

   #genres
   my $genres = parseBetween($response,">Genre : ","</h");
   $genres = removeTag($genres);
   
   #countries
   my $countries = parseBetween($response,">Film ",".&nbsp;</h");
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
   print "Cast:$cast\n";
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
   my ($rc, $response) = myth_url_get($request);
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
	     ($rc, $response) = myth_url_get($request);
        
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
	($rc, $response) = myth_url_get($request);
	$response = parseBetween($response, "sousnav_separe_droite2.gif","sortie");
	$uri = parseBetween($response, "<img src=\"","\"");
   
        #
	# in case no little poster was found get the small DVD poster
	# if exists !
        #
        if ($uri =~ /AffichetteAllocine/)
	{
		$request = "http://www.allocine.fr/film/fichefilm_gen_cfilm=" . $movieid .".html";
		($rc, $response) = myth_url_get($request);
		$response = parseBetween($response, "Disponible en","Zone");
		$uri = parseBetween($response, "<img src=\"","\"");
		return if ($uri eq "");
	}
   }
 
   print "$uri\n";
}

sub getMovieList {
	my ($filename, $options) = @_; # grab parameters

	my $query = cleanTitleQuery($filename);
	if (!$options) { $options = ""; }
	if (defined $opt_d) { 
		printf("# query: '%s', options: '%s'\n", $query, $options);
	}

	# get the search results  page
	my $request = "http://www.allocine.fr/recherche/?rub=1&motcle=$query";
	if (defined $opt_d) { printf("# request: '%s'\n", $request); }
	my ($rc, $response) = myth_url_get($request);

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
	} else {
		my $start = index($data, $beg);
		my $finish = index($data, $end, $start);

		my $title;
		while ($start != -1) {
			$start += length($beg);
			my $sub = substr($data, $start, $finish - $start);
			my ($movienum, $moviename) =
				split(".html\" class=\"link1\">", $sub);
			$title = removeTag($moviename);
			$moviename = removeTag($moviename);
			my ($movieyear)= $moviename =~/\((\d+)\)/;
			if ($movieyear) {
				$title = $title." (".$movieyear.")";
			}
			$moviename=$title ;

			# advance data to next movie
			$data = substr($data, - (length($data) - $finish));
			$start = index($data, $beg);
			$finish = index($data, $end, $start + 1); 

			# add to array
			push(@movies, "$movienum:$moviename");
		}

		# display array of values
		for $movie (@movies) { 
			print "$movie\n"; 
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
# vim: set expandtab ts=3 sw=3 :
