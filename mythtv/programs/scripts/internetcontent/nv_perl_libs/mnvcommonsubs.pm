# @(#)$Header: /home/mythtv/mythtvrep/scripts/mnvcommonsubs.pm,v 1.9 2010/07/17 09:06:20 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MythNetvision Grabber Script utility subs
#
#################################### Util Subs ############################################

package mnvcommonsubs;

use strict;
use warnings;
use HTML::Entities;
use Encode;
use LWP::Simple;
use File::stat;
use File::Basename;
use Sys::Hostname;
# mythbackend sets HOME and MYTHCONFDIR to /var/lib/mythtv.
# So have to hack it back to the real home for require MythTV to work.
$ENV{'HOME'} = (getpwuid $>)[7];
delete $ENV{'MYTHCONFDIR'};
require MythTV;
use Image::Magick;
use File::Copy;

our(@ISA, @EXPORT, @EXPORT_OK, $VERSION);
$VERSION = '$Revision: 1.9 $'; $VERSION =~ s/\D*([\d\.]+)\D*/$1/; # rcs tag populated

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(mnvURLEncode mnvprintversion mnvcleanexit mnvinfomsg mnvcleantext mnvnumresults mnvfilter
 mnvprinttree mnvprintsearch mnvrssitem2mnv mnvistype mnvgetsetting mnvgeticonDir mnvrssheader mnvrssfooter
 mnvgetflashplayerurl mnvloadconfig mnvgetconfig mnvsetconfig);
@EXPORT_OK = qw();  # symbols to export on request

my $mythobj;
eval { $mythobj = MythTV->new(); };
($@) and die("Could not create MythTV-new() $@"); 

# Netvision version access as $mnvcommonsubs::netvisionver
our $netvisionver = mnvgetnetvisionver();

# Get a mnv configuration
# Default config values. These can be overridded by mnvsetconfig
our %config = (
	'mnvinfo' => 0,
	'mnvinfoop' => "",
	'player' => "",
	'playerargs' => "",
	'download' => "",
	'downloadargs' => "",
	# A network player like a flash or html5 html. TODO 0.24 May not be approved
	'netplayer' => "",
	'netplayertype' => "",
	# Seconds to cache results
	'cachetime' => 72000,
	'defaultautoplay' => 'autostart=1&autoplay=1'
);

sub mnvgetconfig {
	my $value = shift @_;

	return $config{$value};
}

# Set a mnv configuration
sub mnvsetconfig {
	my $value = shift @_;
	my $data = shift @_;

	if (ref($config{$value}) eq "ARRAY") {
		push(@{$config{$value}}, $data);
	} else {
		$config{$value} = $data;
	}
}

# Load a mnv configuration file
# $ENV{'HOME'}/.mythtv/MythNetvision/userGrabberPrefs/${configfile}.cfg
# INI Format
# # Default
# player=mplayer
# feedurl+url1 (Creates a array feedurl of all the entries)
# feedurl+url2
# # Section Specific
# [Section A]
# player=vlc
# [Section B]
# player=xine
sub mnvloadconfig {
	my $configfile = shift @_;
	my $section = shift @_;

	my $cfile = "/$ENV{'HOME'}/.mythtv/MythNetvision/userGrabberPrefs/${configfile}.cfg";
	my $ss = stat($cfile);
	if ($ss) {
		open(CF, "<$cfile") or warn "Could not load $cfile";
		my $in = 1; # In default section
		while(<CF>) {
			chomp;
			/^\s*#/ and next;
			s/\s*=\s*/=/g;
			if ($in == 1 && /=/) {
				my ($value, @data) = split('=', $_);
				$config{$value} = join('=', @data);
				next;
			}
			s/\s*\+\s*/\+/g;
			if ($in == 1 && /\+/) {
				my ($value, @data) = split('\+', $_);
				push(@{$config{$value}}, join('+', @data));
				next;
			}
			if (/^\s[(.*)]\s$/) {
				if ($1 eq $section) {
					$in = 1;
				} else {
					$in = 0;
				}
			}
		}
		close(CF);
	}
}

# Get a mythdb setting
sub mnvgetsetting {
	my $setting = shift @_;
	my $hostname = shift @_;

	my $data;
	if ($hostname) { 
		$data = $mythobj->backend_setting($setting, $hostname);
	} else {
		$data = $mythobj->backend_setting($setting);
	}

	return $data;
}

# Find netvision version
sub mnvgetnetvisionver {

	my $netvisionver = "NA";
	my $printerror = $mythobj->{'dbh'}->{'PrintError'};
	my $printwarn = $mythobj->{'dbh'}->{'PrintWarn'};
	$mythobj->{'dbh'}->{'PrintError'} = 0;
	$mythobj->{'dbh'}->{'PrintWarn'} = 0;
	eval { $mythobj->{'dbh'}->do("select count(*) from internetcontent where 1=0") };
	unless ($mythobj->{'dbh'}->err) {
		$netvisionver = "24";
	} else {
		eval { $mythobj->{'dbh'}->do("select count(*) from netvisiontreegrabbers where 1=0") };
		unless ($mythobj->{'dbh'}->err) {
			$netvisionver = "23";
		}
	}
	$mythobj->{'dbh'}->{'PrintError'} = $printerror;
	$mythobj->{'dbh'}->{'PrintWarn'} = $printwarn;

	return $netvisionver;
}

# Find netvision icon directory
sub mnvgeticonDir {

	# 0.23 mythnetvision.iconDir. 0.24 %SHAREDIR%
	my $icondir = mnvgetsetting('mythnetvision.iconDir', hostname());
	($icondir) and return $icondir;
	return "%SHAREDIR%/mythnetvision/icons";
}

# Encode a string
sub mnvURLEncode {
	my $theURL = shift @_;

	$theURL =~ s/([\W])/"%" . uc(sprintf("%2.2x",ord($1)))/eg;
	return $theURL;
}

# print or return a typical <rss...> header
# 
# in list context, return a list of lines with the header in it
# in scalar context, return as a string
# in void context, print the header to stdout
#
sub mnvrssheader {

	my $header='<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/"
xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">
';
 
	if(wantarray()) {
		return split(/\n/,$header);
	} elsif(defined wantarray()) {
		return $header;
	} else {
		print $header;
	}
}
 
# print or return a typical </rss> footer to close out an rss block
# 
# in list context, return a list of lines with the footer in it
# in scalar context, return as a string
# in void context, print the header to stdout
sub mnvrssfooter {

	my $footer="</rss>\n";
	if(wantarray()) {
		 return split(/\n/,$footer);
	} elsif(defined wantarray()) {
		return $footer;
	} else {
		print $footer;
	}
}

sub mnvprintversion {
	my $site = shift @_;
	my $command = shift @_;
	my $author = shift @_;
	my $commandthumbnail = shift @_;
	my $version = shift @_;

	print '<grabber>'."\n";
	print '  <name>'.$site.'</name>'."\n";
	print '  <command>'.$command.'</command>'."\n";
	print '  <author>'.$author.'</author>'."\n";
	print '  <thumbnail>'.$commandthumbnail.'</thumbnail>'."\n";
	print '  <type>video</type>'."\n";
	print '  <description>MythNetVision grabber for '.$site.'</description>'."\n";
	print '  <version>'.$version.'</version>'."\n";
	print '  <search>true</search>'."\n";
	print '  <tree>true</tree>'."\n";
	print '</grabber>'."\n";
}

# Download a icon from url
sub downloadicon {
	my $url = shift @_;
	my $name = shift @_;
	my $icondir = shift @_;

	# Only works for 0.23
	#my $icondir = mnvgeticonDir();

	my $ss = stat("${icondir}/${name}");
	($ss) and return "${icondir}/${name}";

	my @imgexts = ('jpg', 'jpeg', 'png', 'gif');
	my $nameext = (fileparse($name, @imgexts))[2];
	my $urlext = (fileparse($url, @imgexts))[2];

	mnvinfomsg(1, "Getting $url");
	my $content = get($url);
	unless ($content) {
		die "Could not retrieve $url";
	}

	open(IC, ">${icondir}/.tmp") or return 0;
	print IC $content;
	close(IC);

	if ($urlext eq $nameext) {
		copy("${icondir}/.tmp", "${icondir}/${name}");
	} else {
		my $image=Image::Magick->new;
		$image->Read(filename=>"${icondir}/.tmp");
		$image->Write(filename=>"${icondir}/${name}");
	}
	unlink("${icondir}/.tmp");

	mnvinfomsg(1, "Saved icon ${icondir}/${name}");
	return "${icondir}/${name}";
}

sub mnvcleanexit {
	my $esig = shift @_;

	fileno(FH) and close(FH);
	if ($esig =~ /\D/) {
		# called by signalhandler
		exit 1;
	} else {
		exit $esig;
	}
}

# Debug/Info message
# $config{'mnvinfo'} specifies level of messages to print
# $config{'mnvinfoop'} specifies o/p file
sub mnvinfomsg {
	my $level = shift @_;
	my $mesg = shift @_;

	($config{'mnvinfo'} < $level) and return;
	$mesg = encode('utf8', $mesg);
	if ($config{'mnvinfoop'} =~ /\D/) {
		open(FH, ">$config{'mnvinfoop'}") unless fileno(FH);
		my $t = localtime();
		print FH "$t $level $mesg\n";
	} else {
		print STDERR "$mesg\n";
	}
}

# Clean and encode a string
sub mnvcleantext {
	my $text = shift @_;

	($text) or return;
	$text =~ s/\n/ /g;
	$text =~ s/^\s+|\s+$//g;
	$text = encode_entities($text);
	return $text;
}

# Number of items found
sub mnvnumresults {
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
sub mnvfilter {
	my $diritemsref = shift @_;
	my $search = shift @_;

	my $filtereditems = 0;

	foreach my $dir (keys(%$diritemsref)) {
		for (my $c = 0; $c <= $#{$diritemsref->{$dir}}; $c++) {
			if (${$diritemsref->{$dir}}[$c]->{'title'} =~ /$search/i || ${$diritemsref->{$dir}}[$c]->{'author'} =~ /$search/i ||
			${$diritemsref->{$dir}}[$c]->{'description'} =~ /$search/i || ${$diritemsref->{$dir}}[$c]->{'link'} =~ /$search/i) {
				$filtereditems++;
				mnvinfomsg(2, "Saved $dir ".${$diritemsref->{$dir}}[$c]->{'title'});
			} else {
				mnvinfomsg(2, "Deleted $dir ".${$diritemsref->{$dir}}[$c]->{'title'});
				delete(${$diritemsref->{$dir}}[$c]);
			}
		}
	}
	return $filtereditems;
}

# Print a item
sub printitem {
	my $item = shift @_;

	print "      ".'<item>'."\n";
	print "        ".'<title>'.$item->{'title'}.'</title>'."\n";
	print "        ".'<mythtv:subtitle>'.$item->{'mythtv:subtitle'}.'</mythtv:subtitle>'."\n";
	print "        ".'<author>'.$item->{'author'}.'</author>'."\n";
	print "        ".'<pubDate>'.$item->{'pubDate'}.'</pubDate>'."\n";
	print "        ".'<description>'.$item->{'description'}.'</description>'."\n";
	print "        ".'<link>'.$item->{'link'}.'</link>'."\n";
	print "        ".'<player>'.$item->{'player'}.'</player>'."\n";
	print "        ".'<playerargs>'.$item->{'playerargs'}.'</playerargs>'."\n";
	print "        ".'<download>'.$item->{'download'}.'</download>'."\n";
	print "        ".'<downloadargs>'.$item->{'downloadargs'}.'</downloadargs>'."\n";
	print "        ".'<media:group>'."\n";
	print "          ".'<media:thumbnail url="'.$item->{'media:thumbnailurl'}.'"/>'."\n";
	print "          ".'<media:content url="'.$item->{'media:contenturl'}.'" length="'.$item->{'media:contentlength'}.'" duration="'.$item->{'media:contentduration'}.'" width="'.$item->{'media:contentwidth'}.'" height="'.$item->{'media:contentheight'}.'" lang="'.$item->{'media:contentlanguage'}.'"/>'."\n";
	print "        ".'</media:group>'."\n";
	print "        ".'<rating>'.$item->{'rating'}.'</rating>'."\n";
	print "        ".'<mythtv:country>'.$item->{'mythtv:country'}.'</mythtv:country>'."\n";
	print "        ".'<mythtv:season>'.$item->{'mythtv:season'}.'</mythtv:season>'."\n";
	print "        ".'<mythtv:episode>'.$item->{'mythtv:episode'}.'</mythtv:episode>'."\n";
	print "        ".'<mythtv:customhtml>'.$item->{'mythtv:customhtml'}.'</mythtv:customhtml>'."\n";
	print "      ".'</item>'."\n";
	return 1;
}

# print tree
# input hash ref produced by builditems
sub mnvprinttree {
	my $diritemsref = shift @_;
	my $mostrecentnumber = shift @_;

	my $icondir = mnvgeticonDir();

	foreach my $dir (sort(keys(%$diritemsref))) {
		(@{$diritemsref->{$dir}}) or next;
		my $dirprinted = 0;
		if ($mostrecentnumber) {
			my $printcount = 0;
			foreach my $item (@{$diritemsref->{$dir}}) {
				($item) or next; # somewhere I get null references.
				if ($dirprinted == 0) {
					$dirprinted = 1;
					# Need it here as don't have $item outside loop.
					print "    ".'<directory name="'.mnvcleantext($dir).'" thumbnail="'.$item->{'dirthumbnail'}.'">'."\n";
					print "    ".'<directory name="Most Recent" thumbnail="'.$icondir.'/directories/topics/most_recent.png">'."\n";
				}
				printitem($item);
				$printcount++;
				($printcount >= $mostrecentnumber) and last;
			}
			print "    ".'</directory>'."\n";
		}
		foreach my $item (@{$diritemsref->{$dir}}) {
			($item) or next; # somewhere I get null references.
			if ($dirprinted == 0) {
				$dirprinted = 1;
				# Need it here as don't have $item outside loop.
				print "    ".'<directory name="'.mnvcleantext($dir).'" thumbnail="'.$item->{'dirthumbnail'}.'">'."\n";
			}
			printitem($item);
		}
		print "    ".'</directory>'."\n";
	}
}

# print search
# input hash ref produced by builditems
sub mnvprintsearch {
	my $diritemsref = shift @_;
	my $page = shift @_;

	my $numresults = mnvnumresults($diritemsref);
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
			printitem($item);
			($ecount >= $returned) and last L1;
		}
	}
}

# Parse a rss item.
# input default icon
# input rss->parse item
# output ref to %mnvitem
sub mnvrssitem2mnv {
	my $icon = shift @_;
	my $item = shift @_;

	my %mnvitem = (
	'title' => "",
	'weburl' => "",
	'contenturl' => "",
	'author' => "",
	'pubDate' => "",
	'thumbnail' => "",
	'description' => "",
	'length' => "",
	'duration' => "",
	'rating' => "",
	'country' => ""
	);

	#print Dumper($item);

	($item->{'title'}) or return 0;
	$mnvitem{'title'} = mnvcleantext($item->{'title'});

	($item->{'link'}) and $mnvitem{'weburl'} = mnvcleantext($item->{'link'});
	($item->{'enclosure'}->{'url'}) and $mnvitem{'contenturl'} = mnvcleantext($item->{'enclosure'}->{'url'});
	(($mnvitem{'weburl'}) || ($mnvitem{'contenturl'})) or return 0;
	($mnvitem{'contenturl'}) or $mnvitem{'contenturl'} = $mnvitem{'weburl'};

	if ($item->{'itunes'}->{'author'}) {
		$mnvitem{'author'} = mnvcleantext($item->{'itunes'}->{'author'});
	} elsif ($item->{'dc'}->{'creator'}) {
		$mnvitem{'author'} = mnvcleantext($item->{'dc'}->{'creator'});
	} elsif ($item->{'author'}) {
		$mnvitem{'author'} = mnvcleantext($item->{'author'});
	}

	($item->{'pubDate'}) and $mnvitem{'pubDate'} = mnvcleantext($item->{'pubDate'});

	$mnvitem{'thumbnail'} = $icon;
	if ($item->{'thumbnail'}) {
		$mnvitem{'thumbnail'} = mnvcleantext($item->{'thumbnail'});
	} elsif ($item->{'http://blip.tv/dtd/blip/1.0'}->{'smallThumbnail'}) {
		$mnvitem{'thumbnail'} = mnvcleantext($item->{'http://blip.tv/dtd/blip/1.0'}->{'smallThumbnail'});
	} elsif ($item->{'media'}->{'thumbnail'}) {
		# To get this need to hack xml before  $rss->parse with following 
		# $content =~ s/<media:thumbnail\s+url=\"([^\"]*)\"\s*\/>/<media:thumbnail>$1<\/media:thumbnail>/g;
		$mnvitem{'thumbnail'} = mnvcleantext($item->{'media'}->{'thumbnail'});
	} elsif ($item->{'description'}) {
		# Sometimes hide thumbernail in description.
		foreach my $line (split('\n', $item->{'description'})) {
			$line =~ s/.*src=\"(http:.*\.[jpg][pni][gf])\".*/$1/i and $mnvitem{'thumbnail'} = $line and last;
		}
	}

	if ($item->{'media'}->{'description'}) {
		$mnvitem{'description'} = $item->{'media'}->{'description'};
	} elsif ($item->{'itunes'}->{'summary'}) {
		$mnvitem{'description'} = $item->{'itunes'}->{'summary'};
	} elsif ($item->{'description'}) {
		$mnvitem{'description'} = $item->{'description'};
	}
	if ($mnvitem{'description'}) {
		my $scrubber = HTML::Scrubber->new;
		$scrubber->default(0);
		$mnvitem{'description'} = $scrubber->scrub($mnvitem{'description'});
		$mnvitem{'description'} = mnvcleantext($mnvitem{'description'});
		($mnvitem{'description'}) or $mnvitem{'description'} = "";
		$mnvitem{'description'} =~ s/\&nbsp;/ /g;
	}

	($item->{'enclosure'}->{'length'}) and $mnvitem{'length'} = mnvcleantext($item->{'enclosure'}->{'length'});
	if (($mnvitem{'length'}) && $mnvitem{'length'} < 5000) {
		$mnvitem{'duration'} = $mnvitem{'length'};
		$mnvitem{'length'} = "";
	}
	if ($item->{'itunes'}->{'duration'}) {
		my $tmpdur = mnvcleantext($item->{'itunes'}->{'duration'});
		my $count = $tmpdur =~ s/(:)/$1/g;
		if ($count == 1) {
			my ($mins, $secs) = split(':', $tmpdur);
			(($mins =~ /^\d+$/) && ($secs =~ /^\d+$/)) and $tmpdur = ($mins * 60) + $secs;
		} elsif ($count == 2) {
			my ($hours, $mins, $secs) = split(':', $tmpdur);
			(($hours =~ /^\d+$/) && ($mins =~ /^\d+$/) && ($secs =~ /^\d+$/)) and $tmpdur = ($hours * 60 * 60) + ($mins * 60) + $secs;
		}
		(($tmpdur =~ /^\d+$/) && $tmpdur > 0) and $mnvitem{'duration'} = $tmpdur;
	}

	if ($item->{'media'}->{'restriction'}) {
		$mnvitem{'country'} = mnvcleantext($item->{'media'}->{'restriction'});
	}

	return \%mnvitem;
}

# return media type (Not very accurate).
sub mnvistype {
	my $type = shift @_;
	my $file = shift @_;

	$file =~ s/redirect\.mp3//g;

	if ($type eq "flash") {
		$file =~ /\.flv/i and return 1;
		$file =~ /\.m[op4][34v]/i and return 1;
	} elsif ($type eq "html5") {
		$file =~ /\.m[op4][4v]/i and return 1;
		$file =~ /\.ogm/i and return 1;
		$file =~ /\.ogg/i and return 1;
	} elsif ($type eq "mp4html5") {
		$file =~ /\.m[op4][4v]/i and return 1;
	} elsif ($type eq "ogghtml5") {
		$file =~ /\.ogm/i and return 1;
		$file =~ /\.ogg/i and return 1;
	} elsif ($type eq "video") {
		$file =~ /\.flv/i and return 1;
		$file =~ /\.m[op4][4v]/i and return 1;
		$file =~ /\.mpg/i and return 1;
		$file =~ /\.mpeg/i and return 1;
		$file =~ /\.mkv/i and return 1;
		$file =~ /\.avi/i and return 1;
		$file =~ /\.wmv/i and return 1;
		$file =~ /\.as[xf]/i and return 1;
		$file =~ /\.divx/i and return 1;
		$file =~ /\.ogm/i and return 1;
	} elsif ($type eq "audio") {
		$file =~ /\.m4[ab]/i and return 1;
		$file =~ /\.mp[23]/i and return 1;
		$file =~ /\.og[ag]/i and return 1;
		$file =~ /\.aac/i and return 1;
		$file =~ /\.ram/i and return 1;
		$file =~ /\.rm/i and return 1;
		$file =~ /\.wav/i and return 1;
		$file =~ /\.flac/i and return 1;
		$file =~ /\.mka/i and return 1;
		$file =~ /\.au/i and return 1;
	}
	return 0;
}

1;
