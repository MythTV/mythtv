#!/usr/bin/perl -w
#Last Updated: 2005.03.07 (xris)
#
#  export::transcode::SVCD
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::transcode::SVCD;
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

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'      => qr/\bsvcd\b/i,
                     'name'     => 'Export to SVCD',
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

    # Initialize and check for transcode
        $self->init_transcode();

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
    # Split every 795 megs
        $self->{'defaults'}{'split_every'} = 795;
    # Add the svcd preferred bitrates
        $self->{'defaults'}{'quantisation'} = 5;
        $self->{'defaults'}{'a_bitrate'}    = 192;
        $self->{'defaults'}{'v_bitrate'}    = 2500;
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
    # (2756 max, though we round down a bit since some dvd players can't handle
    # the max). Then again, mpeg2enc seems to have trouble with bitrates > 2500.
        my $max_v_bitrate = min(2500, 2742 - $self->{'a_bitrate'});
        if ($self->val('v_bitrate') > $max_v_bitrate || $self->val('confirm')) {
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
    # Split every # megs?
        $self->{'split_every'} = query_text('Split after how many MB?',
                                            'int',
                                            $self->val('split_every'));
        $self->{'split_every'} = $self->{'defaults'}{'split_every'} if ($self->val('split_every') < 1);
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
        $self->{'width'} = 480;
        $self->{'height'} = ($standard eq 'PAL') ? '576' : '480';
        $self->{'out_fps'} = ($standard eq 'PAL') ? 25 : 29.97;
        my $ntsc = ($standard eq 'PAL') ? '' : '-N';
    # Build the transcode string
        $self->{'transcode_xtra'} = " -y mpeg2enc,mp2enc"
                                   .' -F 5,"-q '.$self->{'quantisation'}.'"'    # could add "-M $num_cpus" for multi-cpu here, but it actually seems to slow things down
                                   .' -w '.$self->{'v_bitrate'}
                                   .' -E 44100 -b '.$self->{'a_bitrate'};
    # Add the temporary files that will need to be deleted
        push @tmpfiles, $self->get_outfile($episode, ".$$.m2v"), $self->get_outfile($episode, ".$$.mpa");
    # Execute the parent method
        $self->SUPER::export($episode, ".$$");
    # Need to do this here to get integer context -- otherwise, we get errors about non-numeric stuff.
        my $size = (-s $self->get_outfile($episode, ".$$.m2v") or 0);
            $size += (-s $self->get_outfile($episode, ".$$.mpa") or 0);
    # Create the split file?
        my $split_file;
        if ($size > 0.97 * $self->{'split_every'} * 1024 * 1024) {
            $split_file = "/tmp/nuvexport-svcd.split.$$.$self->{'split_every'}";
            open(DATA, ">$split_file") or die "Can't write to $split_file:  $!\n\n";
            print DATA "maxFileSize = $self->{'split_every'}\n";
            close DATA;
            push @tmpfiles, $split_file;
        }
        else {
            print "Not splitting because combined file size of chunks is < ".(0.97 * $self->{'split_every'} * 1024 * 1024).", which is the requested split size.\n";
        }
    # Multiplex the streams
        my $command = "$NICE tcmplex -m s $ntsc"
                      .($split_file ? ' -F '.shell_escape($split_file) : '')
                      .' -i '.shell_escape($self->get_outfile($episode, ".$$.m2v"))
                      .' -p '.shell_escape($self->get_outfile($episode, ".$$.mpa"))
                      .' -o '.shell_escape($self->get_outfile($episode, $split_file ? '..mpg' : '.mpg'));
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
