#!/usr/bin/perl -w
#
# Makefile for MythTV perl modules
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

use ExtUtils::MakeMaker;

WriteMakefile(
        'NAME'          => 'MythTV',
        'VERSION_FROM'  => 'MythTV/Version.pm',
        'AUTHOR'        => 'Chris Petersen <mythtv@forevermore.net>',
        'PREREQ_PM'     => {'Exporter'         => 0,
                            'Config'           => 0,
                            'DBI'              => 0,
                            'DBD::mysql'       => 0,
                            'Fcntl'            => 0,
                            'File::Copy'       => 0,
                            'HTTP::Request'    => 0,
                            'IO::Socket::INET' => 0,
                            'LWP::UserAgent'   => 0,
                            'Sys::Hostname'    => 0,
                           },
        'PM'            => {'MythTV.pm'                => 'blib/lib/MythTV.pm',
                            'MythTV/Channel.pm'        => 'blib/lib/MythTV/Channel.pm',
                            'MythTV/Program.pm'        => 'blib/lib/MythTV/Program.pm',
                            'MythTV/Recording.pm'      => 'blib/lib/MythTV/Recording.pm',
                            'MythTV/StorageGroup.pm'   => 'blib/lib/MythTV/StorageGroup.pm',
                            'MythTV/Version.pm'        => 'blib/lib/MythTV/Version.pm',
                            'IO/Socket/INET/MythTV.pm' => 'blib/lib/IO/Socket/INET/MythTV.pm',
                           },
        'NO_META'       => 1,
);




