#!/usr/bin/perl -w
#Last Updated: 2005.03.07 (xris)
#
#  export::ffmpeg::DVD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::ffmpeg::DVD;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    add_arg('quantisation|q=i', 'Quantisation');
    add_arg('a_bitrate|a=i',    'Audio bitrate');
    add_arg('v_bitrate|v=i',    'Video bitrate');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/dvd/i,
                     'name'     => 'Export to DVD',
                     'enabled'  => 1,
                     'errors'   => [],
                     'defaults' => {},
                    };
        bless($self, $class);

    # Initialize the default parameters
        $self->load_defaults();

    # Verify any commandline or config file options
        die "Audio bitrate must be > 0\n" unless (!defined $self->val('a_bitrate') || $self->{'a_bitrate'} > 0);
        die "Video bitrate must be > 0\n" unless (!defined $self->val('v_bitrate') || $self->{'v_bitrate'} > 0);

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();

    # Can we even encode dvd?
        if (!$self->can_encode('mpeg2video')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to mpeg2video.";
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
    # Add the dvd preferred bitrates
        $self->{'defaults'}{'quantisation'} = 5;
        $self->{'defaults'}{'a_bitrate'}    = 384;
        $self->{'defaults'}{'v_bitrate'}    = 6000;
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();
    # Audio Bitrate
        while (1) {
            my $a_bitrate = query_text('Audio bitrate?',
                                       'int',
                                       $self->val('a_bitrate'));
            if ($a_bitrate < 64) {
                print "Too low; please choose a bitrate between 64 and 384.\n";
            }
            elsif ($a_bitrate > 384) {
                print "Too high; please choose a bitrate between 64 and 384.\n";
            }
            else {
                $self->{'a_bitrate'} = $a_bitrate;
                last;
            }
        }
    # Ask the user what video bitrate he/she wants, or calculate the max bitrate
        my $max_v_bitrate = 9500 - $self->{'a_bitrate'};
        if ($self->val('v_bitrate') > $max_v_bitrate) {
             $self->{'v_bitrate'} = $max_v_bitrate;
        }
        while (1) {
            my $v_bitrate = query_text('Maximum video bitrate for VBR?',
                                       'int',
                                       $self->val('v_bitrate'));
            if ($v_bitrate < 1000) {
                print "Too low; please choose a bitrate between 1000 and $max_v_bitrate.\n";
            }
            elsif ($v_bitrate > $max_v_bitrate) {
                print "Too high; please choose a bitrate between 1000 and $max_v_bitrate.\n";
            }
            else {
                $self->{'v_bitrate'} = $v_bitrate;
                last;
            }
        }
    # Ask the user what vbr quality (quantisation) he/she wants - 1..31
        while (1) {
            my $quantisation = query_text('VBR quality/quantisation (1-31)?',
                                          'float',
                                          $self->val('quantisation'));
            if ($quantisation < 1) {
                print "Too low; please choose a number between 1 and 31.\n";
            }
            elsif ($quantisation > 31) {
                print "Too high; please choose a number between 1 and 31\n";
            }
            else {
                $self->{'quantisation'} = $quantisation;
                last;
            }
        }
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
        $self->{'width'} = 720;
        $self->{'height'} = ($standard eq 'PAL') ? '576' : '480';
        $self->{'out_fps'} = ($standard eq 'PAL') ? 25 : 29.97;
    # Build the ffmpeg string
        $self->{'ffmpeg_xtra'} = ' -b ' . $self->{'v_bitrate'}
                               . ' -vcodec mpeg2video'
                               . ' -qmin ' . $self->{'quantisation'}
                               . ' -ab ' . $self->{'a_bitrate'}
                               . " -ar 48000 -acodec mp2 -f dvd";
    # Execute the parent method
        $self->SUPER::export($episode, ".mpg");
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
