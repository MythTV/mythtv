#!/usr/bin/perl -w
#
# This script will attempt to find connected firewire video devices known to
# MythTV and run the firewire_tester script to "prime" them for better
# changes of good recordings.  It will not prime devices that are currently
# recording.
#
# Take it as is, no warranty provided.  I take no responsibility for damaged
# hardware, failed marriages due to MythTV not recording your wife's favorite
# show, etc.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
# @copyright MythTV
#
# Modified: 2009-10-16
# Modification Author: John Baab (rhpot1991@ubuntu.com)
# Modification Description: Added command line args so this will be easier to support in Mythbuntu
#

###############################################################################

# Config

    my $plugreport      = '/usr/bin/plugreport';
    my $firewire_tester = '/usr/local/bin/firewire_tester';
    my $retry = 0;
    my $connection = "";
    my $fw_tester_options = "-B";
    
    my $usage = "\nHow to use firewire_primer.pl:\n"
    ."--connection -c = Type of connection, available options: broadcast, p2p, broadcast-fix.  (default: broadcast-fix)\n"
    ."--retry -r = Retry attempts, any integer.  (default: 1)\n"
    ."--help -h = Displays Help.\n"
    ."\nExamples:\nfirewire_primer.pl --connection=broadcast --retry=5"
    ."\nfirewire_primer.pl -c=broadcast -r=5"
    ."\nfirewire_primer.pl -cbroadcast -r5\n\n";

###############################################################################

# Some helpful libraries
    use MythTV;
    use Sys::Hostname;
    
# Get our command line args
    foreach (@ARGV){
        if ($_ =~ m/\-\-retry=(\d+)/ || $_ =~ m/\-r=?(\d+)/) {
            $retry = $1;
        }
        elsif ($_ =~ m/\-\-connection=(\w+)/ || $_ =~ m/\-c=?(\w+)/) {
            $connection = $1;
        }
        else{
            die "$usage";
        }
    }
   
# Figure out what to do with the args   
    if($connection =~ m/^broadcast$/){
        $fw_tester_options = "-b";
    }
    elsif($connection =~ m/^p2p$/){
        $fw_tester_options = "-p";
    }
    else{
        if($connection !~ m/^broadcast\-fix$/  && $connection ne ""){
            die "$usage";
        }
    }
    
    if($retry > 0){
        $fw_tester_options .= " -r $retry";
    }

# Plugreport, etc needs to run as root.
    BEGIN {
        unless ($< < 1) {
            print "This script must be run as root because it calls plugreport, requires root privs.\n";
            exit 1;
        }
    }

# Find all of the firewire devices plugged into this machine
    my $host;
    my %guid_list;

    open PLUG, "$plugreport 2>/dev/null |" or die "Can't run $plugreport\n";
    while (<PLUG>) {
        if (/Host\s+Adapter\s+(\d+)/i) {
            $host = $1;
        }
        elsif (/Node\s+(\d+)\s+GUID\s+0x(\w+)\b/) {
            $guid_list{lc($2)}{node} = $1;
            $guid_list{lc($2)}{host} = $host;
        }
        #else {
        #    print "-> $_";
        #}
    }
    close PLUG;

# Connect to MythTV and run through all of the firewire cards for this host.
    my $m = new MythTV();

    my $sh = $m->{'dbh'}->prepare('SELECT cardid, LOWER(videodevice) FROM capturecard WHERE hostname = ?');
    $sh->execute(hostname);
    while (my ($cardid, $guid) = $sh->fetchrow_array) {
        print "GUID:  $guid (cardid:  $cardid)\n";
        unless ($guid_list{$guid}) {
            print "    not visible in plugreport\n";
            next;
        }
        my $is_recording = $m->backend_command(sprintf('QUERY_RECORDER %s[]:[]IS_RECORDING', $cardid));
        if ($is_recording) {
            print "    currently recording\n";
            next;
        }
        my $overload;
        for (;;) {
            $overload++;
            my $results = `$firewire_tester $fw_tester_options -P $guid_list{$guid}{host} -n $guid_list{$guid}{node}`;
            my $num =()= $results =~ /Success,\s+\d+\s+packets/g;
            if ($num >= 5) {
                print "    success.\n";
                last;
            }
            elsif ($overload >= 10) {
                print "    too many failed attempts to stabilize.\n";
                last;
            }
        }

    }
    $sh->finish;

