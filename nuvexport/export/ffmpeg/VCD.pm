#!/usr/bin/perl -w
#Last Updated: 2005.02.15 (xris)
#
#  export::ffmpeg::VCD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ffmpeg::VCD;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bdvcd\b/i,
                     'name'            => 'Export to VCD',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Transcode-related settings
                     'noise_reduction' => 1,
                     'deinterlace'     => 1,
                     'crop'            => 1,
                    };
        bless($self, $class);

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
    # PAL or NTSC?
        my $standard = ($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? 'PAL' : 'NTSC';
        my $res = ($standard eq 'PAL') ? '352x288' : '352x240';
    # Build the transcode string
        $self->{'ffmpeg_xtra'}  = " -b 1150 -vcodec mpeg1video"
                                 ." -ab 224 -ar 44100 -acodec mp2"
                                 ." -aspect 4:3"
                                 ." -s $res -f vcd";
    # Execute the parent method
        $self->SUPER::export($episode, ".mpg");
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
