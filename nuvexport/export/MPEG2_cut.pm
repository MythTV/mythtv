#!/usr/bin/perl -w
#Last Updated: 2005.03.02 (xris)
#
#  export::MPEG2_cut
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::MPEG2_cut;
    use base 'export::generic';

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
                 'cli'             => qr/\bmpeg2cut\b/i,
                 'name'            => 'MPEG2->MPEG2 cut only',
                 'enabled'         => 1,
                 'errors'          => [],
                # Exporter-related settings
                };
        bless($self, $class);
    # Make sure we have tcprobe
        find_program('tcprobe')
            or push @{$self->{'errors'}}, 'You need tcprobe to use this.';
    # Make sure we have lvemux
        find_program('lvemux')
            or push @{$self->{'errors'}}, 'You need lvemux to use this.';
    # Make sure we have avidemux2
        find_program('avidemux2')
            or push @{$self->{'errors'}}, 'You need avidemux2 to use this.';
    # Make sure we have mpeg2cut
        find_program('mpeg2cut')
            or push @{$self->{'errors'}}, 'You need mpeg2cut to use this.';

    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

# Gather settings from the user
    sub gather_settings {
        my $self    = shift;
    # Load the parent module's settings
        $self->{'path'} = query_savepath($self->val('path'));
    }

    sub export {
        my $self    = shift;
        my $episode = shift;
    # Load nuv info
        load_finfo($episode);

    # Check for an MPEG2 recording with a cutlist
        if (!$episode->{'finfo'}{'is_mpeg'}) {
            print 'Not an MPEG recording, moving on';
            return 0;
        }
        if (!($episode->{'cutlist'} && $episode->{'cutlist'} =~ /\d/)) {
            print 'No cutlist found.  This won\'t do!';
            return 0;
        }

    # Generate some names for the temporary audio and video files
        my $safe_outfile = shell_escape($self->get_outfile($episode, '.mpg'));
        my $standard = ($episode->{'finfo'}{'fps'} =~ /^2(?:5|4\.9)/) ? 'PAL' : 'NTSC';
        my $gopsize = ($standard eq 'PAL') ? 12 : 15;
        my $lastgop = ($episode->{'goptype'} == 6) ?
                      ($gopsize * $episode->{'lastgop'}) :
                      $episode->{'lastgop'};

        my $command = "mpeg2cut $episode->{'filename'} $safe_outfile $lastgop ";

        @cuts = split("\n",$episode->{'cutlist'});
        my @skiplist;
        foreach my $cut (@cuts) {
           push @skiplist, (split(" - ", $cut))[0];
           push @skiplist, (split(" - ", $cut))[1];
            }

        my $cutnum = 0;
        if ($skiplist[0] ne 0) {
           $command .= "-";
           $cutnum = 1;
            }

        foreach my $cut (@skiplist) {
           if ($cutnum eq 0) {
              if( $cut ne 0 ) {
                 $cutnum = 1;
                 $cut++;
                 $command .= "$cut-";
              }
           } else {
              $cutnum = 0;
              $cut--;
              $command .= "$cut ";
           }
        }

        system($command);
    }

1;  #return true

# vim:ts=4:sw=4:ai:et:si:sts=4
