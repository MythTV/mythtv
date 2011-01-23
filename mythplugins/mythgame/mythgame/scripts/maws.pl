#!/usr/bin/env perl
# @(#)$Header: /home/mythtv/mythtvrep/scripts/maws.pl,v 1.3 2010/06/04 07:48:49 mythtv Exp $
# Auric 2010/01/10 http://web.aanet.com.au/auric/
#
# MAWS metadata Grabber Script
#
################################################################################
use strict;
use warnings;
use Getopt::Std;
use LWP::Simple;
use HTML::TreeBuilder;
use HTML::Entities;
use Data::Dumper;
use Encode;

#################################### Settings #################################
my $info = 0; # print info/progress message: 0 - off, 1 - low ,2 - high
my $infoop = 0; # info messages go to: 0 = stderr, filename = filename

#################################### Globals ##################################
my $site = 'MAWS';
my $baseurl = 'http://maws.mameworld.info';
my $searchurl = $baseurl . "/maws/srch.php?search_text=";
my $header = '<?xml version="1.0" encoding="UTF-8"?>
<metadata>';
my $footer = '</metadata>';
my $version = '<grabber>
  <name>MAWS MAME Database</name>
  <author>Auric</author>
  <thumbnail>maws.png</thumbnail>
  <command>maws.pl</command>
  <type>games</type>
  <description>MAWS is a MAME information and aggregation site.</description>
  <version>0.01</version>
</grabber>';
our ($opt_M, $opt_D, $opt_v);
my @metaitems;
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

sub printitems {
	
	foreach my $i (@metaitems) {
		my %item = %{$i};
		print "  ".'<item>'."\n";
		print "    ".'<title>'.$item{'title'}.'</title>'."\n";
		print "    ".'<inetref>'.$item{'inetref'}.'</inetref>'."\n";
		print "    ".'<description>'.$item{'description'}.'</description>'."\n";
		print "    ".'<system>MAME</system>'."\n";
		print "    ".'<categories>'."\n";
		print "      ".'<category type="genre" name="'.$item{'genre'}.'"/>'."\n";
		print "    ".'</categories>'."\n";
		print "    ".'<studios>'."\n";
		print "      ".'<studio name="'.$item{'studio'}.'"/>'."\n";
		print "    ".'</studios>'."\n";
		print "    ".'<year>'.$item{'year'}.'</year>'."\n";
		print "    ".'<images>'."\n";
        print "      ".'<image type="screenshot" thumb="'.$item{'screenshotthumb'}.'" url="'.$item{'screenshoturl'}.'"/>'."\n";
        print "      ".'<image type="coverart" thumb="'.$item{'coverthumb'}.'" url="'.$item{'coverurl'}.'"/>'."\n";
		print "    ".'</images>'."\n";
		print "    ".'<popularity>'.$item{'popularity'}.'</popularity>'."\n";
		print "  ".'</item>'."\n";
	}
}

sub printversion {
        print "$version\n";
}

#################################### Site Specific Subs ##########################
sub search {
	my $searchstr = shift @_;

	my $content = get(${searchurl} . ${searchstr});
	unless ($content) {
		die "Could not retrieve ${searchurl}${searchstr}";
	}
	my $tree = HTML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		die "$searchurl parse failed, $@";
	}
	$tree->eof();

	my @trs = $tree->find_by_tag_name('tr');
	foreach my $tr (@trs) {
		my @as = $tr->find_by_tag_name('a');
		my $count = 0;
		foreach my $a (@as) {
			($count++ < 2) && next;
			($a->as_trimmed_text()) || last;
			$a->attr('href') =~ /romset/ || last;
			my $title = cleantext($a->as_trimmed_text());
			my $inetref = $a->attr('href');
			$inetref =~ s/\#.*$//;
			$inetref =~ s/^\/maws\///;
			$inetref = cleantext($baseurl . "/maws/" . $inetref);
			push(@metaitems, {
			'title' => $title,
			'inetref' => $inetref,
			'description' => "",
			'genre' => "",
			'studio' => "",
			'year' => "",
			'coverthumb' => "",
			'coverurl' => "",
			'screenshotthumb' => "",
			'screenshoturl' => "",
			'popularity' => ""
			});
			last;
        }
	}
	return 0;
}

sub queryinetref {
	my $inetref = shift @_;

	my $content = get(${inetref});
	unless ($content) {
		die "Could not retrieve ${inetref}";
	}
	my $tree = HTML::TreeBuilder->new;
	eval { $tree->parse($content); };
	if ($@) {
		die "$inetref parse failed, $@";
	}
	$tree->eof();

	my $title = "";
	my $description = "";
	my $genre = "";
	my $studio = "";
	my $year = "";
	my $coverthumb = "";
	my $coverurl = "";
	my $screenshoturl = "";
	my $screenshotthumb = "";
	my $popularity = "";
	my @trs = $tree->find_by_tag_name('tr');
	foreach my $tr (@trs) {
		my @tds = $tr->find_by_tag_name('td');
		foreach my $td (@tds) {
			if ($td->as_trimmed_text() eq "title" ) {
				my $right = $td->right();
				($right) and $title = cleantext($right->as_trimmed_text());
			} elsif ($td->as_trimmed_text() eq "manufacturer" ) {
				my $right = $td->right();
				($right) and $studio = cleantext($right->as_trimmed_text());
			} elsif ($td->as_trimmed_text() eq "year" ) {
				my $right = $td->right();
				($right) and $year = cleantext($right->as_trimmed_text());
			} elsif ($td->as_trimmed_text() eq "genre" ) {
				my $right = $td->right();
				($right) and $genre = cleantext($right->as_trimmed_text());
			} elsif ($td->as_trimmed_text() eq "snapshots" ) {
				my $right = $td->right();
				my @as = $tree->find_by_tag_name('a');
				foreach my $a (@as) {
					if ((!$screenshoturl) && $a->as_trimmed_text() =~ /in game/) {
						if ($a->attr('onClick')) {
							$screenshoturl = $a->attr('onClick');
							$screenshoturl =~ s/.*\'(.*)\'.*/$1/;
							$screenshoturl =~ s/^\.\.//;
							$screenshoturl =~ s/^\///;
							$screenshoturl = cleantext($baseurl . "/" . $screenshoturl);
							# Making thumb same as they are small.
							#$screenshotthumb = $screenshoturl;
						}
					}
					if ($a->as_trimmed_text() =~ /flyer/) {
						$coverurl = cleantext($baseurl . $a->attr('href'));
						# Making thumb same as they are small.
						#$coverthumb = $coverurl;
					}
				}
			} elsif ($td->as_trimmed_text() eq "rating" ) {
				my $right = $td->right();
				my $d = $right->find_by_tag_name('div');
				if ($d->attr('title')) {
					$popularity = $d->attr('title');
					$popularity =~ s/(.*)%.*/$1/;
					$popularity = int(($popularity / 10) + 0.5);
				}
			}
		}
	}
	push(@metaitems, {
	'title' => $title,
	'inetref' => $inetref,
	'description' => $description,
	'genre' => $genre,
	'studio' => $studio,
	'year' => $year,
	'coverthumb' => $coverthumb,
	'coverurl' => $coverurl,
	'screenshotthumb' => $screenshotthumb,
	'screenshoturl' => $screenshoturl,
	'popularity' => $popularity
	});
	return 0;
}

#################################### Main #####################################
getopts('vM:D:');

unless (($opt_M) || ($opt_D) || ($opt_v)){
	print "Error must have either -M search str or -D inetref\n";
	cleanexit 1;
}

$SIG{'INT'} = \&cleanexit;
$SIG{'HUP'} = \&cleanexit;
$SIG{'TERM'} = \&cleanexit;
$SIG{'QUIT'} = \&cleanexit;

if ($opt_M) {
	search($opt_M);
        print "$header\n";
	printitems();
        print "$footer\n";
} elsif ($opt_D) {
	queryinetref($opt_D);
        print "$header\n";
	printitems();
        print "$footer\n";
} elsif ($opt_v) {
        printversion();
}

cleanexit 0;
