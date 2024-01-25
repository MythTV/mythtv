#!/usr/bin/perl

# test all the grabbers, by twitham@sbcglobal.net, 2024/01

# Similar to CU LRC's lib/scrapertest.py but for MythTV's *.py
# wrappers.  This tester adds several additional benefits:

# -t test grabbers in priority order
# lookup any -a artist and -n title
# stop at first match (default) or -k keep going
# high resolution timing summary
# report lyric lines found in the summary

use warnings;
use strict;
use Getopt::Long;
use File::Basename;
use Time::HiRes qw(gettimeofday tv_interval);

my $begin = [gettimeofday];
my($name, $path, $suffix) = fileparse($0);

my @opt = qw(k|keepgoing v|version t|test s|search d|debug
    a|artist=s b|album=s n|title=s f|filename=s);
my %opt;
my $usage = "usage: $0 [-d] [-k] [-v | -t | -a artist -n title]
-d | --debug       extra debugging lines to stderr
-k | --keepgoing   don't stop at the first match
-v | --version     show scraper version headers
-t | --test        lookup known good artist/title
-a | --artist      lookup given artist
-n | --title       lookup given name/title
";
GetOptions(\%opt, @opt) or die $usage;
$opt{v} or $opt{t} or $opt{a} and $opt{n} or die $usage;

chdir $path or die $!;
opendir DIR, '.' or die $!;
my(%pri, %sync);

# code reading hack works with current info format:
for my $py (grep /\.py$/, readdir DIR) {
    if (open FILE, $py) {
        while (<FILE>) {
            m/'priority':\s*'(\d+)'/
                and $pri{$py} = $1;
            m/info.'priority'.\s*=\s*'(\d+)'/
                and $pri{$py} = $1;
            m/'syncronized':\s*(\S+)/
                and $sync{$py} = $1;
        }
    }
}
closedir DIR;

my(%found, %time, %lines);
@opt = map { $opt{$_} ? ("-$_", $opt{$_}) : () } qw/a b n f v d t/;
for my $py (sort { $pri{$a} <=> $pri{$b} } keys %pri) {
    my $begin = [gettimeofday];
    my @cmd = ('python3', $py, @opt);
    warn "$pri{$py}\t$sync{$py}\t@cmd\n";
    my $out = '';
    open PIPE, '-|', @cmd;
    while (<PIPE>) {
        $out .= $_;
    }
    close PIPE;
    $lines{$py} = $out =~ s/<lyric>/<lyric>/g;
    $lines{$py} ||= 0;
    $time{$py} = tv_interval($begin, [gettimeofday]);
    $out and print $out;
    $out and !$opt{k} and last;
}
warn join("\t", qw(pri sync lyrics seconds command)), "\n";
for my $py (sort { $pri{$a} <=> $pri{$b} } keys %lines) {
    warn "$pri{$py}\t$sync{$py}\t$lines{$py}\t$time{$py}\t$py\n";
}
warn "TOTAL seconds elapsed\t", tv_interval($begin, [gettimeofday]), "\n";
