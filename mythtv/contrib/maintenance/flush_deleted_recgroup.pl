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
    use Getopt::Long;
    use File::Basename;
    use Cwd 'abs_path';
    use lib '.', dirname(abs_path($0 or $PROGRAM_NAME));
    use MythTV;

# Some variables we'll use here
    our ($command, $force, $usage);

# Load the cli options
    GetOptions('force'                              => \$force,
               'usage|help|h'                       => \$usage
              );

# Print usage
    if ($usage)
    {
        print <<EOF;
Usage:
  $0 [options]

Deletes recording files and metadata for all recordings in the Deleted
recording group.

options:

--force

  Delete the recording metadata even if the file does not exist. Note that
  this option should only be used when all storage is connected to and file
  systems are mounted on all your backends. Otherwise, forcing deletion of the
  metadata may leave orphaned recording files on unconnected file systems.

  This option is primarily useful for deleting metadata for a large number of
  recordings after losing a file system or otherwise losing many recording
  files. Rather than deleting them one by one using mythfrontend and having to
  confirm deletion of each recording when the file is not found, you may add a
  bunch of recordings to a playlist, then choose MENU|Playlist Options|Change
  Recording Group and add the recordings to the Deleted recording group (create
  a new group if necessary--its name /must/ be Deleted and is case-sensitive).
  Finally, run this script with the --force option. Note that if you have
  enabled the setting, "Auto Expire Instead of Delete Recording," this process
  will delete all the recordings you just put into the Deleted recording group
  and any others that were in there from normal recording deletion.

--help

    Show this help text.

EOF
        exit;
    }

# Check whether to force metadata deletion for non-existent files.
    $argument_force = $force ? 'FORCE' : 'NO_FORCE';

# Connect to the backend
    my $myth = new MythTV();

# Load all of the recordings
    my %rows = $myth->backend_rows('QUERY_RECORDINGS Descending');

# Parse each recording, and delete anything in the "Deleted" recgroup
    my $i = 0;
    foreach my $row (@{$rows{'rows'}}) {
        my $show = $myth->new_recording(@$row);
        next unless ($show->{'recgroup'} eq 'Deleted');
        print "DELETE:  $show->{'title'}: $show->{'subtitle'}\n";

        my $delete_command = 'DELETE_RECORDING ' .
                             $show->{'chanid'} .
                             ' ' .
                             unix_to_myth_time ($show->{'recstartts'}) .
                             ' ' .
                             $argument_force;
        my $err = $myth->backend_command($delete_command);
        if ($err ne '-1') {
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
