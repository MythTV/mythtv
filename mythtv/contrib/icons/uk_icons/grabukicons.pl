#!/usr/bin/perl -w
# grabukicons.pl
# By Justin Hornsby 19 October 2006
# 
# A script for downloading UK TV channel icons
#
# Usage info will be output if the script is run with no arguments (or insufficient arguments)
#

# PERL MODULES WE WILL BE USING
use DBI;
use DBD::mysql

#
$host     = '192.168.1.10';
$database = 'mythconverg';
$user     = 'mythtv';
$pass     = $user;

$db = undef;
$connect = undef;
my @name;
my @data;
my $item;

######################################
#                                    #
#        SUBROUTINES ARE HERE        #
#                                    #
######################################


# Try to find the database
#
sub findDatabase()
{
    my $found = 0;
    my @mysql = ('/usr/local/share/mythtv/mysql.txt',
        '/usr/share/mythtv/mysql.txt',
        '/etc/mythtv/mysql.txt',
        '/usr/local/etc/mythtv/mysql.txt',
        "$ENV{HOME}/.mythtv/mysql.txt",
        'mysql.txt'
    );
    foreach my $file (@mysql) {
        next unless (-e $file);
        $found = 1;
        open(CONF, $file) or die "Unable to open $file:  $!\n\n";
        while (my $line = <CONF>) {
            # Cleanup
            next if ($line =~ /^\s*#/);
            $line =~ s/^str //;
            chomp($line);

            # Split off the var=val pairs
            my ($var, $val) = split(/\=/, $line, 2);
            next unless ($var && $var =~ /\w/);
            if ($var eq 'DBHostName') {
                $host = $val;
            } elsif ($var eq 'DBUserName') {
                $user = $val;
            } elsif ($var eq 'DBName') {
                $database = $val;
            } elsif ($var eq 'DBPassword') {
                $pass = $val;
            }
        }
        close CONF;
    }

if ( ! $found )
    {   die "Unable to locate mysql.txt:  $!\n\n"  }
}


#  DB connect subroutine
# =======================
# openDatabase() - Connect or die
#

sub openDatabase()
{
    $db = DBI->connect("dbi:mysql:database=$database" .
                       ":host=$host", $user, $pass)
    or die "Cannot connect to database: $!\n\n";

    return $db;
}


$usage = "\nPlease pass the location of the channel icons as an argument (e.g. /usr/share/mythtv/icons) \n ";

# find the database

findDatabase();

# get this script's ARGS
#

# if user hasn't passed enough arguments, die and print the usage info

unless ($ARGV[0])
{
    die "$usage";
}

$location = $ARGV[0];

$connect = openDatabase(); 

$query = "select name from channel where name != NULL or name NOT like ".'""'.";";
$query_handle = $connect->prepare($query);
$query_handle->execute()  || die "Unable to query channel table";


open(FILE, "lookup.txt") or die("Unable to open file");
                                  @data = <FILE>;
                                  close(FILE);
 
foreach $chan (@data)
{
    @name_url = split('\|', $chan);
    $map{$name_url[0]} = $name_url[1];
}

while (@channame = $query_handle->fetchrow())
{
    foreach $id (@channame)
    {
        if ($map{$id})
        {
            $iconloc=$map{$id};
            $iconloc =~ s/\s+$//;
            if ($iconloc ne "none")
            { 
                @name = split('/', $iconloc);
                $num=@name;
                $filename=$name[$num-1];
                $command = "wget -q $iconloc -O $location/$filename";
                print "grabbing $iconloc ..";
                system ($command);
                print ".. done \n";
                $dbicon{$id} = $location."/".$filename;
            }
        } 
    }
}

print "Icons grabbed and downloaded to $location \n.  Updating the database....\n";
foreach $item (keys %dbicon) {
    print "setting channel $item to have icon from $dbicon{$item} \n";
    $query = "UPDATE channel SET icon = ".'"'.$dbicon{$item}.'"'." WHERE name = ".'"'.$item.'"'.";";
    $query_handle = $connect->prepare($query);
    $query_handle->execute()  || die "Unable to query channel table";
}
print "done! \n";
