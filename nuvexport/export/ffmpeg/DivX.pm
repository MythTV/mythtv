#Last Updated: 2005.02.15 (xris)
#
#  export::ffmpeg::DivX
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ffmpeg::DivX;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    add_arg('a_bitrate|a=i',    'Audio bitrate');
    add_arg('v_bitrate|v=i',    'Video bitrate');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bdivx\b/i,
                     'name'            => 'Export to DivX',
                     'enabled'         => 1,
                     'errors'          => [],
                    # ffmpeg-related settings
                     'noise_reduction' => 1,
                     'deinterlace'     => 1,
                     'crop'            => 1,
                    # DivX-specific settings
                     'a_bitrate'       => 128,
                     'v_bitrate'       => 960,
                     'width'           => 624,
                     'height'          => 464,
                    };
        bless($self, $class);

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();
    # Can we even encode divx?
        if (!$self->can_encode('mpeg4')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mpeg4.";
        }
        if (!$self->can_encode('mp3')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mp3 audio.";
        }
    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();

    # Audio Bitrate
        if ($Args{'a_bitrate'}) {
            $self->{'a_bitrate'} = $Args{'a_bitrate'};
            die "Audio bitrate must be > 0\n" unless ($Args{'a_bitrate'} > 0);
        }
        else {
            $self->{'a_bitrate'} = query_text('Audio bitrate?',
                                              'int',
                                              $self->{'a_bitrate'});
        }
    # Ask the user what video bitrate he/she wants
        if ($Args{'v_bitrate'}) {
            die "Video bitrate must be > 0\n" unless ($Args{'v_bitrate'} > 0);
            $self->{'v_bitrate'} = $Args{'v_bitrate'};
        }
        elsif ($self->{'multipass'} || !$self->{'vbr'}) {
            # make sure we have v_bitrate on the commandline
            $self->{'v_bitrate'} = query_text('Video bitrate?',
                                              'int',
                                              $self->{'v_bitrate'});
        }
    # Query the resolution
        $self->query_resolution();
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Load nuv info
        load_finfo($episode);
    # Build the ffmpeg string
        $self->{'ffmpeg_xtra'} = " -b "  . $self->{'v_bitrate'}
                               . " -vcodec mpeg4"
                               . " -ab " . $self->{'a_bitrate'}
                               . " -acodec mp3"
                               . " -s "  . $self->{'width'} . "x" . $self->{'height'}
                               ;
    # Execute the parent method
        $self->SUPER::export($episode, ".avi");
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
