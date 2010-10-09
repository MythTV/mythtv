#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups in french based on 
# the www.allocine.fr website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Original author: Xavier Hervy (maxpower44 AT tiscali DOT fr)

# changes:
#   20-10-2009: Geoffroy Geerseau ( http://www.soslinux.net : jamdess AT soslinux DOT net )
#   Modified for the new allocine templates
#   25-10-2009: Geoffroy Geerseau ( http://www.soslinux.net : jamdess AT soslinux DOT net )
#   Poster download correction
#   Userrating correction
#   28-10-2009: Robert McNamara (Myth Dev)
#   Fix issues in above patches-- files should never be downloaded to /tmp.
#   Convert script to output in new grabber output format for .23.  Leave backwards compat.
#   02-11-2009: Geoffroy Geerseau
#   Allocine have, once again, change their templates...
#   09-10-2010: Geoffroy Geerseau
#   EOL correction. Cast, Genre, Rates

   
use File::Basename;
use File::Copy;
use lib dirname($0);
use Encode;
use utf8;
use Encode 'from_to';
use MythTV::MythVideoCommon;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_l $opt_M $opt_P $opt_originaltitle $opt_casting $opt_u_dummy);
use Getopt::Long;

$title = "Allocine Query"; 
$version = "v2.05";
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
   my $allocineurl = $request;
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my ($rc, $response) = myth_url_get($request);
   from_to($response,'utf-8','iso-8859-1');
   # parse title and year
   my $title = parseBetween($response, "<title>", "</title>");
   $title =~ s/\s*-\s*AlloCin.*//;
   $title =~ s/(.*)\(.*$/$1/;
   $title =~ s/^\s*(.*)\s*$/$1/;
   my $original_title = parseBetween($response, "Titre original :","<br");
   $original_title = trim(removeTag($original_title));
   if (defined $opt_originaltitle){
      if ($original_title ne  ""){
        $title = $title . " (" . $original_title . ")";
      }
   }
   
   $title = removeTag($title);
   my $year = parseBetween(parseBetween($response,"/film/tous/decennie","/a>"),'>','<');

   # parse director 
   my $tempresponse = $response;
   my $director = parseBetween($tempresponse,"Réalisé par ","</a></span>");
   $director = removeTag($director);

   # parse plot
   my $plot = parseBetween($response,"Synopsis : </span>","</p>");
   $plot =~ s/\n//g;
   $plot = trim(removeTag($plot));
  
   # parse user rating
   my $userrating=0;
   my $tmpratings = parseBetween(parseBetween($response,"/film/critiquepublic_gen_cfilm=$movieid.html\"><img", "</span></p></div>"),'(',')');
   $tmpratings =~ s/,/./gm;
   if($tmpratings =~ /^(\d+\.?\d*|\.\d+)$/ && !$tmpratings eq "")
   {   
	$userrating = int($tmpratings*2.5);
   }
   else
   {
	$userrating =  "";
   }

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

   my $runtime = trim(parseBetween($response,"Durée :","min"));
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
   $castchunk = parseBetween($response,"Réalisé par ","$movieid.html\">plus</a>");
   $castchunk = parseBetween($castchunk, "Avec ","<a class=\"underline\" href=\"/film/casting_gen_cfilm=");
   
   my $cast = "";
   $cast = trim(join(',', removeTag($castchunk)));
   $cast =~ s/,\s([a-zA-Z0-9])/--$1/gm;
   $cast =~ s/,//gm;
   $cast =~ s/\-\-/, /gm;
   #genres
   my $genres = parseBetween($response,"Genre :","<br");
   $genres =~ s/\s*\n*(.*)\s*$/ $1/;
   $genres = trim(removeTag($genres));
   $genres =~ s/\s\s//gm;
   $genres =~ s/,\s*/, /gm;
   #countries
   my $countries = parseBetween($response,"Long-métrage",".");
   $countries = trim(removeTag($countries));
   $countries =~ s/\s*(.*)\s*$/ $1/;
   $countries = trim($countries);
   $countries =~ s/\n//gm;
   $countries =~ s/\s//gm;
   $countries =~ s/,/, /gm;
   # parse for coverart
   my $mediafile = parseBetween($response,"<a href=\"/film/fichefilm-".$movieid."/affiches/detail/?cmediafile=","\" >");
   $covrequest = "http://www.allocine.fr/film/fichefilm-".$movieid."/affiches/detail/?cmediafile=".$mediafile;
   ($rc, $covresponse) = myth_url_get($covrequest);
   my $uri = parseBetween(parseBetween($covresponse,"<div class=\"tac\" style=\"\">","</div>"),"<img src=\"","\" alt");
   if ($uri eq "")
   {
        $request = "http://www.allocine.fr/film/fichefilm-".$movieid."/affiches/";
        ($rc, $response) = myth_url_get($request);
        my $tmp_uri = parseBetween($response, "<a href=\"/film/fichefilm-".$movieid."/affiches/\">"," alt=");
        $tmp_uri =~ s/\n/ /gm;
        $uri = trim(parseBetween($tmp_uri,"<img src='h","'"));
        if($uri ne "")
        {
                $uri = "h$uri";
        }
   }
   # if no picture was found, just download the empty poster
   if($uri eq ""){
        $uri = "http://images.allocine.fr/r_160_214/commons/emptymedia/AffichetteAllocine.gif";
   }

   # output fields (these field names must match what MythVideo is looking for)
   print "Title:$title\n";
   if (!(defined $opt_originaltitle)){
    print "OriginalTitle:$original_title\n";
   }
   print "URL:$allocineurl\n";
   print "Year:$year\n";
   print "Director:$director\n";
   print "Plot:$plot\n";
   print "UserRating:$userrating\n";
   print "MovieRating:$movierating\n";
   print "Runtime:$runtime\n";
   print "Cast:$cast\n";
   print "Genres:$genres\n";
   print "Countries:$countries\n";
   print "Coverart: $uri\n";
}

# dump Movie Poster
sub getMoviePoster {
   my ($movieid)=@_; # grab movieid parameter
   if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

   # get the search results  page
   
   my $request = "http://www.allocine.fr/film/fichefilm-".$movieid."/affiches/";
   if (defined $opt_d) { printf("# request: '%s'\n", $request); }
   my ($rc, $response) = myth_url_get($request);
   my $mediafile = parseBetween($response,"<a href=\"/film/fichefilm-".$movieid."/affiches/detail/?cmediafile=","\" >");

   $request = "http://www.allocine.fr/film/fichefilm-".$movieid."/affiches/detail/?cmediafile=".$mediafile;
   ($rc, $response) = myth_url_get($request);
   my $uri = parseBetween(parseBetween($response,"<div class=\"tac\" style=\"\">","</div>"),"<img src=\"","\" alt");
   if ($uri eq "")
   {
	$request = "http://www.allocine.fr/film/fichefilm-".$movieid."/affiches/";
	($rc, $response) = myth_url_get($request);
	my $tmp_uri = parseBetween($response, "<a href=\"/film/fichefilm-".$movieid."/affiches/\">"," alt=");
        $tmp_uri =~ s/\n/ /gm;
	$uri = trim(parseBetween($tmp_uri,"<img src='h","'"));
	if($uri ne "")
	{
		$uri = "h$uri";
	}
        print "$uri\n";
   }
   
   # if no picture was found, just download the empty poster
   if($uri eq ""){
	$uri = "http://images.allocine.fr/r_160_214/commons/emptymedia/AffichetteAllocine.gif";
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
	my $request = "http://www.allocine.fr/recherche/1/?q=$query";
	if (defined $opt_d) { printf("# request: '%s'\n", $request); }
	my ($rc, $response) = myth_url_get($request);
	from_to($response,'utf-8','iso-8859-1');
	$response =~ s/\n//g;
	# extract possible matches
	#    possible matches are grouped in several catagories:  
	#        exact, partial, and approximate
	my $exact_matches = $response;
	# parse movie list from matches
	my $beg = "<div style=\"margin-top:-5px;\">";
	my $end = "<span class=\"fs11\">";

	my @movies;

	my $data = $exact_matches;
	if ($data eq "") {
		if (defined $opt_d) { printf("# no results\n"); }
	} else {
		my $start = index($data, $beg);
		my $finish = index($data, $end, $start);

		my $title;
		my $movienum;
		my $moviename;
		while ($start != -1) {
			$start += length($beg);
			my $sub1 = substr($data, $start, $finish - $start);
			$sub1 =~ s/(.*)\(.*$/$1/;
			$moviename = trim(removeTag($sub1));
			$movienum = parseBetween($sub1,"<a href='/film/fichefilm_gen_cfilm=",".html");
			
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
			$finish = index($data, $end, $start); 

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
                "language" => \$opt_l,
                "originaltitle" => \$opt_originaltitle,
                "casting" => \$opt_casting,
                "Data" => \$opt_D,
                "Movie" => \$opt_M,
                "Poster" => \$opt_P
                );       
            

# print out info 
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }
if (defined $opt_l) {
    my $lang = shift;
}

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
