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
        'PREREQ_PM'     => {'IO::Socket'    => 0,
                            'Sys::Hostname' => 0,
                            'DBI'           => 0,
                            'DBD::mysql'    => 0,
                           },
        'NO_META'       => 1,
);



