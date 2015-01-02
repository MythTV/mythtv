#!/usr/bin/perl -w
#
# Connects to the mythtv database and repairs/optimizes the tables that it
# finds.  Suggested use is to cron it to run once per day.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @license   GPL
#

# Includes
    use DBI;
    use MythTV;
    use Carp;
    use IO::File;

# Variables
    my $logFileName = "/var/log/mythtv/optimize_mythdb.log";
    my $startTime = localtime;
    my $rc = 0;

# Open log file
    my $logFileHandle = new IO::File ">> $logFileName";
    if (!defined $logFileHandle) {
        croak "Unable to open log file '$logFileName': $!";
    }

# Start a new session in the log file
    $logFileHandle->print("\n");
    $logFileHandle->print("\n");
    $logFileHandle->print("---- optimize_mythdb.pl started $startTime ----\n");
    $logFileHandle->print("\n");

# Connect to mythbackend
    my $Myth = new MythTV({'connect' => 0});

# Connect to the database
    $dbh = $Myth->{'dbh'};

# Repair and optimize each table
    foreach $table ($dbh->tables) {

        $logFileHandle->print("$table: ");

        if ($dbh->do("REPAIR TABLE $table")) {
            $logFileHandle->print("repaired");
        } else {
            $logFileHandle->print("ERROR - unable to repair: $dbh->errstr\n");
            $rc = 1;
            next;
        }

        if ($dbh->do("OPTIMIZE TABLE $table")) {
            $logFileHandle->print(", optimized");
        } else {
            $logFileHandle->print(", ERROR - unable to optimize: $dbh->errstr\n");
            $rc = 1;
            next;
        }

        if ($dbh->do("ANALYZE TABLE $table")) {
            $logFileHandle->print(", analyzed");
        } else {
            $logFileHandle->print(", ERROR - unable to analyze: $dbh->errstr\n");
            $rc = 1;
            next;
        }
        
        $logFileHandle->print("\n");
    }

# Close log file
    $logFileHandle->close;

# Exit
    exit($rc);
