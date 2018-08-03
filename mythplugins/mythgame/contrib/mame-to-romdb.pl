#!/usr/bin/perl -w

use strict;

use utf8;
use XML::Twig;

if (!$ARGV[0])
{
    die("MAME xml file not found.");
}

my $twig = new XML::Twig;
$twig->parsefile($ARGV[0]);

my $mame = $twig->root();
$mame->att('build') =~ /^[0-9.]+/;
my $version = $&;

sub insert_sql
{
    my ($game, $crc, $description, $category, $year, $manufacturer, $size, $file) = @_;

    if (! $year)
    {
	$year = "????";
    }

    $description =~ s/"/\\"/g;

    print "INSERT INTO romdb VALUES ('" . $crc . "'," . '"' . $game . '"' . "," . '"' . $description . '"' . ",'" .
		    $category . "','" . $year . "'," . '"' . $manufacturer . '"'. ",'" .
		    "Unknown','" . "Unknown','MAME'," .
		    $size . ",'','" . $version . "','" . $file . "');\n";
    return;
}

binmode(STDOUT, ":utf8");
foreach my $machine ($mame->children('machine'))
{
    next if ($machine->att('isbios') eq "yes");
    next if ($machine->att('isdevice') eq "yes");

    my $game = $machine->att('name');
    my $description = $machine->first_child('description')->text();
    my $category;
    if ($machine->first_child('driver')->att('status') eq 'good')
    {
        $category="Players " . $machine->first_child('input')->att('players');
    }
    else
    {
        $category='Imperfect';
    }

    my $year = $machine->first_child('year')->text();
    my $manufacturer = $machine->first_child('manufacturer')->text();
    foreach my $rom ($machine->children('rom')) {
        next if not $rom->att('crc');
        insert_sql($game, $rom->att('crc'), $description, $category, $year,
            $manufacturer, $rom->att('size'), $rom->att('name'));
    }
}
