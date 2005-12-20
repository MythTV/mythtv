#!/usr/bin/perl -w

# use LWP::Debug qw(+);  # DEBUG 
use LWP::UserAgent;
use HTTP::Cookies;


use vars qw($opt_h $opt_d $opt_v $opt_i $opt_A $opt_R $opt_L $opt_r $opt_1);
use Getopt::Std; 



$title = "NetFlix"; 
$version = "v1.0";
$author = "John Petrocik";

# display usage
sub usage
{
print "usage: $0 -hviMPD [parameters]\n";
print "       -h           help\n";
print "       -v           display version\n";
print "       -i           display info\n";
print "       -d           debug\n";
print "\n";
print "       -L <userid> <password> login into Netflix. Only needed once, ever.\n";
print "       -A <movieid> <trkid> adds movie to queue\n";
print "       -R <movieid> removes movie to queue\n";
print "       -1 <movieid> move movie to top of queue\n";
exit(-1);
}

# display 1-line of info that describes the version of the program
sub version
{
print "$title ($version) by $author\n"
} 

# display 1-line of info that can describe the type of query used
sub info
{
print "Performs Netflix queue management.\n";
}

# display detailed help 
sub help
{
version();
info();
usage();
}


# sets up the user agent for requests
sub userAgent
{
    $ua = LWP::UserAgent->new;
    $ua->agent('Mozilla/5.0 (compatible; Konqueror/3.4; Linux) KHTML/3.4.0 (like Gecko)');
#   $ua->cookie_jar(HTTP::Cookies->new(file => "lwpcookies.txt",
#                                        autosave => 1));
    $ua->cookie_jar(HTTP::Cookies::Netscape->new(file => "$ENV{\"HOME\"}/.mythtv/MythFlix/netflix.cookies",
                                        autosave => 1));

    push @{ $ua->requests_redirectable }, 'POST';

    return $ua;
}

# login into netflix a stores cookie for future requests
sub login
{

    $ua = &userAgent();
    $res = $ua->post("https://www.netflix.com/Login", [
                                                nextpage=>'http://www.netflix.com/Default',
                                                email=>$userid,
                                                movieid=>'trkid',
                                                password1=>$password,
                                                RememberMe=>'True',
                                                SubmitButton=>'Click Here to Continue']);

    &process($res);

}

# adds movie to queue
sub addToQueue
{
    
    $url = "http://www.netflix.com/AddToQueue?movieid=$movieid&trkid=$trkid";
    if (defined $opt_d) {printf("AddToQueue: $url\n");}
    
    $ua = &userAgent();
    $res = $ua->get($url);
    
    &process($res);
}

# removes movie from queue
sub removeFromQueue
{

    $ua = &userAgent();
    $res = $ua->post("http://www.netflix.com/Queue", [
                                                    "R$movieid"=>'on',
                                                    updateQueueBtn=>'Update+Your+Queue']);
    &process($res);
}

# moves movie to top of queue
sub moveToTop
{
    
    $url = "http://www.netflix.com/MoveToTop?movieid=$movieid&lnkctr=qmtt&fromq=true";
    if (defined $opt_d) {printf("MoveToTop: $url\n");}
    
    $ua = &userAgent();
    $res = $ua->get($url);
    
    &process($res);
}

sub process
{
    
    print "Status Code: " ,$res->code(), "\n";
    
}


#
# Main Program
#

# parse command line arguments 
getopts('hvidALR1');

# print out info 
if (defined $opt_v) { version(); exit 1; }
if (defined $opt_i) { info(); exit 1; }

# print out usage if needed
if (defined $opt_h || $#ARGV<0) { help(); }

if (defined $opt_L)
{
    # take movieid from cmdline arg
    $userid = shift || die "Usage : $0 -L <userid> <password>\n";
    $password = shift || die "Usage : $0 -L <userid> <password>\n";
    &login($userid,$password);
}

if (defined $opt_A)
{
    # take movieid from cmdline arg
    $movieid = shift || die "Usage : $0 -A <movieid> <trkid>\n";
    $trkid = shift || die "Usage : $0 -A <movieid> <trkid>\n";
    &addToQueue($movieid,$trkid);
}

if (defined $opt_R)
{
    # take movieid from cmdline arg
    $movieid = shift || die "Usage : $0 -R <movieid>";
    &removeFromQueue($movieid);
}

if (defined $opt_1)
{
    # take movieid from cmdline arg
    $movieid = shift || die "Usage : $0 -1 <movieid>";
    &moveToTop($movieid);
}



