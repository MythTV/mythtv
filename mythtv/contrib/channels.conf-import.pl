#!/usr/bin/perl -w
# ============================================================================
# = NAME
# channels.conf-import
#
# = PURPOSE
# Insert DVB channel information into the MythTV database
#
# = USAGE
# channels.conf-import
# channels.conf-import -c	# Parse czap format channels.conf file
# channels.conf-import -s	# Parse szap format channels.conf file
# channels.conf-import -t	# Parse tzap format channels.conf file
# channels.conf-import -d	# Extra debugging output
# channels.conf-import -n	# No execute (safe or simulation) mode
#
# = PREREQUISITES
# 1. A MythTV backend with valid VideoSource data
#    (i.e. a listings provider which has downloaded channel information)
# 2. A channels.conf file generated from linuxtv-dvb-apps util/scan.
#    DVB-T (i.e. tzap) is assumed
#
# = DESCRIPTION
# For each channel in the database, this script tries to match
# a line from channels.conf. If it doesn't find a good match,
# it will prompt the user for a search term.
# (e.g. My database had "Seven",
#       but the network publishes itself as "7 Digital")
#
# = KNOWN BUGS
# If this is run before any channel data is inserted
# into the MythTV database, it silently hangs.
#
# = REVISION
# $Id$
#
# = AUTHORS
# Malcolm Smith, Nigel Pearson
# ============================================================================

use strict;
use DBI();
use Getopt::Long;
use File::Basename;

# configuration
#my $channelsconf = "/root/.szap/channels.conf";
#my $channelsconf = "/usr/src/DVB/apps/szap/channels.conf-dvbt-oxford";
my $channelsconf = "channels.conf";

# type of channels.conf file
my $channelsconftype = "tzap";

my $sourceid = 1;

# whether to ask for some "optional" fields
my $ask_networkid   = 0;
my $ask_providerid  = 0;
my $ask_serviceid   = 1;
my $ask_transportid = 0;

my $sql1="Invalid";

# do nothing
my $simulate = 0;

# extra console output
my $DEBUG = 0;

# ============================================================================
# Parse any command-line arguments
#
my $arg;

for $arg ( @ARGV )
{
    if ( $arg eq '-c' )
    {   $channelsconftype = 'czap'   }
    elsif ( $arg eq '-d' )
    {   $DEBUG = 1   }
    elsif ( $arg eq '-n' )
    {   $simulate = 1   }
    elsif ( $arg eq '-s' )
    {   $channelsconftype = 'szap'   }
    elsif ( $arg eq '-t' )
    {   $channelsconftype = 'tzap'   }
    elsif ( $arg eq '-v' )
    {   $channelsconftype = 'vdr'   }
    else
    {   die "Invalid argument '$arg'\n"   }
}

# ============================================================================
# variable definition
#
my $doinserts;
my $dofind;
my $networkid;
my $transportid;
my @channeldata;
my %channelhash;
my $db_host;
my $db_user;
my $db_name;
my $db_pass;

# ============================================================================
# Given a list of parsed fields, generate an SQL statement to
# insert these # fields and values into the dvb_channel table

sub genChan($@)
{
    my ($chanid, @fields) = @_;

    my $sql = 'REPLACE INTO dvb_channel (chanid, ' .
              join (', ', @fields) . ")\n" .
              "VALUES ($chanid, ";

    my $field;
    foreach $field ( @fields )
    {
        if ( defined $channelhash{$field} )
        {
            my $val = $channelhash{$field};

            if ( $val =~ /^\d*$/ )   # If value is a number
            {   $sql .= "$val, "    }
            else
            {   $sql .= "'$val', "  }
        }
        else
        {   print "\$channelhash{'$field'} does not exist\n"   }
    }

    chop $sql;chop $sql;    # Remove trailing ', '

    return "$sql);\n";
}

# ============================================================================
# Routines to parse ZAP-style files
#

sub fix_zFEC
{
    $channelhash{'fec'} =~ s/FEC_//g;
    $channelhash{'fec'} =~ s/_/\//g;
}

sub fix_zInversion
{
    if ($channelhash{'inversion'} eq "INVERSION_ON" )
    {   $channelhash{'inversion'} = '1'  }

    if ($channelhash{'inversion'} eq "INVERSION_OFF" )
    {   $channelhash{'inversion'} = '0'  }

    if ($channelhash{'inversion'} eq "INVERSION_AUTO" )
    {   $channelhash{'inversion'} = 'a'  }
}

sub fix_zMod
{
    # make modulation lowercase
    $channelhash{'modulation'} =~ s/QAM/qam/g;
}

sub parseCZAP(@)
{
    $channelhash{'frequency'}  = shift @channeldata;
    $channelhash{'inversion'}  = shift @channeldata;
    $channelhash{'symbolrate'} = shift @channeldata;
    $channelhash{'fec'}        = shift @channeldata;
    $channelhash{'modulation'} = shift @channeldata;

    &fix_zInversion;
    &fix_zFEC;
    &fix_zMod;
}

sub parseSZAP(@)
{
    $channelhash{'frequency'}  = (shift @channeldata) . '000';
    $channelhash{'polarity'}   = shift @channeldata;
    $channelhash{'satid'}      = shift @channeldata;
    $channelhash{'symbolrate'} = (shift @channeldata) . '000';

    # on dvb-s we always have qpsk
    $channelhash{'modulation'} = 'qpsk';
}

sub parseTZAP(@)
{
    $channelhash{'frequency'} = shift @channeldata;
    $channelhash{'inversion'} = shift @channeldata;
    $channelhash{'bandwidth'} = shift @channeldata;
    $channelhash{'fec'}       = shift @channeldata;
    $channelhash{'lp_code_rate'}      = shift @channeldata;
    $channelhash{'modulation'}        = shift @channeldata;
    $channelhash{'transmission_mode'} = shift @channeldata;
    $channelhash{'guard_interval'}    = shift @channeldata;
    $channelhash{'hierarchy'}         = shift @channeldata;

    # Set polarity and symbolrate here to defaults
    $channelhash{'polarity'}   = 'V';
    $channelhash{'symbolrate'} = '69000';

    &fix_zInversion;
    &fix_zFEC;
    &fix_zMod;

    $channelhash{'bandwidth'} =~ s/BANDWIDTH_//;
    $channelhash{'bandwidth'} =~ s/_.*//;

    $channelhash{'guard_interval'} =~ s/GUARD_INTERVAL_//g;
    $channelhash{'guard_interval'} =~ s/_/\//g;

    if ($channelhash{'hierarchy'} eq "HIERARCHY_NONE" )
    {   $channelhash{'hierarchy'} = 'n'  }
    else
    {   $channelhash{'hierarchy'} = 'auto'  }

    $channelhash{'lp_code_rate'} =~ s/FEC_//g;
    $channelhash{'lp_code_rate'} =~ s/_/\//g;

    $channelhash{'transmission_mode'} =~ s/TRANSMISSION_MODE_//g;
    $channelhash{'transmission_mode'} =~ s/K//g;
}

sub parseZAP
{
    if (    $channelsconftype eq 'szap' )
    {    &parseSZAP(@channeldata)    }
    elsif ( $channelsconftype eq 'czap' )
    {    &parseCZAP(@channeldata)    }
    elsif ( $channelsconftype eq 'tzap' )
    {    &parseTZAP(@channeldata)    }
    else
    {    die "Illegal channelconftype in perl script"    }

    $channelhash{'vpid'}      = shift @channeldata;
    $channelhash{'apid'}      = shift @channeldata;
    $channelhash{'serviceid'} = shift @channeldata;
    chop $channelhash{'serviceid'};
}

# ============================================================================
# Routines to parse VDR-style file

sub parseVDRs
{
    die "Sorry - Nigel hasn't implemented VDRs parsing yet";
}

sub parseVDRc
{
    die "Sorry - Nigel hasn't implemented VDRc parsing yet";
}

sub parseVDRt
{
    die "Sorry - Nigel hasn't implemented VDRt parsing yet";
}

sub parseVDR
{
    if ( grep m/:T:27500:/, @channeldata )
    {   &parseVDRt   }
    elsif ( grep m/:M.*:C:/, @channeldata )
    {   &parseVDRc   }
    else
    {   &parseVDRs   }
}

# ============================================================================

# Read the mysql.txt file in use by MythTV.
# could be in a couple places, so try the usual suspects
open(CONF, "/usr/share/mythtv/mysql.txt")
  or open(CONF, "/usr/local/share/mythtv/mysql.txt")
    or die ("Unable to open /usr/share/mythtv/mysql.txt:  $!\n\n");
while (my $line = <CONF>) {
  chomp($line);
  $line =~ s/^str //;
  my ($var, $val) = split(/\=/, $line, 2);
  next unless ($var && $var =~ /\w/);
  if ($var eq 'DBHostName') {
    $db_host = $val;
  }
  elsif ($var eq 'DBUserName') {
    $db_user = $val;
  }
  elsif ($var eq 'DBName') {
    $db_name = $val;
  }
  elsif ($var eq 'DBPassword') {
    $db_pass = $val;
  }
}
close CONF;

# Connect to the database
my $dbh = DBI->connect("dbi:mysql:database=$db_name:host=$db_host",
                       $db_user, $db_pass)
          or die "Cannot connect to database: $!\n\n";

if ($simulate) {
  print "Simulation mode. SQL not done.\n";
};

# Now retrieve data from the channel table.
my $tb_channel = $dbh->prepare("SELECT * FROM channel WHERE sourceid=$sourceid");
$tb_channel->execute();
while (my $ref = $tb_channel->fetchrow_hashref()) {
  $dofind = 0;
  print "\nFound a row: chanid = $ref->{'chanid'}, name = $ref->{'name'}\n";

  # search corresponding rows in dvb_channel
  my $tb_dvb_channel = $dbh->prepare("SELECT * FROM dvb_channel WHERE chanid=" . $ref->{'chanid'});
  my $count_dvbchannel = $tb_dvb_channel->execute();

  if ($count_dvbchannel >= 1) {
    # channel already exists in dvb_channel
    print "Found $count_dvbchannel corresponding row(s) in dvb_channel table\n";

    # integrity check: there have to be at least two rows in dvb_pids
    my $tb_dvb_pids = $dbh->prepare("SELECT * FROM dvb_pids WHERE chanid=" . $ref->{'chanid'});
    my $count_dvbpids = $tb_dvb_pids->execute();
    
    if ($count_dvbpids < 2) {
      print "Warning: Less than 2 dvb_pids not found\n";
    }
    print "Do you want to re-insert? [n]/y:";
    my $selection = <>;
    chomp($selection);
    if ($selection eq "y" ) {
      $dofind = 1 ;
    }
    $tb_dvb_pids->finish();
  } else {
    # no corresponding row in dvb_channel, so let's insert data
    print "no corresponding row in dvb_channel, need data, searching channels.conf\n";
    $dofind = 1;
  }

  if ($dofind) {
    open (CHANNELSCONF, "cat $channelsconf | grep -i \"" . $ref->{'name'} . "\" |") or die "failed opening channels.conf: $!";
    my @channelsconf = <CHANNELSCONF>;
    close CHANNELSCONF;

    my $channelsconfrows=$#channelsconf + 1;

    if ( $channelsconfrows == 0 ) {
      print "Zero rows found. Do you want to enter a search term? [n]/y:";
      my $selection = <>;
      chomp($selection);
      if ($selection eq "y" ) {
        print "Enter search term:";
        $selection = <>;
        chomp($selection);

        open (CHANNELSCONF, "cat $channelsconf | grep -i \"" . $selection . "\" |") or die "failed opening channels.conf: $!";
        @channelsconf = <CHANNELSCONF>;
        close CHANNELSCONF;
        $channelsconfrows=$#channelsconf + 1;
      }
    }

    print "found $channelsconfrows matching rows in channels.conf\n";
    
    for (my $i = 1;$i < $channelsconfrows + 1; $i++) {
      if ( $DEBUG )
      { print "$i: $channelsconf[$i-1]" }
      else
      {
        # Print just the name, frequency and ids

        @channeldata=split(/:/, $channelsconf[$i-1]);

        print "$i: ", shift @channeldata, "\t:", shift @channeldata;
        my $sid = pop @channeldata;
        my $aid = pop @channeldata;
        my $vid = pop @channeldata;
        print ":...:$vid:$aid:$sid"; 
      }
    }

    print "What to do [number to select, anything else to ignore]? ";
    my $selection = <>;
    chomp($selection);

    #print $selection . "\n";

    if ($selection =~ /^\d+$/) {
      ($DEBUG) && print "using row $selection\n";

      # get the information out of the channelsconf row
      @channeldata=split(/:/, $channelsconf[$selection-1]);

      %channelhash = ();         # Clear previous insert's values
      $channelhash{'satid'} = 0; # Default of NULL is inappropriate

      shift @channeldata; # Station-published service name not used

      if ( $channelsconftype =~ m/zap$/ )
      {  &parseZAP  }
      elsif ($channelsconftype eq 'vdr')
      {  &parseVDR(@channeldata)  }
 

      if ( $DEBUG )
      {
        print "fec : $channelhash{'fec'}, ",
              "fec_lp : $channelhash{'lp_code_rate'}\n";
        print "inversion         : $channelhash{'inversion'}\n";
        print "transmission_mode : $channelhash{'transmission_mode'}\n";
        print "guard_interval    : $channelhash{'guard_interval'}\n";
        print "hierarchy    : $channelhash{'hierarchy'}\n";
        print "chanid       : $ref->{'chanid'}\n";
        print "frequency    : $channelhash{'frequency'}\n";
        print "bandwidth    : $channelhash{'bandwidth'}\n";
        print "polarity     : $channelhash{'polarity'}\n";
        print "symbolrate   : $channelhash{'symbolrate'}\n";
        print "satid        : $channelhash{'satid'}\n";
        print "modulation   : $channelhash{'modulation'}\n";
        print "VPID         : $channelhash{'vpid'}\n";
        print "APID         : $channelhash{'apid'}\n";
      }

      if ( $ask_serviceid && ! defined $channelhash{'serviceid'} ) {
        print "serviceid    : ";
        $channelhash{'serviceid'}=<>;
        chomp ($channelhash{'serviceid'});
      } else {
        print "serviceid    : $channelhash{'serviceid'}\n";
      }

      if ( $ask_transportid && ! defined $channelhash{'transportid'} ) {        
        print "transportid  : ";
        $channelhash{'transportid'}=<>;
        chomp ($channelhash{'transportid'});
      } else {
        $channelhash{'transportid'} = '0';
        print "transportid  : $channelhash{'transportid'}\n";
      }

      if ( $ask_networkid && ! defined $channelhash{'networkid'} ) {
        print "networkid    : ";
        $channelhash{'networkid'}=<>;
        chomp ($channelhash{'networkid'});
      } else {
        $channelhash{'networkid'} = '0';
        print "networkid    : $channelhash{'networkid'}\n";
      }

      if ( $ask_providerid && ! defined $channelhash{'providerid'} ) {
        print "providerid   : ";
        $channelhash{'providerid'}=<>;
        chomp ($channelhash{'providerid'});
      } else {
        $channelhash{'providerid'} = '0';
        print "providerid   : $channelhash{'providerid'}\n";
      }

      $doinserts=1;
    } else {
      $doinserts=0;
    }

    if ($doinserts==1) {
      # insert1: dvb_channel

      print "Channel type : $channelsconftype\n";

      if ($channelsconftype eq 'czap') {
        $sql1 = &genChan($ref->{'chanid'}, 
                         ('serviceid', 'networkid', 'transportid', 
                          'frequency', 'inversion', 'symbolrate', 
                          'bandwidth', 'modulation', 'fec') );
      }

      if ($channelsconftype eq 'szap') {
        $sql1 = &genChan($ref->{'chanid'},
                         ('serviceid', 'networkid', 'transportid',
                          'frequency', 'inversion', 'symbolrate',
                          'polarity', 'satid', 'modulation') );
      } 

      if ($channelsconftype eq 'tzap') {
        $sql1 = &genChan($ref->{'chanid'},
                         ('serviceid', 'networkid', 'providerid',
                          'transportid', 'frequency', 'inversion',
                          'symbolrate', 'fec', 'polarity', 'modulation',
                          'bandwidth', 'lp_code_rate', 'transmission_mode',
                          'guard_interval', 'hierarchy') );
      }

      #insert2: vpid
      my $sql2 = "REPLACE INTO dvb_pids (chanid, pid, type) VALUES (" .
                 $ref->{'chanid'} . ", $channelhash{'vpid'}, \'v\');"; 

      #insert3: apid
      my $sql3 = "REPLACE INTO dvb_pids (chanid, pid, type) VALUES (" .
                 $ref->{'chanid'} . ", $channelhash{'apid'}, \'a\');"; 

      print "Generated SQL:\n$sql1\n$sql2\n$sql3\n";

      if ($simulate) {
        print "Simulation mode. SQL not done.\n";
      } else {
        $dbh->do($sql1);
        $dbh->do($sql2);
        $dbh->do($sql3);

        print "SQL insert complete.\n";
      }
    }
  }
  $tb_dvb_channel->finish();
}
$tb_channel->finish();

$dbh->disconnect();
