#!/usr/bin/perl -w
#Last Updated: 2004.12.26 (xris)
#
#  export::VCD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::VCD;
    use base 'export::transcode';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bvcd\b/i,
                     'name'            => 'Export to VCD',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Transcode-related settings
                     'denoise'         => 1,
                     'deinterlace'     => 1,
                     'crop'            => 1,
                    };
        bless($self, $class);

    # Initialize and check for transcode
        $self->init_transcode();
    # Make sure that we have an mplexer
        $Prog{'mplexer'} = find_program('tcmplex', 'mplex');
        push @{$self->{'errors'}}, 'You need tcmplex or mplex to export a vcd.' unless ($Prog{'mplexer'});

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
        my $size = ($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? '352x288' : '352x240';
    # Build the transcode string
        $self->{'transcode_xtra'} = " -y mpeg2enc,mp2enc -Z $size"
                                   .' -F 1 -E 44100 -b 224';
    # Add the temporary files that will need to be deleted
        push @tmpfiles, $self->get_outfile($episode, ".$$.m1v"), $self->get_outfile($episode, ".$$.mpa");
    # Execute the parent method
        $self->SUPER::export($episode);
    # Multiplex the streams
        my $command = "nice -n $Args{'nice'} tcmplex -m v"
                      .' -i '.shell_escape($self->get_outfile($episode, ".$$.m1v"))
                      .' -p '.shell_escape($self->get_outfile($episode, ".$$.mpa"))
                      .' -o '.shell_escape($self->get_outfile($episode, '.mpg'));
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
