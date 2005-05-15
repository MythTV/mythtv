#
# $Date$
# $Revision$
# $Author$
#
#  export::transcode::VCD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::transcode::VCD;
    use base 'export::transcode';

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

    # Initialize and check for transcode
        $self->init_transcode();

    # Make sure that we have an mplexer
        find_program('tcmplex')
            or push @{$self->{'errors'}}, 'You need tcmplex to export a vcd.';

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
    }

# Gather settings from the user
    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings
        $self->SUPER::gather_settings();
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
        $self->{'width'} = 352;
        $self->{'height'} = ($standard eq 'PAL') ? '288' : '240';
        $self->{'out_fps'} = ($standard eq 'PAL') ? 25 : 29.97;
        my $ntsc = ($standard eq 'PAL') ? '' : '-N';
    # Build the transcode string
        $self->{'transcode_xtra'} = " -y mpeg2enc,mp2enc"
                                   .' -F 1 -E 44100 -b 224';
    # Add the temporary files that will need to be deleted
        push @tmpfiles, $self->get_outfile($episode, ".$$.m1v"), $self->get_outfile($episode, ".$$.mpa");
    # Execute the parent method
        $self->SUPER::export($episode, ".$$");
    # Need to do this here to get integer context -- otherwise, we get errors about non-numeric stuff.
        my $size = (-s $self->get_outfile($episode, ".$$.m1v") or 0);
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
        my $command = "$NICE tcmplex -m v $ntsc"
                      .($split_file ? ' -F '.shell_escape($split_file) : '')
                      .' -i '.shell_escape($self->get_outfile($episode, ".$$.m1v"))
                      .' -p '.shell_escape($self->get_outfile($episode, ".$$.mpa"))
                      .' -o '.shell_escape($self->get_outfile($episode, $split_file ? '..mpg' : '.mpg'));
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
