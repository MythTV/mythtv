#!/usr/bin/perl -w

use Getopt::Long;

GetOptions( "verbose!"     => \$verbose,
            "directory=s"  => \$directory,
            "file=s"       => \$file,
            "archivedir=s" => \$archiveDir
            );

#
# Values are: "DIRECTORY:FREESPACE"
#
# Free Space is in Megabytes
#
my( @ArchiveDirEntries ) = (
	"/usr2/video/archive/1/:3072",
	"/usr2/video/archive/2/:3072",
	"/usr2/video/archive/3/:3072",
	);

if ( ! $directory || ! $file ) {
	printf( "USAGE: myth_archive_job.pl --directory RECORDINGDIR --file " .
			"VIDEOFILE <OPTIONS>\n" );
	printf( "\n" );
	printf( "   OPTIONS:\n" );
	printf( "            --archivedir ARCHIVEDIR      Force archive to go to " .
			"ARCHIVEDIR\n" );
	exit(-1);
}

if ( ! -r "$directory/$file" ) {
	die( "ERROR: $directory/$file is not readable or does not exist: $!" );
}

if ( $verbose ) {
	printf( "Myth 'Archive' Script\n\n" );

	printf( "Dir      : %s\n", $directory );
	printf( "File     : %s\n", $file );

	printf( "\n" );
}

if ( $archiveDir ) {
	if (MoveFileToArchiveDir( $file, $directory, $archiveDir )) {
		exit(0);
	}
} else {
	my( $dirEntry);
	foreach $dirEntry ( @ArchiveDirEntries ) {
		my( $archiveDir, $keepFree ) = split( ':', $dirEntry );
		my( $freeSpace ) = GetFreeSpace( $archiveDir );
		my( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime,
			$mtime, $ctime, $blksize, $blocks) = stat("$directory/$file");

		$size = $size * 1.0 / 1024 / 1024;
		if ( $verbose ) {
			printf( "Checked %s and found %dMB free (we want to keep %dMB and need %dMB)\n",
				$archiveDir, $freeSpace, $keepFree, $size );
			printf( "\n" );
		}

		if (( $freeSpace - ($size / 1024.0 / 1024.0)) > $keepFree ) {
			if (MoveFileToArchiveDir( $file, $directory, $archiveDir )) {
				exit(0);
			}
		}
	}
}

if ( $verbose ) {
	printf( "ERROR: Unable to find a directory with enough free space!!\n" );
}

exit(-1);


# Don't like doing this, but it was easier than requiring Filesys::Statfs
sub GetFreeSpace {
	my( $dir ) = shift;

	if ( ! -r $dir ) {
		return 0;
	} else {
		my( $freeSpace ) = `df -m $dir | grep -v Available | awk '{print \$4}'`;

		return $freeSpace;
	}
}

sub MoveFileToArchiveDir {
	my( $file, $oldDir, $newDir ) = @_;

	$oldDir =~ s/\/$//;
	$newDir =~ s/\/$//;

	my( $old ) = "$oldDir/$file";
	my( $new ) = "$newDir/$file";

	if ( $verbose ) {
		printf( "Archiving from %s to %s\n", $old, $new );
	}

	# don't like doing this this way, but this is an example script that
	# people can enhance if they want
	my( $cmd ) = sprintf( "mv %s %s && ln -s %s %s", $old, $new, $new, $old );

	return (system( $cmd ) == 0);
}

