#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/topdocumentaryfilm.pl,v 1.17 2010/07/24 23:28:11 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MythNetvision Grabber Script for topdocumentaryfilm site.
#
# If you want to alter any of the default settings.
# Create/Change $HOME/.mythtv/MythNetvision/userGrabberPrefs/topdocumentaryfilm.cfg
# Format of file
# player=mplayer
# playerargs=-fs -zoom %MEDIAURL%
#
# Some settings you can have in this are
# Print info/progress message: 0 - off, 1 - low ,2 - high
#	mnvinfo
# Info messages go to: 0 = stderr, filename = filename
#	mnvinfoop
# External player to use
#	player
# Args to external player %MEDIAURL% will be replaced with content url
#	playerargs
# External download to use
#	download
# Args to external download %MEDIAURL% will be replaced with content url
#	downloadargs
# A network player like a flash or html5 html. TODO 0.24 May not be approved
#	netplayer
# Type flash or html5
#	netplayertype
# Seconds to cache results Default 72000
#	cachetime
#
################################################################################
use strict;
use warnings;
use Getopt::Std;
use LWP::Simple;
use HTML::TreeBuilder;
use HTML::Entities;
use Data::Dumper;
use Date::Parse;
use Date::Format;
use Encode;
use Storable;
use File::stat;
use File::Basename;
use FindBin '$Bin', '$Script';
use lib "$Bin/nv_perl_libs";
use mnvcommonsubs;

#################################### Settings #################################
# Load from config file. May overwrite above.
mnvloadconfig(fileparse($Script, '.pl'), "notused");
# SKIP completely skips the video
my %autoplay = (
'youtube.com' => '&autoplay=1',
'220.ro' => '&aplay=true',
'megavideo.com' => 'SKIP',
'veoh.com' => 'videoAutoPlay=1',
'crunchyroll.com' => 'auto_play=true',
'mediaservices.myspace.com' => ',AutoPlay=true',
);

#################################### Globals ##################################
my $version = '$Revision: 1.17 $'; $version =~ s/\D*([\d\.]+)\D*/$1/; # rcs tag populated
my $command = "topdocumentaryfilm.pl"; my $commandthumbnail = "topdocumentaryfilm.png"; my $author = "Auric";
my $site = 'TopDocumentaryFilms';
my $description = 'Great collection of documentary movies';
my $baseurl = 'http://topdocumentaryfilms.com/';
my $baseicon = 'http://www.danaroc.com/ezine_pics_031510_websites.jpg';
my $store = "/tmp/.${site}.diritemsref.store";
our ($opt_v, $opt_T, $opt_p, $opt_S);
my %diritems;

#################################### Site Specific Subs ##########################
# Build all vid items for all directories
# input hash ref to { "directory name" => [array of anonymous hash's] }
# anonymous hash {
#		'dirthumbnail' => $icon,
#		'title' => $title,
#		'mythtv:subtitle' => "",
#		'author' => $author,
#		'pubDate' => $pubDate,
#		'description' => $description,
#		'link' => $url,
#		'player' => $player,
#		'playerargs' => $playerargs,
#		'download' => $download,
#		'downloadargs' => $downloadargs,
#		'media:thumbnailurl' => "",
#		'media:contenturl' => $contenturl,
#		'media:contentlength' => $length,
#		'media:contentduration' => "",
#		'media:contentwidth' => "",
#		'media:contentheight' => "",
#		'media:contentlanguage' => $language,
#		'rating' => ""
#		'mythtv:country' => ""
#		'mythtv:season' => ""
#		'mythtv:episode' => ""
#		'mythtv:customhtml' => ""
#	}
# Basically this hash ref is what you need to build.
# input base url
# output items found

sub builddiritems {
	my $diritemsref = shift @_;
	my $baseurl = shift @_;

	my $dirurlsref = builddirurls($baseurl);
	my $vidurlsref = buildvidurls($dirurlsref);
	my $itemsfound = 0;
	foreach my $dir (keys(%$vidurlsref)) {
		my $diritemsfound = 0;
		foreach my $urltitle (@{$vidurlsref->{$dir}}) {
			my($url, $title) = @{$urltitle};
			my $found = builditems($diritemsref, $dir, $url, $title);
			$itemsfound += $found;
			$diritemsfound += $found;
		}
		mnvinfomsg(1, "$dir Items found $diritemsfound");
	}
	return $itemsfound;
}

sub addautoplay {
	my $link = shift @_;

	$link = decode_entities($link);
	unless ($link =~ s/(.*[?&]autoplay=)false(.*)/${1}true${2}/i) {
		unless ($link =~ s/(.*[?&]autostart=)false(.*)/${1}true${2}/i) {
			unless ($link =~ s/(.*[?&]aplay=)false(.*)/${1}true${2}/i) {
				unless ($link =~ s/(.*[?&]autoplay=)0(.*)/${1}1${2}/i) {
					unless ($link =~ s/(.*[?&]autostart=)0(.*)/${1}1${2}/i) {
						unless ($link =~ s/(.*[?&]aplay=)0(.*)/${1}1${2}/i) {
							foreach my $ap (keys(%autoplay)) {
								if ($link =~ /$ap/) {
									($autoplay{$ap}) or return encode_entities($link);
									($autoplay{$ap} eq 'SKIP') and return 0;
									if ($autoplay{$ap} =~ /^[\?\&,]/) {
											$link .= $autoplay{$ap};
									} else {
										if ($link =~ /\?/) {
											$link .= '&' . $autoplay{$ap};
										} else {
											$link .= '?' . $autoplay{$ap};
										}
									}
									return encode_entities($link);
								}
							}
							if ($link =~ /\?/) {
								$link .= '&' . mnvgetconfig('defaultautoplay');
							} else {
								$link .= '?' . mnvgetconfig('defaultautoplay');
							}
							return encode_entities($link);
						}
					}
				}
			}
		}
	}
}

# Collect url's of all the podcasts
# input base url
# return hash ref to { "directory name" => "url" }
sub builddirurls {
	my $baseurl = shift @_;

    my %dirurls;

	mnvinfomsg(1, "Getting $baseurl");
	my $content = get($baseurl);
	unless ($content) {
		die "Could not retrieve $baseurl";
	}
	my $tree = HTML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		die "$baseurl parse failed, $@";
	}
	$tree->eof();

	my @ptrs = $tree->find_by_tag_name('a');
	foreach my $ptr (@ptrs) {
		if ($ptr->attr('href') =~ /topdocumentaryfilms.com\/category\//) {
			my $dir = $ptr->as_trimmed_text();
			$dirurls{$dir} = mnvcleantext($ptr->attr('href'));
		}
	}
    (keys(%dirurls)) or die "No urls found";

    return \%dirurls;
}

# Collect url's to all vids
# return hash ref to { "directory name" => "url" }
# return hash ref to { "directory name" => [[url,title]] }
sub buildvidurls {
	my $dirurls = shift @_;

    my %vidurls;

	foreach my $dir (sort(keys(%$dirurls))) {
		mnvinfomsg(1, "Getting $dir $dirurls->{$dir}");
		my $content = get($dirurls->{$dir});
		unless ($content) {
			warn "Could not retrieve $dirurls->{$dir}";
			next;
		}
		my $tree = HTML::TreeBuilder->new;
		eval { $tree->parse($content); };
		if ($@) {
			warn "$dirurls->{$dir} parse failed, $@";
			next;
		}
		$tree->eof();

		my @ptrs = $tree->find_by_tag_name('h2');
		(@ptrs) or next;

		foreach my $ptr (@ptrs) {
			my $a = $ptr->find_by_tag_name('a');
			($a) or next;
			my $url = mnvcleantext($a->attr('href'));
			my $title = mnvcleantext($a->as_trimmed_text());
			push(@{$vidurls{$dir}}, [$url, $title]);
		}
	}
    return \%vidurls;;
}

# Build all items
# input hash ref to { "directory name" => [array of anonymous hash's] }
# input "directory name"
# input url
# input title
# output number of items added
sub builditems {
	my $diritemsref = shift @_;
	my $dir = shift @_;
	my $url = shift @_;
	my $title = shift @_;

	mnvinfomsg(2, "Getting $dir Episode $url");
	my $content = get($url);
	unless ($content) {
		warn "Could not retrieve $url";
		return 0;
	}
	my $tree = HTML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		warn "$url parse failed, $@";
		return 0;
	}
	$tree->eof();

	my $desc = ""; my $icon = $baseicon; my @links;
	my $pc = $tree->look_down('class', 'postContent');
	($pc) or return 0;
	my $ptr = $pc->find_by_tag_name('p');
	($ptr) and $desc = mnvcleantext($ptr->as_trimmed_text());
	$ptr = $pc->find_by_tag_name('img');
	($ptr) and $icon = mnvcleantext($ptr->attr('src'));
	my @ptrs = $pc->find_by_tag_name('embed');
	foreach my $ptr (@ptrs) {
		my $l = mnvcleantext($ptr->attr('src'));
		($l) or next;
		my $lap = addautoplay($l);
		if ($lap) {
			push(@links, $lap);
		} else {
			mnvinfomsg(2, "Skipped $l");
		}
	}
	(@links) or return 0;

	my $country = "";
	my $addpart = 1;
	my $oldtitle = $title;
	foreach my $link (@links) {
		if ($#links > 0) {
			$title = "$oldtitle Pt $addpart";
			$addpart++;
		}
        push(@{$diritemsref->{$dir}}, {
            'dirthumbnail' => $icon,
            'title' => $title,
			'mythtv:subtitle' => "",
            'author' => "",
            'pubDate' => "",
            'description' => $desc,
            'link' => $link,
			'player' => mnvgetconfig('player'),
			'playerargs' => mnvgetconfig('playerargs'),
			'download' => mnvgetconfig('download'),
			'downloadargs' => mnvgetconfig('downloadargs'),
            'media:thumbnailurl' => $icon,
            'media:contenturl' => $link,
            'media:contentlength' => "",
            'media:contentduration' => "",
            'media:contentwidth' => "",
            'media:contentheight' => "",
            'media:contentlanguage' => "",
            'rating' => "",
			'mythtv:country' => $country,
			'mythtv:season' => "",
			'mythtv:episode' => "",
			'mythtv:customhtml' => "no"
        });

        mnvinfomsg(2, "Added $title");
    }
	return $#links + 1;
}

#################################### Main #####################################
# If you copy this for another site, hopefully these won't need to changed
getopts('vtTp:S:');

if ($opt_v) {
	($mnvcommonsubs::netvisionver == 23) and print "$site|TS\n";
	($mnvcommonsubs::netvisionver > 23) and mnvprintversion($site, $command, $author, $commandthumbnail, $version, $description);
	exit 0;
}

my $type; my $page = 1; my $search = "";
if ($opt_T) {
	$type = "tree";
} elsif ($opt_S) {
	$type = "search";
	$search = $opt_S;
	($opt_p) and $page = $opt_p;
} else {
	print STDERR "Must have -T or -S option\n";
	exit 1;
}

$SIG{'INT'} = \&mnvcleanexit;
$SIG{'HUP'} = \&mnvcleanexit;
$SIG{'TERM'} = \&mnvcleanexit;
$SIG{'QUIT'} = \&mnvcleanexit;

my $diritemsref = \%diritems;
my $totalitems = 0; my $filtereditems = 0;
my $ss = stat($store);
if (($ss) && (time() - $ss->mtime) < mnvgetconfig('cachetime')) {
	eval { $diritemsref = retrieve($store); };
	if ($@) {
		die "Could not load store, $@";
	}
	$totalitems = mnvnumresults($diritemsref);
	mnvinfomsg(1, "Using previous run data");
} else {
	$totalitems = builddiritems($diritemsref, $baseurl);
	eval { store($diritemsref, $store); };
	if ($@) {
		warn "Could not save store, $@";
	}
}

mnvrssheader();
print '<channel>
  <title>'.$site.'</title>
  <link>'.$baseurl.'</link>
  <description>'.$description.'</description>'."\n";
if ($type eq "search") {
	$filtereditems = mnvfilter($diritemsref, $search);
	mnvprintsearch($diritemsref, $page);
	mnvinfomsg(1, "Total Items match $filtereditems of $totalitems");
} else {
	mnvprinttree($diritemsref);
	mnvinfomsg(1, "Total Items found $totalitems");
}
print "</channel>\n";
mnvrssfooter();

mnvcleanexit 0;
