#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV recording.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

package MythTV::Recording;
    use base 'MythTV::Program';
    use MythTV::StorageGroup;

# Required for checking byteorder when processing NuppelVideo files
    use Config;

# Sometimes used by get_pixmap()
    use File::Copy;
    use Sys::Hostname;

# Cache the results from find_program
    our %find_program_cache;

# Constructor
    sub new {
        my $class = shift;
        my $self  = { 'file_host'  => '',
                      'file_port'  => '',
                      'file_path'  => '',
                      'basename'   => '',
                      'local_path' => '',
                      'finfo'      => undef,
                    };
        bless($self, $class);

    # We need MythTV
        die "Please create a MythTV object before creating a $class object.\n" unless ($MythTV::last);

    # Var
        my $sh;

    # Load the parent module's settings
        $self->_parse_data(@_);

    # Load fields from the DB that don't get passed into the object
        $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT transcoder
                                                    FROM recorded
                                                   WHERE starttime  = FROM_UNIXTIME(?)
                                                         AND chanid = ?');
        $sh->execute($self->{'recstartts'}, $self->{'chanid'});
        ($self->{'transcoder'}) = $sh->fetchrow_array();
        $sh->finish;

    # These fields will only be set for recorded programs
        $self->{'cutlist'}         = '';
        $self->{'last_frame'}      = 0;
        $self->{'cutlist_frames'}  = 0;

    # Calculate the filesize
    # This should be deprecated (left here in early 0.24 beta in case I'm mistaken)
        #unless ($self->{'filesize'}) {
        #    $self->{'filesize'} = ($self->{'fs_high'} + ($self->{'fs_low'} < 0)) * 4294967296
        #                          + $self->{'fs_low'};
        #}
    # Pull the last known frame from the database, to help guestimate the
    # total frame count.
        $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT MAX(mark)
                                                    FROM recordedseek
                                                   WHERE chanid=? AND starttime=FROM_UNIXTIME(?)');
        $sh->execute($self->{'chanid'}, $self->{'recstartts'});
        ($self->{'last_frame'}) = $sh->fetchrow_array();
        $sh->finish();

    # Split the filename up into its respective parts
        if ($self->{'filename'} =~ m#myth://(.+?)(?::(\d+))?/(.*?)$#) {
            $self->{'file_host'} = $1;
            $self->{'file_port'} = ($2 or 6543);
            $self->{'basename'}  = $3;
        }
        else {
            $self->{'basename'}  = $self->{'filename'};
            $self->{'basename'} =~ s/.*\/([^\/]*)$/$1/;
        }

    # Pull the cutlist info from the database
        if ($self->{'has_cutlist'}) {
            my $last_mark = 0;
            $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT type, mark
                                                        FROM recordedmarkup
                                                       WHERE chanid=? AND starttime=FROM_UNIXTIME(?)
                                                             AND type IN (0,1)
                                                    ORDER BY mark');
            $sh->execute($self->{'chanid'}, $self->{'recstartts'});
            while (my ($type, $mark) = $sh->fetchrow_array) {
                if ($type == 1) {
                    $self->{'cutlist'} .= " $mark";
                    $last_mark          = $mark;
                }
                elsif ($type == 0) {
                    $self->{'cutlist'}        .= "-$mark";
                    $self->{'cutlist_frames'} += $mark - $last_mark;
                }
            }
            if ($type && $type == 1) {
                $info{'cutlist'}          .= '-'.$self->{'last_frame'};
                $self->{'cutlist_frames'} += $self->{'last_frame'} - $last_mark;
            }
            $sh->finish();
        }

    # File exists locally
        my $sgroup = new MythTV::StorageGroup();
        $self->{'local_path'} = $sgroup->FindRecordingFile($self->{'basename'});

    # Return
        return $self;
    }

# Load the credits
    sub load_credits {
        my $self = shift;
        $self->{'credits'} = ();
        my $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT credits.role, people.name
                                                       FROM people,
                                                            recordedcredits AS credits
                                                      WHERE credits.person        = people.person
                                                            AND credits.chanid    = ?
                                                            AND credits.starttime = FROM_UNIXTIME(?)
                                                   ORDER BY credits.role, people.name');
        $sh->execute($self->{'chanid'}, $self->{'starttime'});
        while (my ($role, $name) = $sh->fetchrow_array) {
            push @{$self->{'credits'}{$role}}, $name;
        }
        $sh->finish;
    }

# Pull the recording file details out of the file itself.  This is often too
# slow to run on each file at load time, so it is left to the program itself to
# figure out when is most appropriate to run it.
    sub load_file_info {
        my $self = shift;
    # Not a local file?
        return undef unless ($self->{'local_path'} && -e $self->{'local_path'});
    # Extract the info -- detect mpg from the filename
        if ($self->{'local_path'} =~ /\.mpe?g$/) {
            $self->{'finfo'} = $self->_mpeg_info();
        }
    # Probably nupplevideo, but fall back to mpeg just in case
        else {
            $self->{'finfo'} = ($self->_nuv_info()
                                or $self->_mpeg_info());
        }
    }

# Opens a .nuv file and returns information about it
    sub _nuv_info {
        my $self = shift;
        my $file = $self->{'local_path'};
        my(%info, $buffer);
    # open the file
        open(DATA, $file) or die "Can't open $file:  $!\n\n";
    # Read the file info header
        read(DATA, $buffer, 72);
    # Byte swap the buffer
        if ($Config{'byteorder'} == 4321) {
            substr($buffer, 20, 4) = byteswap32(substr($buffer, 20, 4));
            substr($buffer, 24, 4) = byteswap32(substr($buffer, 24, 4));
            substr($buffer, 28, 4) = byteswap32(substr($buffer, 28, 4));
            substr($buffer, 32, 4) = byteswap32(substr($buffer, 32, 4));
            substr($buffer, 40, 8) = byteswap64(substr($buffer, 40, 8));
            substr($buffer, 48, 8) = byteswap64(substr($buffer, 48, 8));
            substr($buffer, 56, 4) = byteswap32(substr($buffer, 56, 4));
            substr($buffer, 60, 4) = byteswap32(substr($buffer, 60, 4));
            substr($buffer, 64, 4) = byteswap32(substr($buffer, 64, 4));
            substr($buffer, 68, 4) = byteswap32(substr($buffer, 68, 4));
        }
    # Unpack the data structure
        ($info{'finfo'},          # "NuppelVideo" + \0
         $info{'version'},        # "0.05" + \0
         $info{'width'},
         $info{'height'},
         $info{'desiredheight'},  # 0 .. as it is
         $info{'desiredwidth'},   # 0 .. as it is
         $info{'pimode'},         # P .. progressive, I .. interlaced  (2 half pics) [NI]
         $info{'aspect'},         # 1.0 .. square pixel (1.5 .. e.g. width=480: width*1.5=720 for capturing for svcd material
         $info{'fps'},
         $info{'videoblocks'},    # count of video-blocks -1 .. unknown   0 .. no video
         $info{'audioblocks'},    # count of audio-blocks -1 .. unknown   0 .. no audio
         $info{'textsblocks'},    # count of text-blocks  -1 .. unknown   0 .. no text
         $info{'keyframedist'}
            ) = unpack('Z12 Z5 xxx i i i i a xxx d d i i i i', $buffer);
    # Is this even a NUV file?
        if ($info{'finfo'} !~ /MythTVVideo/) {
            close DATA;
            return undef;
        }
    # Perl occasionally over-reads on the previous read()
        seek(DATA, 72, 0);
    # Read and parse the first frame header
        read(DATA, $buffer, 12);
    # Byte swap the buffer
        if ($Config{'byteorder'} == 4321) {
            substr($buffer, 4, 4) = byteswap32(substr($buffer, 4, 4));
            substr($buffer, 8, 4) = byteswap32(substr($buffer, 8, 4));
        }
        my ($frametype,
            $comptype,
            $keyframe,
            $filters,
            $timecode,
            $packetlength) = unpack('a a a a i i', $buffer);
    # Parse the frame
        die "Illegal nuv file format:  $file\n\n" unless ($frametype eq 'D');
    # Read some more stuff if we have to
        read(DATA, $buffer, $packetlength) if ($packetlength);
    # Read the remaining frame headers
        while (12 == read(DATA, $buffer, 12)) {
        # Byte swap the buffer
            if ($Config{'byteorder'} == 4321) {
                substr($buffer, 4, 4) = byteswap32(substr($buffer, 4, 4));
                substr($buffer, 8, 4) = byteswap32(substr($buffer, 8, 4));
            }
        # Parse the frame header
            ($frametype,
             $comptype,
             $keyframe,
             $filters,
             $timecode,
             $packetlength) = unpack('a a a a i i', $buffer);
        # Read some more stuff if we have to
            read(DATA, $buffer, $packetlength) if ($packetlength);
        # Look for the audio frame
            if ($frametype eq 'X') {
            # Byte swap the buffer
                if ($Config{'byteorder'} == 4321) {
                    substr($buffer, 0, 4)  = byteswap32(substr($buffer, 0, 4));
                    substr($buffer, 12, 4) = byteswap32(substr($buffer, 12, 4));
                    substr($buffer, 16, 4) = byteswap32(substr($buffer, 16, 4));
                    substr($buffer, 20, 4) = byteswap32(substr($buffer, 20, 4));
                    substr($buffer, 24, 4) = byteswap32(substr($buffer, 24, 4));
                    substr($buffer, 28, 4) = byteswap32(substr($buffer, 28, 4));
                    substr($buffer, 32, 4) = byteswap32(substr($buffer, 32, 4));
                    substr($buffer, 36, 4) = byteswap32(substr($buffer, 36, 4));
                    substr($buffer, 40, 4) = byteswap32(substr($buffer, 40, 4));
                    substr($buffer, 44, 4) = byteswap32(substr($buffer, 44, 4));
                    substr($buffer, 48, 4) = byteswap32(substr($buffer, 48, 4));
                    substr($buffer, 52, 4) = byteswap32(substr($buffer, 52, 4));
                    substr($buffer, 56, 4) = byteswap32(substr($buffer, 56, 4));
                    substr($buffer, 60, 8) = byteswap64(substr($buffer, 60, 8));
                    substr($buffer, 68, 8) = byteswap64(substr($buffer, 68, 8));
                }
                my $frame_version;
                ($frame_version,
                 $info{'video_type'},
                 $info{'audio_type'},
                 $info{'audio_sample_rate'},
                 $info{'audio_bits_per_sample'},
                 $info{'audio_channels'},
                 $info{'audio_compression_ratio'},
                 $info{'audio_quality'},
                 $info{'rtjpeg_quality'},
                 $info{'rtjpeg_luma_filter'},
                 $info{'rtjpeg_chroma_filter'},
                 $info{'lavc_bitrate'},
                 $info{'lavc_qmin'},
                 $info{'lavc_qmax'},
                 $info{'lavc_maxqdiff'},
                 $info{'seektable_offset'},
                 $info{'keyframeadjust_offset'}
                 ) = unpack('ia4a4iiiiiiiiiiiill', $buffer);
            # Found the audio data we want - time to leave
                 last;
            }
        # Done reading frames - let's leave
            else {
                last;
            }
        }
    # Close the file
        close DATA;
    # Make sure some things are actually numbers
        $info{'width'}  += 0;
        $info{'height'} += 0;
    # HD fix
        if ($info{'height'} == 1080) {
            $info{'height'} = 1088;
        }
    # Make some corrections for myth bugs
        $info{'audio_sample_rate'} = 44100 if ($info{'audio_sample_rate'} == 42501 || $info{'audio_sample_rate'} =~ /^44\d\d\d$/);
        $info{'aspect'} = '4:3';
    # Cleanup
        $info{'aspect'}   = aspect_str($info{'aspect'});
        $info{'aspect_f'} = aspect_float($info{'aspect'});
    # Return
        return \%info;
    }

# Uses one of two mpeg info programs to load data about mpeg-based nuv files
    sub _mpeg_info {
        my $self = shift;
        my $file = $self->{'local_path'};
        $file =~ s/'/'\\''/sg;
        my %info;
    # First, we check for the existence of  an mpeg info program
        my $program = find_program('mplayer');
    # Nothing found?  Die
        die "You need mplayer to use this script on mpeg-based files.\n\n" unless ($program);
    # Grab the info we want from mplayer (go uber-verbose to override --really-quiet)
        my $idargs = "-v -v -v -v -nolirc -nojoystick -vo null -ao null -frames 1 -identify";
        my $data = `$program $idargs '$file' 2>/dev/null`;
        study $data;
        ($info{'video_type'})            = $data =~ m/^VIDEO:?\s*(MPEG[12]|H264)/m;
        ($info{'width'})                 = $data =~ m/^ID_VIDEO_WIDTH=0*([1-9]\d*)/m;
        ($info{'height'})                = $data =~ m/^ID_VIDEO_HEIGHT=0*([1-9]\d*)/m;
        ($info{'fps'})                   = $data =~ m/^ID_VIDEO_FPS=0*([1-9]\d*(?:\.\d+)?)/m;
        ($info{'audio_sample_rate'})     = $data =~ m/^ID_AUDIO_RATE=0*([1-9]\d*)/m;
        ($info{'audio_bitrate'})         = $data =~ m/^ID_AUDIO_BITRATE=0*([1-9]\d*)/m;
        ($info{'audio_bits_per_sample'}) = $data =~ m/^AUDIO:.+?ch,\s*[su](8|16)/mi;
        ($info{'audio_channels'})        = $data =~ m/^ID_AUDIO_NCH=0*([1-9]\d*)/m;
        ($info{'aspect'})                = $data =~ m/^ID_VIDEO_ASPECT=0*([1-9]\d*(?:[\.\,]\d+)?)/m;
        ($info{'audio_type'})            = $data =~ m/^ID_AUDIO_CODEC=0*([1-9]\d*(?:\.\d+)?)/m;
        ($info{'mpeg_stream_type'})      = $data =~ m/^ID_DEMUXER=(\w+)/mi;

    # Set the is_mpeg flag
        if ($info{'video_type'} eq "H264") {
            $info{'is_h264'} = 1;
        }
        else {
            $info{'is_mpeg'} = 1;
        }

    # Mplayer can't find the needed details.  Let's try again, forcing the use
    # of the ffmpeg lavf demuxer
        if (!defined($info{'width'}) || !defined($info{'audio_sample_rate'})) {
            my $altdata = `$program $idargs -demuxer lavf '$file' 2>/dev/null`;
            study $altdata;
            ($info{'width'})              = $altdata =~ m/^ID_VIDEO_WIDTH=0*([1-9]\d*)/m;
            ($info{'height'})             = $altdata =~ m/^ID_VIDEO_HEIGHT=0*([1-9]\d*)/m;
            ($info{'audio_bitrate'})      = $altdata =~ m/^ID_AUDIO_BITRATE=0*([1-9]\d*)/m;
            ($info{'audio_bits_per_sample'}) = $altdata =~ m/^AUDIO:.+?ch,\s*[su](8|16)/mi;
            ($info{'audio_sample_rate'})  = $altdata =~ m/^ID_AUDIO_RATE=0*([1-9]\d*)/m;
            ($info{'audio_channels'})     = $altdata =~ m/^ID_AUDIO_NCH=0*([1-9]\d*)/m;
            ($info{'aspect'})             = $altdata =~ m/^ID_VIDEO_ASPECT=0*([1-9]\d*(?:[\.\,]\d+)?)/m;
        } 

    # Stream type
        $info{'mpeg_stream_type'} = lc($info{'mpeg_stream_type'});
        if ($info{'mpeg_stream_type'} && $info{'mpeg_stream_type'} !~ /^mpeg/) {
            die "Stream type '$info{'mpeg_stream_type'}' is not an mpeg, and will\n"
               ."not work with this program.\n";
        }
    # Detect things the old way...
        elsif ($data =~ m/\bMPEG-(PE?S) file format detected/m) {
            $info{'mpeg_stream_type'} = lc($1);
        }
        elsif ($data =~ m/^TS file format detected/m) {
            $info{'mpeg_stream_type'} = 'ts';
        }
    # French localisation
        elsif ($data =~ m/Fichier de type MPEG-(PE?S) détecté./m) {
            $info{'mpeg_stream_type'} = lc($1);
        }
        elsif ($data =~ m/Fichier de type TS détecté./m) {
            $info{'mpeg_stream_type'} = 'ts';
        }
    # No matches on stream type?
        if (!$info{'mpeg_stream_type'}) {
            die "Unrecognized stream type.  Please execute the following and see if you\n"
               ."notice errors (make sure that you don't have the \"really quiet\" option\n"
               ."set in your mplayer config).  If not, create a ticket at\n"
               ."http://svn.mythtv.org/trac/newticket and attach the output from:\n\n"
               ."    $program -v -v -v -v -nolirc -nojoystick -vo null -ao null \\\n"
               ."             -frames 0 -identify '$file'\n\n";
        }
    # HD fix
        if ($info{'height'} == 1080) {
            $info{'height'} = 1088;
        }
    # mplayer is confused and we need to detect the aspect on our own
        if (!$info{'aspect'}) {
            if ($info{'height'} == 1088 || $info{'height'} == 720) {
                $info{'aspect'} = 16 / 9;
            }
            else {
                $info{'aspect'} = 4 / 3;
            }
        }
    # Cleanup
        $info{'aspect'}   = aspect_str($info{'aspect'});
        $info{'aspect_f'} = aspect_float($info{'aspect'});
    # Return
        return \%info;
    }

    sub aspect_str {
        my $aspect = shift;
    # Already in ratio format
        return $aspect if ($aspect =~ /^\d+:\d+$/);
    # European decimals...
        $aspect =~ s/\,/\./;
    # Parse out decimal formats
        if ($aspect == 1)           { return '1:1';    }
        elsif ($aspect =~ m/^1.3/)  { return '4:3';    }
        elsif ($aspect =~ m/^1.5$/) { return '3:2';    }
        elsif ($aspect =~ m/^1.55/) { return '14:9';   }
        elsif ($aspect =~ m/^1.7/)  { return '16:9';   }
        elsif ($aspect == 2.21)     { return '2.21:1'; }
    # Unknown aspect
        print STDERR "Unknown aspect ratio:  $aspect\n";
        return $aspect.':1';
    }

    sub aspect_float {
        my $aspect = shift;
    # European decimals...
        $aspect =~ s/\,/\./;
    # In ratio format -- do the math
        if ($aspect =~ /^\d+:\d+$/) {
            my ($w, $h) = split /:/, $aspect;
            return $w / $h;
        }
    # Parse out decimal formats
        if ($aspect eq '1')         { return  1;     }
        elsif ($aspect =~ m/^1.3/)  { return  4 / 3; }
        elsif ($aspect =~ m/^1.5$/) { return  3 / 2; }
        elsif ($aspect =~ m/^1.55/) { return 14 / 9; }
        elsif ($aspect =~ m/^1.7/)  { return 16 / 9; }
    # Unknown aspect
        return $aspect;
    }

# Get the modification date of the pixmap
    sub pixmap_last_mod {
        my $self = shift;
        my $mod = $self->{'_mythtv'}->backend_command(join($MythTV::BACKEND_SEP,
                                                           'QUERY_PIXMAP_LASTMODIFIED',
                                                           $self->to_string())
                                                     );
        if ($mod eq 'BAD') {
            return 0;
        }
        return $mod;
    }

# Generate a pixmap for this recording
    sub generate_pixmap {
        my $self = shift;
        my $ret = $self->{'_mythtv'}->backend_command(join($MythTV::BACKEND_SEP,
                                                           'QUERY_GENPIXMAP2',
                                                           "do_not_care",
                                                           $self->to_string())
                                                     );
        if ($ret eq 'ERROR') {
            print STDERR "Unknown error generating pixmap for $self->{'chanid'}:$self->{'starttime'}\n";
            return 0;
        }
        return 1;
    }

# Get a preview pixmap from the backend and store it into $fh/$path if
# appropriate.  Return values are:
#
# undef:  Error
# 1:      File copied into place
# 2:      File retrieved from the backend
# 3:      Current file is up to date
    sub get_pixmap {
        my $self       = shift;
        my $fh_or_path = shift;
    # Find out when the pixmap was last modified
        my $png_mod = $self->pixmap_last_mod();
    # Regenerate the pixmap if the recording has since been updated
        if ($png_mod < $self->{'lastmodified'}) {
            $png_mod = $self->{'lastmodified'};
            unless ($self->generate_pixmap()) {
                return undef;
            }
        }
    # Is our target file already up to date?
        my $mtime = (stat($fh_or_path))[9];
            $mtime ||= 0;
        if ($mtime >= $png_mod) {
            return 1;
        }
    # Now that we've done our best to avoid transfering the file, we can pull
    # it from the backend.
        return $self->{'_mythtv'}->stream_backend_file($self->{'filename'}.'.png',
                                                       $fh_or_path,
                                                       ($self->{'local_path'}
                                                            ? $self->{'local_path'}.'.png'
                                                            : undef),
                                                       $self->{'file_host'},
                                                       $self->{'file_port'});
    }

# Get the actual recording data from the backend and store it into $fh/$path as
# appropriate.  Return values are:
#
# undef:  Error
# 1:      File copied into place
# 2:      File retrieved from the backend
    sub get_data {
        my $self       = shift;
        my $fh_or_path = shift;
        my $seek       = (shift or 0);
    # Now we just pass the rest to the MythTV connection to stream the file.
        return $self->{'_mythtv'}->stream_backend_file($self->{'filename'},
                                                       $fh_or_path,
                                                       $self->{'local_path'},
                                                       $self->{'file_host'},
                                                       $self->{'file_port'},
                                                       $seek);
    }

# Find the requested program in the path.
# This searches the path for the specified programs, and returns the
#   lowest-index-value program found, caching the results
    sub find_program {
    # Get the hash id
        my $hash_id = join("\n", @_);
    # No cache?
        if (!defined($MythTV::Recording::find_program_cache{$hash_id})) {
        # Load the programs, and get a count of the priorities
            my (%programs, $num_programs);
            foreach my $program (@_) {
                $programs{$program} = ++$num_programs;
            }
        # No programs requested?
            return undef unless ($num_programs > 0);
        # Need a path?
            $ENV{'PATH'} ||= '/usr/local/bin:/usr/bin:/bin';
        # Search for the program(s)
            my %found;
            foreach my $path ('.', split(/:/, $ENV{'PATH'})) {
                foreach my $program (keys %programs) {
                    if (-e "$path/$program" && (!$found{'name'} || $programs{$program} < $programs{$found{'name'}})) {
                        $found{'name'} = $program;
                        $found{'path'} = $path;
                    }
                    elsif ($^O eq "darwin" && -e "$path/$program.app" && (!$found{'name'} || $programs{$program} < $programs{$found{'name'}})) {
                        $found{'name'} = $program;
                        $found{'path'} = "$path/$program.app/Contents/MacOS";
                    }
                # Leave early if we found the highest priority program
                    last if ($found{'name'} && $programs{$found{'name'}} == 1);
                }
            }
        # Set the cache
            $MythTV::Recording::find_program_cache{$hash_id} = ($found{'path'} && $found{'name'})
                                                                ? $found{'path'}.'/'.$found{'name'}
                                                                : '';
        }
    # Return
        return $MythTV::Recording::find_program_cache{$hash_id};
    }

# Return true
    1;
