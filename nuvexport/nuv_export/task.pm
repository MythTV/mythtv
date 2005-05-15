#
# $Date$
# $Revision$
# $Author$
#
#  nuv_export::task
#

package nuv_export::task;

# Load the myth and nuv utilities, and connect to the database
    use nuvexport::shared_utils;
    use mythtv::db;
    use mythtv::recordings;

    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;

        our @EXPORT = qw/ &transcode &system /;
    }



# vim:ts=4:sw=4:ai:et:si:sts=4
