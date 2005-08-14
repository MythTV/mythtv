#!/bin/bash
#
# $Date$
# $Revision$
# $Author$
#
# mythlink.sh
#
# Creates readable symlinks referring to MythTV recordings
#
# Based on the script mythlink.sh by Dale Gass
#
# Modified by Mike Dean
#  - Provides multiple easily-configurable "views"
#  - System specific information is specified as environment variables
#  - Uses '-' to separate portions of the filename (instead of '_' since '_'
#    is used to replace special characters (such as the space character))
#  - Shows recording year
#  - Shows end time
#  - Separates date and time info
#  - Removes trailing separator (i.e. for movies, which have no subtitle)
#  - Adds an optional extension (for specifying filetype to Windows)


### Modify these values for your installation ###
# The location of the MythTV Recordings (with "ugly" filenames)
MYTH_RECORDINGS_PATH=/var/storage/mythtv
# The path in which the views ("pretty" links) will be created
VIEWS_PATH=/var/storage/mythtv/views
# The extension added to the end of the links, including the period.
# For no extension, specify '' for the value.
EXTENSION='.mpg'
# Enables output for debugging (set to 0 for no output)
DEBUG=0

### The following directory names and formats may be customized ###

# Formats may consist of any combination of
# ${title}, ${subtitle}
# ${date}, ${starttime}, ${endtime}, ${originalairdate}
# ${category}, ${recgroup}

# Files will be sorted "alphabetically" so the appropriate fields on which you
# would like to sort should be placed at the beginning of the format

# Formats of the individual fields were chosen to minimize confusion caused by
# use of different sorting algorithms by clients (i.e. alphabetically versus
# alphanumerically).

# To add a custom format, simply include a directory name and format in the
# environment variables below.  (Whitespace separates the values.)

# The names of the directories containing the views
DIRECTORIES='time
             title
	     group
	     category
	     original_airdate'
# The formats used for the respective directories specified above
FORMATS='${date}-${starttime}-${endtime}-${title}-${subtitle}
         ${title}-${date}-${starttime}-${endtime}-${subtitle}
         ${recgroup}-${title}-${date}-${starttime}-${endtime}-${subtitle}
         ${category}-${title}-${date}-${starttime}-${endtime}-${subtitle}
         ${title}-${originalairdate}-${subtitle}'


### These values most likely do not need modification ###
# The name of the MythTV database
MYTH_DB=mythconverg
# The database username and password
MYTH_DB_USER=mythtv
MYTH_DB_PASSWD=mythtv


export MYTH_RECORDINGS_PATH VIEWS_PATH EXTENSION DEBUG DIRECTORIES FORMATS


for dir in ${DIRECTORIES}
  do
  rm -rf ${VIEWS_PATH}/${dir}/*
done
mysql -u${MYTH_DB_USER} -p${MYTH_DB_PASSWD} ${MYTH_DB} -B --exec "select chanid,starttime,endtime,title,subtitle,recgroup,category,originalairdate from recorded;" >/tmp/mythlink.$$
perl -w -e '
  my $mythpath=$ENV{"MYTH_RECORDINGS_PATH"};
  my $viewspath=$ENV{"VIEWS_PATH"};
  my $extension=$ENV{"EXTENSION"};
  my $debug=$ENV{"DEBUG"};
  my $dirs=$ENV{"DIRECTORIES"};
  my $fmts=$ENV{"FORMATS"};
  my @directories=split(/\s+/,$dirs);
  my @formats=split(/\s+/,$fmts);
  if (!-d ${viewspath}) {
    mkdir ${viewspath} or die "Failed to make directory: ${viewspath}\n";
  }
  foreach (@directories) {
    if (!-d "${viewspath}/$_") {
      mkdir "${viewspath}/$_" or die "Failed to make directory: ${viewspath}/$_\n";
    }
  }
  <>;
  while (<>) {
    chomp;
    my ($chanid,$start,$end,$title,$subtitle,$recgroup,$category,$originalairdate) = split /\t/;
    $start =~ s/[^0-9]//g;
    $end =~ s/[^0-9]//g;
    $subtitle = "" if(!defined $subtitle);
    my $filename = "${chanid}_${start}_${end}.nuv";
    do { print "Skipping ${mythpath}/${filename}\n"; next } unless -e "${mythpath}/${filename}";
    $end =~ /^........(....)/;
    my $endtime = $1;
    $start =~ /^(........)(....)/;
    my $date = $1;
    my $starttime = $2;
    $originalairdate =~ s/[^0-9]//g;
    $originalairdate = "00000000" if(($originalairdate eq ""));
    for ($i = 0; $i < @directories; $i++) {
      my $directory = $directories[$i];
      my $link=$formats[$i];
      $link =~ s/(\${\w+})/$1/gee;
      $link =~ s/-$//;
      $link =~ s/ /_/g;
      $link =~ s/&/+/g;
      $link =~ s/[^+0-9a-zA-Z_-]+/_/g;
      $link = $link . $extension;
      print "Creating $link\n" if ($debug);
      unlink "${viewspath}/${directory}/${link}" if(-e "${viewspath}/${directory}/${link}");
      symlink "${mythpath}/${filename}", "${viewspath}/${directory}/${link}" or die "Failed to create symlink ${viewspath}/${directory}/${link}: $!";
    }
  }
' /tmp/mythlink.$$
rm /tmp/mythlink.$$


