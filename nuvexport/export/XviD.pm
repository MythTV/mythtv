#!/usr/bin/perl -w
#Last Updated: 2004.09.26 (xris)
#
#  export::XviD
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::XviD;
    use base 'export::transcode';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    $cli_args{'quantisation|q=i'} = 1; # Quantisation
    $cli_args{'a_bitrate|a=i'}    = 1; # Audio bitrate
    $cli_args{'v_bitrate|v=i'}    = 1; # Video bitrate
    $cli_args{'height|v_res|h=i'} = 1; # Height
    $cli_args{'width|h_res|w=i'}  = 1; # Width
    $cli_args{'multipass'}        = 1; # Two-pass encoding

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bxvid\b/i,
                     'name'            => 'Export to XviD',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Transcode-related settings
                     'denoise'         => 1,
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
    # Make sure that we have an mplexer
        $Prog{'mplexer'} = find_program('tcmplex', 'mplex');
        push @{$self->{'errors'}}, 'You need tcmplex or mplex to export an svcd.' unless ($Prog{'mplexer'});

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
    # VBR options
        if ($Args{'multipass'}) {
            $self->{'multipass'} = 1;
            $self->{'vbr'}       = 1;
        }
        elsif ($Args{'quantisation'}) {
            die "Quantisation must be a number between 1 and 31 (lower means better quality).\n" if ($Args{'quantisation'} < 1 || $Args{'quantisation'} > 31);
            $self->{'quantisation'} = $Args{'quantisation'};
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
    # Height will default to whatever is the appropriate aspect ratio for the width
    # someday, we should check the aspect ratio here, too...
        $self->{'height'} = sprintf('%.0f', $self->{'width'} * 3/4);
    # Ask about the height
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
            $self->SUPER::export($episode);
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
