#!/usr/bin/perl
##
## Script to email database log entries.
##
## 21 Feb 04	1.0	Initial version
##
## written by Matt White (whitem@arts.usask.ca)
## 
## Remember to edit the settings below to your taste
##

use DBI;

## 
## User Configurable Settings
##

# The email address to send the logs to
# ** REMEMBER TO ESCAPE THE @ SYMBOL!! **
$mailto = "someone\@somewhere.changeme";

# The "from" address used in the sent mail
# ** REMEMBER TO ESCAPE THE @ SYMBOL!! **
$mailfrom = "someoneelse\@somewhere.changeme";

# Location of your sendmail binary
$sendmail = "/usr/sbin/sendmail";

# What do you want to get?
# 1 = Mail out all unacknowledged log entries
# 2 = Mail out all entries since last email was sent
$mailwhat = 1;

# Do you want to automatically acknowledge entries that
# were sent out?  Yes=1, No=0
$autoack = 1;

# Database connection details for Myth DB
$dbhost = "localhost";
$dbname = "mythconverg";
$dbuser = "mythtv";
$dbpass = "mythtv";

##
## End of User-configurable settings
##

my @priorities = ("Emergency","Alert","Critical","Error","Warning","Notice",
                  "Info","Debug");
my $dbh = 
DBI->connect("dbi:mysql:database=$dbname:host=$dbhost","$dbuser","$dbpass") or
		 die "Cannot connect to database ($!)\n";
if ($mailwhat == 2) {
    $q = "select data from settings where value = 'LogLastEmail'";
    $sth = $dbh->prepare($q);
    $numrows = $sth->execute or die "Could not execute ($q)\n";
    if ($numrows!=0) {
        @row=$sth->fetchrow_array;
        $lastemail = 0 + $row[0];
    } else {
        $lastemail = 0;
    }
    ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = 
        localtime($lastemail);
    $lastdate = sprintf("%4d-%02d-%02dT%02d:%02d:%02d",$year+1900,$mon+1,
        $mday,$hour,$min,$sec);

    $where = "(logdate > '$lastdate')";
} else {
    $where = "(acknowledged = 0)";
}

$now = time();
$email = "";
$q = "select logid,module,priority,acknowledged,logdate,host," .
     "message,details from mythlog where $where order by logdate";
$sth = $dbh->prepare($q);
$numrows = $sth->execute or die "Could not execute ($q)\n";
while (@row = $sth->fetchrow_array) {
    $logid = $row[0];
    $module = $row[1];
    $priority = $row[2];
    $ack = $row[3];
    $logdate = $row[4];
    $host = $row[5];
    $message = $row[6];
    $details = $row[7];
    if ($mailwhat == 2) {
        if ($ack == 1) {
            $printack = "Acknowledged: Yes";
        } else {
            $printack = "Acknowledged: No";
        }
    } else {
        $printack = "";
    }

    $email .= sprintf("%-20s %-15s %s\n",$logdate,$host,$message);
    $email .= sprintf("    Module: %-20s Priority: %-12s %s\n\n",$module,$priority,
                      $printack);
}

if ($numrows == 0) {
    exit(0);
}

if ($autoack == 1) {
    $q = "update mythlog set acknowledged = 1 where $where";
    $sth = $dbh->prepare($q);
    $sth->execute or die "Could not execute ($q)\n";
}

if ($mailwhat == 2) {
    if ($lastemail == 0) {
        $q = "insert into settings (value,data) values ('LogLastEmail','$now')";
    } else {
        $q = "update settings set data='$now' where value='LogLastEmail'";
    }
    $sth = $dbh->prepare($q);
    $sth->execute or die "Could not execute ($q)\n";
}

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = 
        localtime($now);
$subject = sprintf("Myth Event Report for %4d-%02d-%02d",$year+1900,$mon+1,$mday);
open MAIL,"| $sendmail -t";
print MAIL "From: $mailfrom\n";
print MAIL "To: $mailto\n";
print MAIL "Subject: $subject\n\n";
print MAIL $email;
close MAIL;

exit(0);
