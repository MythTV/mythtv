#!/usr/bin/perl -w
#Last Updated: 2005.02.17 (xris)
#
#  export::transcode::XviD
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::transcode::XviD;
    use base 'export::transcode';

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
    add_arg('multipass!',       'Enably two-pass encoding.');

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bxvid\b/i,
                     'name'            => 'Export to XviD',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Transcode-related settings
                     'noise_reduction' => 1,
                     'deinterlace'     => 1,
                     'crop'            => 1,
                    # VBR-specific settings
                     'vbr'             => 1,        # This enables vbr, and the multipass/quantisation options
                     'multipass'       => 0,        # You get multipass or quantisation, multipass will override
                     'quantisation'    => 6,        # 4 through 6 is probably right...
                    # Other video options
                     'a_bitrate'       => 128,
                     'v_bitrate'       => 960,      # Remember, quantisation overrides video bitrate
                     'width'           => 624,
                    };
        bless($self, $class);

    # Initialize and check for transcode
        $self->init_transcode();

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
        if (arg('a_bitrate')) {
            $self->{'a_bitrate'} = arg('a_bitrate');
            die "Audio bitrate must be > 0\n" unless (arg('a_bitrate') > 0);
        }
        else {
            $self->{'a_bitrate'} = query_text('Audio bitrate?',
                                              'int',
                                              $self->{'a_bitrate'});
        }
    # VBR options
        if (arg('multipass')) {
            $self->{'multipass'} = 1;
            $self->{'vbr'}       = 1;
        }
        elsif (arg('quantisation')) {
            die "Quantisation must be a number between 1 and 31 (lower means better quality).\n" if (arg('quantisation') < 1 || arg('quantisation') > 31);
            $self->{'quantisation'} = arg('quantisation');
            $self->{'vbr'}          = 1;
        }
        elsif (!$is_cli) {
            $self->{'vbr'} = query_text('Variable bitrate video?',
                                        'yesno',
                                        $self->{'vbr'} ? 'Yes' : 'No');
            if ($self->{'vbr'}) {
                $self->{'multipass'} = query_text('Multi-pass (slower, but better quality)?',
                                                  'yesno',
                                                  $self->{'multipass'} ? 'Yes' : 'No');
                if (!$self->{'multipass'}) {
                    while (1) {
                        my $quantisation = query_text('VBR quality/quantisation (1-31)?', 'float', $self->{'quantisation'});
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
            }
        }
    # Ask the user what audio and video bitrates he/she wants
        if (arg('v_bitrate')) {
            die "Video bitrate must be > 0\n" unless (arg('v_bitrate') > 0);
            $self->{'v_bitrate'} = arg('v_bitrate');
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
    # Make sure we have finfo
        load_finfo($episode);
    # Build the transcode string
        my $params = " -Z $self->{'width'}x$self->{'height'}"
                    ." -N 0x55" # make *sure* we're exporting mp3 audio
                    ." -b $self->{'a_bitrate'},0,2,0"
                    ;
       # unless ($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) {
       #    $params .= " -J modfps=buffers=7 --export_fps 23.976";
       # }
    # Dual pass?
        if ($self->{'multipass'}) {
        # Add the temporary file to the list
            push @tmpfiles, "/tmp/xvid.$$.log";
        # Back up the path and use /dev/null for the first pass
            my $path_bak = $self->{'path'};
            $self->{'path'} = '/dev/null';
        # First pass
            print "First pass...\n";
            $self->{'transcode_xtra'} = " -y xvid,null $params"
                                       ." -R 1,/tmp/xvid.$$.log"
                                       ." -w $self->{'v_bitrate'} ";
            $self->SUPER::export($episode, '', 1);
        # Restore the path
            $self->{'path'} = $path_bak;
        # Second pass
            print "Final pass...\n";
            $self->{'transcode_xtra'} = " -y xvid $params"
                                       ." -R 2,/tmp/xvid.$$.log"
                                       ." -w $self->{'v_bitrate'} ";
            $self->SUPER::export($episode, '.avi');
        }
    # Single pass
        else {
            $self->{'transcode_xtra'} = " -y xvid4 $params";
            if ($self->{'quantisation'}) {
                $self->{'transcode_xtra'} .= " -R 3 -w ".$self->{'quantisation'};
            }
            else {
                $self->{'transcode_xtra'} .= " -w $self->{'v_bitrate'} ";
            }
            $self->SUPER::export($episode, '.avi');
        }
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
