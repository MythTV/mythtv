#!/usr/bin/perl 
# mplayernetwork startup script by Mark Musone, mmusone at shatterit dot com
# instructions by Robert Kulagowski, rkulagow at rocketmail dot com
# To use, you must install Net::Server::Fork first
# perl -MCPAN -e shell
# cpan>install Net::Server::Fork
# cpan>exit
# This script is designed to be called from the mythexplorer-settings.txt
# file.
# To use this script, ensure that you have done a 
# chmod a+x mplayernetwork.pl and copy it to /usr/local/bin
# Then, edit the mythexplorer-settings.txt file and replace any 
# /usr/local/bin/mplayer {options} with /usr/local/bin/mplayernetwork.pl %s

package MyPackage;
use FileHandle;
#use strict;
#  use vars qw(@ISA);
use Net::Server::Fork; # any personality will do

$| = 1;
sub CHLD_handler
{
    print "got handler!\n";
    exit(0);
}


$SIG{'CHLD'} = 'CHLD_handler';
$SIG{'PIPE'} = 'CHLD_handler';

$pid = open(WRITEME, "| /usr/local/bin/mplayer -nolirc -display :0.0 -slave '$ARGV[0]' ");
#$pid = open(WRITEME, "| cat ");
WRITEME->autoflush(1);

  @ISA = qw(Net::Server::Fork);
  MyPackage->run();
  exit;
  ### over-ridden subs below
  sub process_request {

     my $self = shift;
    eval {
##      local $SIG{ALRM} = sub { die "Timed Out!\n" };
      local $SIG{PIPE} = sub { $self->server_close() };
      local $SIG{CHLD} = sub { $self->server_close() };
      my $timeout = 30; # give the user 30 seconds to type a line
      my $previous_alarm = alarm($timeout);
WRITEME->autoflush(1);
      while( <STDIN> ){
        s/\r$//;
	$status = print WRITEME $_;
	if(!$status || $_ =~ /quit$/)
	{
print "closing server\n";
{
close(WRITEME);
system("killall mplayer");
$self->server_close(); 
}
	}
	return;
        alarm($timeout);
      }
      alarm($previous_alarm);
    };
    if( $@=~/timed out/i ){
      print STDOUT "Timed Out.\r\n";
      return;
    }
  }

  1;
