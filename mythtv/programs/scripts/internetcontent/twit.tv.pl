#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/twit.tv.pl,v 1.18 2010/01/26 00:31:12 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MythNetvision Grabber Script for TWiT.tv site.
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
use XML::RSS;
use Storable;
use File::stat;

#################################### Settings #################################
my $cachetime = 7200; # Seconds to cache results
my $info = 0; # print info/progress message: 0 - off, 1 - low ,2 - high
my $infoop = 0; # info messages go to: 0 = stderr, filename = filename
# Set to skip a podcast. Skipping those you don't want will speed up scan.
# e.g. @skippodcast = (q/Abby's Road/, 'Roz Rows');
my @skippodcast = ();

#################################### Globals ##################################
my $site = 'TWiT.tv';
my $description = 'Leo Laporte &amp; Friends';
my $baseurl = 'http://twit.tv';
my $store = "/tmp/.${site}.diritemsref.store";
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
  <title>'.$site.'</title>
  <link>'.$baseurl.'</link>
  <description>'.$description.'</description>';
my $footer = '</channel>
</rss>';
our ($opt_v, $opt_T, $opt_p, $opt_S);
my %diritems;

#################################### Util Subs ############################################
# If you copy this for another site, hopefully these won't need to changed

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
	$mesg = encode('utf8', $mesg);
	if ($infoop =~ /\D/) {
		open(FH, ">$infoop") unless fileno(FH);
		my $t = localtime();
		print FH "$t $level $mesg\n";
	} else {
		print STDERR "$mesg\n";
	}
}

sub cleantext {
	my $text = shift @_;

	($text) or return;
	$text =~ s/\n/ /g;
	$text =~ s/^\s+|\s+$//g;
	$text = encode_entities($text);
	return $text;
}

sub numresults {
	my $diritemsref = shift @_;

	my $nr = 0;
	foreach my $dir (keys(%$diritemsref)) {
		(@{$diritemsref->{$dir}}) or next;
		foreach my $item (@{$diritemsref->{$dir}}) {
			($item) or next; # somewhere I get null references.
			$nr += 1;
		}
	}
	return $nr;
}

# filter the items hash
# input hash ref produced by builditems
# output number of items
sub filter {
	my $diritemsref = shift @_;
	my $search = shift @_;

	my $filtereditems = 0;

	foreach my $dir (keys(%$diritemsref)) {
		for (my $c = 0; $c <= $#{$diritemsref->{$dir}}; $c++) {
			if (${$diritemsref->{$dir}}[$c]->{'title'} =~ /$search/i || ${$diritemsref->{$dir}}[$c]->{'author'} =~ /$search/i ||
			${$diritemsref->{$dir}}[$c]->{'description'} =~ /$search/i || ${$diritemsref->{$dir}}[$c]->{'link'} =~ /$search/i) {
				$filtereditems++;
				infomsg(2, "Saved $dir ".${$diritemsref->{$dir}}[$c]->{'title'});
			} else {
				infomsg(2, "Deleted $dir ".${$diritemsref->{$dir}}[$c]->{'title'});
				delete(${$diritemsref->{$dir}}[$c]);
			}
		}
	}
	return $filtereditems;
}

# print tree
# input hash ref produced by builditems
sub printtree {
	my $diritemsref = shift @_;

	foreach my $dir (sort(keys(%$diritemsref))) {
		(@{$diritemsref->{$dir}}) or next;
		my $dirprinted = 0;
		foreach my $item (@{$diritemsref->{$dir}}) {
			($item) or next; # somewhere I get null references.
			if ($dirprinted == 0) {
				# Need it here as don't have $item outside loop.
				print "    ".'<directory name="'.cleantext($dir).'" thumbnail="'.$item->{'dirthumbnail'}.'">'."\n";
				$dirprinted = 1;
			}
			print "      ".'<item>'."\n";
			print "        ".'<title>'.$item->{'title'}.'</title>'."\n";
			print "        ".'<author>'.$item->{'author'}.'</author>'."\n";
			print "        ".'<pubDate>'.$item->{'pubDate'}.'</pubDate>'."\n";
			print "        ".'<description>'.$item->{'description'}.'</description>'."\n";
			print "        ".'<link>'.$item->{'link'}.'</link>'."\n";
			print "        ".'<media:group>'."\n";
			print "          ".'<media:thumbnail url="'.$item->{'media:thumbnailurl'}.'"/>'."\n";
			print "          ".'<media:content url="'.$item->{'media:contenturl'}.'" length="'.$item->{'media:contentlength'}.'" duration="'.$item->{'media:contentduration'}.'" width="'.$item->{'media:contentwidth'}.'" height="'.$item->{'media:contentheight'}.'" lang="'.$item->{'media:contentlanguage'}.'"/>'."\n";
			print "        ".'</media:group>'."\n";
			print "        ".'<rating>'.$item->{'rating'}.'</rating>'."\n";
			print "      ".'</item>'."\n";
		}
		print "    ".'</directory>'."\n";
	}
}

# print search
# input hash ref produced by builditems
sub printsearch {
	my $diritemsref = shift @_;
	my $page = shift @_;

	my $numresults = numresults($diritemsref);
	if ($numresults < 1) {
		print "  ".'<numresults>0</numresults>'."\n";
		print "  ".'<returned>0</returned>'."\n";
		print "  ".'<startindex>0</startindex>'."\n";
		return;
	}
	my $returned;
	if ($page > 1) {
		$returned = $numresults - (($page - 1) * 20);
	} else {
		$returned = $numresults;
	}
	($returned > 20) and $returned = 20;
	($returned < 0) and $returned = 0;
	my $startindex = ($page * 20) + 1;
	($startindex >= $numresults) and $startindex = 0;

	print "  ".'<numresults>'.$numresults.'</numresults>'."\n";
	print "  ".'<returned>'.$returned.'</returned>'."\n";
	print "  ".'<startindex>'.$startindex.'</startindex>'."\n";

	my $scount = 0; my $ecount = 0;
L1:	foreach my $dir (sort(keys(%$diritemsref))) {
		(@{$diritemsref->{$dir}}) or next;
		foreach my $item (@{$diritemsref->{$dir}}) {
			($item) or next; # somewhere I get null references.
			if ($page > 1 && $scount < (($page - 1) * 20)) {
				$scount++;
				next;
			}
			$ecount++;
			print "      ".'<item>'."\n";
			print "        ".'<title>'.$item->{'title'}.'</title>'."\n";
			print "        ".'<author>'.$item->{'author'}.'</author>'."\n";
			print "        ".'<pubDate>'.$item->{'pubDate'}.'</pubDate>'."\n";
			print "        ".'<description>'.$item->{'description'}.'</description>'."\n";
			print "        ".'<link>'.$item->{'link'}.'</link>'."\n";
			print "        ".'<media:group>'."\n";
			print "          ".'<media:thumbnail url="'.$item->{'media:thumbnailurl'}.'"/>'."\n";
			print "          ".'<media:content url="'.$item->{'media:contenturl'}.'" length="'.$item->{'media:contentlength'}.'" duration="'.$item->{'media:contentduration'}.'" width="'.$item->{'media:contentwidth'}.'" height="'.$item->{'media:contentheight'}.'" lang="'.$item->{'media:contentlanguage'}.'"/>'."\n";
			print "        ".'</media:group>'."\n";
			print "        ".'<rating>'.$item->{'rating'}.'</rating>'."\n";
			print "      ".'</item>'."\n";
			($ecount >= $returned) and last L1;
		}
	}
}

#################################### Site Specific Subs ##########################
# Build all vid items for all directories
# input hash ref to { "directory name" => [array of anonymous hash's] }
# anonymous hash {
#		'dirthumbnail' => $icon,
#		'title' => $title,
#		'author' => $author,
#		'pubDate' => $pubDate,
#		'description' => $description,
#		'link' => $url,
#		'media:thumbnailurl' => "",
#		'media:contenturl' => $contenturl,
#		'media:contentlength' => $length,
#		'media:contentduration' => "",
#		'media:contentwidth' => "",
#		'media:contentheight' => "",
#		'media:contentlanguage' => $language,
#		'rating' => ""
#	}
# Basically this hash ref is what you need to build.
# input base url
# output items found
sub builddiritems {
	my $diritemsref = shift @_;
	my $baseurl = shift @_;

	my $dirurls = builddirurls($baseurl);
	my $vidurls = buildvidurls($dirurls);
	my $itemsfound = 0;
	foreach my $dir (keys(%$vidurls)) {
		my $diritemsfound = 0;
		for (my $c = 0; $c <= $#{$vidurls->{$dir}}; $c++) {
			my $found = builditems($diritemsref, $dir, ${$vidurls->{$dir}}[$c]);
			$itemsfound += $found;
			$diritemsfound += $found;
			# Skip rest as nothing found so far. (To speed things up)
			if ($c > 0 && $diritemsfound == 0) {
				infomsg(2, "Skipping rest of $dir as nothing found so far");
				last;
			}
		}
		infomsg(1, "$dir Items found $diritemsfound");
	}
	return $itemsfound;
}

# Collect url's of all the podcasts
# input base url
# return hash ref to { "directory name" => "url" }
sub builddirurls {
	my $baseurl = shift@_;

	my %dirurls;

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
			$dirurls{$dir} = cleantext($baseurl.$a->attr('href'));
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
		infomsg(1, "Getting $dir $dirurls->{$dir}");
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
			push(@{$vidurls{$dir}}, cleantext($baseurl.$urlp->attr('href')));
		}
	}
	#print STDERR Dumper(%vidurls);
	return \%vidurls;
}

# Build all items
# input hash ref to { "directory name" => [array of anonymous hash's] }
# input "directory name", url
# output number of items added
sub builditems {
	my $diritemsref = shift @_;
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

	my @links;
	my @as = $tree->find_by_tag_name('a');
	foreach my $a (@as) {
		$a->as_trimmed_text() =~ /Download Video/ or next;
		$a->attr('href') =~ /^http:.*video.*mp4$/ and push(@links, cleantext($a->attr('href')));
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
		$title = cleantext($ptr->attr('title'));
		$ptr = $ptr->parent();
		my $ptr2 = $ptr->look_down('class', 'podcast-date');
		if ($ptr2) {
			my $time = str2time($ptr2->as_trimmed_text());
			$pubDate = time2str("%a, %d %b %Y 00:00:00 GMT", $time);
			$pubDate = cleantext($pubDate);
		}
		$ptr2 = $ptr->find_by_tag_name('p');
		($ptr2) and $desc = cleantext($ptr2->as_trimmed_text());
	}
	($title) or return 0;

	my $icon = "";
	$ptr = $tree->look_down('class', 'imagecache imagecache-coverart');
	($ptr) and $icon = cleantext($ptr->attr('src'));

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
		($tmpdur > 0) and $duration = cleantext($tmpdur);
	}
NODURATION:

	foreach my $link (@links) {
		my ($width, $height, $widthheight);
		$link =~ /_(\d\d\d)x(\d\d\d)_/;
		if ($1 && $2) {
			$width = $1;
			$height = $2;
			$widthheight = "${width}x${height}";
		}
		push(@{$diritemsref->{$dir}}, {
			'dirthumbnail' => $icon,
			'title' => "$title (${widthheight})",
			'author' => "twit.tv",
			'pubDate' => $pubDate,
			'description' => $desc,
			'link' => $url,
			'media:thumbnailurl' => "$icon",
			'media:contenturl' => $link,
			'media:contentlength' => "",
			'media:contentduration' => $duration,
			'media:contentwidth' => $width,
			'media:contentheight' => $height,
			'media:contentlanguage' => "",
			'rating' => ""
		});

		infomsg(2, "Added $title");
	}
	return $#links + 1;
}

#################################### Main #####################################
getopts('vtTp:S:');

if ($opt_v) {
        print "<grabber>\n";
        print "  <name>Twit.tv</name>\n";
        print "  <author>Auric</author>\n";
        print "  <thumbnail>twit.tv.png</thumbnail>\n";
        print "  <type>video</type>\n";
        print "  <description>This Week in Tech is netcasts you love from people you trust.</description>\n";
        print "  <version>1.00</version>\n";
        print "  <search>true</search>\n";
        print "  <tree>true</tree>\n";
        print "</grabber>\n";
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

$SIG{'INT'} = \&cleanexit;
$SIG{'HUP'} = \&cleanexit;
$SIG{'TERM'} = \&cleanexit;
$SIG{'QUIT'} = \&cleanexit;

my $diritemsref = \%diritems;
my $totalitems = 0; my $filtereditems = 0;
my $ss = stat($store);
if (($ss) && (time() - $ss->mtime) < $cachetime) {
	eval { $diritemsref = retrieve($store); };
	if ($@) {
		die "Could not load store, $@";
	}
	$totalitems = numresults($diritemsref);
	infomsg(1, "Using previous run data");
} else {
	$totalitems = builddiritems($diritemsref, $baseurl);
	eval { store($diritemsref, $store); };
	if ($@) {
		warn "Could not save store, $@";
	}
}

print "$header\n";
if ($type eq "search") {
	$filtereditems = filter($diritemsref, $search);
	printsearch($diritemsref, $page);
	infomsg(1, "Total Items match $filtereditems of $totalitems");
} else {
	printtree($diritemsref);
	infomsg(1, "Total Items found $totalitems");
}
print "$footer\n";

cleanexit 0;

