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

    # Fields from the channel table
        $self->{'atsc_major_chan'}  = $data->{'atsc_major_chan'};
        $self->{'atsc_minor_chan'}  = $data->{'atsc_minor_chan'};
        $self->{'atscsrcid'}        = $data->{'atscsrcid'};
        $self->{'brightness'}       = $data->{'brightness'};
        $self->{'callsign'}         = $data->{'callsign'};
        $self->{'chanid'}           = $data->{'chanid'};
        $self->{'channum'}          = $data->{'channum'};
        $self->{'colour'}           = $data->{'colour'};
        $self->{'commfree'}         = $data->{'commfree'};
        $self->{'contrast'}         = $data->{'contrast'};
        $self->{'finetune'}         = $data->{'finetune'};
        $self->{'freqid'}           = $data->{'freqid'};
        $self->{'hue'}              = $data->{'hue'};
        $self->{'icon'}             = $data->{'icon'};
        $self->{'mplexid'}          = $data->{'mplexid'};
        $self->{'name'}             = $data->{'name'};
        $self->{'outputfilters'}    = $data->{'outputfilters'};
        $self->{'recpriority'}      = $data->{'recpriority'};
        $self->{'serviceid'}        = $data->{'serviceid'};
        $self->{'sourceid'}         = $data->{'sourceid'};
        $self->{'tmoffset'}         = $data->{'tmoffset'};
        $self->{'tvformat'}         = $data->{'tvformat'};
        $self->{'useonairguide'}    = $data->{'useonairguide'};
        $self->{'videofilters'}     = $data->{'videofilters'};
        $self->{'visible'}          = $data->{'visible'};
        $self->{'xmltvid'}          = $data->{'xmltvid'};

    # DVB fields, from the dtv_multiplex table
        $self->{'dtv_bandwidth'}            = $data->{'dtv_bandwidth'};
        $self->{'dtv_constellation'}        = $data->{'dtv_constellation'};
        $self->{'dtv_fec'}                  = $data->{'dtv_fec'};
        $self->{'dtv_frequency'}            = $data->{'dtv_frequency'};
        $self->{'dtv_guard_interval'}       = $data->{'dtv_guard_interval'};
        $self->{'dtv_hierarchy'}            = $data->{'dtv_hierarchy'};
        $self->{'dtv_hp_code_rate'}         = $data->{'dtv_hp_code_rate'};
        $self->{'dtv_inversion'}            = $data->{'dtv_inversion'};
        $self->{'dtv_lp_code_rate'}         = $data->{'dtv_lp_code_rate'};
        $self->{'dtv_modulation'}           = $data->{'dtv_modulation'};
        $self->{'dtv_mplexid'}              = $data->{'dtv_mplexid'};
        $self->{'dtv_networkid'}            = $data->{'dtv_networkid'};
        $self->{'dtv_polarity'}             = $data->{'dtv_polarity'};
        $self->{'dtv_serviceversion'}       = $data->{'dtv_serviceversion'};
        $self->{'dtv_sistandard'}           = $data->{'dtv_sistandard'};
        $self->{'dtv_sourceid'}             = $data->{'dtv_sourceid'};
        $self->{'dtv_symbolrate'}           = $data->{'dtv_symbolrate'};
        $self->{'dtv_transmission_mode'}    = $data->{'dtv_transmission_mode'};
        $self->{'dtv_transportid'}          = $data->{'dtv_transportid'};
        $self->{'dtv_updatetimestamp'}      = $data->{'dtv_updatetimestamp'};
        $self->{'dtv_visible'}              = $data->{'dtv_visible'};

    # Return
        return $self;
    }

# Return true
    1;
