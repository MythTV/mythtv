#
# ffmpeg-based XviD export module for nuvexport.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
# @copyright Silicon Mechanics
#

package export::ffmpeg::XviD;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::cli;
    use nuv_export::ui;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    add_arg('quantisation|q=i', 'Quantisation');
    add_arg('a_bitrate|a=i',    'Audio bitrate');
    add_arg('v_bitrate|v=i',    'Video bitrate');
    add_arg('multipass!',       'Enable two-pass encoding.');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/\bxvid\b/i,
                     'name'     => 'Export to XviD',
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
        die "Width must be > 0\n"         unless (!defined $self->val('width')     || $self->{'width'} =~ /^\s*\D/  || $self->{'width'}  > 0);
        die "Height must be > 0\n"        unless (!defined $self->val('height')    || $self->{'height'} =~ /^\s*\D/ || $self->{'height'} > 0);

    # VBR, multipass, etc.
        if ($self->val('multipass')) {
            $self->{'vbr'} = 1;
        }
        elsif ($self->val('quantisation')) {
            die "Quantisation must be a number between 1 and 31 (lower means better quality).\n" if ($self->{'quantisation'} < 1 || $self->{'quantisation'} > 31);
            $self->{'vbr'} = 1;
        }

    # Initialize and check for ffmpeg
        $self->init_ffmpeg();

    # Can we even encode xvid?
        if (!$self->can_encode('xvid') && !$self->can_encode('libxvid')) {
            push @{$self->{'errors'}}, "Your ffmpeg installation doesn't support encoding to xvid.\n"
                                      ."  (It must be compiled with the --enable-libxvid option)";
        }
        if (!$self->can_encode('mp3') && !$self->can_encode('libmp3lame')) {
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
    # Default bitrates and resolution
        $self->{'defaults'}{'a_bitrate'} = 128;
        $self->{'defaults'}{'v_bitrate'} = 960;
        $self->{'defaults'}{'width'}     = 624;
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();
    # Audio Bitrate
        $self->{'a_bitrate'} = query_text('Audio bitrate?',
                                          'int',
                                          $self->val('a_bitrate'));
    # VBR options
        if (!$is_cli) {
            $self->{'vbr'} = query_text('Variable bitrate video?',
                                        'yesno',
                                        $self->val('vbr'));
            if ($self->{'vbr'}) {
                $self->{'multipass'} = query_text('Multi-pass (slower, but better quality)?',
                                                  'yesno',
                                                  $self->val('multipass'));
                if (!$self->{'multipass'}) {
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
            } else {
                $self->{'multipass'} = 0;
            }
        # Ask the user what video bitrate he/she wants
            $self->{'v_bitrate'} = query_text('Video bitrate?',
                                              'int',
                                              $self->val('v_bitrate'));
        }
    # Query the resolution
        $self->query_resolution();
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Make sure we have the framerate
        $self->{'out_fps'} = $episode->{'finfo'}{'fps'};
    # Embed the title
        $safe_title = $episode->{'title'};
        if ($episode->{'subtitle'} ne 'Untitled') {
            $safe_title .= ' - '.$episode->{'subtitle'};
        }
        my $safe_title = shell_escape($safe_title);
    # Codec name changes between ffmpeg versions
        my $codec = $self->can_encode('libxvid') ? 'libxvid' : 'xvid';
    # Build the common ffmpeg string
        my $ffmpeg_xtra  = ' -vcodec '.$codec
                          .$self->param('bit_rate', $self->{'v_bitrate'})
                          .($self->{'vbr'}
                            ? $self->param('rc_min_rate', 32)
                             . $self->param('rc_max_rate', (2 * $self->{'v_bitrate'}))
                             . $self->param('bit_rate_tolerance', 32)
                             . ' -bufsize 65535'
                            : '')
                          .' -flags +mv4+loop+aic+cgop'
                          .' -trellis 1'
                          .' -mbd 1'
                          .' -cmp 2 -subcmp 2'
                           .$self->param('b_quant_factor',     150)
                           .$self->param('b_quant_offset',     100)
                           .$self->param('max_b_frames',       1)
                          ;
    # Dual pass?
        if ($self->{'multipass'}) {
        # Add the temporary file to the list
            push @tmpfiles, "/tmp/xvid.$$.log";
        # First pass
            print "First pass...\n";
            $self->{'ffmpeg_xtra'} = $ffmpeg_xtra
                                    ." -pass 1 -passlogfile '/tmp/xvid.$$.log'"
                                    .' -f avi';
            $self->SUPER::export($episode, '', 1);
        # Second pass
            print "Final pass...\n";
            $self->{'ffmpeg_xtra'} = $ffmpeg_xtra
                                   . " -pass 2 -passlogfile '/tmp/xvid.$$.log'";
        }
    # Single Pass
        else {
            $self->{'ffmpeg_xtra'} = $ffmpeg_xtra
                                    .($self->{'vbr'}
                                      ? ' -qmax 31 -qmin '.$self->{'quantisation'}
                                      : '');
        }
    # Don't forget the audio, etc.
        $self->{'ffmpeg_xtra'} .= ' -acodec '
                                 .($self->can_encode('libmp3lame') ? 'libmp3lame' : 'mp3')
                                 .' -async 1 '
                                 .$self->param('ab', $self->{'a_bitrate'})
                                 .' -f avi';
    # Execute the (final pass) encode
        $self->SUPER::export($episode, '.avi');
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
