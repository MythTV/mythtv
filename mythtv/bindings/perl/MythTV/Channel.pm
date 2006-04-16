#
# MythTV bindings for perl.
#
# Object containing info about a particular MythTV channel.
#
# @url       $URL$
# @date      $Date$
# @version   $Revision$
# @author    $Author$
#

# Make sure that the main MythTV package is loaded
    use MythTV;

package MythTV::Channel;

# Constructor
    sub new {
        my $class = shift;
        my $self  = { };
        bless($self, $class);

    # The information passed in will be a hashref
        my $data = shift;

        $self->{'atscsrcid'}     = $data->{'atscsrcid'};
        $self->{'brightness'}    = $data->{'brightness'};
        $self->{'callsign'}      = $data->{'callsign'};
        $self->{'chanid'}        = $data->{'chanid'};
        $self->{'channum'}       = $data->{'channum'};
        $self->{'colour'}        = $data->{'colour'};
        $self->{'commfree'}      = $data->{'commfree'};
        $self->{'contrast'}      = $data->{'contrast'};
        $self->{'finetune'}      = $data->{'finetune'};
        $self->{'freqid'}        = $data->{'freqid'};
        $self->{'hue'}           = $data->{'hue'};
        $self->{'icon'}          = $data->{'icon'};
        $self->{'mplexid'}       = $data->{'mplexid'};
        $self->{'name'}          = $data->{'name'};
        $self->{'outputfilters'} = $data->{'outputfilters'};
        $self->{'recpriority'}   = $data->{'recpriority'};
        $self->{'serviceid'}     = $data->{'serviceid'};
        $self->{'sourceid'}      = $data->{'sourceid'};
        $self->{'tmoffset'}      = $data->{'tmoffset'};
        $self->{'tvformat'}      = $data->{'tvformat'};
        $self->{'useonairguide'} = $data->{'useonairguide'};
        $self->{'videofilters'}  = $data->{'videofilters'};
        $self->{'visible'}       = $data->{'visible'};
        $self->{'xmltvid'}       = $data->{'xmltvid'};

    # Return
        return $self;
    }

# Return true
    1;
