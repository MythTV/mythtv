// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _MPEG_DESCRIPTORS_H_
#define _MPEG_DESCRIPTORS_H_

// C++ headers
#include <vector>
using namespace std;

// Qt headers
#include <QMutex>
#include <QString>

// MythTV
#include "iso639.h"

typedef vector<const unsigned char*> desc_list_t;

class DescriptorID
{
  public:
    enum
    {
        // MPEG
        video                       = 0x02,
        audio                       = 0x03,
        hierarchy                   = 0x04,
        registration                = 0x05, /* implemented */
        data_stream_alignment       = 0x06,
        target_background_grid      = 0x07,
        video_window                = 0x08,
        conditional_access          = 0x09, /* implemented */
        iso_639_language            = 0x0A, /* implemented */
        system_clock                = 0x0B,
        multiplex_buffer_utilization= 0x0C,
        copyright                   = 0x0D,
        maximum_bitrate             = 0x0E,
        private_data_indicator      = 0x0F,
        smoothing_buffer            = 0x10,
        std                         = 0x11,
        ibp                         = 0x12,
        carousel_identifier         = 0x13,
        association_tag             = 0x14,
        deferred_association_tag    = 0x15,
        /*  RESERVED                = 0x16, */
        npt_reference               = 0x17,
        npt_endpoint                = 0x18,
        stream_mode                 = 0x19,
        stream_event                = 0x1A,
        mpeg4_video                 = 0x1B,
        mpeg4_audio                 = 0x1C,
        iod                         = 0x1D,
        sl                          = 0x1E,
        fmc                         = 0x1F,
        external_es_id              = 0x20,
        mux_code                    = 0x21,
        fmx_buffer_size             = 0x22,
        multiplex_buffer            = 0x23,
        content_labeling            = 0x24,
        metadata_pointer            = 0x25,
        metadata                    = 0x26,
        metadata_std                = 0x27,
        avc_video                   = 0x28, /* implemented */
        ipmp                        = 0x29,
        avc_timing_and_hrd          = 0x2A, /* partial */
        mpeg2_aac_audio             = 0x2B,
        flex_mux_timing             = 0x2C,

        // DVB
        network_name                = 0x40, /* implemented */
        service_list                = 0x41, /* implemented */
        dvb_stuffing                = 0x42, /* implemented */
        satellite_delivery_system   = 0x43, /* implemented */
        cable_delivery_system       = 0x44, /* implemented */
        vbi_data                    = 0x45, /* partial */
        vbi_teletext                = 0x46, /* partial */
        bouquet_name                = 0x47, /* implemented */
        service                     = 0x48, /* implemented, toString lacking */
        country_availability        = 0x49, /* implemented */
        linkage                     = 0x4A, /* partial */
        nvod_reference              = 0x4B, /* partial */
        dvb_time_shifted_service    = 0x4C, /* partial */
        short_event                 = 0x4D, /* implemented */
        extended_event              = 0x4E, /* implemented */
        time_shifted_event          = 0x4F, /* partial */

        component                   = 0x50, /* implemented, toString lacking */
        mosaic                      = 0x51, /* partial */
        stream_identifier           = 0x52, /* implemented */
        ca_identifier               = 0x53, /* partial */
        content                     = 0x54, /* implemented */
        parental_rating             = 0x55, /* partial */
        teletext                    = 0x56, /* partial */
        telephone                   = 0x57, /* partial */
        local_time_offset           = 0x58,
        subtitling                  = 0x59, /* partial */
        terrestrial_delivery_system = 0x5A, /* implemented */
        multilingual_network_name   = 0x5B, /* partial */
        multilingual_bouquet_name   = 0x5C, /* partial */
        multilingual_service_name   = 0x5D, /* partial */
        multilingual_component      = 0x5E,
        private_data_specifier      = 0x5F, /* partial */

        service_move                = 0x60, /* partial */
        short_smoothing_buffer      = 0x61, /* partial */
        frequency_list              = 0x62, /* implemented */
        partial_transport_stream    = 0x63, /* partial */
        data_broadcast              = 0x64, /* partial */
        scrambling                  = 0x65, /* partial */
        data_broadcast_id           = 0x66, /* partial */
        transport_stream            = 0x67, /* partial */
        dsng                        = 0x68, /* partial */
        pdc                         = 0x69, /* partial */
        ac3                         = 0x6A, /* partial */
        ancillary_data              = 0x6B, /* partial */
        cell_list                   = 0x6C, /* partial */
        cell_frequency_link         = 0x6D, /* partial */
        announcement_support        = 0x6E, /* partial */
        application_signalling      = 0x6F,

        adaptation_field_data       = 0x70, /* partial */
        service_identifier          = 0x71,
        service_availability        = 0x72, /* partial */
        default_authority           = 0x73, /* implemented */
        related_content             = 0x74,
        tva_id                      = 0x75,
        dvb_content_identifier      = 0x76, /* partial */
        time_slice_fec_identifier   = 0x77,
        ecm_repetition_rate         = 0x78,
        s2_delivery_system          = 0x79,
        eac3                        = 0x7A,
        dts                         = 0x7B,
        aac                         = 0x7C,

        // ATSC
        atsc_stuffing               = 0x80,
        ac3_audio_stream            = 0x81, /* partial */
        atsc_program_identifier     = 0x85,
        caption_service             = 0x86, /* implemented */
        content_advisory            = 0x87,
        atsc_ca_descriptor          = 0x88,
        atsc_descriptor_tag         = 0x89,

        // SCTE
        scte_frame_rate             = 0x82, /* implemented, also ATSC */
        scte_extended_video         = 0x83, /* implemented, also ATSC */
        scte_component_name         = 0x84, /* implemented, also ATSC */
        scte_cue_identifier         = 0x8A, /* implemented */
        scte_frequency_spec         = 0x90, /* implemented */
        scte_modulation_params      = 0x91, /* implemented */
        scte_transport_stream_id    = 0x92, /* implemented */
        scte_revision_detection     = 0x93, /* implemented */

        // ATSC
        extended_channel_name       = 0xA0, /* implemented */
        service_location            = 0xA1,
        atsc_time_shifted_service   = 0xA2,
        component_name              = 0xA3, /* implemented */
        atsc_data_service           = 0xA4,
        atsc_pid_count              = 0xA5,
        atsc_download               = 0xA6,
        multiprotocol_encapsulation = 0xA7,
        dcc_departing_request       = 0xA8,
        dcc_arriving_request        = 0xA9,
        drm_control                 = 0xAA,
        atsc_genre                  = 0xAB,
        atsc_private_information    = 0xAD,

        atsc_content_identifier     = 0xB6, /* partial */
    };
};

class PrivateDescriptorID
{
  public:
    enum
    {
        // These can conflict and should only be used
        // on these specific networks.

        // Private -- UK
        dvb_uk_channel_list         = 0x83, /* implemented */

        // Private -- Dish Network
        dish_event_rights           = 0x87,
        dish_event_mpaa             = 0x89, /* implemented */
        dish_event_name             = 0x91, /* implemented */
        dish_event_description      = 0x92, /* implemented */
        dish_event_properties       = 0x94, /* implemented */
        dish_event_vchip            = 0x95, /* implemented */
        dish_event_tags             = 0x96, /* implemented */

        // Private -- premiere.de
        premiere_content_order        = 0xF0,
        premiere_parental_information = 0xF1,
        premiere_content_transmission = 0xF2,
    };
};

class MPEGDescriptor
{
  public:
    operator const unsigned char*() const { return _data; }

    MPEGDescriptor(const unsigned char *data, int len = 300) : _data(data)
    {
        if ((len < 2) || (int(DescriptorLength()) + 2) > len)
            _data = NULL;
    }
    MPEGDescriptor(const unsigned char *data,
                   int len, uint tag) : _data(data)
    {
        if ((len < 2) || (int(DescriptorLength()) + 2) > len)
            _data = NULL;
        else if (DescriptorTag() != tag)
            _data = NULL;
    }
    MPEGDescriptor(const unsigned char *data,
                   int len, uint tag, uint req_desc_len) : _data(data)
    {
        if ((len < 2) || (int(DescriptorLength()) + 2) > len)
            _data = NULL;
        else if (DescriptorTag() != tag)
            _data = NULL;
        else if (DescriptorLength() != req_desc_len)
            _data = NULL;
    }
    virtual ~MPEGDescriptor() {}

    bool IsValid(void) const { return _data; }
    uint size(void) const { return DescriptorLength() + 2; }

    uint DescriptorTag(void) const { return _data[0]; }
    QString DescriptorTagString(void) const;
    uint DescriptorLength(void) const { return _data[1]; }

    virtual QString toString(void) const;
    virtual QString toStringXML(uint indent_level) const;
    
    static desc_list_t Parse(const unsigned char *data, uint len);
    static desc_list_t ParseAndExclude(const unsigned char *data, uint len,
                                       int descriptorid);
    static desc_list_t ParseOnlyInclude(const unsigned char *data, uint len,
                                        int descriptorid);

    static const unsigned char *Find(const desc_list_t &parsed, uint desc_tag);
    static desc_list_t FindAll(const desc_list_t &parsed, uint desc_tag);

    static const unsigned char *FindBestMatch(
        const desc_list_t &parsed, uint desc_tag, QMap<uint,uint> &langPref);
    static desc_list_t FindBestMatches(
        const desc_list_t &parsed, uint desc_tag, QMap<uint,uint> &langPref);

  protected:
    const unsigned char *_data;
};

// a_52a.pdf p119, Table A1
class RegistrationDescriptor : public MPEGDescriptor
{
  public:
    RegistrationDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::registration)
    {
        // The HD-PVR outputs a registration descriptor with a length
        // of 8 rather than 4, so we accept any length >= 4, not just 4.
        if (DescriptorLength() < 4)
            _data = NULL;
    }

    uint FormatIdentifier(void) const
        { return (_data[2]<<24) | (_data[3]<<16) | (_data[4]<<8) | _data[5]; }
    QString FormatIdentifierString(void) const
    {
        return QString("") + QChar(_data[2]) + QChar(_data[3]) +
            QChar(_data[4]) + QChar(_data[5]);
    }
    QString toString() const;

  private:
    static void InitializeDescriptionMap(void);
    static QString GetDescription(const QString &fmt);

  private:
    static QMutex                description_map_lock;
    static bool                  description_map_initialized;
    static QMap<QString,QString> description_map;
};

class ConditionalAccessDescriptor : public MPEGDescriptor
{
  public:
    ConditionalAccessDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::conditional_access) { }

    uint SystemID(void) const { return  _data[2] << 8 | _data[3]; }
    uint PID(void) const      { return (_data[4] & 0x1F) << 8 | _data[5]; }
    uint DataSize(void) const { return DescriptorLength() - 4; }
    const unsigned char *Data(void) const { return _data+6; }
    QString toString() const;
};

class ISO639LanguageDescriptor : public MPEGDescriptor
{
  public:
    ISO639LanguageDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::iso_639_language) { }

    const unsigned char* CodeRaw() const { return &_data[2]; }
    uint AudioType() const { return _data[5]; }

    int LanguageKey(void) const
        { return iso639_str3_to_key(&_data[2]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    
    QString toString() const;
};

/// ISO 13818-1:2000/Amd.3:2004 page 11
class AVCVideoDescriptor : public MPEGDescriptor
{
  public:
    AVCVideoDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::avc_video) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x
    // descriptor_length        8   1.0
    // profile_idc              8   2.0
    uint ProfileIDC(void)         const { return _data[2]; }
    // constraint_set0_flag     1   3.0
    bool ConstaintSet0(void)      const { return _data[3]&0x80; }
    // constraint_set1_flag     1   3.1
    bool ConstaintSet1(void)      const { return _data[3]&0x40; }
    // constraint_set2_flag     1   3.2
    bool ConstaintSet2(void)      const { return _data[3]&0x20; }
    // AVC_compatible_flags     5   3.3
    uint AVCCompatible(void)      const { return _data[3]&0x1f; }
    // level_idc                8   4.0
    uint LevelIDC(void)           const { return _data[4]; }
    // AVC_still_present        1   5.0
    bool AVCStill(void)           const { return _data[5]&0x80; }
    // AVC_24_hour_picture_flag 1   5.1
    bool AVC24HourPicture(void)   const { return _data[5]&0x40; }
    bool FramePackingSEINotPresentFlag(void)
                                  const { return _data[5]&0x20; }
    // reserved 6 bslbf
    QString toString() const;
};

/// ISO 13818-1:2000/Amd.3:2004 page 12
class AVCTimingAndHRDDescriptor : public MPEGDescriptor
{
    AVCTimingAndHRDDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::avc_timing_and_hrd) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x
    // descriptor_length        8   1.0
    // hrd_management_valid     1   2.0
    bool HRDManagementValid(void)      const { return _data[2]&0x80; }
    // reserved                 6   2.1
    // picture_and_timing_info_present 1 2.7
    bool HasPictureAndTimingInfo(void) const { return _data[2]&0x01;}
    // if (picture_and_timing_info_present) {
    //   90kHz_flag             1   3.0
    //   reserved               7   3.1
    //   if (90kHz_flag == '0') {
    //     N                   32   4.0 uimsbf
    //     K                   32   8.0 uimsbf
    //   }
    //   num_units_in_tick     32 (90khz)?4.0:12.0 uimsbf
    // }
    // fixed_frame_rate_flag    1 (pict_info)?((90khz)?8.0:16.0):3.0
    // temporal_poc_flag        1 (pict_info)?((90khz)?8.1:16.1):3.1
    // picture_to_display_conversion_flag 1 (pict_info)?((90khz)?8.2:16.2):3.2
    // reserved 5 bslbf
};

#endif // _MPEG_DESCRIPTORS_H_
