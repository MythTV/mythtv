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

    # Load the parent module's settings
        $self->SUPER::_parse_data(@_);

    # These fields will only be set for recorded programs
        $self->{'cutlist'}         = '';
        $self->{'last_frame'}      = 0;
        $self->{'cutlist_frames'}  = 0;

    # Is this a previously-recorded program?
        if ($self->{'filename'}) {
            my $sh;
        # Calculate the filesize
            $self->{'filesize'} = ($self->{'fs_high'} + ($self->{'fs_low'} < 0)) * 4294967296
                                  + $self->{'fs_low'};
        # Pull the last known frame from the database, to help guestimate the
        # total frame count.
            $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT MAX(mark) FROM recordedseek WHERE chanid=? AND starttime=FROM_UNIXTIME(?)');
            $sh->execute($self->{'chanid'}, $self->{'recstartts'});
            ($self->{'last_frame'}) = $sh->fetchrow_array();
            $sh->finish();
        # Split the filename up into its respective parts
            if ($self->{'filename'} =~ m#myth://(.+?)(?::(\d+))?/(.*?)$#) {
                $self->{'file_host'} = $1;
                $self->{'file_port'} = ($2 or 6543);
                $self->{'basename'}  = $3;
            }
        }

    # Pull the cutlist info from the database
        if ($self->{'has_cutlist'}) {
            my $last_mark = 0;
            $sh = $self->{'_mythtv'}{'dbh'}->prepare('SELECT type, mark FROM recordedmarkup WHERE chanid=? AND starttime=FROM_UNIXTIME(?) AND type IN (0,1) ORDER BY mark');
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
        if ($self->{'_mythtv'}{'video_dir'}) {
            $self->{'local_path'} = $self->{'_mythtv'}{'video_dir'}.'/'.$self->{'basename'};
            $self->{'local_path'} = '' unless (-e $self->{'local_path'});
        }

    # Return
        return $self;
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
        my $program = MythTV::find_program('mplayer');
    # Nothing found?  Die
        die "You need mplayer to use this script on mpeg-based files.\n\n" unless ($program);
    # Set the is_mpeg flag
        $info{'is_mpeg'} = 1;
    # Grab the info we want from mplayer (go uber-verbose to override --really-quiet)
        my $data = `$program -v -v -v -v -nolirc -nojoystick -vo null -ao null -frames 0 -identify '$file' 2>/dev/null`;
        study $data;
        ($info{'video_type'})            = $data =~ m/^VIDEO:\s*(MPEG[12])/m;
        ($info{'width'})                 = $data =~ m/^ID_VIDEO_WIDTH=(\d+)/m;
        ($info{'height'})                = $data =~ m/^ID_VIDEO_HEIGHT=(\d+)/m;
        ($info{'fps'})                   = $data =~ m/^ID_VIDEO_FPS=(\d+(?:\.\d*)?)/m;
        ($info{'audio_sample_rate'})     = $data =~ m/^ID_AUDIO_RATE=(\d+)/m;
        ($info{'audio_bitrate'})         = $data =~ m/^ID_AUDIO_BITRATE=(\d+)/m;
        ($info{'audio_bits_per_sample'}) = $data =~ m/^AUDIO:.+?ch,\s*[su](8|16)/mi;
        ($info{'audio_channels'})        = $data =~ m/^ID_AUDIO_NCH=(\d+)/m;
        ($info{'fps'})                   = $data =~ m/^ID_VIDEO_FPS=(\d+(?:\.\d*)?)/m;
        ($info{'aspect'})                = $data =~ m/^ID_VIDEO_ASPECT=(\d+(?:[\.\,]\d*)?)/m;
        ($info{'audio_type'})            = $data =~ m/^ID_AUDIO_CODEC=(\d+(?:\.\d*)?)/m;
        ($info{'mpeg_stream_type'})      = $data =~ m/^ID_DEMUXER=(\w+)/mi;
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
        if ($aspect == 1)          { return '1:1';    }
        elsif ($aspect =~ m/^1.3/) { return '4:3';    }
        elsif ($aspect =~ m/^1.7/) { return '16:9';   }
        elsif ($aspect == 2.21)    { return '2.21:1'; }
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
        if ($aspect == 1)          { return  1;     }
        elsif ($aspect =~ m/^1.3/) { return  4 / 3; }
        elsif ($aspect =~ m/^1.7/) { return 16 / 9; }
    # Unknown aspect
        return $aspect;
    }

# Return true
    1;