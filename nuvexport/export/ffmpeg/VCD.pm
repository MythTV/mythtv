#!/usr/bin/perl -w
#Last Updated: 2005.03.29 (xris)
#
#  export::ffmpeg::VCD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ffmpeg::VCD;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/\bvcd\b/i,
                     'name'     => 'Export to VCD',
                     'enabled'  => 1,
                     'errors'   => [],
                     'defaults' => {},
                    };
        bless($self, $class);

    # Initialize the default parameters
        $self->load_defaults();

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();

    # Can we even encode vcd?
        if (!$self->can_encode('mpeg1video')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mpeg1video.";
        }
        if (!$self->can_encode('mp2')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mp2 audio.";
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
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Load nuv info
        load_finfo($episode);
    # Force to 4:3 aspect ratio
        $self->{'out_aspect'} = 1.3333;
        $self->{'aspect_stretched'} = 1;
    # PAL or NTSC?
        my $standard = ($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? 'PAL' : 'NTSC';
        $self->{'width'} = 352;
        $self->{'height'} = ($standard eq 'PAL') ? '288' : '240';
        $self->{'out_fps'} = ($standard eq 'PAL') ? 25 : 29.97;
    # Build the transcode string
        $self->{'ffmpeg_xtra'}  = " -b 1150 -vcodec mpeg1video"
                                 ." -ab 224 -ar 44100 -acodec mp2"
                                 ." -f vcd";
    # Execute the parent method
        $self->SUPER::export($episode, ".mpg");
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
