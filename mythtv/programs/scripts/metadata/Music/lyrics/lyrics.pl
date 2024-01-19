#!/usr/bin/perl

# test all the grabbers, by twitham@sbcglobal.net, 2024/01

use warnings;
use strict;
use Getopt::Long;
use File::Basename;
use Time::HiRes qw(gettimeofday tv_interval);

my($name, $path, $suffix) = fileparse($0);

my @opt = qw(k|keepgoing v|version t|test s|search d|debug
    a|artist=s b|album=s n|title=s f|filename=s);
my %opt;
my $usage = "usage: $0 options\n@opt\n";
GetOptions(\%opt, @opt) or die $usage;
$opt{t} or $opt{a} and $opt{n} or die $usage;

chdir $path or die $!;
opendir DIR, '.' or die $!;
my($cmd, %py, %sync);
for my $py (grep /\.py$/, readdir DIR) {
    if (open FILE, $py) {
	while (<FILE>) {
	    /command\b.*\b(\*?\w+\.py)\b/
		and $cmd = $1;
	    /priority\b.*?\b(\d+)\b/
		and $py{$cmd} = $1;
	    /syncronized\b.*?=\s*(\S+)/
		and $sync{$cmd} = $1;
	}
    }
}

my(%found, %time);
@opt = map { $opt{$_} ? ("-$_", $opt{$_}) : () } qw/a b n f v d t/;
for (sort { $py{$a} <=> $py{$b} } keys %py) {
    my $begin = [gettimeofday];
    my @cmd = ('python3', $_, @opt);
    warn "$py{$_}\t$sync{$_}\t@cmd\n";
    my $ex = system @cmd;
    $time{$_} = tv_interval($begin, [gettimeofday]);
    $found{$_} = $ex ? 0 : 1;
    $found{$_} and ($opt{k} or last);
}
for (sort { $py{$a} <=> $py{$b} } keys %found) {
    warn "$py{$_}\t$sync{$_}\t$_\t$found{$_}\t$time{$_}\n";
}
