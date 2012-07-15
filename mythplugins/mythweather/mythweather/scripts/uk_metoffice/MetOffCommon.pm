#! /usr/bin/perl
# vim:ts=4:sw=4:ai:et:si:sts=4

package MetOffCommon;
use strict;
use warnings;
require Exporter;

use utf8;
use encoding 'utf8';
use LWP::UserAgent;
use LWP::Simple;
use XML::Simple;
#use Data::Dumper;


our @EXPORT = qw(Search FindLoc);
our @EXPORT_OK = qw($apikey $copyrightstr $copyrightlogo $forecast_url);
our $VERSION = 0.1;


# MythTV API key, DO NOT reuse in another application
# Free keys can be requested from
# https://register.metoffice.gov.uk/register/ddc
our $api_key = '40af3680-8fd5-4c68-a762-4a6fe107f4e2';
our $copyright_str = '© Crown copyright 2012, the Met Office';
our $copyright_logo = 'http://www.metoffice.gov.uk/lib/template/logos/MO_Landscape_W.jpg';
our $forecast_url = 'http://partner.metoffice.gov.uk/public/val/wxfcs/all/xml/';

my @searchresults;
my @resulturl;
my $resultcount = -1;

sub Search {
    my ($search_string, $dir, $timeout, $logdir) = @_;
    $search_string = uri_escape($search_string);

    my %locations = GetLocationsList();

    my @searchresults = ();
    foreach my $key (keys %locations)
    {
        if ($locations{$key} =~ /.*$search_string.*/i) {
            my $resultline = $key . "::" . $locations{$key};
            push (@searchresults, $resultline);
        }
    }
    return @searchresults;
}

sub GetLocationsList {

    my $locations_url = $forecast_url . 'sitelist/?key=' . $api_key;
    
    my $response = get $locations_url;
    die "404 retrieving " . $locations_url unless defined $response;

    my $xml = XMLin($response, KeyAttr => {});

    if (!$xml) {
        die "Response not xml";
    }

    my $i = 0;
    my %locations;

    #print Dumper($xml);

    foreach my $loc (@{$xml->{Location}}) {
        if (defined $loc->{id}) {
            $locations{$loc->{id}} = $loc->{name};
        }
        $i++;
    }

    return %locations;
#     open OF, "<:utf8", $cachefile or die "Can't read $cachefile: $!\n";
#     my $content = do { local $/; <OF> };
#     close OF;

}

sub log_print {
    return if not defined $::opt_D;
    my $dir = shift;

    open OF, ">>$dir/uk_metoff.log";
    print OF @_;
    close OF;
}


1;
