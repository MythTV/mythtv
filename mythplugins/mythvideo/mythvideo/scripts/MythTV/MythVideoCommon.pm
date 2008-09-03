package MythTV::MythVideoCommon;

use strict;
use warnings;

use URI::Escape qw(uri_escape uri_unescape);
use HTML::Entities;
require LWP::UserAgent;

BEGIN {
    use Exporter ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK);
    $VERSION = "0.1.0";
    @ISA = qw(Exporter);
    @EXPORT = qw(&myth_url_get &parseBetween &cleanTitleQuery &trim &getPosterURLsFromIMDB);
    @EXPORT_OK = qw(@URL_get_extras);
}

our @URL_get_extras;

@URL_get_extras = ();

my $mythtv_version = "0.21";

sub myth_url_get($) {
    my ($url) = @_;
    my @extras = (@URL_get_extras);
    my $agent_id = "MythTV/$mythtv_version (" . join('; ', @extras) . ")";
    my $agent = LWP::UserAgent->new(agent => $agent_id);
    my $rc = $agent->get($url);

    if ($rc->is_success) {
        return ($rc, $rc->content);
    }
    return ($rc, undef);
}

# returns text within 'data' between 'beg' and 'end' matching strings
sub parseBetween($$$) {
    my ($data, $beg, $end) = @_; # grab parameters

    my $ldata = lc($data);
    my $start = index($ldata, lc($beg)) + length($beg);
    my $finish = index($ldata, lc($end), $start);
    if ($start != (length($beg) -1) && $finish != -1) {
        my $result = substr($data, $start, $finish - $start);
        # return w/ decoded numeric character references
        # (see http://www.w3.org/TR/html4/charset.html#h-5.3.1)
        _decode_entities($result, { nbsp => '' });
        decode_entities($result);
        return $result;
    }
    return "";
}

sub cleanTitleQuery($;$) {
    my ($ret, $enc_func) = @_;
    $ret = uri_unescape($ret);  # in case it was escaped

    # Strip off the file extension
    if (rindex($ret, '.') != -1) {
        $ret = substr($ret, 0, rindex($ret, '.'));
    }

    # Strip off anything following '(' - people use this for general comments
    if (rindex($ret, '(') != -1) {
        $ret = substr($ret, 0, rindex($ret, '('));
    }

    # Strip off anything following '[' - people use this for general comments
    if (rindex($ret, '[') != -1) {
        $ret = substr($ret, 0, rindex($ret, '['));
    }

    # Strip off anything following '-' - people use this for general comments
    #if (index($ret, '-') != -1) {
    #    $ret = substr($ret, 0, index($ret, '-'));
    #}

    # Some searches do better if any trailing ,The is left off
    $ret =~ s/,\s+The\s+$//i;

    if ($enc_func) {
        $ret = $enc_func->($ret);
    }

    return uri_escape($ret);
}

sub trim($) {
   my ($str) = @_;
   $str =~ s/^\s+//;
   $str =~ s/\s+$//;
   return $str;
}

sub getPosterURLsFromIMDB($$$) {
    my ($movie_id, $debug, $debug_r) = @_;
    my @ret = ();
    if (defined $debug) { printf("# looking for movie id: '%s'\n", $movie_id);}

    # get the search results  page
    my $request = "http://www.imdb.com/title/tt" . $movie_id . "/posters";
    if (defined $debug) { printf("# request: '%s'\n", $request); }
    my ($rc, $response) = myth_url_get($request);
    if (defined $debug_r) { printf("%s", $response); }

    if (!defined $response) {return;}

    # look for references to impawards.com posters - they are high quality
    my $site = "http://www.impawards.com";
    my $impsite = parseBetween($response, "<a href=\"".$site, "\">".$site);

    # jersey girl fix
    if ($impsite eq "") {
        $impsite = parseBetween($response, "<a href=\"http://impawards.com","\">http://impawards.com");
    }

    if ($impsite) {
        $impsite = $site . $impsite;

        if (defined $debug) { print "# Searching for poster at: ".$impsite."\n"; }
        my ($rc, $impres) = myth_url_get($impsite);
        if (defined $debug) { printf("# got %i bytes\n", length($impres)); }
        if (defined $debug_r) { printf("%s", $impres); }

        # making sure it isnt redirect
        my $uri = parseBetween($impres, "0;URL=..", "\">");
        if ($uri ne "") {
            if (defined $debug) { printf("# processing redirect to %s\n",$uri); }
            # this was redirect
            $impsite = $site . $uri;
            ($rc, $impres) = myth_url_get($impsite);
        }

        # do stuff normally
        $uri = parseBetween($impres, "<img SRC=\"posters/", "\" ALT");
        # uri here is relative... patch it up to make a valid uri
        if (!($uri =~ /http:(.*)/ )) {
            my $path = substr($impsite, 0, rindex($impsite, '/') + 1);
            $uri = $path."posters/".$uri;
        }

        if ($uri =~ /\.(jpe?g|gif|png)$/)
        {
            if (defined $debug) { print "# found ipmawards poster: $uri\n"; }
            push(@ret, $uri);
        }
    }

    # try looking on nexbase
    if ($response =~ m/<a href="([^"]*)">([^"]*?)nexbase/i) {
        if ($1 ne "") {
            if (defined $debug) { print "# found nexbase poster page: $1 \n"; }
            my ($rc, $cinres) = myth_url_get($1);
            if (defined $debug) { printf("# got %i bytes\n", length($cinres)); }
            if (defined $debug_r) { printf("%s", $cinres); }

            if ($cinres &&
                $cinres =~ m/<a id="photo_url" href="([^"]*?)" ><\/a>/i) {
                if (defined $debug) { print "# nexbase url retreived $1\n"; }
                push(@ret, $1);
            }
        }
    }

    # try looking on cinemablend
    if ($response =~ m/<a href="([^"]*)">([^"]*?)cinemablend/i) {
        if ($1 ne "") {
            if (defined $debug) { print "# found cinemablend poster page: $1 \n"; }
            my ($rc, $cinres) = myth_url_get($1);
            if (defined $debug) { printf("# got %i bytes\n", length($cinres)); }
            if (defined $debug_r) { printf("%s", $cinres); }

            if ($cinres =~ m/<div><img [^>]*?src="(\/images\/[^"]*?)"\s*><\/div>/i) {
                if (defined $debug) { print "# cinemablend url retreived\n"; }
                push(@ret, "http://www.cinemablend.com/$1");
            }
        }
    }

    # grab default IMDB poster URL
    if (defined $debug) { print "# looking for imdb posters\n"; }
    my $host = "http://posters.imdb.com/posters/";

    my $uri = parseBetween($response, $host, "\"><td><td><a href=\"");
    if ($uri ne "") {
        $uri = $host.$uri;
    } else {
        if (defined $debug) { print "# no poster found\n"; }
    }

    # no poster found, take lowres image from imdb
    if ($uri eq "") {
        my @movie_titles;
        my $found_low_res = 0;
        my $k = 0;

        if (defined $debug) { print "# looking for lowres imdb posters\n"; }
        my $host = "http://www.imdb.com/title/tt" . $movie_id . "/";
        ($rc, $response) = myth_url_get($host);

        # Better handling for low resolution posters
        if ($response =~ m/<a name="poster".*<img.*src="([^"]*).*<\/a>/ig) {
            if (defined $debug) { print "# found low res poster at: $1\n"; }
            $uri = $1;
            $found_low_res = 1;
        } else {
            if (defined $debug) { print "# no low res poster found\n"; }
            $uri = "";
        }

        if (defined $debug) { print "# starting to look for movie title\n"; }

        # get main title
        if (defined $debug) { print "# Getting possible movie titles:\n"; }
        $movie_titles[$k++] = parseBetween($response, "<title>", "<\/title>");
        if (defined $debug) { print "# Title: ".$movie_titles[$k-1]."\n"; }

        # now we get all other possible movie titles and store them in the titles array
        while($response =~ m/>([^>^\(]*)([ ]{0,1}\([^\)]*\)[^\(^\)]*[ ]{0,1}){0,1}\(informal title\)/g) {
            $movie_titles[$k++] = trim($1);
            if (defined $debug) { print "# Title: ".$movie_titles[$k-1]."\n"; }
        }
    }

    if ($uri ne "") {
        push(@ret, $uri);
    }

    return @ret;
}

END {}

1;

# vim: set expandtab ts=4 sw=4 :
