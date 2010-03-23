#
# $Date$
# $Revision$
# $Author$
#
#  export::ffmpeg::PSP
#
#   obtained and slightly modified from http://mysettopbox.tv/phpBB2/viewtopic.php?t=5030&
#

package export::ffmpeg::PSP;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    add_arg('psp_fps:s',        'PSP frame rate (high=29.97, low=14.985)');
    add_arg('psp_bitrate:s',    'PSP Resolution (high=768, low=384)');
    add_arg('psp_resolution:s', 'PSP Resolution (320x240, 368x208, 400x192)');
    add_arg('psp_thumbnail!',   'Create a thumbnail to go with the PSP video export?');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/\bpsp\b/i,
                     'name'     => 'Export to PSP',
                     'enabled'  => 1,
                     'errors'   => [],
                     'defaults' => {},
                    };
        bless($self, $class);

    # Initialize the default parameters
        $self->load_defaults();

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();

    # Can we even encode psp?
        if (!$self->can_encode('psp')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to psp video.";
        }
        if (!$self->can_encode('aac') || !$self->can_encode('libfaac')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to aac audio.";
        }
    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

# Load default settings
    sub load_defaults {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::load_defaults();
    # Not really anything to add
        $self->{'defaults'}{'psp_fps'}        = 'high';
        $self->{'defaults'}{'psp_bitrate'}    = 'high';
        $self->{'defaults'}{'psp_resolution'} = '320x240';
        $self->{'defaults'}{'psp_thumbnail'}  = 1;
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();

    # restrict the output FPS to one of the 2 valid for the PSP format
        while (1) {
            my $fps = query_text('Frame rate (high=29.97, low=14.985)?',
                                 'string',
                                 $self->val('psp_fps'));
            if ($fps =~ /^(h|29|30)/i) {
                $self->{'out_fps'} = 29.97;
            }
            elsif ($fps =~ /^(l|14|15)/i) {
                $self->{'out_fps'} = 14.985;
            }
            else {
                if ($is_cli) {
                    die "Frame rate for PSP must be high (29.97) or low (14.985)\n";
                }
                print "Unrecognized response.  Please try again.\n";
                next;
            }
            last;
        }

    # restrict the output resolution to one of the 3 valid for the PSP format
        while (1) {
            my $res = query_text('Resolution (320x240, 368x208, 400x192)?',
                                 'string',
                                 $self->val('psp_resolution'));
            if ($res =~ /^32/i) {
                $self->{'width'}  = 320;
                $self->{'height'} = 240;
            }
            elsif ($res =~ /^36/i) {
                $self->{'width'}  = 368;
                $self->{'height'} = 208;
            }
            elsif ($res =~ /^4/i) {
                $self->{'width'}  = 400;
                $self->{'height'} = 192;
            }
            else {
                if ($is_cli) {
                    die "Resolution for PSP must be 320x240, 368x208 or 400x192\n";
                }
                print "Unrecognized response.  Please try again.\n";
                next;
            }
            last;
        }

    # restrict the video bitrate to one of the 2 valid for the PSP format
        while (1) {
            my $btr = query_text('Video bitrate (high=768, low=384)?',
                                 'string',
                                 $self->val('psp_bitrate'));
            if ($btr =~ /^(h|7)/i) {
                $self->{'v_bitrate'} = 768;
            }
            elsif ($btr =~ /^(l|3)/i) {
                $self->{'v_bitrate'} = 384;
            }
            else {
                if ($is_cli) {
                    die "PSP bitrate must be high (768) or low (384)\n";
                }
                print "Unrecognized response.  Please try again.\n";
                next;
            }
            last;
        }

    # Offer to create a thumbnail for the user
        $self->{'psp_thumbnail'} = query_text('Create thumbnail for video?',
                                                'yesno',
                                                $self->val('psp_thumbnail'));

    # Let the user know how to install the files.
        print "\n\nIn order for the movie files to be of use on your PSP, you must\n",
              "copy the movie file (and thumbnail if present) to the PSP's memory\n",
              "stick in the following location (create the directories if necessary):\n\n",
              "  \\mp_root\\100mnv01\\ \n\n",
              "The movie must be renamed into the format M4V<5 digit number>.MP4.\n";
        if ($self->{'psp_thumbnail'}) {
            print "If you have a thumbnail, it should be named in the same way, but have\n",
                  "a THM file extension\n";
        }
        print "\n";
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Force to 4:3 aspect ratio
        $self->{'out_aspect'}       = 1.3333;
        $self->{'aspect_stretched'} = 1;
    # Audio codec name changes between ffmpeg versions
        my $acodec = $self->can_encode('libfaac') ? 'libfaac' : 'aac';
    # Build the ffmpeg string
        my $safe_title = shell_escape($episode->{'title'}.' - '.$episode->{'subtitle'});
        $self->{'ffmpeg_xtra'}  = $self->param('bit_rate', $self->{'v_bitrate'})
                                 .' -bufsize 65535'
                                 .$self->param('ab', 32)
                                 .' -acodec '.$acodec
                                 ." -f psp";
    # Execute the parent method
        $self->SUPER::export($episode, '.MP4');

    # Make the thumbnail if needed
        if ($self->{'psp_thumbnail'}) {
            my $ffmpeg = find_program('ffmpeg')
                        or die("where is ffmpeg, we had it when we did an ffmpeg_init?");

            $ffmpeg .= ' -y -i ' .shell_escape($self->get_outfile($episode, '.MP4'))
                      .' -s 160x90 -padtop 16 -padbottom 14 -r 1 -t 1'
                      .' -ss 7:00.00 -an -f mjpeg '
                      .shell_escape($self->get_outfile($episode, '.THM'));
            `$ffmpeg` unless ($DEBUG);

            if ($DEBUG) {
                print "\n$ffmpeg 2>&1\n";
            }
        }
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
