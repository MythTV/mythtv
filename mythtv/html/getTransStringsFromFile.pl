#!/usr/bin/perl -w

if (!defined($ARGV[0])) {
    print "ERROR: Usage: getTransStringsFromFile.pl FILENAME\n";
    exit -1;
}

my $inFile = $ARGV[0];
my $baseFile = defined($ARGV[1]) ? $ARGV[1] : $inFile;
my %strings;
my $contents = "";
open( IN, "< $inFile" ) || die "Can not open $inFile: $!";
while( my $line = <IN> ) {
    chomp($line);

    $line =~ s/^\s*//;
    $line =~ s/\s*$//;

    $contents .= " ";
    $contents .= $line;
}
close( IN );

my @lines = split('>', $contents);
foreach my $line ( @lines ) {
    if ($line =~ /<\/i18n$/i) {
        $line =~ s/<\/i18n$//;
        $line =~ s/"/\\"/g;
        $strings{$line} = 1;
    }
}

@lines = split('\)', $contents);
foreach my $line ( @lines ) {
    if ($line =~ /qsTr\(/i) {
        $line =~ s/.*qsTr\(\s*["']//i;
        $line =~ s/['"]$//;
        $strings{$line} = 1;
    }
}

if (scalar(keys %strings)) {
    print "    /* $baseFile */\n";
    foreach my $string ( sort keys %strings ) {
        print "    QObject::tr(\"" . $string . "\"),\n";
    }
}
