#
# $Date$
# $Revision$
# $Author$
#
#  export::ffmpeg::MP3
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::ffmpeg::MP3;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    add_arg('bitrate=i',    'Audio bitrate');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/\bmp3\b/i,
                     'name'     => 'Export to MP3',
                     'enabled'  => 1,
                     'errors'   => [],
                     'defaults' => {},
                    };
        bless($self, $class);

    # Initialize the default parameters
        $self->load_defaults();

    # Verify any commandline or config file options
        $self->{'bitrate'} = $self->val('a_bitrate') unless (defined $self->val('bitrate'));
        die "Bitrate must be > 0\n" unless (!defined $self->val('bitrate') || $self->{'bitrate'} > 0);

    # Initialize and check for transcode
        $self->init_ffmpeg(1);

    # Make sure that we have an mplexer
        find_program('id3tag')
            or push @{$self->{'errors'}}, 'You need id3tag to export an mp3.';

    # Can we even encode vcd?
        if (!$self->can_encode('mp3')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mp3 audio.";
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
    # MP3 bitrate
        $self->{'defaults'}{'bitrate'} = 128;
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings (skipping one parent, since we don't need the ffmpeg-specific options)
        $self->SUPER::gather_settings(1);
    # Audio Bitrate
        $self->{'bitrate'} = query_text('Audio bitrate?',
                                        'int',
                                        $self->val('bitrate'));
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Make sure we have finfo
        load_finfo($episode);
    # Build the ffmpeg string
        $self->{'ffmpeg_xtra'} = " -acodec mp3 -f mp3";
    # Execute ffmpeg
        $self->SUPER::export($episode, '.mp3');
    # Now tag it
        my $safe_title       = shell_escape($episode->{'title'});
        my $safe_channel     = shell_escape($episode->{'channel'});
        my $safe_description = shell_escape($episode->{'description'});
        my $safe_show_name   = shell_escape($episode->{'show_name'});
        my $safe_outfile     = shell_escape($self->get_outfile($episode, '.mp3'));
        my $command = "id3tag -A $safe_title -a $safe_channel -c $safe_description -s $safe_show_name $safe_outfile";
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
