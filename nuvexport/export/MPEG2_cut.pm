#!/usr/bin/perl -w
#Last Updated: 2004.09.26 (gjhurlbu)
#
#  export::MPEG2_cut
#  Maintained by Gavin Hurlbut <gjhurlbu@gmail.com>
#

package export::MPEG2_cut;
    use base 'export::generic';

# Load the myth and nuv utilities, and make sure we're connected to the database
    use nuv_export::shared_utils;
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
        $Prog{'tcprobe'} = find_program('tcprobe');
        push @{$self->{'errors'}}, 'You need tcprobe to use this.' unless ($Prog{'tcprobe'});
    # Make sure we have lvemux
        $Prog{'mplexer'} = find_program('lvemux');
        push @{$self->{'errors'}}, 'You need lvemux to use this.' unless ($Prog{'mplexer'});
    # Make sure we have avidemux2
        $Prog{'avidemux'} = find_program('avidemux2');
        push @{$self->{'errors'}}, 'You need avidemux2 to use this.' unless ($Prog{'avidemux'});
    # Make sure we have mpeg2cut
        $Prog{'mpeg2cut'} = find_program('mpeg2cut');
        push @{$self->{'errors'}}, 'You need mpeg2cut to use this.' unless ($Prog{'mpeg2cut'});

    # Any errors?  disable this function
        $self->{'enabled'} = 0 if ($self->{'errors'} && @{$self->{'errors'}} > 0);
    # Return
        return $self;
    }

    sub gather_settings {
        my $self    = shift;
    # No parameters to setup
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
        my $safe_outfile = shell_escape($self->{'path'}.'/'.$episode->{'outfile'});

        my $command = "mpeg2cut $episode->{'filename'} $safe_outfile.mpg $episode->{'lastgop'} ";

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
