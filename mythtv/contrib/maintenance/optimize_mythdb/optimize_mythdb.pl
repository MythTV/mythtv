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

# Variables
    my $logFileName = "/var/log/mythtv/optimize_mythdb.log";
    my $startTime = localtime;
    my $rc = 0;

# If STDIN isn't a terminal, send output to a log file
    if (! -t STDIN) {
        open(STDOUT, ">> $logFileName") || croak "Unable to open log file '$logFileName': $!";
    }

# Start a new session in the log file
    print("\n");
    print("\n");
    print("---- optimize_mythdb.pl started $startTime ----\n");
    print("\n");

# Connect to mythbackend
    my $Myth = new MythTV({'connect' => 0});

# Connect to the database
    $dbh = $Myth->{'dbh'};

# Repair and optimize each table
    foreach $table ($dbh->tables) {

        print("$table: ");

        if ($dbh->do("REPAIR TABLE $table")) {
            print("repaired");
        } else {
            print("ERROR - unable to repair: $dbh->errstr\n");
            $rc = 1;
            next;
        }

        if ($dbh->do("OPTIMIZE TABLE $table")) {
            print(", optimized");
        } else {
            print(", ERROR - unable to optimize: $dbh->errstr\n");
            $rc = 1;
            next;
        }

        if ($dbh->do("ANALYZE TABLE $table")) {
            print(", analyzed");
        } else {
            print(", ERROR - unable to analyze: $dbh->errstr\n");
            $rc = 1;
            next;
        }
        
        print("\n");
    }

# Close output
    close(STDOUT);

# Exit
    exit($rc);
