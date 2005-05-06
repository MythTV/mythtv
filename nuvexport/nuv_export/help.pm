#!/usr/bin/perl -w
#Last Updated: 2005.02.16 (xris)
#
#  help.pm
#

    if (arg('mode')) {
        my $exporter = query_exporters($export_prog);
        print "No help for $export_prog/", arg('mode'), "\n\n";
    }

    if (arg('help') eq 'debug') {
        print "Please read https://svn.forevermore.net/nuvexport/wiki/debug\n";
    }

    else {
        print "Help section still needs to be updated.\n"
             ."   For now, use `man nuvexport` or visit the nuvexport "
             ."wiki at\n"
             ."   https://svn.forevermore.net/nuvexport/\n\n";
    }

    exit;

# vim:ts=4:sw=4:ai:et:si:sts=4
