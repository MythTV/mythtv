#Last Updated: 2004.11.22 (xris)
#
#  export::ASF
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ASF;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    $cli_args{'a_bitrate|a=i'}    = 1; # Audio bitrate
    $cli_args{'v_bitrate|v=i'}    = 1; # Video bitrate
    $cli_args{'height|v_res|h=i'} = 1; # Height
    $cli_args{'width|h_res|w=i'}  = 1; # Width

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\basf\b/i,
                     'name'            => 'Export to ASF',
                     'enabled'         => 1,
                     'errors'          => [],
                    # ffmpeg-related settings
                     'noise_reduction' => 1,
                     'deinterlace'     => 1,
                     'crop'            => 1,
                    # ASF-specific settings
                     'a_bitrate'       => 64,
                     'v_bitrate'       => 256,
                     'width'           => 320,
                     'height'          => 240,
                    };
        bless($self, $class);

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();

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
    # Ask the user what resolution he/she wants
        if ($Args{'width'}) {
            die "Width must be > 0\n" unless ($Args{'width'} > 0);
            $self->{'width'} = $Args{'width'};
        }
        else {
            $self->{'width'} = query_text('Width?',
                                          'int',
                                          $self->{'width'});
        }
        if ($Args{'height'}) {
            die "Height must be > 0\n" unless ($Args{'height'} > 0);
            $self->{'height'} = $Args{'height'};
        }
        else {
            $self->{'height'} = query_text('Height?',
                                           'int',
                                           $self->{'height'});
        }
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Load nuv info
        load_finfo($episode);
    # Build the ffmpeg string
        $self->{'ffmpeg_xtra'} = " -b "  . $self->{'v_bitrate'}
                               . " -vcodec msmpeg4"
                               . " -ab " . $self->{'a_bitrate'}
                               . " -acodec mp3"
                               . " -s "  . $self->{'width'} . "x" . $self->{'height'}
                               . " -f asf";
    # Execute the parent method
        $self->SUPER::export($episode, ".asf");
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
