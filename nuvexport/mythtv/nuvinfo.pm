#!/usr/bin/perl -w
#Last Updated: 2004.09.22 (xris)
#
#  mythtv::nuvinfo.pm
#
#   exports one routine:  nuv_info($path_to_nuv)
#   This routine inspects a specified nuv file, and returns information about
#   it, gathered either from its nuv file structure, or if it is actually an
#   mpeg file, from mpgtx or tcprobe.
#

package mythtv::nuvinfo;

    use nuv_export::shared_utils;

# Export the one routine that this package should export
    BEGIN {
        use Exporter;
        our @ISA = qw/ Exporter /;
        our @EXPORT = qw/ &nuv_info /;
    }

# Opens a .nuv file and returns information about it
    sub nuv_info {
        my $file = shift;
        my(%info, $buffer);
    # open the file
        open(DATA, $file) or die "Can't open $file:  $!\n\n";
    # Read the file info header
        read(DATA, $buffer, 72);
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
        return mpeg_info($file) unless ($info{'finfo'} =~ /MythTVVideo/);
#        return mpeg_info($file) unless ($info{'finfo'} =~ /\w/);
    # Perl occasionally over-reads on the previous read()
        seek(DATA, 72, 0);
    # Read and parse the first frame header
        read(DATA, $buffer, 12);
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
    # Make some corrections for a myth bug
        $info{'audio_sample_rate'} = 44100 if ($info{'audio_sample_rate'} == 42501 || $info{'audio_sample_rate'} =~ /^44\d\d\d$/);
    # Return
        return %info;
    }

# Uses one of two mpeg info programs to load data about mpeg-based nuv files
    sub mpeg_info {
        my $file = shift;
        $file =~ s/'/\\'/sg;
        my %info;
    # First, we check for the existence of  an mpeg info program
        my $program = find_program('tcprobe', 'mpgtx');
    # Nothing found?  Die
        die "You need tcprobe (transcode) or mpgtx to use this script on mpeg-based nuv files.\n\n" unless ($program);
    # Set the is_mpeg flag
        $info{'is_mpeg'}    = 1;
        $info{'video_type'} = 'MPEG';
    # Grab tcprobe info
        if ($program =~ /tcprobe$/) {
            my $data = `$program -i '$file' 2>/dev/null`;
            ($info{'width'}, $info{'height'}) = $data =~ /frame\s+size:\s+-g\s+(\d+)x(\d+)\b/m;
            ($info{'fps'})                  = $data =~ /frame\s+rate:\s+-f\s+(\d+(?:\.\d+)?)\b/m;
            ($info{'audio_sample_rate'}, $info{'audio_bits_per_sample'},
             $info{'audio_channels'}) = $data =~ /audio\s+track:.+?-e\s+(\d+),(\d+),(\d+)\b/m;
            if ($data =~ m/MPEG\s+(system|program)/i) {
                $info{'mpeg_stream_type'}  = "\L$1";
            }
        }
    # Grab tcmplex info
        elsif ($program =~ /mpgtx$/) {
            my $data = `$program -i '$file' 2>/dev/null`;
            ($info{'width'}, $info{'height'}, $info{'fps'}) = $data =~ /\bSize\s+\[(\d+)\s*x\s*(\d+)\]\s+(\d+(?:\.\d+)?)\s*fps/m;
            ($info{'audio_sample_rate'})                = $data =~ /\b(\d+)\s*Hz/m;
            if ($data =~ m/Mpeg\s+(\d)\s+(system|program)/i) {
                $info{'video_type'}       .= $1;
                $info{'mpeg_stream_type'}  = "\L$2";
            }
        }
    # Return
        return %info;
    }

# return true
1;

# vim:ts=4:sw=4:ai:et:si:sts=4
