#!/usr/bin/perl -w
#Last Updated: 2004.09.27 (xris)
#
#  export::MP3
#  Maintained by Chris Petersen <mythtv@forevermore.net>
#

package export::MP3;
    use base 'export::ffmpeg';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
    use nuv_export::ui;
    use mythtv::db;
    use mythtv::recordings;

# Load the following extra parameters from the commandline
    $cli_args{'bitrate=i'}    = 1; # Audio bitrate

    sub new {
        my $class = shift;
        my $self  = {
                     'cli'             => qr/\bmp3\b/i,
                     'name'            => 'Export to MP3',
                     'enabled'         => 1,
                     'errors'          => [],
                    # Options
                     'bitrate'       => 128,
                    };
        bless($self, $class);

    # Initialize and check for transcode
        $self->init_ffmpeg(1);

    # Make sure that we have an mplexer
        $Prog{'id3tag'} = find_program('id3tag');
        push @{$self->{'errors'}}, 'You need id3tag to export an mp3.' unless ($Prog{'id3tag'});

    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

    sub gather_settings {
        my $self = shift;
    # Load the parent module's settings (skipping one parent, since we don't need the ffmpeg-specific options)
        $self->SUPER::gather_settings(1);
    # Just in case someone used the wrong commandline parameter
        $Args{'bitrate'} ||= $Args{'a_bitrate'};
    # Audio Bitrate
        if ($Args{'bitrate'}) {
            $self->{'bitrate'} = $Args{'bitrate'};
            die "Audio bitrate must be > 0\n" unless ($Args{'bitrate'} > 0);
        }
        else {
            $self->{'bitrate'} = query_text('Audio bitrate?',
                                            'int',
                                            $self->{'bitrate'});
        }
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Make sure we have finfo
        load_finfo($episode);
    # Build the ffmpeg string
        $self->{'ffmpeg_xtra'} = " -acodec mp3 -f mp3";

    # Execute ffmpeg
        $self->SUPER::export($episode, ".mp3");
    # Now tag it
        my $safe_title       = shell_escape($episode->{'title'});
        my $safe_channel     = shell_escape($episode->{'channel'});
        my $safe_description = shell_escape($episode->{'description'});
        my $safe_show_name   = shell_escape($episode->{'show_name'});
        my $safe_outfile     = shell_escape($self->{'path'}.'/'.$episode->{'outfile'});
        $command = "id3tag -A $safe_title -a $safe_channel -c $safe_description -s $safe_show_name $safe_outfile.mp3";
        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
