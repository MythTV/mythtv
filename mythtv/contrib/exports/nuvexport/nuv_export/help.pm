# vim:ts=4:sw=4:ai:et:si:sts=4
#
# help
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

    if (arg('mode')) {
        my $exporter = query_exporters($export_prog);
        print "No help for $export_prog/", arg('mode'), "\n\n";
    }

    if (arg('help') eq 'debug') {
        print "Please read http://www.mythtv.org/wiki/index.php/Nuvexport#Debug_Mode\n";
    }

    else {
        print "Help section still needs to be updated.\n"
             ."   For now, please read /etc/nuvexportrc or the nuvexport wiki at\n"
             ."   http://mythtv.org/wiki/index.php/Nuvexport\n\n";
    }

    exit;
