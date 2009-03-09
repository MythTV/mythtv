#!/usr/bin/perl -w

#
# This perl script is intended to perform movie data lookups based on
# the themoviedb.org website
#
# For more information on MythVideo's external movie lookup mechanism, see
# the README file in this directory.
#
# Author: William Stewart, Stuart Morgan
#
# v0.1
# - Initial script

use File::Basename;
use lib dirname($0);

use MythTV::MythVideoCommon;

eval "use DateTime::Format::Strptime"; my $has_date_format = $@ ? 0 : 1;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P $opt_B);
use Getopt::Std;
use Data::Dumper;
use LWP::Simple;
use XML::Simple;

$title = "themoviedb Query";
$version = "v0.1.0";
$author = "William Stewart, Stuart Morgan";
push(@MythTV::MythVideoCommon::URL_get_extras, ($title, $version));

my @countries = qw(USA UK Canada Japan);

my $base_url       = "http://api.themoviedb.org/2.0/";
my $posterimdb_url = "http://api.themoviedb.org/cover.php?imdb=";

# themoviedb.org api key given by Travis Bell for Mythtv
my $api_key        = "c27cb71cff5bd76e1a7a009380562c62";

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
print "       -P <movieid>  get movie poster\n";
print "       -B <movieid>  get movie backdrop\n";
print "       -D <movieid>  get movie data\n";
exit(-1);
}

# display 1-line of info that describes the version of the program
sub version {
print "$title ($version) by $author\n"
}

# display 1-line of info that can describe the type of query used
sub info {
print "Performs queries using the themoviedb.org website.\n";
}

# display detailed help
sub help {
version();
info();
usage();
}

# get Movie Data
sub getMovieData {
my ($movieid)=@_; # grab movieid parameter
if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid);}

# get the search results page via Movie.imdbLookup
my $request = $base_url . "Movie.imdbLookup?imdb_id=tt" . $movieid . "&api_key=" . $api_key;
if (defined $opt_d) { printf("# request: '%s'\n", $request); }
my ($rc, $response) = myth_url_get($request);
if (defined $opt_r) { printf("%s", $response); }

my $xs = new XML::Simple(suppressempty => '');
my $xml = $xs->XMLin($response);

if ($xml->{"opensearch:totalResults"} > 0) {
        #
        # now get the movie data via Movie.getInfo, Movie.imdbLookup does not provide us all the data
        #
        my $tmdbid = $xml->{moviematches}->{movie}->{id};
        $request = $base_url . "Movie.getInfo?id=" . $tmdbid . "&api_key=" . $api_key;
        if (defined $opt_d) { printf("# request: '%s'\n", $request); }
        my ($rc, $response) = myth_url_get($request);
        if (defined $opt_r) { printf("%s", $response); }

        $xml = $xs->XMLin($response, KeyAttr => ['key', 'id'], forcearray => ['category','production_countries']);

        my $title       = $xml->{moviematches}->{movie}->{title};
        my $releasedate = $xml->{moviematches}->{movie}->{release};
        my $year        = substr($releasedate, 0, 4);
        my $plot        = $xml->{moviematches}->{movie}->{short_overview};
        my $userrating  = $xml->{moviematches}->{movie}->{rating};
        my $runtime     = $xml->{moviematches}->{movie}->{runtime};
        my $budget      = $xml->{moviematches}->{movie}->{budget};
        my $revenue     = $xml->{moviematches}->{movie}->{revenue};
        my $trailer     = $xml->{moviematches}->{movie}->{trailer}->{content};
        my $homepage    = $xml->{moviematches}->{movie}->{homepage};

        # Country
        my @countrylist = @{$xml->{moviematches}->{movie}->{production_countries}};
        my $country;
        if (@countrylist > 0) {
                $country = $countrylist[0]->{name};
        }

        # Genre
        my $genres;
        my @lgenres;
        if (exists $xml->{moviematches}->{movie}->{categories}) {
            my @catlist = @{$xml->{moviematches}->{movie}->{categories}->{category}};
            if (@catlist > 0) {
                my $j = 0;
                for (my $i = 0; $i < @catlist; $i++)
                {
                    $lgenres[$j++] = $catlist[$i]->{name};
                }
            }
        }
        $genres = join(',', @lgenres);

        # People
        my @lcast;
        my @ldirector;
        my @lwriter;
        my $dc = 0;
        my $cc = 0;
        my $wc = 0;
        if (exists $xml->{moviematches}->{movie}->{people}) {
            my @castlist = @{$xml->{moviematches}->{movie}->{people}->{person}};
            for (my $i = 0; $i < @castlist; $i++)
            {
                if ($castlist[$i]->{job} eq "actor")
                {
                    $lcast[$cc++] = $castlist[$i]->{name};
                }

                if ($castlist[$i]->{job} eq "director")
                {
                    $ldirector[$dc++] = $castlist[$i]->{name};
                }

                if ($castlist[$i]->{job} eq "writer")
                {
                    $lwriter[$wc++] = $castlist[$i]->{name};
                }

            }
        }
        my $cast     = join(',', @lcast);
        my $director = join(',', @ldirector);
        my $writer = join(',', @lwriter);

        # output fields (these field names must match what MythVideo is looking for)
        print "Title:$title\n";
        if ($year) {print "Year:$year\n";}
        if ($releasedate) {print "ReleaseDate:$releasedate\n";}
        print "Director:$director\n";
        if ($plot) {print "Plot:$plot\n";}
        if ($userrating) {print "UserRating:$userrating\n";}
#   print "MovieRating:$movierating\n";
        if ($runtime) {print "Runtime:$runtime\n";}
        print "Writers: $writer\n";
        print "Cast:$cast\n";
        if ($genres) {print "Genres: $genres\n";}
        if ($country) {print "Countries: $country\n";}
        if ($budget)  {print "Budget: $budget\n";}
        if ($revenue) {print "Revenue: $revenue\n";}
        if ($trailer) {print "trailer: $trailer\n";}
        if ($homepage) {print "Homepage: $homepage\n";}
    }
}

# dump Movie Poster
sub getMoviePoster {
    my ($movieid)=@_; # grab movieid parameter
    if (defined $opt_d) { printf("# looking for poster for movie id: '%s'\n", $movieid);}

    # get the search results  page
    my $request = $base_url . "Movie.imdbLookup?imdb_id=tt" . $movieid . "&api_key=" . $api_key;
    if (defined $opt_d) { printf("# request: '%s'\n", $request); }
    my ($rc, $response) = myth_url_get($request);
    if (defined $opt_r) { printf("%s", $response); }

    my $xml = XMLin($response, forcearray => [ 'poster', 'backdrop' ]);

    if ($xml->{"opensearch:totalResults"} > 0) {
        my $poster = $xml->{moviematches}->{movie}->{poster}->[0]->{content};

        if ($poster) {
        # check for a mid sized poster
        foreach my $p (@{$xml->{moviematches}->{movie}->{poster}})
        {
           if ($p->{size} eq "mid")
           {
               $poster = $p->{content};
               last;
           }
        }

        if ($poster) {
            print "$poster\n";
        }
        }
#        else {
#            my $backdrop = $xml->{moviematches}->{movie}->{backdrop}->[0]->{content};
#
#            if ($backdrop)
#            {
#                print "$backdrop\n";
#            }
#        }
    }
}

# dump Movie Backdrop
sub getMovieBackdrop {
    my ($movieid)=@_; # grab movieid parameter
    if (defined $opt_d) { printf("# looking for backdrop for movie id: '%s'\n", $movieid);}

    # get the search results page via Movie.imdbLookup
    my $request = $base_url . "Movie.imdbLookup?imdb_id=tt" . $movieid . "&api_key=" . $api_key;
    if (defined $opt_d) { printf("# request: '%s'\n", $request); }
    my ($rc, $response) = myth_url_get($request);
    if (defined $opt_r) { printf("%s", $response); }

    my $xs = new XML::Simple(suppressempty => '');
    my $xml = $xs->XMLin($response);

    if ($xml->{"opensearch:totalResults"} > 0) {
        #
        # now get the movie data via Movie.getInfo, Movie.imdbLookup does not provide us all the data
        #
        my $tmdbid = $xml->{moviematches}->{movie}->{id};
        $request = $base_url . "Movie.getInfo?id=" . $tmdbid . "&api_key=" . $api_key;
        if (defined $opt_d) { printf("# request: '%s'\n", $request); }
        my ($rc, $response) = myth_url_get($request);
        if (defined $opt_r) { printf("%s", $response); }

        $xml = XMLin($response, KeyAttr => ['key', 'id']);

        foreach my $backdrop (@{$xml->{moviematches}->{movie}->{backdrop}})
        {
         # print "$backdrop->{content}\n";

            if ($backdrop->{size} eq "original")
            {
                print "$backdrop->{content}\n";
            }
        }
    }
}

# dump Movie list:  1 entry per line, each line as 'movieid:Movie Title'
sub getMovieList {
my ($filename)=@_; # grab parameters

#
# Convert filename into a query string
# (use same rules that Metadata::guesTitle does)
my $query = cleanTitleQuery($filename);

if (defined $opt_d) {
    printf("# query: '%s'\n", $query);
}

# get the search results  page
my $request = $base_url . "Movie.search?title=" . $query . "&api_key=" . $api_key;
if (defined $opt_d) { printf("# request: '%s'\n", $request); }
my ($rc, $response) = myth_url_get($request);
if (defined $opt_r) {
    print $response;
    exit(0);
}

my $xs = new XML::Simple(suppressempty => '');
my $xml = $xs->XMLin($response, forcearray => [ 'movie' ]);

if ($xml->{"opensearch:totalResults"} > 0) {
    my @movies;
    my $j = 0;

    foreach my $key (keys(%{$xml->{moviematches}->{movie}}))
    {
        my $type = $xml->{moviematches}->{movie}->{$key}->{type};
        if ($type ne "movie")
        {
            next;
        }

        my $movienum  = $xml->{moviematches}->{movie}->{$key}->{imdb};
        my $moviename = $xml->{moviematches}->{movie}->{$key}->{title};
        my $release   = $xml->{moviematches}->{movie}->{$key}->{release};
        my $movieyear = 0;

        if ($release) {
            $movieyear = substr($release, 0, 4);
        }

        if ($movienum) {
            if ($release) {
                $movies[$j++]= substr($movienum,2) . ":" . $moviename . " (". $movieyear . ")";
            }
            else {
                $movies[$j++]= substr($movienum,2) . ":" . $moviename;
            }
        }
    }
    # display array of values

    for $movie (@movies) { print "$movie\n"; }
    }
}

#
# Main Program
#

# parse command line arguments
getopts('ohrdivDMPB');

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

elsif (defined $opt_B) {
# take movieid from cmdline arg
$movieid = shift || die "Usage : $0 -P <movieid>\n";
getMovieBackdrop($movieid);
}

elsif (defined $opt_M) {
# take query from cmdline arg
$query = shift || die "Usage : $0 -M <query>\n";
getMovieList($query);
}
# vim: set expandtab ts=3 sw=3 :
