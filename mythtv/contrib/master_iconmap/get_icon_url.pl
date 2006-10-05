#!/usr/bin/perl -w
#
# Look up the logo icon URL for a specified callsign.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Data file locations
    my $callsigntonetwork = 'data/callsigntonetwork.txt';
    my $networktourl      = 'data/networktourl.txt';
    my $baseurl           = 'data/baseurl.txt';

# Nothing requested?
    unless ($ARGV[0] && $ARGV[0] =~ /\w/ && $ARGV[0] !~ /\W/) {
        print "usage:  ./get_icon_url.pl CALLSIGN\n";
        exit;
    }

# All callsigns are stored in upper case
    my $lookup = uc($ARGV[0]);

# Variables we will use
    my $ignore  = '';
    my $network = '';
    my $url     = '';

# First, we find the callsign
    open(DATA, $callsigntonetwork) or die "Can't open $callsigntonetwork:  $!\n";
    while (<DATA>) {
        next unless (/^$lookup\s/);
        chomp;
        ($ignore, $network) = split(/\s+/, $_, 2);
        last;
    }
    close DATA;

# Leave early?
    if (!$network) {
        print "Unknown callsign:  $lookup\n";
        exit;
    }

# On to the network
    open(DATA, $networktourl) or die "Can't open $networktourl:  $!\n";
    while (<DATA>) {
        next unless (/^$network\s/);
        chomp;
        ($ignore, $url) = split(/\s+/, $_, 2);
        last;
    }
    close DATA;

# Leave early?
    if (!$url) {
        print "Unknown network:  $network\n";
        exit;
    }

# Only load the stub list if we have to
    if ($url =~ /\[.+?\]/) {
        my %urls;
        open(DATA, $baseurl) or die "Can't open $baseurl:  $!\n";
        while (<DATA>) {
            chomp;
            next if (/^\s*#/ || /^\s*$/);
            my ($stub, $url) = split(/\s+/, $_, 2);
            $urls{$stub} = $url;
        }
        close DATA;
    # Replace any URL stubs with the actual values
        $url =~ s/\[(.+?)\]/$urls{$1}/sg;
    }

# Print the result (if there was one)
    print "$url\n";

# Done
    exit;

