#!/usr/bin/perl -w

my $verbose = 1;
my $directory;
my $file;
my $title = "";    # for display purposes only
my $archiveDir;

use Getopt::Long;

GetOptions( "verbose!"     => \$verbose,
            "directory=s"  => \$directory,
            "file=s"       => \$file,
            "title=s"      => \$title,
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

my( $dev, $ino, $mode, $nlink, $uid, $gid, $rdev, $size, $atime,
	$mtime, $ctime, $blksize, $blocks) = stat("$directory/$file");
$size = $size * 1.0 / 1024 / 1024;

if ( $verbose ) {
	printf( "+-----------------------+\n" );
	printf( "| Myth 'Archive' Script |\n" );
	printf( "+-----------------------+\n" );
	printf( "Title     : %s\n", $title ) if ($title ne "");
	printf( "Source Dir: %s\n", $directory );
	printf( "Filename  : %s\n", $file );
	printf( "Filesize  : %d MB\n", $size );

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

		if ( $verbose ) {
			printf( "Trying Dir: %s\n", $archiveDir );
			printf( "    Keep Free: %6d MB\n", $keepFree );
			printf( "    Curr Free: %6d MB\n", $freeSpace );
		}

		if (( $freeSpace - ($size / 1024.0 / 1024.0)) > $keepFree ) {
			printf( "Attempting archive to %s\nStatus: ", $archiveDir );
			if (MoveFileToArchiveDir( $file, $directory, $archiveDir )) {
				printf( "Success.\n" );
				exit(0);
			}
			printf( "ERROR!\n" );
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

	if ( -l $old ) {
		printf( "ERROR: The original '$file' is already a link, will not archive to '$new'!\n" );
		return(1);
	}

	# don't like doing this this way, but this is an example script that
	# people can enhance if they want
	my( $cmd ) = sprintf( "mv %s %s && ln -s %s %s", $old, $new, $new, $old );

	#printf( "Archiving by running '%s'\n", $cmd );

	return (system( $cmd ) == 0);
}

