#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV recording.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

package MythTV::Recording;

    sub new {
        my $class = shift;
        my $self  = { };
        bless($self, $class);

    # Figure out how the data was passed in
        my $rows = (ref $_[0]) ? $_[0] : \@_;

    # Load the passed-in data
        $self->{'title'}           = $rows->[0];  # program name/title
        $self->{'subtitle'}        = $rows->[1];  # episode name
        $self->{'description'}     = $rows->[2];  # episode description
        $self->{'category'}        = $rows->[3];  #
        $self->{'chanid'}          = $rows->[4];  #
        $self->{'channum'}         = $rows->[5];  #
        $self->{'callsign'}        = $rows->[6];  #
        $self->{'channame'}        = $rows->[7];  #
        $self->{'filename'}        = $rows->[8];  # Recorded filename
        $self->{'fs_high'}         = $rows->[9];  # High byte of the filesize
        $self->{'fs_low'}          = $rows->[10]; # Low byte of the filesize
        $self->{'starttime'}       = $rows->[11]; # Scheduled starttime (unix timestamp)
        $self->{'endtime'}         = $rows->[12]; # Scheduled endtime (unix timestamp)
        $self->{'hostname'}        = $rows->[16]; #
        $self->{'sourceid'}        = $rows->[17]; #
        $self->{'cardid'}          = $rows->[18]; #
        $self->{'inputid'}         = $rows->[19]; #
        $self->{'recpriority'}     = $rows->[20]; #
        $self->{'recstatus'}       = $rows->[21]; #
        $self->{'recordid'}        = $rows->[22]; #
        $self->{'rectype'}         = $rows->[23]; #
        $self->{'dupin'}           = $rows->[24]; # Fields to use for duplicate checking
        $self->{'dupmethod'}       = $rows->[25]; #
        $self->{'recstartts'}      = $rows->[26]; # ACTUAL start time (unix timestamp)
        $self->{'recendts'}        = $rows->[27]; # ACTUAL end time (unix timestamp)
        $self->{'previouslyshown'} = $rows->[28]; # Rerun
        $self->{'progflags'}       = $rows->[29]; #
        $self->{'recgroup'}        = $rows->[30]; #
        $self->{'commfree'}        = $rows->[31]; #
        $self->{'outputfilters'}   = $rows->[32]; #
        $self->{'seriesid'}        = $rows->[33]; #
        $self->{'programid'}       = $rows->[34]; #
        $self->{'lastmodified'}    = $rows->[35]; #
        $self->{'recpriority'}     = $rows->[36]; #
        $self->{'airdate'}         = $rows->[37]; # Original airdate (unix timestamp)
        $self->{'hasairdate'}      = $rows->[38]; #
        $self->{'timestretch'}     = $rows->[39]; #
        $self->{'recpriority2'}    = $rows->[40]; #

    # Is this a previously-recorded program?
        if ($self->{'filename'}) {
        #Calculate the filesize
            $self->{'filesize'} = ($self->{'fs_high'} + ($self->{'fs_low'} < 0)) * 4294967296
                                  + $self->{'fs_low'};
        }
    # Assign the program flags
        $self->{'has_commflag'} = ($self->{'progflags'} & 0x01) ? 1 : 0;    # FL_COMMFLAG  = 0x01
        $self->{'has_cutlist'}  = ($self->{'progflags'} & 0x02) ? 1 : 0;    # FL_CUTLIST   = 0x02
        $self->{'auto_expire'}  = ($self->{'progflags'} & 0x04) ? 1 : 0;    # FL_AUTOEXP   = 0x04
        $self->{'is_editing'}   = ($self->{'progflags'} & 0x08) ? 1 : 0;    # FL_EDITING   = 0x08
        $self->{'bookmark'}     = ($self->{'progflags'} & 0x10) ? 1 : 0;    # FL_BOOKMARK  = 0x10
    # And some other calculated fields
        $self->{'will_record'} = ($self->{'rectype'} && $self->{'rectype'} != rectype_dontrec) ? 1 : 0;

    # Return
        return $self;
    }

# Return true
    1;