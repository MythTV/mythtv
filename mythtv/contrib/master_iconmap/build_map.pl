#!/usr/bin/perl -w
#
# Build master_iconmap.xml from the config files
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Includes
    use Getopt::Long;

# Variables
    my $help;   # Name of a new xml file to input
    my $input;  # Name of a new xml file to input
    my $lookup; # Requested callsign to look up

# Load the cli options
    GetOptions('input=s'           => \$input,
               'callsign|lookup=s' => \$lookup,
               'help|h|usage'      => sub { print_help(); }
              );

# Load the callsigns
    my %callsigns;
    open(DATA, 'data/callsigntonetwork.txt') or die "Can't open data/callsigntonetwork.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($callsign, $network) = split(/\s+/, $_, 2);
        $callsigns{$callsign} = $network;
    }
    close DATA;

# Load the networks
    my %networks;
    open(DATA, 'data/networktourl.txt') or die "Can't open data/networktourl.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($network, $url) = split(/\s+/, $_, 2);
        $networks{$network} = $url;
    }
    close DATA;

# Load the URL stubs
    my %urls;
    open(DATA, 'data/baseurl.txt') or die "Can't open data/baseurl.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($stub, $url) = split(/\s+/, $_, 2);
        $urls{$stub} = $url;
    }
    close DATA;

# Looking for the URL for a specific callsign?
    if ($lookup) {
        $lookup = uc($lookup);
        if ($callsigns{$lookup}) {
            my $url = $networks{$callsigns{$lookup}};
            $url =~ s/\[(.+?)\]/$urls{$1}/sg;
            print $url;
        }
        print "\n";
    # Done
        exit;
    }

# Import a new file?
    if ($input) {
        die "input file '$input' does not exist" unless (-e $input);
    # Slurp in the data
        my $text = '';
        open(DATA, $input) or die "Can't read $input:  $!\n";
        $text .= $_ while (<DATA>);
        close DATA;
    # Parse and count changes
        my %changes;
        while ($text =~ /<callsigntonetwork>\s*<callsign>\s*([^<>]+?)\s*<\/callsign>\s*<network>\s*([^<>]+?)\s*<\/network>\s*<\/callsigntonetwork>/sg) {
            $changes{'cs'}++ unless ($callsigns{$1});
            $callsigns{$1} = $2;
        }
        while ($text =~ /<networktourl>\s*<network>\s*([^<>]+?)\s*<\/network>\s*<url>\s*([^<>]+?)\s*<\/url>\s*<\/networktourl>/sg) {
            $changes{'n'}++ unless ($networks{$1});
            $networks{$1} = $2;
        }
        while ($text =~ /<baseurl>\s*<stub>\s*([^<>]+?)\s*<\/stub>\s*<url>\s*([^<>]+?)\s*<\/url>\s*<\/baseurl>/sg) {
            $changes{'u'}++ unless ($urls{$1});
            $urls{$1} = $2;
        }
    # If there are changes, rewrite the appropriate files
        if ($changes{'cs'}) {
            write_file('data/callsigntonetwork.txt', \%callsigns);
        }
        if ($changes{'n'}) {
            write_file('data/networktourl.txt', \%networks);
        }
        if ($changes{'u'}) {
            write_file('data/baseurl.txt', \%urls);
        }
    }

# Print the header
    open(OUT, '>master_iconmap.xml') or die "Can't create master_iconmap.xml:  $!\n";
    print OUT <<EOF;
<?xml version="1.0" encoding="UTF-8"?>
<iconmappings>
EOF

# Print the callsigns
    foreach my $callsign (sort keys %callsigns) {
        print OUT <<EOF;
    <callsigntonetwork>
        <callsign>$callsign</callsign>
        <network>$callsigns{$callsign}</network>
    </callsigntonetwork>
EOF
    # Warn about missing networks
        unless ($networks{$callsigns{$callsign}}) {
            print STDERR "Callsign $callsign specifies unknown network:  $callsigns{$callsign}\n";
        }
    }

# Next, the network URL's
    foreach my $network (sort keys %networks) {
        print OUT <<EOF;
    <networktourl>
        <network>$network</network>
        <url>$networks{$network}</url>
    </networktourl>
EOF
    }
    close DATA;

# Load the base URL's
    foreach my $stub (sort keys %urls) {
        print OUT <<EOF;
    <baseurl>
        <stub>$stub</stub>
        <url>$urls{$stub}</url>
    </baseurl>
EOF
    }
    close DATA;

# Close the XML tab
    print OUT "</iconmappings>\n";
    close OUT;


# Function
    sub print_help {
        print <<EOF;
usage:  ./build_map.pl [options]

    If no options are given, this will build a new master_iconmap.xml file
    from the existing data files.

options:

    --input=<filename>

        Parse a preexisting channelmap.xml file into the master_iconmap data
        directory.

    --callsign=<callsign>
    --lookup=<callsign>

        Look up a specific callsign and return the URL for its icon.

    --help
    --usage

        Print this help message.

EOF
        exit;
    }

    sub write_file {
        my $file = shift;
        my $hash = shift;
    # First, get the longest hash key, and round up to the nearest tabstop
        my $length = 0;
        foreach my $key (keys %$hash) {
            my $l = length($key);
            $length = $l if ($l > $length);
        }
        $length = 4 + int(($length + 2) / 4) * 4;
    # Next, write the file
        open(TXTOUT, ">$file") or die "Can't write to $file:  $!\n";
        foreach my $key (sort keys %$hash) {
            print TXTOUT $key, ' ' x ($length - length($key)), $hash->{$key}, "\n";
        }
        close TXTOUT;
    }
