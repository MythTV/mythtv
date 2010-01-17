#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/twit.tv.pl,v 1.14 2010/01/17 22:03:05 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MythNetvision Grabber Script for TWiT site. Tree view only
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

#################################### Settings #################################
my $info = 0; # print info/progress message: 0 - off, 1 - low ,2 - high
my $infoop = 0; # info messages go to: 0 = stderr, filename = filename
# Set to skip a podcast. Skipping those you don't want will speed up scan.
# e.g. @skippodcast = (q/Abby's Road/, 'Roz Rows');
my @skippodcast = ();

#################################### Globals ##################################
my $baseurl = 'http://twit.tv';
my $totalitemsfound = 0;
my $printdirdone = 0; # Have we printed a <directory>
my %dirurls;
my $header = '<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/">
<channel>
  <title>twit.tv</title>
  <link>http://twit.tv</link>
  <description>Leo Laporte &amp; Friends</description>';
my $footer = '</channel>
</rss>';
our ($opt_v, $opt_T, $opt_p, $opt_S);
my ($mythobj, $sthlogentry);

#################################### Subs ##################################
sub cleanexit {
	my $esig = shift @_;

	fileno(FH) and close(FH);
	if ($esig =~ /\D/) {
		# called by signalhandler
		exit 1;
	} else {
		exit $esig;
	}
}

sub infomsg {
	my $level = shift @_;
	my $mesg = shift @_;

	($info < $level) and return;
	if ($infoop =~ /\D/) {
		open(FH, ">$infoop") unless fileno(FH);
		my $t = localtime();
		print FH "$t $level $mesg\n";
	} else {
		print STDERR "$mesg\n";
	}
}

sub fetchitem {
	my $dir = shift @_;
	my $url = shift @_;

	infomsg(2, "Getting $dir Episode $url");
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
	#print STDERR Dumper($tree)."\n";

	my @links;
	my @as = $tree->find_by_tag_name('a');
	foreach my $a (@as) {
		$a->as_trimmed_text() =~ /Download Video/ or next;
		$a->attr('href') =~ /^http:.*video.*mp4$/ and push(@links, $a->attr('href'));
	}
	(@links) || return 0;

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
		$title = $ptr->attr('title');
		$ptr = $ptr->parent();
		my $ptr2 = $ptr->look_down('class', 'podcast-date');
		if ($ptr2) {
			my $time = str2time($ptr2->as_trimmed_text());
			$pubDate = time2str("%a, %d %b %Y 00:00:00 GMT", $time);
		}
		$ptr2 = $ptr->find_by_tag_name('p');
		($ptr2) and $desc = $ptr2->as_trimmed_text();
	}
	($title) || return 0;

	my $img = "";
	$ptr = $tree->look_down('class', 'imagecache imagecache-coverart');
	($ptr) and $img = $ptr->attr('src');

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
		($tmpdur > 0) and $duration = $tmpdur;
	}
NODURATION:

	if ($printdirdone == 0) {
		print "    ".'<directory name="'.encode_entities($dir).'" thumbnail="'.encode_entities($img).'">'."\n";
		$printdirdone = 1;
	}

	foreach my $link (@links) {
		infomsg(2, "Found $title");
		my ($width, $height, $widthheight);
		$link =~ /_(\d\d\d)x(\d\d\d)_/;
		if ($1 && $2) {
			$width = $1;
			$height = $2;
			$widthheight = "${width}x${height}";
		}
		print "      ".'<item>'."\n";
		print "        ".'<title>'.encode_entities("${title} (${widthheight})").'</title>'."\n";
		print "        ".'<author>twit.tv</author>'."\n";
		print "        ".'<pubDate>'.$pubDate.'</pubDate>'."\n";
		print "        ".'<description>'.encode_entities($desc).'</description>'."\n";
		print "        ".'<link>'.encode_entities($url).'</link>'."\n";
        print "        ".'<media:group>'."\n";
        print "          ".'<media:thumbnail url="'.encode_entities($img).'"/>'."\n";
        print "          ".'<media:content url="'.encode_entities($link).'" length="" duration="'.$duration.'" width="'.$width.'" height="'.$height.'" lang=""/>'."\n";
        print "        ".'</media:group>'."\n";
		print "        ".'<rating></rating>'."\n";
		print "      ".'</item>'."\n";
	}

	return $#links + 1;
}

#################################### Main #####################################
getopts('vtTp:S:');

if ($opt_v) {
	print 'TWiT.tv|T'."\n";
	exit 0;
}
if ($opt_S || $opt_p) {
	print STDERR "Search not supported\n";
	exit 1;
}
unless ($opt_T) {
	print STDERR "Must have -T option\n";
	exit 1;
}

$SIG{'INT'} = \&cleanexit;
$SIG{'HUP'} = \&cleanexit;
$SIG{'TERM'} = \&cleanexit;
$SIG{'QUIT'} = \&cleanexit;

infomsg(1, "Getting $baseurl");
my $content = get($baseurl);
unless ($content) {
	die "Could not retrieve $baseurl";
}
my $tree = HTML::TreeBuilder->new;
eval { $tree->parse($content); };
if ($@) {
	die "$baseurl parse failed, $@";
}

my @ptrs;
my $tmp = $tree->look_down('class', 'leaf first');
($tmp) and push(@ptrs, $tmp);
my @tmp = $tree->look_down('class', 'leaf');
(@tmp) and push(@ptrs, @tmp);
foreach my $ptr (@ptrs) {
	my @as = $ptr->find_by_tag_name('a');
	foreach my $a (@as) {
		my $dir = $a->as_trimmed_text();
		if (grep(/$dir/, @skippodcast)) {
			infomsg(2, "Skipping $dir");
			next;
		}
		$dirurls{$dir} = $baseurl.$a->attr('href');
	}
}
#print STDERR Dumper(%dirurls);
(keys(%dirurls)) || cleanexit 0;

print "$header\n";
 
foreach my $dir (sort(keys(%dirurls))) {
	infomsg(1, "Getting $dir $dirurls{$dir}");
	my $content = get($dirurls{$dir});
	unless ($content) {
		warn "Could not retrieve $dirurls{$dir}";
		next;
	}
	my $tree = HTML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		warn "$dirurls{$dir} parse failed, $@";
		next;
	}
	#print STDERR Dumper($tree)."\n";

	# Not used anywhere.
	#my $dirdesc;
	#my $ptr = $tree->look_down('class', 'podcast-description');
	#($ptr) and $dirdesc = $ptr->as_trimmed_text();

	my @ptrs;
	my $tmp = $tree->look_down('class', 'podcast-number current');
	($tmp) and push(@ptrs, $tmp);
	my @tmp = $tree->look_down('class', 'podcast-number');
	(@tmp) and push(@ptrs, @tmp);
	my $diritemsfound = 0;
	$printdirdone = 0;
	for (my $c = 0; $c <= $#ptrs; $c++) {
		$diritemsfound += fetchitem($dir, $baseurl.$ptrs[$c]->attr('href'));
		# Skip rest as nothing found so far. (To speed things up)
		if ($c > 0 && $diritemsfound == 0) {
			infomsg(2, "Skipping rest of $dir as nothing found so far");
			last;
		}
	}
	($printdirdone == 1) and print "    ".'</directory>'."\n";
	infomsg(1, "$dir items found $diritemsfound");
	$totalitemsfound += $diritemsfound;
}

print "$footer\n";
infomsg(1, "Total items found $totalitemsfound");
cleanexit 0;
