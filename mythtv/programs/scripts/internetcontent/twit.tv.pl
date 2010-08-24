#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/twit.tv.pl,v 1.32 2010/07/24 23:28:11 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MythNetvision Grabber Script for TWiT.tv site.
#
# If you want to alter any of the default settings.
# Create/Change $HOME/.mythtv/MythNetvision/userGrabberPrefs/twit.tv.cfg
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

#################################### Globals ##################################
my $version = '$Revision: 1.32 $'; $version =~ s/\D*([\d\.]+)\D*/$1/; # rcs tag populated
my $command = "twit.tv.pl"; my $commandthumbnail = "twit.tv.png"; my $author = "Auric";
my $site = 'TWiT.tv';
my $description = 'Leo Laporte &amp; Friends';
my $baseurl = 'http://twit.tv';
my $baseicon = 'http://twit.tv/sites/all/themes/twit/img/logo.gif';
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
		for (my $c = 0; $c <= $#{$vidurlsref->{$dir}}; $c++) {
			my $found = builditems($diritemsref, $dir, ${$vidurlsref->{$dir}}[$c]);
			$itemsfound += $found;
			$diritemsfound += $found;
			# Skip rest as nothing found so far. (To speed things up)
			if ($c > 0 && $diritemsfound == 0) {
				mnvinfomsg(2, "Skipping rest of $dir as nothing found so far");
				last;
			}
		}
		mnvinfomsg(1, "$dir Items found $diritemsfound");
	}
	return $itemsfound;
}

# Collect url's of all the podcasts
# input base url
# return hash ref to { "directory name" => "url" }
sub builddirurls {
	my $baseurl = shift@_;

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

	my @ptrs;
	my $tmp = $tree->look_down('class', 'leaf first');
	($tmp) and push(@ptrs, $tmp);
	my @tmp = $tree->look_down('class', 'leaf');
	(@tmp) and push(@ptrs, @tmp);
	foreach my $ptr (@ptrs) {
		my @as = $ptr->find_by_tag_name('a');
		foreach my $a (@as) {
			my $dir = $a->as_trimmed_text();
			$dirurls{$dir} = mnvcleantext($baseurl.$a->attr('href'));
		}
	}
	#print STDERR Dumper(%dirurls);
	(keys(%dirurls)) or die "No urls found";

	return \%dirurls;
}

# Collect url's to all vids
# input hash ref to  { "directory name" => "url" }
# return hash ref to { "directory name" => [url] }
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

		# Not used anywhere.
		#my $dirdesc;
		#my $ptr = $tree->look_down('class', 'podcast-description');
		#($ptr) and $dirdesc = $ptr->as_trimmed_text();

		my @ptrs;
		my $tmp = $tree->look_down('class', 'podcast-number current');
		($tmp) and push(@ptrs, $tmp);
		my @tmp = $tree->look_down('class', 'podcast-number');
		(@tmp) and push(@ptrs, @tmp);
		foreach my $urlp (@ptrs) {
			push(@{$vidurls{$dir}}, mnvcleantext($baseurl.$urlp->attr('href')));
		}
	}
	#print STDERR Dumper(%vidurls);
	return \%vidurls;
}

# Build all items
# input hash ref to { "directory name" => [array of anonymous hash's] }
# input "directory name"
# input url
# output number of items added
sub builditems {
	my $diritemsref = shift @_;
	my $dir = shift @_;
	my $url = shift @_;

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

	my @links;
	my @as = $tree->find_by_tag_name('a');
	foreach my $a (@as) {
		$a->as_trimmed_text() =~ /Download Video/ or next;
		$a->attr('href') =~ /^http:.*video.*mp4$/ and push(@links, mnvcleantext($a->attr('href')));
	}
	(@links) or return 0;

	my $title = ""; my $pubDate = ""; my $desc = "";
	my @ptrs;
	my $tmp = $tree->look_down('class', 'podcast-number current');
	($tmp) and push(@ptrs, $tmp);
	my @tmp = $tree->look_down('class', 'podcast-number');
	(@tmp) and push(@ptrs, @tmp);
	my $ptr;
	foreach my $tmp (@ptrs) {
		my $testurl = $tmp->attr('href');
		$url =~ /http:.*${testurl}/ and $ptr = $tmp and last;
	}
	if ($ptr) {
		$title = mnvcleantext($ptr->attr('title'));
		$ptr = $ptr->parent();
		my $ptr2 = $ptr->look_down('class', 'podcast-date');
		if ($ptr2) {
			my $time = str2time($ptr2->as_trimmed_text());
			$pubDate = time2str("%a, %d %b %Y 00:00:00 GMT", $time);
			$pubDate = mnvcleantext($pubDate);
		}
		$ptr2 = $ptr->find_by_tag_name('p');
		($ptr2) and $desc = mnvcleantext($ptr2->as_trimmed_text());
	}
	($title) or return 0;

	my $icon = $baseicon;
	$ptr = $tree->look_down('class', 'imagecache imagecache-coverart');
	($ptr) and $icon = mnvcleantext($ptr->attr('src'));

	my $duration = "";
	$ptr = $tree->look_down('class', 'running-time');
	($ptr) and my $tmpdur = $ptr->as_trimmed_text();
	if ($tmpdur =~ s/Running time: //) {
		my $hours = 0; my $mins = 0; my $secs = 0;
		my $count = $tmpdur =~ s/(:)/$1/g;
		if ($count == 1) {
			($mins, $secs) = split(':', $tmpdur);
		} elsif ($count == 2) {
			($hours, $mins, $secs) = split(':', $tmpdur);
		} else {
			goto NODURATION;
		}
		$tmpdur = ($hours * 60 * 60) + ($mins * 60) + $secs;
		($tmpdur > 0) and $duration = mnvcleantext($tmpdur);
	}
NODURATION:

	my $country = "";

	my $count = 0;
	foreach my $contenturl (@links) {
		my ($width, $height, $titleresolution);
		$contenturl =~ /_(\d\d\d)x(\d\d\d)_/;
		if ($1 && $2) {
			$width = $1;
			$height = $2;
		}
		if (mnvgetconfig('resolution')) {
			$titleresolution = $title;
			if ((mnvgetconfig('resolution') eq "high") && ($width < 750)) {
				mnvinfomsg(1, "Skipping $contenturl due to wrong resolution");
				next;
			}
			if ((mnvgetconfig('resolution') eq "low") && ($width >= 750)) {
				mnvinfomsg(1, "Skipping $contenturl due to wrong resolution");
				next;
			}
		} else {
			$titleresolution = "$title (${width}x${height})";
		}
		my $link = $url;
		if ((mnvgetconfig('netplayer')) && ($contenturl)) {
			if (mnvistype(mnvgetconfig('netplayertype'), $contenturl)) {
				my $encodedtitle = decode_entities($title);
				$encodedtitle = mnvURLEncode($encodedtitle);
				$link = mnvcleantext(mnvgetconfig('netplayer')."?title=${encodedtitle}&videofile=").$contenturl;
			} else {
				mnvinfomsg(1, "Not ".mnvgetconfig('netplayertype')." $contenturl");
			}
		}
		push(@{$diritemsref->{$dir}}, {
			'dirthumbnail' => $icon,
			'title' => $titleresolution,
			'mythtv:subtitle' => "",
			'author' => "twit.tv",
			'pubDate' => $pubDate,
			'description' => $desc,
			'link' => $link,
			'player' => mnvgetconfig('player'),
			'playerargs' => mnvgetconfig('playerargs'),
			'download' => mnvgetconfig('download'),
			'downloadargs' => mnvgetconfig('downloadargs'),
			'media:thumbnailurl' => $icon,
			'media:contenturl' => $contenturl,
			'media:contentlength' => "",
			'media:contentduration' => $duration,
			'media:contentwidth' => $width,
			'media:contentheight' => $height,
			'media:contentlanguage' => "",
			'rating' => "",
			'mythtv:country' => $country,
			'mythtv:season' => "",
			'mythtv:episode' => "",
			'mythtv:customhtml' => "no"
		});

		mnvinfomsg(2, "Added $title");
		$count ++;
	}
	return $count;
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
	mnvprinttree($diritemsref, 4);
	mnvinfomsg(1, "Total Items found $totalitems");
}
print "</channel>\n";
mnvrssfooter();

mnvcleanexit 0;
