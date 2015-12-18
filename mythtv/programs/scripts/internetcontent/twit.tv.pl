#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/twit.tv.pl,v 1.34 2015/11/04 11:29:00 mythtv Exp $
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
use XML::TreeBuilder;
use Data::Dumper;
use Date::Parse;
use Date::Format;
use DateTime;
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
my $version = '$Revision: 1.34 $'; $version =~ s/\D*([\d\.]+)\D*/$1/; # rcs tag populated
my $command = "twit.tv.pl"; my $commandthumbnail = "twit.tv.png"; my $author = "Auric (modififed by Neoh4x0r)";
my $site = 'TWiT.tv';
my $description = 'Leo Laporte &amp; Friends';
my $baseurl = 'http://wiki.twit.tv';
my $feedsurl = 'http://wiki.twit.tv/wiki/TWiT_Show_Feeds';
my $baseicon = 'http://wiki.twit.tv/w/images/twitipedialogo.png';
my $store = "/tmp/.${site}.diritemsref.store";

my $video_small = "_video_small.xml";
my $video_large = "_video_large.xml";
my $video_hd = "_video_hd.xml";

my $video_size = $video_large;

my $max_page_count = 1; # 0=disabled ; limit the number of pages parsed for episodes (there are 24 ep/page * num_pages)
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
printf "DEBUG:builddiritems called\n";
	my $diritemsref = shift @_;
	my $baseurl = shift @_;

	my $dirurlsref = builddirurls($baseurl);
	my $vidurlsref = buildvidurls($dirurlsref);
	my $itemsfound = 0;
	foreach my $dir (keys(%$vidurlsref)) {
		my $diritemsfound = 0;
		my $count = $#{$vidurlsref->{$dir}};
# printf "DEBUG:builddiritems:count = '%d'\n", $count;
		for (my $c = 0; $c <= $count; $c++) {
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
printf "DEBUG:builddirurls called\n";
	my $baseurl = shift@_;

	my %dirurls;

printf "DEBUG:builddirurls:Getting %s\n", $baseurl;
	mnvinfomsg(1, "Getting $baseurl");
	# only grab active shows
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

	my @show_dirs;
	my @show_urls;
	my $bReadingActiveShows = 0;
	my @ptrs;
	# read the active show TOC
	my @tmp = $tree->look_down('class', 'toclevel-1 tocsection-1');
	(@tmp) and push(@ptrs, @tmp);
	foreach my $ptr (@ptrs) {
		#printf "DEBUG:builddirurls:ptr = '%s'\n", $ptr->as_trimmed_text();
		my $ul = $ptr->find_by_tag_name('ul');
		#printf "DEBUG:builddirurls:ul = '%s'\n", $ul->as_trimmed_text();
		my @li = $ul->find_by_tag_name('li');
		foreach my $tag (@li) {	
			#printf "DEBUG:builddirurls:li = '%s'\n", $tag->as_trimmed_text();
			my @as = $tag->find_by_tag_name('a');
			foreach my $a (@as) {	
				my $id = mnvcleantext($a->attr('href'));
				$id =~ s/^.//; 
				#printf "DEBUG:builddirurls:id = '%s'\n", $id;
				# lookup this id
				my @ptrs1;
				# locate the active show by id
				my @tmp1 = $tree->look_down('class', 'mw-headline', 'id' , $id);
				(@tmp1) and push(@ptrs1, @tmp1);				
				foreach my $ptr1 (@ptrs1) {
					# get the show name
					my $dir = $ptr1->as_trimmed_text();
					#printf "DEBUG:builddirurls:dir = '%s'\n", $dir;
					if($dir)
					{
						push(@show_dirs, $dir);
					}
				}
			}
		}
	}
	my $show_dirs_size = @show_dirs;
	printf "DEBUG:builddirurls:found '%d' active shows\n", $show_dirs_size;
#	foreach my $dir (@show_dirs)
#	{
#		printf "DEBUG:builddirurls:show_dir = '%s'\n", $dir;
#	}

	# find show feeds
	undef @tmp;
	undef @ptrs;
	my $url_count=0;
	@tmp = $tree->look_down('class', 'external free');
	(@tmp) and push(@ptrs, @tmp);
	foreach my $ptr (@ptrs) {
		my @as = $ptr->find_by_tag_name('a');
		foreach my $a (@as) {
			if ( ($url_count + 1) > $show_dirs_size)
			{
				last;
			}
			my $url = mnvcleantext($a->attr('href'));
			if ($url && $url =~ /$video_size$/)
			{
				my $dir = $show_dirs[$url_count];
				printf "DEBUG:builddirurls:feed [%d] = '%s' -> '%s'\n", $url_count, $dir, $url;
				$dirurls{$dir} = $url;
				$url_count++;
			}
		}
	}



	(keys(%dirurls)) or die "No urls found";

	return \%dirurls;
}

# Collect url's to all vids
# input hash ref to  { "directory name" => "url" }
# return hash ref to { "directory name" => [url] }
sub buildvidurls {
printf "DEBUG:buildvidurls called\n";
	my $dirurls = shift @_;

	my %vidurls;

	foreach my $dir (sort(keys(%$dirurls))) {
		# this originally expected a streaming video on  page, but we have a direct link to the feed
                # just return the feed url
		push(@{$vidurls{$dir}}, $dirurls->{$dir});
#printf "DEBUG:buildvidurls:dir = %s -> %s\n", $dir, $dirurls->{$dir};
#		mnvinfomsg(1, "Getting $dir $dirurls->{$dir}");
#		my $content = get($dirurls->{$dir});
#		unless ($content) {
#			warn "Could not retrieve $dirurls->{$dir}";
#			next;
#		}
#		my $tree = HTML::TreeBuilder->new;
#		eval { $tree->parse($content); };
#		if ($@) {
#			warn "$dirurls->{$dir} parse failed, $@";
#			next;
#		}
#		$tree->eof();
#
#		my @ptrs1;
#		my $tmp1 = $tree->look_down('class', 'all-episodes');
#		($tmp1) and push(@ptrs1, $tmp1);
#		foreach my $ptr (@ptrs1) {
#			my @as = $ptr->find_by_tag_name('a');
#			foreach my $a (@as) {
#				my $href = $a->as_trimmed_text();
#				my $url = mnvcleantext($baseurl.$a->attr('href'));
#				$epurl = $url
##			}
#		}
#
#
#		my @ptrs1;
#		my $tmp1 = $tree->look_down('class', 'all-episodes');
#		($tmp1) and push(@ptrs1, $tmp1);
#		foreach my $ptr (@ptrs1) {
#			my @as = $ptr->find_by_tag_name('a');
#			foreach my $a (@as) {
#				my $href = $a->as_trimmed_text();
#				my $url = mnvcleantext($baseurl.$a->attr('href'));
#				$epurl = $url
#			}
#		}

#		my $epurl;
#		# parse episode url
#		my @ptrs1;
#		my $tmp1 = $tree->look_down('class', 'all-episodes');
#		($tmp1) and push(@ptrs1, $tmp1);
#		foreach my $ptr (@ptrs1) {
#			my @as = $ptr->find_by_tag_name('a');
#			foreach my $a (@as) {
#				my $href = $a->as_trimmed_text();
#				my $url = mnvcleantext($baseurl.$a->attr('href'));
#				$epurl = $url
#			}
#		}
#		printf "DEBUG:buildvidurls:Parsing url = '%s' -> '%s'\n", $dir, $epurl;
#		$content = get($epurl);
#		unless ($content) {
#			warn "Could not retrieve $epurl";
#			next;
#		}
#		$tree = HTML::TreeBuilder->new;
#		eval { $tree->parse($content); };
#		if ($@) {
#			warn "$epurl parse failed, $@";
#			next;
#		}
#		$tree->eof();
#
#		my $ep_num=0;
#		my $page_count=1;
#
#		# parse episode number
#		my @ep_words = split /=/, $epurl;
#		$ep_num = $ep_words[1];
#		#printf "DEBUG:buildvidurls:ep_num = '%d'\n", $ep_num;
#
#		# parse page count
#		my @ptrs;
#		my $tmp = $tree->look_down('class', 'pagination');
#		($tmp) and push(@ptrs, $tmp);
#		my @tmp = $tree->look_down('class', 'page-number');
#		(@tmp) and push(@ptrs, @tmp);
#		foreach my $ptr (@ptrs) {
#			my @as = $ptr->find_by_tag_name('span');
#			foreach my $a (@as) {
#				my $item = $a->as_trimmed_text();
#				#printf "DEBUG:buildvidurls:item = '%s'\n", $item;
#				# parse the page count
#				my @words = split / /, $item;
#				$page_count = $words[3];
#			}
#		}
#		#printf "DEBUG:buildvidurls:page_count = '%d'\n", $page_count;
#
#
#		# generate list of urls
#                if ($max_page_count <= 0)
#		{
#			# disable max_page_count if it is 0 or less
#	 		$max_page_count = $page_count;
#		}
#		for (my $i=1; $i <= $max_page_count; $i++) {
#			# url format: http://twit.tv/episodes?page=<page>&filter[shows]=<episode>
#			my $page_url = "http://twit.tv/episodes?page=" . $i. "&filter[shows]=" . $ep_num;
#			printf "DEBUG:buildvidurls:parseing = '%s'\n", $page_url;
#			$content = get($page_url );
#			unless ($content) {
#				warn "Could not retrieve $page_url";
#				next;
#			}
#			$tree = HTML::TreeBuilder->new;
#			eval { $tree->parse($content); };
#			if ($@) {
#				warn "$epurl parse failed, $@";
#				next;
#			}
#			$tree->eof();
#			my @ptrs;
#			#my $tmp = $tree->look_down('class', 'list hero episodes');
#			#($tmp) and push(@ptrs, $tmp);
#			my @tmp = $tree->look_down('class', 'episode item');
#			(@tmp) and push(@ptrs, @tmp);
#			foreach my $urlp (@ptrs) {
#				#push(@{$vidurls{$dir}}, mnvcleantext($baseurl.$urlp->attr('href')));
#				my @as = $urlp->find_by_tag_name('a');
#				foreach my $a (@as) {
#					my $link_tag = $a->as_trimmed_text();
#					my $link_url = mnvcleantext($baseurl.$a->attr('href'));
#					$link_url =~ s/autostart=false/autostart=true/g;
#					my $link_title = mnvcleantext($a->attr('title'));
#					printf "DEBUG:buildvidurls:[tag = '%s'] [title = '%s'] [url = '%s']\n", $link_tag, $link_title, $link_url;
#					printf "DEBUG:buildvidurls:[add '%s']\n",$link_url;
#					push(@{$vidurls{$dir}}, mnvcleantext($link_url));
#				}
#			}
#		}
#
#		# Not used anywhere.
#		#my $dirdesc;
#		#my $ptr = $tree->look_down('class', 'podcast-description');
#		#($ptr) and $dirdesc = $ptr->as_trimmed_text();
#
#		my @ptrs;
#		my $tmp = $tree->look_down('class', 'podcast-number current');
#		($tmp) and push(@ptrs, $tmp);
#		my @tmp = $tree->look_down('class', 'podcast-number');
#		(@tmp) and push(@ptrs, @tmp);
#		foreach my $urlp (@ptrs) {
#			push(@{$vidurls{$dir}}, mnvcleantext($baseurl.$urlp->attr('href')));
#		}
	}
#	#print STDERR Dumper(%vidurls);
	return \%vidurls;
}

# Build all items
# input hash ref to { "directory name" => [array of anonymous hash's] }
# input "directory name"
# input url
# output number of items added
sub builditems {
printf "DEBUG:buildvidurls called\n";
	my $diritemsref = shift @_;
	my $dir = shift @_;
	my $url = shift @_;

	printf "DEBUG:builditems:%s -> %s\n", $dir, $url;
	mnvinfomsg(2, "Getting $dir Episode $url");
	my $content = get($url);
	unless ($content) {
		warn "Could not retrieve $url";
		return 0;
	}
	my $tree = XML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		warn "$url parse failed, $@";
		return 0;
	}
	$tree->eof();
	




	my $thumbnail = $tree->find_by_tag_name('channel')->find_by_tag_name('image')->find_by_tag_name('url')->as_trimmed_text();;
	my $count =0;	
	my @ptrs;
	my @tmp = $tree->find_by_tag_name('item');
	(@tmp) and push(@ptrs, @tmp);
	foreach my $ptr (@ptrs) {
		#printf "DEBUG:builditems:ptr = %s\n", $ptr->as_trimmed_text();
		my $title = HTML::Entities::encode($ptr->find_by_tag_name('title')->as_trimmed_text());
		my $pubdate = $ptr->find_by_tag_name('pubDate')->as_trimmed_text();
		my $description = HTML::Entities::encode($ptr->find_by_tag_name('description')->as_trimmed_text());
		my $subtitle = HTML::Entities::encode($ptr->find_by_tag_name('itunes:subtitle')->as_trimmed_text());
		my $contenturl = $ptr->find_by_tag_name('link')->as_trimmed_text();
		my $duration = $ptr->find_by_tag_name('itunes:duration')->as_trimmed_text();


		my $author = $ptr->find_by_tag_name('author')->as_trimmed_text();

		my $raw_episode = $ptr->find_by_tag_name('comments')->as_trimmed_text();
		my @words = split /\//, $raw_episode;
		my $episode = "";
		foreach my $part (@words)
		{
			$episode = $part;
		}

		my $epoch = str2time($pubdate);
		my $dt = DateTime->from_epoch(epoch => $epoch);
		my $format = '%Y/%m/%d';
		my $timestamp = $dt->strftime($format);
		$pubdate = $timestamp;

		

		my ($width, $height);
		my $tmp_contenturl = $contenturl;
		$tmp_contenturl =~ /_(\d\d\d\d)x(\d\d\d)_/;
		if ($1 && $2) {
			$width = $1;
			$height = $2;
		}
		else
		{
			$tmp_contenturl = $contenturl;
			$tmp_contenturl =~ /_(\d\d\d)x(\d\d\d)_/;
			if ($1 && $2) {
				$width = $1;
				$height = $2;
			}
		}
		my $titleresolution = "$pubdate [EP#$episode] - $subtitle";
		printf "DEBUG:builditems:ptr = [%s]\n", $titleresolution;
		push(@{$diritemsref->{$dir}}, {
			'dirthumbnail' => $thumbnail,
			'title' => $titleresolution,
			'mythtv:subtitle' => "",
			'author' => $author,
			'pubDate' => $pubdate,
			'description' => $description,
			'link' => $raw_episode,
			'player' => mnvgetconfig('player'),
			'playerargs' => mnvgetconfig('playerargs'),
			'download' => mnvgetconfig('download'),
			'downloadargs' => mnvgetconfig('downloadargs'),
			'media:thumbnailurl' => $thumbnail,
			'media:contenturl' => $contenturl,
			'media:contentlength' => "",
			'media:contentduration' => $duration,
			'media:contentwidth' => $width,
			'media:contentheight' => $height,
			'media:contentlanguage' => "eng",
			'rating' => "",
			'mythtv:country' => "usa",
			'mythtv:season' => "",
			'mythtv:episode' => $episode,
			'mythtv:customhtml' => "no"
		});
		$count ++;
	}

#	my $link_count=0;
#	my @links;
#	my @as = $tree->find_by_tag_name('a');
#	foreach my $a (@as) {
#		#$a->as_trimmed_text() =~ /Download Video/ or next;
#		#$a->attr('href') =~ /^http:.*video.*mp4$/ and push(@links, mnvcleantext($a->attr('href')));
#		if ($a->attr('href') =~ /^http:.*video.*mp4$/)
##			#printf "DEBUG:builditems:%s\n", $a->attr('href');
#			push(@links, mnvcleantext($a->attr('href')));
#		$link_count++;			
#		}
#		if($link_count == 1)
#		{
#		  last;
#		}
#	}
#	(@links) or return 0;
#	my $title = ""; my $pubDate = ""; my $desc = ""; my $subtitle = "";#
#
#	my @ptrs;
#	my $tmp = $tree->look_down('class', 'media-bd');
#	($tmp) and push(@ptrs, $tmp);
#
#	# get title/pubdate/subtitle/description
#foreach my $t (@ptrs) {
#	#my $text = $t->as_trimmed_text();
#	#printf "DEBUG:builditems:text = %s\n", $text;
#	my @as = $t->find_by_tag_name('p');
#	foreach my $a (@as) {
#		$pubDate = $a->as_trimmed_text();
#		# format pubDate from [monthname day year] to [year/month/day]
#		my $epoch = str2time($pubDate);
#		my $dt = DateTime->from_epoch(epoch => $epoch);
#		my $format = '%Y/%m/%d';
#		my $timestamp = $dt->strftime($format);
#		$pubDate = $timestamp;
#		printf "DEBUG:builditems:pubDate = %s\n", $pubDate;
#	}
#	@as = $t->find_by_tag_name('h1');
#	foreach my $a (@as) {
#		$title = HTML::Entities::encode($a->as_trimmed_text());
#		#printf "DEBUG:builditems:title = %s\n", $title;
#	}
#	@as = $t->find_by_tag_name('h2');
#	foreach my $a (@as) {
#		$subtitle = HTML::Entities::encode($a->as_trimmed_text());
#		#printf "DEBUG:builditems:subtitle = %s\n", $subtitle;
#	}
#}
#	my $html = $tmp->as_HTML();
#	printf "DEBUG:builditems:html = %s\n", $html;
#
#	my @elements = $tree->look_down('class', 'media-bd')->find_by_tag_name('div');
#	my $elements_size = @elements;
#	if ( $elements_size != 0 ) {
#		my $element_index = ($elements_size >= 2) ? 2 : 1;
#		if ( $elements[$element_index] )
#		{
#		  $desc = HTML::Entities::encode($elements[$element_index]->as_trimmed_text());
#		  printf "DEBUG:builditems:description = %s\n", $desc;
#		}
#	}
#	my $ptr;
#	foreach my $tmp (@ptrs) {
#		my $testurl = $tmp->attr('href');
#		$url =~ /http:.*${testurl}/ and $ptr = $tmp and last;
#
#	}
#	if ($ptr) {
#		$title = mnvcleantext($ptr->attr('title'));
#		$ptr = $ptr->parent();
#		my $ptr2 = $ptr->look_down('class', 'podcast-date');
#		if ($ptr2) {
#			my $time = str2time($ptr2->as_trimmed_text());
#			$pubDate = time2str("%a, %d %b %Y 00:00:00 GMT", $time);
#			$pubDate = mnvcleantext($pubDate);
#		}
#		$ptr2 = $ptr->find_by_tag_name('p');
#		($ptr2) and $desc = mnvcleantext($ptr2->as_trimmed_text());#
#	}
#	($title) or return 0;
#
#	my $icon = $baseicon;
#	$ptr = $tree->look_down('class', 'imagecache imagecache-coverart');
#	($ptr) and $icon = mnvcleantext($ptr->attr('src'));
#
#	my $duration = "0";
#	$ptr = $tree->look_down('class', 'running-time');
#	($ptr) and my $tmpdur = $ptr->as_trimmed_text();
#	if ($tmpdur =~ s/Running time: //) {
#		my $hours = 0; my $mins = 0; my $secs = 0;
#		my $count = $tmpdur =~ s/(:)/$1/g;
#		if ($count == 1) {
#			($mins, $secs) = split(':', $tmpdur);
#		} elsif ($count == 2) {
#			($hours, $mins, $secs) = split(':', $tmpdur);
#		} else {
#			goto NODURATION;
#		}
#		$tmpdur = ($hours * 60 * 60) + ($mins * 60) + $secs;
#		($tmpdur > 0) and $duration = mnvcleantext($tmpdur);
#	}
#NODURATION:
#
#	my $country = "";
#
#$link_count=0;
#	my $count = 0;
#	foreach my $contenturl (@links) {
#                my $tmp_contenturl = $contenturl;
#		my ($width, $height, $titleresolution);
#		$tmp_contenturl =~ /_(\d\d\d\d)x(\d\d\d)_/;
#		if ($1 && $2) {
#			$width = $1;
#			$height = $2;
#		}
#		else
#		{
#			$tmp_contenturl = $contenturl;
#			$tmp_contenturl =~ /_(\d\d\d)x(\d\d\d)_/;
#			if ($1 && $2) {
#				$width = $1;
#				$height = $2;
#			}
#		}
#		
#		$titleresolution = "$pubDate - $dir - $desc";
#		if (mnvgetconfig('resolution')) {
#			$titleresolution = $title;
#			if ((mnvgetconfig('resolution') eq "high") && ($width < 750)) {
#				mnvinfomsg(1, "Skipping $contenturl due to wrong resolution");
#				next;
#			}
#			if ((mnvgetconfig('resolution') eq "low") && ($width >= 750)) {
#				mnvinfomsg(1, "Skipping $contenturl due to wrong resolution");
#				next;
#			}
#		}
#		my $link = $url;
#		if ((mnvgetconfig('netplayer')) && ($contenturl)) {
#			if (mnvistype(mnvgetconfig('netplayertype'), $contenturl)) {
#				my $encodedtitle = decode_entities($title);
#				$encodedtitle = mnvURLEncode($encodedtitle);
#				$link = mnvcleantext(mnvgetconfig('netplayer')."?title=${encodedtitle}&videofile=").$contenturl;
#			} else {
#				mnvinfomsg(1, "Not ".mnvgetconfig('netplayertype')." $contenturl");
#			}
#		}
#		push(@{$diritemsref->{$dir}}, {
#			'dirthumbnail' => $icon,
#			'title' => $titleresolution,
#			'mythtv:subtitle' => "",
#			'author' => "twit.tv",
#			'pubDate' => $pubDate,
#			'description' => $desc,
#			'link' => $link,
#			'player' => mnvgetconfig('player'),
#			'playerargs' => mnvgetconfig('playerargs'),
#			'download' => mnvgetconfig('download'),
#			'downloadargs' => mnvgetconfig('downloadargs'),
#			'media:thumbnailurl' => $icon,
#			'media:contenturl' => $contenturl,
#			'media:contentlength' => "",
#			'media:contentduration' => $duration,
#			'media:contentwidth' => $width,
#			'media:contentheight' => $height,
#			'media:contentlanguage' => "",
#			'rating' => "",
#			'mythtv:country' => $country,
#			'mythtv:season' => "",
#			'mythtv:episode' => "",
#			'mythtv:customhtml' => "no"
#		});
#
#		mnvinfomsg(2, "Added $title");
#		$count ++;
#	}
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
printf "DEBUG: calling builddiritems\n";
	$totalitems = builddiritems($diritemsref, $feedsurl);
printf "DEBUG: called builddiritems : totalitems = %d\n", $totalitems;

printf "DEBUG: storing items\n";
	eval { store($diritemsref, $store); };
	if ($@) {
		warn "Could not save store, $@";
	}
printf "DEBUG: stored items\n";
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
