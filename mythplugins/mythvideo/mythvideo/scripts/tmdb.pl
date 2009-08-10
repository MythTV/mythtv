#!/usr/bin/perl -w

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

#TODO: use TMDB IDs and fallback to assuming imdb ID rather than the
# current way.

use strict;

use File::Basename;
use URI::Escape;
use lib dirname($0);

use MythTV::MythVideoCommon;

eval "use DateTime::Format::Strptime"; my $has_date_format = $@ ? 0 : 1;

use vars qw($opt_h $opt_r $opt_d $opt_i $opt_v $opt_D $opt_M $opt_P $opt_B);
use Getopt::Std;
use Data::Dumper;
use LWP::Simple;
#use XML::Simple qw(:strict);
use XML::Simple;

my $title = "themoviedb Query";
my $version = "v0.1.1";
my $author = "William Stewart, Stuart Morgan";
push(@MythTV::MythVideoCommon::URL_get_extras, ($title, $version));

my @countries = qw(USA UK Canada Japan);

my $base_url       = "http://api.themoviedb.org/2.0";
#my $posterimdb_url = "http://api.themoviedb.org/cover.php?imdb=";

# themoviedb.org api key given by Travis Bell for Mythtv
my $API_KEY        = "c27cb71cff5bd76e1a7a009380562c62";

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

sub TMDBAPIRequest($$) {
    sub encode_args {
        my ($paramshashref) = @_;
        my @ret;
        while (my ($key, $value) = each(%{$paramshashref})) {
            push(@ret, uri_escape($key) . "=" . uri_escape($value));
        }

        return join('&', @ret);
    }

    my ($relurl, $queryargs_hashref) = @_;

    if (!defined $queryargs_hashref) {
        $queryargs_hashref = {}
    }

    my $request = join('/', $base_url, $relurl);

    $queryargs_hashref->{'api_key'} = $API_KEY;
    $request = $request . "?" . encode_args($queryargs_hashref);

    if (defined $opt_d) { printf("# request: '%s'\n", $request); }

    my ($rc, $response) = myth_url_get($request);

    if (defined $opt_r) { printf(STDERR "%s", $response); }

    return ($rc, $response);
}

# get Movie Data
sub getMovieData {
    my ($movieid) = @_; # grab movieid parameter
    if (defined $opt_d) { printf("# looking for movie id: '%s'\n", $movieid); }

    # get the search results page via Movie.imdbLookup
    my ($rc, $response) =
        TMDBAPIRequest('Movie.imdbLookup', {'imdb_id' => "tt$movieid"});

    if (!defined $response) {
        die "Unable to contact themoviedb.org while retrieving ".
            "movie data, stopped";
    }

    my $xs = new XML::Simple(SuppressEmpty => '', ForceArray => ['movie'],
        KeyAttr => []);
    my $xml = $xs->XMLin($response);

    if ($xml->{"opensearch:totalResults"} > 0) {
        # now get the movie data via Movie.getInfo, Movie.imdbLookup does not
        # provide us all the data
        my $tmdbid = $xml->{moviematches}->{movie}->[0]->{id};
        my ($rc, $response) =
            TMDBAPIRequest('Movie.getInfo', {'id' => $tmdbid});

        if (!defined $response) {
            die "Unable to contact themoviedb.org while retrieving ".
                "movie data, stopped";
        }

        $xml = $xs->XMLin($response,
            ForceArray => ['category', 'production_countries', 'person'],
            KeyAttr => ['key', 'id']);

        my $movie = $xml->{moviematches}->{movie};
        my $title       = $movie->{title};
        my $releasedate = $movie->{release};
        my $year        = substr($releasedate, 0, 4);
        my $plot        = $movie->{short_overview};
        my $userrating  = $movie->{rating};
        my $runtime     = $movie->{runtime};
        my $budget      = $movie->{budget};
        my $revenue     = $movie->{revenue};
        my $trailer     = $movie->{trailer}->{content};
        my $homepage    = $movie->{homepage};

        # Country
        my @countrylist =
            @{$xml->{moviematches}->{movie}->{production_countries}};
        my $country;
        if (@countrylist > 0 && ref $countrylist[0] eq 'HASH' &&
                exists $countrylist[0]->{name}) {
            $country = $countrylist[0]->{name};
        }

        # Genre
        my $genres;
        my @lgenres;
        if (exists $xml->{moviematches}->{movie}->{categories}) {
            my @catlist;
            @catlist = @{$xml->{moviematches}->{movie}->{categories}->{category}}
                if ref $xml->{moviematches}->{movie}->{categories};
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

        # output fields (these field names must match what MythVideo is looking
        # for)
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
    my ($movieid) = @_; # grab movieid parameter
    if (defined $opt_d) {
        printf("# looking for poster for movie id: '%s'\n", $movieid);
    }

    # get the search results  page
    my ($rc, $response) =
        TMDBAPIRequest('Movie.imdbLookup', {'imdb_id' => "tt$movieid"});

    if (!defined $response) {
        die "Unable to contact themoviedb.org while retrieving ".
            "movie poster, stopped";
    }

    my $xml = XMLin($response, ForceArray => ['movie', 'poster', 'backdrop'],
        KeyAttr => {poster => 'size'});

    if ($xml->{"opensearch:totalResults"} > 0) {
        if (exists $xml->{moviematches}->{movie}->[0]->{poster}->{mid})
        {
            print($xml->{moviematches}->{movie}->[0]->{poster}->
                {mid}->{content} . "\n");
        }
    }
}

# dump Movie Backdrop
sub getMovieBackdrop {
    my ($movieid) = @_; # grab movieid parameter
    if (defined $opt_d) {
        printf("# looking for backdrop for movie id: '%s'\n", $movieid);
    }

    # get the search results page via Movie.imdbLookup
    my ($rc, $response) =
        TMDBAPIRequest('Movie.imdbLookup', {'imdb_id' => "tt$movieid"});

    if (!defined $response) {
        die "Unable to contact themoviedb.org while retrieving ".
            "movie backdrop, stopped";
    }

    my $xs = new XML::Simple(SuppressEmpty => '', ForceArray => ['movie'],
        KeyAttr => []);
    my $xml = $xs->XMLin($response);

    if ($xml->{"opensearch:totalResults"} > 0) {
        # now get the movie data via Movie.getInfo, Movie.imdbLookup does not
        # provide us all the data
        my $tmdbid = $xml->{moviematches}{movie}[0]{id};

        my ($rc, $response) =
            TMDBAPIRequest('Movie.getInfo', {'id' => $tmdbid});

        if (!defined $response) {
            die "Unable to contact themoviedb.org while retrieving ".
                "movie backdrop, stopped";
        }

        $xml = XMLin($response, ForceArray=> ['backdrop'], KeyAttr => ['key', 'id']);

        foreach my $backdrop (@{$xml->{moviematches}->{movie}->{backdrop}}) {
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

    # Convert filename into a query string
    # (use same rules that Metadata::guesTitle does)
    my $query = cleanTitleQuery($filename);

    if (defined $opt_d) {
        printf("# query: '%s'\n", $query);
    }

    # get the search results  page
    my ($rc, $response) =
        TMDBAPIRequest('Movie.search', {'title' => $query});

    if (!defined $response) {
        die "Unable to contact themoviedb.org while retrieving ".
            "movie list, stopped";
    }

    my $xs = new XML::Simple(SuppressEmpty => '', ForceArray => ['movie'],
        KeyAttr => []);
    my $xml = $xs->XMLin($response);

    if ($xml->{"opensearch:totalResults"} > 0) {
        my @movies;

        foreach my $movie (@{$xml->{moviematches}->{movie}}) {
            if ($movie->{type} ne 'movie') {
                next;
            }

            my $movienum  = $movie->{imdb};
            my $moviename = $movie->{title};
            my $release   = $movie->{release};
            my $movieyear = 0;

            if ($release) {
                $movieyear = substr($release, 0, 4);
            }

            if ($movienum) {
                $movienum =~ s/^tt//;
                my $entry = $movienum . ":" . $moviename;
                if ($release) {
                    $entry = $entry . " (". $movieyear . ")";
                }
                push(@movies, $entry);
            }
        }

        for my $movie (@movies) { print "$movie\n"; }
    }
}

# Main Program

# parse command line arguments
getopts('ohrdivDMPB');

# print out info
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }

# print out usage if needed
if (defined $opt_h || $#ARGV < 0) {
    help();
}

if (defined $opt_D) {
    # take movieid from cmdline arg
    my $movieid = shift || die "Usage : $0 -D <movieid>\n";
    getMovieData($movieid);
} elsif (defined $opt_P) {
    # take movieid from cmdline arg
    my $movieid = shift || die "Usage : $0 -P <movieid>\n";
    getMoviePoster($movieid);
} elsif (defined $opt_B) {
    # take movieid from cmdline arg
    my $movieid = shift || die "Usage : $0 -B <movieid>\n";
    getMovieBackdrop($movieid);
} elsif (defined $opt_M) {
    # take query from cmdline arg
    # ignore any options, such as those used by imdb.pl
    my $options = shift || die "Usage : $0 -M <query>\n";
    my $query = shift;
    if (!$query) {
       $query = $options;
       $options = "";
    }
    getMovieList($query);
}

# vim: set expandtab ts=4 sw=4 :
