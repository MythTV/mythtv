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

# Print the header
    open(OUT, '>master_iconmap.xml') or die "Can't create master_iconmap.xml:  $!\n";
    print OUT <<EOF;
<?xml version="1.0" encoding="UTF-8"?>
<iconmappings>
EOF

# Print the callsigns
    open(DATA, 'data/callsigntonetwork.txt') or die "Can't open data/callsigntonetwork.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($callsign, $network) = split(/\s+/, $_, 2);
        print OUT <<EOF;
    <callsigntonetwork>
        <callsign>$callsign</callsign>
        <network>$network</network>
    </callsigntonetwork>
EOF
    }
    close DATA;

# Next, the network URL's
    open(DATA, 'data/networktourl.txt') or die "Can't open data/networktourl.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($network, $url) = split(/\s+/, $_, 2);
        print OUT <<EOF;
    <networktourl>
        <network>$network</network>
        <url>$url</url>
    </networktourl>
EOF
    }
    close DATA;

# Load the base URL's
    open(DATA, 'data/baseurl.txt') or die "Can't open data/baseurl.txt:  $!\n";
    while (<DATA>) {
        chomp;
        next if (/^\s*#/ || /^\s*$/);
        my ($stub, $url) = split(/\s+/, $_, 2);
        print OUT <<EOF;
    <baseurl>
        <stub>$stub</stub>
        <url>$url</url>
    </baseurl>
EOF
    }
    close DATA;

# Close the XML tab
    print OUT "</iconmappings>\n";
    close OUT;