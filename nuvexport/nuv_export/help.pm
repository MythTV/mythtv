#!/usr/bin/perl -w
#Last Updated: 2005.02.16 (xris)
#
#  help.pm
#

    if (arg('mode')) {
        my $exporter = query_exporters($export_prog);
        print "No help for $export_prog/", arg('mode'), "\n\n";
    }

    print "Help section still needs to be updated.\n"
         ."   For now, use `man nuvexport`\n\n";

    exit;
