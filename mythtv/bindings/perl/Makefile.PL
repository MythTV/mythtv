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
        'VERSION_FROM'  => 'MythTV.pm',
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
        'PMLIBDIRS'     => [ 'MythTV', 'IO' ],
        'NO_META'       => 1,
);




