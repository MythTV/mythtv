#!/usr/bin/perl -w

# Script to scrape weather.com 'map room' to get available maps, outputs them to
# file named maps in format of URL::Description.

use strict;
use WWW::Mechanize;
use Data::Dumper;

my @array = <<EOF =~ m/(\S.*\S)/g;
'garden' = 'Lawn and Garden';
'achesandpains' = 'Aches & Pains';
'aviation' = 'Aviation';
'boatbeach' = 'Boat & Beach';
'travel' = 'Business Travel';
'dopplerradarusnational' = 'Doppler Radar';
'driving' = 'Driving';
'fallfoliage' = 'Fall Foliage';
'golf' = 'Golf';
'nationalparks' = 'Outdoors';
'oceans' = 'Oceans';
'pets' = 'Pets';
'ski' = 'Ski';
'specialevents' = 'Special Events';
'sportingevents' = 'Sporting Events';
'vacationplanner' = 'Vacation Planner';
'spring' = 'Weddings - Spring';
'summer' = 'Weddings - Summer';
'fall' = 'Weddings - Fall';
'winter' = 'Weddings - Winter';
'holidays' = 'Holidays';
'achesandpains' = 'Aches & Pains';
'airquality' = 'Air Quality';
'allergies' = 'Allergies';
'coldandflu' = 'Cold & Flu';
'earthquakereports' = 'Earthquake Reports';
'home' = 'Home Planner';
'schoolday' = 'Schoolday';
'severeusnational' = 'Severe Weather Alerts';
'skinprotection' = 'Skin Protection';
'fitness' = 'Fitness';
'alaskaus' = 'Alaska';
'currentweatherusnational' = 'Current Weather';
'dopplerradarusnational' = 'Doppler Radar';
'tendayforecastusnational' = 'Extended Forecasts';
'hawaiius' = 'Hawaii';
'satelliteusnational' = 'Satellite - US';
'satelliteworld' = 'Satellite - World';
'severeusnational' = 'Severe Alerts - US';
'severeusregional' = 'Severe Alerts - Regional';
'forecastsusnational' = 'Short Term Forecast';
'weeklyplannerusnational' = 'Weekly Planner';
'currentweatherusregional' = 'US Regions - Current';
'forecastsusregional' = 'US Regions - Forecasts';
'centralus' = 'US Regions - Central';
'eastcentralus' = 'US Regions - East Central';
'midwestus' = 'US Regions - Midwest';
'northcentralus' = 'US Regions - North Central';
'northeastus' = 'US Regions - Northeast';
'northwestus' = 'US Regions - Northwest';
'southcentralus' = 'US Regions - South Central';
'southeastus' = 'US Regions - Southeast';
'southwestus' = 'US Regions - Southwest';
'westus' = 'US Regions - West ';
'westcentralus' = 'US Regions - West Central';
'africaandmiddleeast' = 'World - Africa & Mid East';
'asia' = 'World - Asia';
'australia' = 'World - Australia';
'centralamerica' = 'World - Central America';
'europe' = 'World - Europe';
'northamerica' = 'World - North America';
'pacific' = 'World - Pacific';
'polar' = 'World - Polar';
'southamerica' = 'World - South America';
EOF
my %hash;
foreach $_ (@array) {
    my ($key, $val) = split / = /;
    $key =~ s/\'//g;
    $val =~ s/\'//g;
    $hash{$key} = $val;
}

my @urls;
my $tmp;
my $base = "http://www.weather.com";
# This logic and above array are taken from the javascript on the map pages, this is necessary 
# since the WWW::Mechanize doesn't support javascript
foreach my $key (keys %hash) {
    if (grep m/$key/, ("currentweatherusnational", "dopplerradarusnational", "earthquakereports", "tendayforecastusnational", "satelliteusnational", "satelliteworld", "severeusregional", "forecastsusnational", "weeklyplannerusnational", "severeusnational", "currentweatherusregional", "forecastsusregional")) {
       $tmp = "/maps/maptype/$key/index_large.html"; 
    } elsif ( grep m/$key/, ("oceans", "hawaiius", "centralus", "eastcentralus", "midwestus", "northcentralus", "northeastus", "northwestus", "southcentralus", "southeastus", "southwestus", "westus", "westcentralus", "asia", "australia", "centralamerica", "europe", "northamerica", "pacific", "southamerica", "africaandmiddleeast", "alaskaus", "polar")) {
       $tmp = "/maps/geography/$key/index_large.html"; 
    } elsif ( $key =~ m/winter/) {
       $tmp = "/maps/activity/weddings/winter/index_large.html";
    } elsif ( $key =~ m/summer/) {
       $tmp = "/maps/activity/weddings/summer/index_large.html";
    } elsif ( $key =~ m/fall/) {
       $tmp = "/maps/activity/weddings/fall/index_large.html";
    } elsif ( $key =~ m/spring/) {
       $tmp = "/maps/activity/weddings/spring/index_large.html";
    } else {
        $tmp = "/maps/activity/$key/index_large.html";
    }

    push @urls, $base . $tmp;
}

my $mech = WWW::Mechanize->new();
my %urlmap;
my $file = shift;
open (MAPS, ">$file") or die "cannot open $file";
foreach my $url (@urls) {
    print "$url\n";
    $mech->get($url);
    my $menu = $mech->form_number(3)->{inputs}->[0]->{menu};
    map {$urlmap{$_->{name}} = $_->{value} if ($_->{value})} @$menu;
}
print MAPS "<maplist>\n";
foreach my $mapkey (sort keys %urlmap) {
    $mech->get($base . $urlmap{$mapkey});
    my $image = $mech->find_image( alt => $mapkey );
    warn "Missing url for $mapkey" && next if (!$image->{url});
    print MAPS "<entry>\n";
    # Escape things, 
    $mapkey =~ s/&/&amp;/g; 
    $mapkey =~ s/</&lt;/g; 
    $mapkey =~ s/>/&gt;/g; 
    print "$mapkey\n";
    print MAPS ' 'x4 . "<description>$mapkey</description>\n";
    print MAPS ' 'x4 . "<static_url>$image->{url}</static_url>\n";

    # I would really like to be able to search for the link to the animated 
    # page, but since Mechanize refuses to see it, I just have to test if it 
    # exists

    # animated images are stored in
    # images.weather.com/looper/archive/imagename(without
    # extension)/{1...}L.jpg    
    my $base = $image->{url};
    $base =~ s{(http://.*/)}{};
    $base =~ s/[.]jpg//;
    $base = "http://images.weather.com/looper/archive/$base/";
    if ($mech->get($base)->is_success) {
        print MAPS ' 'x4 . "<animated>\n";
        print MAPS ' 'x8 . "<base_url>$base</base_url>\n";
        my @links = $mech->find_all_links(url_regex=>qr/[0-9]+L[.]jpg/);
        print MAPS ' 'x8 . "<image>" . $_->text . "</image>\n" foreach (@links);
        print MAPS ' 'x4 . "</animated>\n";
        $mech->back();
    }

    $mech->back();
    print MAPS "</entry>\n";
}
print MAPS "</maplist>\n";
