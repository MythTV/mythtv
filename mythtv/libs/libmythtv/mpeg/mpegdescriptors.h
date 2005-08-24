// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _MPEG_DESCRIPTORS_H_
#define _MPEG_DESCRIPTORS_H_

typedef vector<const unsigned char*> desc_list_t;

class DescriptorID
{
  public:
    enum
    {
        // MPEG
        registration                = 0x05,
        conditional_access          = 0x09,
        ISO_639_language            = 0x0A,

        // DVB
        network_name                = 0x40,
        service_list                = 0x41,
        dvb_stuffing                = 0x42,
        satellite_delivery_system   = 0x43,
        cable_delivery_system       = 0x44,
        VBI_data                    = 0x45,
        VBI_teletext                = 0x46,
        bouquet_name                = 0x47,
        service                     = 0x48,
        country_availability        = 0x49,
        linkage                     = 0x4A,
        NVOD_reference              = 0x4B,
        dvb_time_shifted_service    = 0x4C,
        short_event                 = 0x4D,
        extended_event              = 0x4E,
        time_shifted_event          = 0x4F,
        component                   = 0x50,
        mosaic                      = 0x51,
        stream_identifier           = 0x52,
        CA_identifier               = 0x53,
        content                     = 0x54,
        parental_rating             = 0x55,
        teletext                    = 0x56,
        telephone                   = 0x57,
        local_time_offset           = 0x58,
        subtitling                  = 0x59,
        terrestrial_delivery_system = 0x5A,
        multilingual_network_name   = 0x5B,
        multilingual_bouquet_name   = 0x5C,
        multilingual_service_name   = 0x5D,
        multilingual_component      = 0x5E,
        private_data_specifier      = 0x5F,
        service_move                = 0x60,
        short_smoothing_buffer      = 0x61,
        frequency_list              = 0x62,
        partial_transport_stream    = 0x63,
        data_broadcast              = 0x64,
        scrambling                  = 0x65,
        data_broadcast_id           = 0x66,
        transport_stream            = 0x67,
        DSNG                        = 0x68,
        PDC                         = 0x69,
        AC3                         = 0x6A,
        ancillary_data              = 0x6B,
        cell_list                   = 0x6C,
        cell_frequency_link         = 0x6D,
        announcement_support        = 0x6E,
        application_signalling      = 0x6F,
        adaptation_field_data       = 0x70,
        service_identifier          = 0x71,
        service_availability        = 0x72,
        default_authority           = 0x73,
        related_content             = 0x74,
        TVA_id                      = 0x75,
        dvb_content_identifier      = 0x76,
        time_slice_fec_identifier   = 0x77,
        ECM_repetition_rate         = 0x78,

        // private
        dvb_uk_channel_list         = 0x83,

        // ATSC
        atsc_stuffing               = 0x80,
        audio_stream                = 0x81,
        caption_service             = 0x86,
        content_advisory            = 0x87,
        extended_channel_name       = 0xA0,
        service_location            = 0xA1,
        atsc_time_shifted_service   = 0xA2,
        component_name              = 0xA3,
        DCC_departing_request       = 0xA8,
        DCC_arriving_request        = 0xA9,
        DRM_control                 = 0xAA,
        atsc_content_identifier     = 0xB6,
    };
};

class MPEGDescriptor
{
  public:
    //operator (const unsigned char*)() const { return _data; }

    MPEGDescriptor(const unsigned char* data) : _data(data) { ; }
    uint DescriptorTag() const    { return _data[0]; }
    QString DescriptorTagString() const;
    uint DescriptorLength() const { return _data[1]; }
    static desc_list_t Parse(const unsigned char* data, uint len);
    static const unsigned char* Find(const desc_list_t& parsed, uint desc_tag);
    QString toString() const;
  protected:
    const unsigned char* _data;
};

// a_52a.pdf p119, Table A1
class RegistrationDescriptor : public MPEGDescriptor
{
  public:
    RegistrationDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
        assert(DescriptorID::registration == DescriptorTag());
        if (0x04 != DescriptorLength())
        {
            cerr<<"Registration Descriptor length != 4 !!!!"<<endl;
        }
    }

    uint FormatIdentifier(void) const
        { return (_data[2]<<24) | (_data[3]<<16) | (_data[4]<<8) | _data[5]; }
    QString FormatIdentifierString(void) const
    {
        return QString("") + QChar(_data[2]) + QChar(_data[3]) +
            QChar(_data[4]) + QChar(_data[5]);
    }
    QString toString() const;
};

class ConditionalAccessDescriptor : public MPEGDescriptor
{
  public:
    ConditionalAccessDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
        assert(DescriptorID::conditional_access == DescriptorTag());
    }
    uint SystemID(void) const { return  _data[2] << 8 | _data[3]; }
    uint PID(void) const      { return (_data[4] & 0x1F) << 8 | _data[5]; }
    uint DataSize(void) const { return DescriptorLength() - 4; }
    const unsigned char* Data(void) const { return _data+6; }
    QString toString() const;
};

#endif // _MPEG_DESCRIPTORS_H_
