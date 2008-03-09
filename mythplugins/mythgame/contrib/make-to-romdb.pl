#!/usr/bin/perl -w

use strict;

use XML::Simple;
use Date::Manip;

Date_Init();

if (!$ARGV[0])
{
    die("Not exist mame xml");
}


my $xml = new XML::Simple;

my $data = $xml->XMLin($ARGV[0]);

$data->{build} =~ /\((.*?)\)/gs;

my $date = UnixDate(ParseDate($1), '%Y%m%d');

my $category;

sub insert_sql
{
    my ($crc, $description, $category, $year, $manufacturer, $size, $file) = @_;

    if (! $year)
    {
	$year = "????";
    }

    #my $game = $description;
    #$game =~ s/\s\(.*?\)//g;

    print "INSERT INTO romdb VALUES ('" . $crc . "'," . '"' . $description . '"' . "," . '"' . $description . '"' . ",'" .
		    $category . "','" . $year . "'," . '"' . $manufacturer . '"'. ",'" .
		    "Unknown','" . "Unknown','MAME'," .
		    $size . ",'','" . $date . "','" . $file . "');\n";
    return;
}

foreach my $key (keys (%{$data->{game}}))
{

    next if ($data->{game}->{$key}->{isbios});

    if ($data->{game}->{$key}->{driver}->{status} eq 'good')
    {
        $category="Players " . $data->{game}->{$key}->{input}->{players};
    }
    else
    {
        $category='Inperfect';
    }

    if ($data->{game}->{$key}->{rom}->{crc})
    {
	insert_sql($data->{game}->{$key}->{rom}->{crc}, $data->{game}->{$key}->{description}, $category, $data->{game}->{$key}->{year},
		$data->{game}->{$key}->{manufacturer}, $data->{game}->{$key}->{rom}->{size}, $data->{game}->{$key}->{rom}->{name});
    }
    else
    {
        foreach my $rom (keys (%{$data->{game}->{$key}->{rom}}))
	{
	    if ($data->{game}->{$key}->{rom}->{$rom}->{crc})
	    {
		insert_sql($data->{game}->{$key}->{rom}->{$rom}->{crc}, $data->{game}->{$key}->{description},
		    $category, $data->{game}->{$key}->{year}, $data->{game}->{$key}->{manufacturer},
		    $data->{game}->{$key}->{rom}->{$rom}->{size}, $rom);
	    }
	}
    }
}
