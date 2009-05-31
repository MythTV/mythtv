#!/usr/bin/perl -w
#
# Tell MythTV to truly delete all recordings in the "Deleted" recgroup
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Libraries we'll use
    use English;
    use File::Basename;
    use Cwd 'abs_path';
    use lib '.', dirname(abs_path($0 or $PROGRAM_NAME));
    use MythTV;

# Connect to the backend
    my $myth = new MythTV();

# Load all of the recordings
    my %rows = $myth->backend_rows('QUERY_RECORDINGS Delete');

# Parse each recording, and delete anything in the "Deleted" recgroup
    my $i = 0;
    foreach my $row (@{$rows{'rows'}}) {
        my $show = $myth->new_recording(@$row);
        next unless ($show->{'recgroup'} eq 'Deleted');
        print "DELETE:  $show->{'title'}: $show->{'subtitle'}\n";
        my $err = $myth->backend_command(join($MythTV::BACKEND_SEP, 'DELETE_RECORDING', $show->to_string(), '0'));
        if ($err != -1) {
            print "  error:  $err\n";
        }
        else {
            $i++;
        }
    }

# Done
    if ($i > 0) {
        print
            "\n",
            "Depending on your configuration, it may be several minutes before\n",
            "MythTV completely flushes these recordings from your system.\n";
    }
    else {
        print "No recordings were found in the Deleted recording group.\n";
    }
