#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

package BBCLocation;
use strict;
use warnings;
require Exporter;

use utf8;
use encoding 'utf8';
use LWP::UserAgent;
use JSON;
use XML::XPath;
use XML::XPath::XMLParser;
use URI::Escape;


our @EXPORT = qw(Search FindLoc);
our $VERSION = 0.3;

my @searchresults;
my @resulturl;
my $resultcount = -1;

sub Search {
    my ($search_string, $dir, $timeout, $logdir) = @_;
    $search_string = uri_escape($search_string);

    my $base_url = 'http://www.bbc.co.uk/locator/client/weather/en-GB/' .
                   'search.json';
    my $search_url = $base_url . '?ptrt=/&search=';


    my $file = $search_string;
    getCachedJSON($search_url . $search_string, $dir, $file, $timeout, $logdir);

    my $cachefile  = "$dir/$file.json";
    my $cachefile1 = "$dir/$file-results.html";
    my $cachefile2 = "$dir/$file-pagination.html";

    open OF, "<:utf8", $cachefile or die "Can't read $cachefile: $!\n";
    my $content = do { local $/; <OF> };
    close OF;

    my $decoded = decode_json $content;
    $resultcount = $decoded->{"noOfResults"};

    my %loc_hash = ();

    get_results($cachefile1, \%loc_hash);

    if (exists $decoded->{"pagination"}) {
        my %pages = ();
        my $xp = XML::XPath->new(filename => $cachefile2);
        my $nodeset = $xp->find("//ol/li/a");
        foreach my $node ($nodeset->get_nodelist) {
            my $url = $node->getAttribute("href");
            my $num = $node->string_value;
            $url =~ s/&amp;/&/;
            $num =~ s/ //g;
            $pages{$num} = $url;
        }

        foreach my $page (keys %pages)
        {
            getCachedJSON($base_url . $pages{$page}, $dir, $file . "-$page",
                          $timeout, $logdir);

            my $cachefile3 = "$dir/$file-$page-results.html";
            get_results($cachefile3, \%loc_hash);
        }
    }

    my @searchresults = ();
    foreach my $key (keys %loc_hash)
    {
    	my $resultline = $key."::".$loc_hash{$key};
        push (@searchresults, $resultline);
    }
    return @searchresults;
}

sub getCachedJSON {
    my ($url, $dir, $file, $timeout, $logdir) = @_;

    my $cachefile  = "$dir/$file.json";
    my $cachefile1 = "$dir/$file-results.html";
    my $cachefile2 = "$dir/$file-pagination.html";

    my $now = time();
    my $decoded;

    log_print( $logdir, "Loading URL: $url\n" );

    if( (-e $cachefile) and ((stat($cachefile))[9] >= ($now - $timeout)) ) {
        # File cache is still recent.
        log_print( $logdir, "cached in $cachefile\n" );
    } else {
        log_print( $logdir, "$url\ncaching to $cachefile\n" );

        my $ua = LWP::UserAgent->new;
        $ua->timeout(30);
        $ua->env_proxy;
        $ua->default_header('Accept-Language' => "en");

        my $response = $ua->get($url);
        if ( !$response->is_success ) {
            die $response->status_line;
        }

        open OF, ">:utf8", $cachefile or die "Can't open $cachefile: $!\n";
        print OF $response->content;
        close OF;

        $decoded = decode_json $response->content;

        open OF, ">:utf8", $cachefile1 or die "Can't open $cachefile1: $!\n";
        print OF "<html>".$decoded->{"results"}."</html>";
        close OF;

        if (exists $decoded->{"pagination"}) {
            open OF, ">:utf8", $cachefile2 or
                die "Can't open $cachefile2: $!\n";
            print OF "<html>".$decoded->{"pagination"}."</html>";
            close OF;
        } else {
            unlink $cachefile2;
        }
    }
}

sub get_results {
    my ($file, $outhash) = @_;

    my $xp = XML::XPath->new(filename => $file);
    my $nodeset = $xp->find("//ul/li/a");
    foreach my $node ($nodeset->get_nodelist) {
        my $url = $node->getAttribute("href");
        my $loc = $node->string_value;

        $url =~ s/^\/weather\///;
        $outhash->{$url} = $loc;
    }
}

sub log_print {
    return if not defined $::opt_D;
    my $dir = shift;

    open OF, ">>$dir/uk_bbc.log";
    print OF @_;
    close OF;
}

sub FindLoc {
    my ($locid, $dir, $timeout, $logdir) = @_;

    my $url = "http://www.bbc.co.uk/weather/$locid";

    my $file = "$locid.html";
    getCachedHTML($url, $dir, $file, $timeout, $logdir);

    my $cachefile  = "$dir/$file";

    open OF, "<:utf8", $cachefile;
    my $contents = do { local $/; <OF>; };
    close OF;

    my ($rssid) = ($contents =~ /data-loc="(.*?)"/);
    die "No RSS Location found for ID $locid!\n" unless defined $rssid;

    $rssid =~ s/^LOC-//;
    return $rssid;
}


sub getCachedHTML {
    my ($url, $dir, $file, $timeout, $logdir) = @_;

    my $cachefile  = "$dir/$file";

    my $now = time();

    log_print( $logdir, "Loading URL: $url\n" );

    if( (-e $cachefile) and ((stat($cachefile))[9] >= ($now - $timeout)) ) {
        # File cache is still recent.
        log_print( $logdir, "cached in $cachefile\n" );
    } else {
        log_print( $logdir, "$url\ncaching to $cachefile\n" );

        my $ua = LWP::UserAgent->new;
        $ua->timeout(30);
        $ua->env_proxy;
        $ua->default_header('Accept-Language' => "en");

        my $response = $ua->get($url);
        if ( !$response->is_success ) {
            die $response->status_line;
        }

        my $content = $response->content;
        open OF, ">:utf8", $cachefile or die "Can't open $cachefile: $!\n";
        print OF $content;
        close OF;
    }
}


1;
