#!/usr/bin/perl -w
#Last Updated: 2004.10.02 (xris)
#
#  export::DVCD
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::DVCD;
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
                     'cli'             => qr/\bdvcd\b/i,
                     'name'            => 'Export to DVCD (VCD with 48kHz audio for making DVDs)',
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
        push @{$self->{'errors'}}, 'You need tcmplex or mplex to export a dvcd.' unless ($Prog{'mplexer'});

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
                                   .' -F 1 -E 48000 -b 224';
    # Add the temporary files that will need to be deleted
        push @tmpfiles, "$self->{'path'}/$episode->{'outfile'}.m1v", "$self->{'path'}/$episode->{'outfile'}.mpa";
    # Execute the parent method
        $self->SUPER::export($episode);
    # Multiplex the streams
        my $safe_outfile = shell_escape($self->{'path'}.'/'.$episode->{'outfile'});
        $command = "nice -n $Args{'nice'} mplex -f 1 $safe_outfile.m1v $safe_outfile.mpa -o $safe_outfile.mpg";
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
