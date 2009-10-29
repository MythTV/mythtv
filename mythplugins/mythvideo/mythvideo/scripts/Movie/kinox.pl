#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups in russian
# based on the www.kinox.ru website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: Denys Dmytriyenko (denis AT denix DOT org)
# Based on the allocine script by Xavier Hervy
#

# Note:
#  Encoding on the Web page is cp1251
#  Internal encoding of this script is koi8-r
#  The output of this script is in utf8 (set by "outcp" below)

use File::Basename;
use lib dirname($0);

use MythTV::MythVideoCommon;

no encoding;

use Encode;
use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P $opt_originaltitle $opt_casting $opt_u_dummy);
use Getopt::Long;

$title = "KinoX Query"; 
$version = "v0.04";
$author = "Denys Dmytriyenko";
push(@MythTV::MythVideoCommon::URL_get_extras, ($title, $version));

# This is the output encoding
$outcp = "utf8";

# binmode() does not work for some reason
# The output ends up being in the wrong encoding
#binmode(STDOUT, ":utf8");

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
#   print "       -P <movieid>, --poster <movieid> get movie poster\n";
	exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
	print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info {
	print "Performs queries using the www.kinox.ru website.\n";
}

# display detailed help 
sub help {
	version();
	info();
	usage();
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
	my $request = "http://www.kinox.ru/index.asp?comm=4&num=" . $movieid;
	if (defined $opt_d) { printf("# request: '%s'\n", $request); }
	my ($rc, $response) = myth_url_get($request);

	# parse title and year
	my $sub = parseBetween($response, "<h1>", "</h1>");
	my ($sub1, $countries) = split("<br>", $sub);

	$countries = removeTag($countries);
	$countries =~ s/[\n\r]/ /g;
	Encode::from_to($countries, "windows-1251", $outcp);

	my ($title, $original_title) = split("<font size=4 color=#000000> / </font>", $sub1);
	$title = removeTag($title);
	if (!$original_title) { $original_title = "" ;}
	$original_title = removeTag($original_title);

	Encode::from_to($title, "windows-1251", $outcp);
	Encode::from_to($original_title, "windows-1251", $outcp);

	if (defined $opt_originaltitle){
		if ($original_title  ne  ""){
			$title = $title . " (" . $original_title . ")";
		}
	}

	# parse director 
	my $dirq = "<b>Режиссер:</b>";
	Encode::from_to($dirq, "koi8-r", "windows-1251");
	my $director = parseBetween($response, $dirq, "</a>");
	$director = removeTag($director);
	$director =~ s/\s{2,}//;
	Encode::from_to($director, "windows-1251", $outcp);

	# parse plot
	my $plotq = "<b>Краткое содержание:</b>";
	Encode::from_to($plotq, "koi8-r", "windows-1251");
	my $plot = parseBetween($response, $plotq, "</p>");
	$plot = removeTag($plot);
	$plot =~ s/\s{2,}//;
	Encode::from_to($plot, "windows-1251", $outcp);

	# parse cast
	my $castq = "<b>В ролях:</b>";
	Encode::from_to($castq, "koi8-r", "windows-1251");
	my $cast = parseBetween($response, $castq, "<b>");
	$cast = removeTag($cast);
	$cast =~ s/\s{2,}//;
	$cast =~ s/\s\(.*?\)//g;
	$cast =~ s/\s*,\s*/,/g;
	$cast =~ s/\.$//;
	Encode::from_to($cast, "windows-1251", $outcp);

	# studio, year, genres, runtime
	$sub = parseBetween($response, "<td colspan=2 bgcolor=f8f8f8 align=\"center\" valign=\"top\">", "</td></tr></table>");
	$sub =~ s/&nbsp;//g;
	$sub =~ s/&nbsp//g;
	Encode::from_to($sub, "windows-1251", $outcp);

	my $beg = "<font color=\"#008000\">";
	my $end = "</font>";

	my $start = index($sub, $beg);
	my $finish = index($sub, $end, $start);

	$start += length($beg);
	my $studio = substr($sub, $start, $finish - $start);
	$studio = removeTag($studio);

	$sub = substr($sub, - (length($sub) - $finish));

	$start = index($sub, $beg);
	$finish = index($sub, $end, $start);

	$start += length($beg);
	my $year = substr($sub, $start, $finish - $start);
	$year = removeTag($year);

	$sub = substr($sub, - (length($sub) - $finish));

	$start = index($sub, $beg);
	$finish = index($sub, $end, $start);

	$start += length($beg);
	my $genres = substr($sub, $start, $finish - $start);
	$genres = removeTag($genres);
	$genres =~ s|\s*/\s*|,|g;

	$sub = substr($sub, - (length($sub) - $finish));

	$start = index($sub, $beg);
	$finish = index($sub, $end, $start);

	$start += length($beg);
	my $runtime = substr($sub, $start, $finish - $start);
	$runtime = removeTag($runtime);

	# output fields (these field names must match what MythVideo is looking for)
	print "Title:$title\n";
	if (!(defined $opt_originaltitle)){
		print "OriginalTitle:$original_title\n";
	}  
	print "Year:$year\n";
	print "Director:$director\n";
	print "Plot:$plot\n";
	print "Runtime:$runtime\n";
	print "Cast:$cast\n";
	print "Genres:$genres\n";
	print "Countries:$countries\n";
}

# dump Movie list:  1 entry per line, each line as 'movieid:Movie Title'
sub getMovieList {
	my ($filename, $options)=@_; # grab parameters

	# If we wanted to inspect the file for any reason we can do that now

	#
	# Convert filename into a query string 
	# (use same rules that Metadata::guesTitle does)
	my $query = cleanTitleQuery($filename, sub {
			my ($arg) = @_;
			Encode::from_to($arg, "koi8-r", "windows-1251");
			return $arg;
		});
	if (!$options) { $options = "" ;}
	if (defined $opt_d) {
		printf("# query: '%s', options: '%s'\n", $query, $options);
	}

	my $count = 0;
	my $typerecherche = 3;

	while (($typerecherche <=5) && ($count ==0)){
		# get the search results  page
		my $request = "http://www.kinox.ru/index.asp?comm=1&fop=false&pack=0&kw=$query";
		if (defined $opt_d) { printf("# request: '%s'\n", $request); }
		my ($rc, $response) = myth_url_get($request);
		if (defined $opt_d) { printf("# response: '%s'\n", $response); }

		#
		# don't try to invent if it doesn't exist
		#
		my $notfnd = "ничего небыло найдено";
		Encode::from_to($notfnd, "koi8-r", "windows-1251");
		return if $response =~ /$notfnd/;

		# extract possible matches
		#    possible matches are grouped in several catagories:  
		#        exact, partial, and approximate
		my $exact_matches = $response;
		# parse movie list from matches
		my $beg = "<a class=l2 href=\"index.asp?comm=4&num=";
		my $end = "</a>";
		my $begy = "colspan=2 align=center>";
		my $endy = "</td>";

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
				my ($movienum, $moviename) = split("\">", $sub);
				$title = removeTag($moviename);
				$moviename = removeTag($moviename);

				$title =~ s/\s{2,}//;
				Encode::from_to($title, "windows-1251", $outcp);

				# advance data to next field
				$data = substr($data, - (length($data) - $finish));

				$start = index($data, $begy);
				$finish = index($data, $endy, $start);
				$start += length($begy);
				$sub = substr($data, $start, $finish - $start);
				my $movieyear = removeTag($sub);

				if ($movieyear){$title = $title." (".$movieyear.")"; }
				$moviename=$title ;

				# advance data to next movie
				$data = substr($data, - (length($data) - $finish));
				$start = index($data, $beg);
				$finish = index($data, $end, $start);

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

#$opt_d = 1;

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
# vim: ts=4 sw=4:
