// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef DVB_DESCRIPTORS_H
#define DVB_DESCRIPTORS_H

#include <cassert>
#include <cstdint>
#include <ctime>

using uint = unsigned int;

#include <QDateTime>
#include <QMutex>
#include <QString>

#include "libmythbase/programinfo.h" // for subtitle types and audio and video properties
#include "libmythtv/mythtvexp.h" // MTV_PUBLIC - Symbol Visibility
#include "mpegdescriptors.h"

MTV_PUBLIC QDateTime dvbdate2qt(const unsigned char *buf);
MTV_PUBLIC time_t dvbdate2unix(const unsigned char *buf);

/*
// needed for scanning
        conditional_access          = 0x09, // maybe
        satellite_delivery_system   = 0x43,*
        cable_delivery_system       = 0x44,*
        service                     = 0x48,*
        terrestrial_delivery_system = 0x5A,*
        frequency_list              = 0x62,*

// needed for captions
        teletext                    = 0x56,
        subtitling                  = 0x59,

// needed for sound
        registration                = 0x05,
        AC3                         = 0x6A,

// needed for eit
        short_event                 = 0x4D,
        extended_event              = 0x4E,
        content                     = 0x54,
*/

static QString coderate_inner(uint coderate);

using enc_override = std::vector<uint8_t>;
MTV_PUBLIC QString dvb_decode_text(const unsigned char *src, uint length,
                               const enc_override &encoding_override);

inline QString dvb_decode_text(const unsigned char *src, uint length)
{
    return dvb_decode_text(src, length, {} );
}

MTV_PUBLIC QString dvb_decode_short_name(const unsigned char *src, uint raw_length);

static constexpr uint8_t byteBCDH2int(uint8_t i) { return i >> 4; };
static constexpr uint8_t byteBCDL2int(uint8_t i) { return i & 0x0f; };
static constexpr uint8_t byteBCD2int(uint8_t i)
{ return (byteBCDH2int(i) * 10) + byteBCDL2int(i); };
static constexpr uint16_t byte2BCD2int(uint8_t i, uint8_t j)
{ return ((byteBCDH2int(i) * 1000)     + (byteBCDL2int(i) * 100)       +
          (byteBCDH2int(j) * 10)       +  byteBCDL2int(j)); };
static constexpr uint32_t byte3BCD2int(uint8_t i, uint8_t j, uint8_t k)
{ return ((byteBCDH2int(i) * 100000)   + (byteBCDL2int(i) * 10000)     +
          (byteBCDH2int(j) * 1000)     + (byteBCDL2int(j) * 100)       +
          (byteBCDH2int(k) * 10)       +  byteBCDL2int(k)); };
static constexpr uint32_t byte4BCD2int(uint8_t i, uint8_t j, uint8_t k, uint8_t l)
{ return ((byteBCDH2int(i) * 10000000)   + (byteBCDL2int(i) * 1000000) +
          (byteBCDH2int(j) * 100000)     + (byteBCDL2int(j) * 10000)   +
          (byteBCDH2int(k) * 1000)       + (byteBCDL2int(k) * 100)     +
          (byteBCDH2int(l) * 10)         +  byteBCDL2int(l)); };
static constexpr uint64_t byte4BCD2int64(uint8_t i, uint8_t j, uint8_t k, uint8_t l)
{ return static_cast<uint64_t>(byte4BCD2int(i, j, k, l)); }

static_assert( byteBCD2int(0x98) == 98);
static_assert(byte2BCD2int(0x98, 0x76) == 9876);
static_assert(byte3BCD2int(0x98, 0x76, 0x54) == 987654);
static_assert(byte4BCD2int(0x98, 0x76, 0x54, 0x32) == 98765432);
static_assert(byte4BCD2int64(0x98, 0x76, 0x54, 0x32) == 98765432ULL);

// DVB Bluebook A038 (Sept 2011) p 77
class NetworkNameDescriptor : public MPEGDescriptor
{
  public:
    explicit NetworkNameDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::network_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x40
    // descriptor_length        8   1.0
    // for (i=0;i<N;i++){ char 8 uimsbf }
    QString Name(void) const
        { return dvb_decode_text(m_data+2, DescriptorLength()); }
    QString ShortName(void) const
        { return dvb_decode_short_name(m_data+2, DescriptorLength()); }
    QString toString(void) const override // MPEGDescriptor
        { return QString("NetworkNameDescriptor: ")+Name(); }
};

// DVB Bluebook A038 (Sept 2011) p 63
class LinkageDescriptor : public MPEGDescriptor
{
  public:
    explicit LinkageDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::linkage)
    {
        if (!m_data)
            return;
        if (DescriptorLength() < 7)
        {
            m_data = nullptr;
        }
        else if (kMobileHandOver == LinkageType())
        {
            uint end = 8;
            if (DescriptorLength() < end)
            {
                m_data = nullptr;
                return;
            }
            end += (HasMobileNetworkID()) ? 2 : 0;
            end += (HasMobileInitialServiceID()) ? 2 : 0;
            if (DescriptorLength() < end)
                m_data = nullptr;
            m_offset = end + 2;
        }
        else if (kEventLinkage == LinkageType())
        {
            if (DescriptorLength() < 10)
                m_data = nullptr;
            m_offset = 12;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4A
    // descriptor_length        8   1.0
    // transport_stream_id     16   2.0
    uint TSID(void)            const { return (m_data[2]<<8) | m_data[3]; }
    // original_network_id     16   4.0
    uint OriginalNetworkID()   const { return (m_data[4]<<8) | m_data[5]; }
    // service_id              16   6.0
    uint ServiceID(void)       const { return (m_data[6]<<8) | m_data[7]; }
    // linkage_type             8   8.0
    enum : std::uint8_t
    {
        kInformationService          = 0x01,
        kEPGService                  = 0x02,
        kCAReplacementService        = 0x03,
        kTSContainingCompleteNetworkBouquetSI = 0x04,
        kServiceReplacementService   = 0x05,
        kDataBroadcastService        = 0x06,
        kRCSMap                      = 0x07,
        kMobileHandOver              = 0x08,
        kSystemSoftwareUpdateService = 0x09,
        kTSContaining_SSU_BAT_NIT    = 0x0A,
        kIP_MACNotificationService   = 0x0B,
        kTSContaining_INT_BAT_NIT    = 0x0C,
        kEventLinkage                = 0x0D,
    };
    uint LinkageType(void)     const { return m_data[8]; }
    QString LinkageTypeString(void) const;

    // if (linkage_type == 0x08)
    // {
    //    hand-over_type        4   9.0
    enum : std::uint8_t
    {
        kHandOverIdentical         = 0x01,
        kHandOverLocalVariation    = 0x02,
        kHandOverAssociatedService = 0x03,
    };
    uint MobileHandOverType(void) const { return m_data[9]>>4; }
    QString MobileHandOverTypeString(void) const;
    //    reserved_future_use   3   9.4
    //    origin_type           1   9.7
    enum : std::uint8_t
    {
        kOriginNIT = 0x0,
        kOriginSDT = 0x1,
    };
    uint MobileOriginType(void) const { return m_data[9]&0x1; }
    QString MobileOriginTypeString(void) const;
    //    if (hand-over_type == 0x01 || hand-over_type == 0x02 ||
    //        hand-over_type == 0x03)
    //    { network_id         16  10.0 }
    bool HasMobileNetworkID(void) const
        { return bool(MobileHandOverType() & 0x3); }
    uint MobileNetworkID(void) const { return (m_data[10]<<8) | m_data[11]; }
    //    if (origin_type ==0x00)
    //    { initial_service_id 16  HasNetworkID()?10.0:12.0 }
    bool HasMobileInitialServiceID(void) const
        { return kOriginNIT == MobileOriginType(); }
    uint MobileInitialServiceID(void) const
    {
        return HasMobileNetworkID() ?
            ((m_data[12]<<8) | m_data[13]) : ((m_data[10]<<8) | m_data[11]);
    }
    // }
    // if (linkage_type == 0x0D)
    // {
    //    target_event_id      16   9.0
    uint TargetEventID(void) const { return (m_data[9]<<8) | m_data[10]; }
    //    target_listed         1  11.0
    bool IsTargetListed(void) const { return ( m_data[11]&0x80 ) != 0; }
    //    event_simulcast       1  11.1
    bool IsEventSimulcast(void) const { return ( m_data[11]&0x40 ) != 0; }
    //    reserved              6  11.2
    // }
    //      for (i=0;i<N;i++)
    //        { private_data_byte 8 bslbf }
    const unsigned char *PrivateData(void) const
        { return &m_data[m_offset]; }
    uint PrivateDataLength(void) const
        { return DescriptorLength() + 2 - m_offset; }

  private:
    uint m_offset;
};

// ETSI TS 102 809 V1.3.1 (2017-06) p 36
class ApplicationSignallingDescriptor : public MPEGDescriptor
{
  public:
    explicit ApplicationSignallingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::application_signalling) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6F
    // descriptor_length        8   1.0
    // for( i=0; i<N; i++ ){
    //   reserved_future_use    1   2.0
    //   application_type      15   2.1
    //   reserved_future_use    3   4.0
    //   AIT_version_number     5   4.3
    //  }
    uint Count() const { return DescriptorLength() / 3; }
    uint ApplicationType(uint i) const
        { return (m_data[2 + (i*3)] & 0x7F) << 8 | m_data[2 + (i*3) + 1] ; }
    uint AITVersionNumber(uint i) const
        { return m_data[2 + (i*3) + 2] & 0x1F ; }
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 38
class AdaptationFieldDataDescriptor : public MPEGDescriptor
{
  public:
    explicit AdaptationFieldDataDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::adaptation_field_data, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x70
    // descriptor_length        8   1.0
    // adapt_field_data_id      8   2.0
    uint AdaptationFieldDataID(void) const { return m_data[2]; }
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("AdaptationFieldDataDescriptor  "
                       "adaptation_field_data_identifier(%1)")
            .arg(AdaptationFieldDataID());
    }
};

// DVB Bluebook A038 (Sept 2011) p 38
class AncillaryDataDescriptor : public MPEGDescriptor
{
  public:
    explicit AncillaryDataDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ancillary_data, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6b
    // descriptor_length        8   1.0
    // ancillary_data_id        8   2.0
    uint AncillaryDataID(void) const { return m_data[2]; }
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("AncillaryDataDescriptor "
                       "ancillary_data_identifier(%1)")
            .arg(AncillaryDataID());
    }
};

// DVB Bluebook A038 (Sept 2011) p 39
class AnnouncementSupportDescriptor : public MPEGDescriptor
{
  public:
    explicit AnnouncementSupportDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::announcement_support) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6e
    // descriptor_length        8   1.0
    // announcmnt_supprt_indic 16   2.0
    // for (i=0; i<N; i++)
    // {
    //   announcement_type      4   0.0+p
    //   reserved_future_use    1   0.4+p
    //   reference_type         3   0.5+p
    //   if (reference_type & 0x3)
    //   {
    //     original_network_id 16   0.0+p
    //     transport_stream_id 16   2.0+p
    //     service_id          16   4.0+p
    //     component_tag        8   6.0+p
    //   }                          7.0
    // }
};

// DVB Bluebook A038 (Sept 2011) p 41
class BouquetNameDescriptor : public MPEGDescriptor
{
  public:
    explicit BouquetNameDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::bouquet_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x47
    // descriptor_length        8   1.0
    // for(i=0;i<N;i++) { char 8 }
    QString BouquetName(void) const
         { return dvb_decode_text(m_data+2, m_data[1]); }
    QString BouquetShortName(void) const
         { return dvb_decode_short_name(m_data+2, m_data[1]); }

    QString toString(void) const override // MPEGDescriptor
    {
        return QString("BouquetNameDescriptor: Bouquet Name(%1)")
            .arg(BouquetName());
    }
};

// DVB Bluebook A038 (Sept 2011) p 41
class CAIdentifierDescriptor : public MPEGDescriptor
{
  public:
    explicit CAIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ca_identifier) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x53
    // descriptor_length        8   1.0
    //
    uint CASystemCount(void) const { return DescriptorLength() >> 1; }
    // for (i=0; i<N; i++)
    //   { CA_system_id 16 }
    int CASystemId(uint i) const
        { return (m_data[2 + (i*2)] << 8) | m_data[3 + (i*2)]; }
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 42
class CellFrequencyLinkDescriptor : public MPEGDescriptor
{
  public:
    explicit CellFrequencyLinkDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::cell_frequency_link) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6d
    // descriptor_length        8   1.0
    // for (i=0; i<N; i++)
    // {
    //   cell_id               16   0.0+p
    //   frequency             32   2.0+p
    //   subcell_info_loop_len  8   6.0+p
    //   for (j=0;j<N;j++)
    //   {
    //     cell_id_extension    8   0.0+p2
    //     transposer_freq     32   1.0+p2
    //   }                          5.0
    // }
};

// DVB Bluebook A038 (Sept 2011) p 42
class CellListDescriptor : public MPEGDescriptor
{
  public:
    explicit CellListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::cell_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6c
    // descriptor_length        8   1.0
    // for (i=0; i<N; i++)
    // {
    //   cell_id               16  0.0+p
    //   cell_latitude         16  2.0+p
    //   cell_longitude        16  4.0+p
    //   cell_extent_of_lat    12  6.0+p
    //   cell_extent_of_longit 12  7.4+p
    //   subcell_info_loop_len  8  9.0+p
    //   for (j=0;j<N;j++)
    //   {
    //     cell_id_extension    8  0.0+p2
    //     subcell_latitude    16  1.0+p2
    //     subcell_longitude   16  3.0+p2
    //     subcell_ext_of_lat  12  4.0+p2
    //     subcell_ext_of_long 12  5.4+p2
    //   }                         7.0
    // }
};

// DVB Bluebook A038 (Sept 2011) p 44
class ComponentDescriptor : public MPEGDescriptor
{
  public:
    explicit ComponentDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::component) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x50
    // descriptor_length        8   1.0
    // reserved_future_use      4   2.0
    // stream_content           4   2.4
    uint StreamContent(void) const { return m_data[2] & 0xf; }
    // component_type           8   3.0
    uint ComponentType(void) const { return m_data[3]; }
    // component_tag            8   4.0
    uint ComponentTag(void)  const { return m_data[4]; }
    // ISO_639_language_code   24   5.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[5]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    //
    // for (i=0; i<N; i++) { text_char 8 }

    bool IsVideo(void) const
    {
        return 0x1 == StreamContent() || // MPEG-2
               0x5 == StreamContent() || // H.264
               0x9 == StreamContent();   // HEVC
    }
    bool IsAudio(void) const
    {
        switch(StreamContent())
        {
            case 0x02:
            case 0x04:
            case 0x06:
            case 0x07:
                return true;
            default:
                return false;
        }
    }
    bool IsSubtitle(void) const { return 0x3 == StreamContent(); }

    uint VideoProperties(void) const
    {
        if (0x1 == StreamContent())
            return MPEG2Properties();
        if (0x5 == StreamContent())
            return VID_AVC | AVCProperties();
        if (0x9 == StreamContent())
            return VID_HEVC | HEVCProperties();

        return VID_UNKNOWN;
    }

    uint MPEG2Properties(void) const
    {
        switch(ComponentType())
        {
            case 0x2: case 0x3: case 0x4:
            case 0x6: case 0x7: case 0x8:
                return VID_WIDESCREEN;
            case 0x09:
            case 0x0D:
                return VID_HDTV;
            case 0x0A: case 0x0B: case 0x0C:
            case 0x0E: case 0x0F: case 0x10:
                return VID_WIDESCREEN | VID_HDTV;
            default:
                return VID_UNKNOWN;
        }
    }

    uint AVCProperties(void) const
    {
        switch(ComponentType())
        {
            case 0x3: case 0x4:
            case 0x7: case 0x8:
                return VID_WIDESCREEN;
            case 0x0B: case 0x0C:
            case 0x0F: case 0x10:
                return VID_WIDESCREEN | VID_HDTV;
            case 0x80: case 0x81:
            case 0x82: case 0x83:
                return VID_WIDESCREEN | VID_HDTV | VID_3DTV;
            default:
                return VID_UNKNOWN;
        }
    }

    uint HEVCProperties(void) const
    {
        switch(ComponentType())
        {
            // NOLINTNEXTLINE(bugprone-branch-clone)
            case 0x0: case 0x1:
            case 0x2: case 0x3:
                return VID_HDTV;
            case 0x5:
                return VID_HDTV; // | VID_UHDTV;
            default:
                return VID_UNKNOWN;
        }
    }

    uint AudioProperties(void) const
    {
        switch (StreamContent())
        {
            case 0x2:
                return MP2Properties();
            case 0x04:
                return AC3Properties();
            case 0x06:
                return HEAACProperties();
            default:
                return AUD_UNKNOWN;
        }
    }

    uint MP2Properties(void) const
    {
        switch (ComponentType())
        {
            case 0x1:
                return AUD_MONO;
            case 0x3:
                return AUD_STEREO;
            case 0x5:
                return AUD_SURROUND;
            case 0x40:
                return AUD_VISUALIMPAIR;
            case 0x41:
                return AUD_HARDHEAR;
            default:
                return AUD_UNKNOWN;
        }
    }

    uint AC3Properties(void) const
    {
        unsigned char properties = AUD_UNKNOWN;

        switch (ComponentType() & 0x7)
        {
            case 0x0:
                properties |= AUD_MONO;
                break;
            case 0x2:
                properties |= AUD_STEREO;
                break;
            case 0x3:
                properties |= AUD_DOLBY;
                break;
            case 0x4: case 0x5:
                properties |= AUD_SURROUND;
                break;
        }

        if (((ComponentType() >> 3) & 0x7) == 0x2)
            properties |= AUD_VISUALIMPAIR;

        if (((ComponentType() >> 3) & 0x7) == 0x3)
            properties |= AUD_HARDHEAR;

        return properties;
    }

    uint HEAACProperties(void) const
    {
        switch (ComponentType())
        {
            case 0x1:
                return AUD_MONO;
            case 0x3:
            case 0x43:
                return AUD_STEREO;
            case 0x5:
                return AUD_SURROUND;
            case 0x40:
            case 0x44:
                return AUD_VISUALIMPAIR;
            case 0x41:
            case 0x45:
                return AUD_HARDHEAR;
            default:
                return AUD_UNKNOWN;
        }
    }

    uint SubtitleType(void) const
    {
        if (!IsSubtitle())
            return SUB_UNKNOWN;

        switch (ComponentType())
        {
            case 0x1:
            case 0x3:
            case 0x10: case 0x11: case 0x12: case 0x13:
                return SUB_NORMAL;
            case 0x20: case 0x21: case 0x22: case 0x23:
                return SUB_HARDHEAR;
            default:
                return SUB_UNKNOWN;
        }
    }

    QString toString(void) const override // MPEGDescriptor
    {
        return QString("ComponentDescriptor(stream_content: 0x%1, "
                       "component_type: 0x%2)").arg(StreamContent(), 0, 16)
            .arg(ComponentType(), 0, 16);
    }
};

// DVB Bluebook A038 (Sept 2011) p 46
class ContentDescriptor : public MPEGDescriptor
{
  public:
    explicit ContentDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::content) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x54
    // descriptor_length        8   1.0

    uint Count(void)         const { return DescriptorLength() >> 1; }
    // for (i=0;i<N;i++)
    // {
    //   content_nibble_level_1 4   0.0+p
    uint Nibble1(uint i)     const { return m_data[2 + (i<<1)] >> 4; }
    //   content_nibble_level_2 4   0.4+p
    uint Nibble2(uint i)     const { return m_data[2 + (i<<1)] & 0xf; }

    uint Nibble(uint i)      const { return m_data[2 + (i<<1)]; }

    //   user_nibble            4   1.0+p
    uint UserNibble1(uint i) const { return m_data[3 + (i<<1)] >> 4; }
    //   user_nibble            4   1.4+p
    uint UserNibble2(uint i) const { return m_data[3 + (i<<1)] & 0xf; }
    uint UserNibble(uint i)  const { return m_data[3 + (i<<1)]; }
    // }                            2.0

    ProgramInfo::CategoryType GetMythCategory(uint i) const;
    QString GetDescription(uint i) const;
    QString toString(void) const override; // MPEGDescriptor

  protected:
    static void Init(void);

  protected:
    static QMutex             s_categoryLock;
    static QMap<uint,QString> s_categoryDesc;
    static volatile bool      s_categoryDescExists;
};

// DVB Bluebook A038 (Sept 2011) p 49
class CountryAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    explicit CountryAvailabilityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::country_availability) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x49
    // descriptor_length        8   1.0

    uint CountryCount(void) const { return ((DescriptorLength() - 1) / 3); }

    // country_avail_flag       1   2.0
    bool IsAvailable(void) const { return (m_data[2] & 0x1) != 0; }
    // reserved_future_use      7   2.1
    //
    // for (i=0; i<N; i++)
    //   { country_code        24 }
    QString CountryNames(void) const
    {
        QString countries="";
        for (size_t i=0; i<CountryCount(); i++)
        {
            if (i!=0) countries.append(" ");
            countries.append(QString::fromLatin1(
                                 (const char *)m_data+(3*(i+1)), 3));
        };
        return countries;
    }

    QString toString(void) const override // MPEGDescriptor
    {
        return QString("CountryAvailabilityDescriptor: Available(%1) in (%2)")
            .arg(static_cast<int>(IsAvailable())).arg(CountryNames());
    }
};

// DVB Bluebook A038 (Sept 2011) p 50
class DataBroadcastDescriptor : public MPEGDescriptor
{
  public:
    explicit DataBroadcastDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::data_broadcast) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x64
    // descriptor_length        8   1.0

    // data_broadcast_id       16   2.0
    uint DataBroadcastId(void) const { return m_data[2] << 8 | m_data[3]; }
    // component_tag            8   4.0
    uint DataComponentTag(void) const { return m_data[4]; }
    // selector_length          8   5.0
    uint SelectorLength(void) const { return m_data[5]; }
    // for (i=0; i<selector_length; i++)
    // {
    //   selector_byte          8
    const unsigned char *Selector(void) const { return &m_data[6]; }
    // }
    // ISO_639_language_code   24
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[6 + SelectorLength()]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // text_length              8
    uint TextLength(void) const { return m_data[6 + SelectorLength() + 3]; }
    // for (i=0; i<text_length; i++) { text_char 8 }
    QString Text(void) const
    {
        return dvb_decode_text(&m_data[6 + SelectorLength() + 4], TextLength());
    }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 51
class DataBroadcastIdDescriptor : public MPEGDescriptor
{
  public:
    explicit DataBroadcastIdDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::data_broadcast_id) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x66
    // descriptor_length        8   1.0

    // data_broadcast_id       16   2.0
    uint DataBroadCastId(void) const { return m_data[2] << 8 | m_data[3]; }
    // for(i=0; i < N;i++ )
    // { id_selector_byte       8 }
};

// DVB Bluebook A038 (Sept 2011) p 51
class CableDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit CableDeliverySystemDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::cable_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x44
    // descriptor_length        8   1.0

    // frequency               32   2.0
    uint FrequencyRaw(void) const
    {
        return ((m_data[2]<<24) | (m_data[3]<<16) |
                (m_data[4]<<8)  | (m_data[5]));
    }
    unsigned long long FrequencyHz(void) const
    {
        if (m_data == nullptr)
            return 0;
        return byte4BCD2int64(m_data[2], m_data[3], m_data[4], m_data[5]) * 100;
    }
    // reserved_future_use     12   6.0
    // FEC_outer                4   7.4
    enum : std::uint8_t
    {
        kOuterFEC_None        = 0x1,
        kOuterFEC_RS204_RS188 = 0x2,
    };
    uint FECOuter(void) const { return m_data[7] & 0xf; }
    QString FECOuterString(void) const
    {
        if (FECOuter() == kOuterFEC_None)
            return "None";
        if (FECOuter() == kOuterFEC_RS204_RS188)
            return "RS(204/188)";
        return "unknown";
    }
    // modulation               8   8.0
    enum : std::uint8_t
    {
        kModulationQAM16  = 0x01,
        kModulationQAM32  = 0x02,
        kModulationQAM64  = 0x03,
        kModulationQAM128 = 0x04,
        kModulationQAM256 = 0x05,
    };
    uint Modulation(void) const { return m_data[8]; }
    QString ModulationString(void) const
    {
        static std::array<QString,6> ms
            { "auto", "qam_16", "qam_32", "qam_64", "qam_128", "qam_256" };
        return (Modulation() <= kModulationQAM256) ?
            ms[Modulation()] : QString("auto");
    }
    // symbol_rate             28   9.0
    uint SymbolRateRaw(void) const
    {
        return ((m_data[9]<<20) | (m_data[10]<<12) |
                (m_data[11]<<4) | (m_data[12]>>4));
    }
    uint SymbolRateHz(void) const
    {
        return ((byte3BCD2int(m_data[9], m_data[10], m_data[11]) * 1000) +
                (byteBCDH2int(m_data[12]) * 100));
    }
    // FEC_inner                4  12.4
    enum : std::uint8_t
    {
        kInnerFEC_1_2_ConvolutionCodeRate = 0x1,
        kInnerFEC_2_3_ConvolutionCodeRate = 0x2,
        kInnerFEC_3_4_ConvolutionCodeRate = 0x3,
        kInnerFEC_5_6_ConvolutionCodeRate = 0x4,
        kInnerFEC_7_8_ConvolutionCodeRate = 0x5,
        kInnerFEC_8_9_ConvolutionCodeRate = 0x6,
        kInnerFEC_None                    = 0xF,
    };
    uint FECInner(void) const { return m_data[12] & 0xf; }
    QString FECInnerString(void) const { return coderate_inner(FECInner()); }
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 58
class SatelliteDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit SatelliteDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::satellite_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x43
    // descriptor_length        8   1.0

    /// frequency              32   2.0
    uint FrequencyRaw(void) const
    {
        return ((m_data[2]<<24) | (m_data[3]<<16) |
                (m_data[4]<<8)  | (m_data[5]));
    }
    uint64_t FrequencykHz(void) const
    {
        return byte4BCD2int64(m_data[2], m_data[3], m_data[4], m_data[5]) * 10;
    }
    /// orbital_position       16   6.0
    uint OrbitalPosition(void) const
        { return byte2BCD2int(m_data[6], m_data[7]); }
    QString OrbitalPositionString(void) const
    {
        uint num = OrbitalPosition();
        return QString("%1.%2 %3").arg(num / 10).arg(num % 10)
            .arg((IsEast()) ? "East" : "West");
    }
    double OrbitalPositionFloat()  const
        { return ((double) OrbitalPosition()) / 10.0; }
    /// west_east_flag          1   8.0
    bool IsEast(void)             const { return ( (m_data[8]&0x80) ) != 0; }
    bool IsWest(void)             const { return !IsEast(); }
    // polarization             2   8.1
    uint Polarization(void)       const { return (m_data[8]>>5)&0x3; }
    QString PolarizationString()  const
    {
        static const std::array<const QString,4> ps { "h", "v", "l", "r" };
        return ps[Polarization()];
    }
    bool IsCircularPolarization(void) const       { return ((m_data[8]>>6)&0x1) != 0; }
    bool IsLinearPolarization(void) const         { return ((m_data[8]>>6)&0x1) == 0; }
    bool IsHorizontalLeftPolarization(void) const { return ((m_data[8]>>5)&0x1) != 0; }
    bool IsVerticalRightPolarization(void) const  { return ((m_data[8]>>5)&0x1) == 0; }
    // roll off                 2   8.3
    enum : std::uint8_t
    {
        kRollOff_35,
        kRollOff_20,
        kRollOff_25,
        kRollOff_Auto,
    };
    uint RollOff(void) const { return (m_data[8]>>3)&0x3; }
    QString RollOffString(void) const
    {
        static const std::array<const QString,4> ro { "0.35", "0.20", "0.25", "auto" };
        return ro[RollOff()];
    }
    // modulation system        1   8.5
    uint ModulationSystem(void) const { return (m_data[8]>>2)&0x1; }
    QString ModulationSystemString(void) const
    {
        return ModulationSystem() ? "DVB-S2" : "DVB-S";
    }
    // modulation               2   8.6
    enum : std::uint8_t
    {
        kModulationQPSK_NS = 0x0, // Non standard QPSK for Bell ExpressVu
        // should be "auto" according to DVB SI standard
        kModulationQPSK   = 0x1,
        kModulation8PSK   = 0x2,
        kModulationQAM16  = 0x3,
    };
    uint Modulation(void) const { return m_data[8]&0x03; }
    QString ModulationString(void) const
    {
        static const std::array<const QString,4> ms { "qpsk", "qpsk", "8psk", "qam_16" };
        return ms[Modulation()];
    }
    // symbol_rate             28   9.0
    uint SymbolRate(void) const
    {
        return ((m_data[9]<<20) | (m_data[10]<<12) |
                (m_data[11]<<4) | (m_data[12]>>4));
    }
    uint SymbolRateHz(void) const
    {
        return ((byte3BCD2int(m_data[9], m_data[10], m_data[11]) * 1000) +
                (byteBCDH2int(m_data[12]) * 100));
    }
    // FEC_inner                4  12.4
    enum : std::uint8_t
    {
        kInnerFEC_1_2_ConvolutionCodeRate = 0x1,
        kInnerFEC_2_3_ConvolutionCodeRate = 0x2,
        kInnerFEC_3_4_ConvolutionCodeRate = 0x3,
        kInnerFEC_5_6_ConvolutionCodeRate = 0x4,
        kInnerFEC_7_8_ConvolutionCodeRate = 0x5,
        kInnerFEC_8_9_ConvolutionCodeRate = 0x6,
        kInnerFEC_None                    = 0xF,
    };
    uint FECInner(void) const { return m_data[12] & 0xf; }
    QString FECInnerString(void) const { return coderate_inner(FECInner()); }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 60
class TerrestrialDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit TerrestrialDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::terrestrial_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5a
    // descriptor_length        8   1.0

    // centre_frequency        32   2.0
    uint Frequency(void) const
    {
        return ((m_data[2]<<24) | (m_data[3]<<16) |
                (m_data[4]<<8)  | (m_data[5]));
    }
    uint64_t FrequencyHz(void) const { return uint64_t(Frequency()) * 10ULL; }

    // bandwidth                3   6.0
    enum : std::uint8_t
    {
        kBandwidth8Mhz = 0x0,
        kBandwidth7Mhz = 0x1,
        kBandwidth6Mhz = 0x2,
        kBandwidth5Mhz = 0x3,
    };
    uint Bandwidth(void) const { return m_data[6]>>5; }
    uint BandwidthHz(void) const { return (8 - Bandwidth()) * 1000000; }
    QString BandwidthString(void) const
    {
        static std::array<QString,4> bs { "8", "7", "6", "5" };
        return (Bandwidth() <= kBandwidth5Mhz) ? bs[Bandwidth()] : "auto";
    }
    // priority                 1   6.3
    bool HighPriority(void) const { return ( m_data[6] & 0x10 ) != 0; }
    // time_slicing_indicator   1   6.4
    bool IsTimeSlicingIndicatorUsed(void) const { return (m_data[6] & 0x08) == 0; }
    // MPE-FEC_indicator        1   6.5
    bool IsMPE_FECUsed(void) const { return (m_data[6] & 0x04) == 0; }
    // reserved_future_use      2   6.6
    // constellation            2   7.0
    enum : std::uint8_t
    {
        kConstellationQPSK  = 0x0,
        kConstellationQAM16 = 0x1,
        kConstellationQAM64 = 0x2,
        kConstellationQAM256 = 0x3,
    };
    uint Constellation(void) const { return m_data[7]>>6; }
    QString ConstellationString(void) const
    {
        static std::array<QString,4> cs { "qpsk", "qam_16", "qam_64", "qam_256" };
        return (Constellation() <= kConstellationQAM256) ?
            cs[Constellation()] : "auto";
    }
    // hierarchy_information    3   7.2
    enum : std::uint8_t
    {
        kHierarchyInfoNonHierarchicalNativeInterleaver  = 0x0,
        kHierarchyInfoAlpha1NativeInterleaver           = 0x1,
        kHierarchyInfoAlpha2NativeInterleaver           = 0x2,
        kHierarchyInfoAlpha4NativeInterleaver           = 0x3,
        kHierarchyInfoNonHierarchicalInDepthInterleaver = 0x4,
        kHierarchyInfoAlpha1InDepthInterleaver          = 0x5,
        kHierarchyInfoAlpha2InDepthInterleaver          = 0x6,
        kHierarchyInfoAlpha4InDepthInterleaver          = 0x7,
    };
    uint Hierarchy(void) const { return (m_data[7]>>3) & 0x7; }

    /// \bug returns "a" for values >= 4 for compatibility with siparser.cpp
    QString HierarchyString(void) const
    {
        static const std::array<const QString,8> hs { "n", "1", "2", "4", "a", "a", "a", "a" };
        return hs[Hierarchy()];
    }
    bool NativeInterleaver(void) const { return ( m_data[7] & 0x20 ) != 0; }
    uint Alpha(void) const
    {
        uint i = (m_data[7]>>3) & 0x3;
        return (0x2 == i) ? 4 : i;
    }
    // code_rate-HP_stream      3   7.5
    enum : std::uint8_t
    {
        kCodeRate_1_2 = 0x0,
        kCodeRate_2_3 = 0x1,
        kCodeRate_3_4 = 0x2,
        kCodeRate_5_6 = 0x3,
        kCodeRate_7_8 = 0x4,
    };
    uint CodeRateHP(void) const { return m_data[7] & 0x7; }
    QString CodeRateHPString(void) const
    {
        static const std::array<const QString,8> cr {
            "1/2", "2/3", "3/4", "5/6", "7/8", "auto", "auto", "auto"
        };
        return cr[CodeRateHP()];
    }
    // code_rate-LP_stream      3   8.0
    uint CodeRateLP(void) const { return (m_data[8]>>5) & 0x7; }
    QString CodeRateLPString(void) const
    {
        static const std::array<const QString,8> cr {
            "1/2", "2/3", "3/4", "5/6", "7/8", "auto", "auto", "auto"
        };
        return cr[CodeRateLP()];
    }
    // guard_interval           2   8.3
    enum : std::uint8_t
    {
        kGuardInterval_1_32 = 0x0,
        kGuardInterval_1_16 = 0x1,
        kGuardInterval_1_8  = 0x2,
        kGuardInterval_1_4  = 0x3,
    };
    uint GuardInterval(void) const { return (m_data[8]>>3) & 0x3; }
    QString GuardIntervalString(void) const
    {
        static const std::array<const QString,4> gi { "1/32", "1/16", "1/8", "1/4" };
        return gi[GuardInterval()];
    }
    // transmission_mode        2   8.5
    enum : std::uint8_t
    {
        kTransmissionMode2k = 0x00,
        kTransmissionMode8k = 0x01,
        kTransmissionMode4k = 0x02,
    };
    uint TransmissionMode(void) const { return (m_data[8]>>1) & 0x3; }
    QString TransmissionModeString(void) const
    {
        static const std::array<const QString,4> tm { "2", "8", "4", "auto" };
        return tm[TransmissionMode()];
    }
    // other_frequency_flag     1   8.7
    bool OtherFrequencyInUse(void) const { return (m_data[8] & 0x1) != 0; }
    // reserved_future_use     32   9.0

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 110
class ImageIconDescriptor : public MPEGDescriptor
{
  public:
    explicit ImageIconDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
        {
            if (IsValid() && (DescriptorTagExtension() != DescriptorID::image_icon))
            {
                m_data = nullptr;
            }
        }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x00      image_icon

    // descriptor_number        4   3.0
    uint DescriptorNumber(void) const
    {
        return m_data[3] >> 4;
    }

    // last_descriptor_number   4   3.4
    uint LastDescriptorNumber(void) const
    {
        return m_data[3] & 0xF;
    }

    // icon_id                  3   4.5
    uint IconID(void) const
    {
        return m_data[4] & 0x7;
    }

    //
    // TBD
    //

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 104       0x7f 0x04
class T2DeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit T2DeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid())
        {
            if (DescriptorTagExtension() != DescriptorID::t2_delivery_system)
                m_data = nullptr;
            else
                Parse();
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x04      t2_delivery_system

    // plp_id                   8   3.0
    uint PlpID(void) const
    {
        return m_data[3];
    }

    // T2_system_id            16   4.0
    uint T2SystemID(void) const
    {
        return ((m_data[4]<<8) | (m_data[5]));
    }

    // SISO/MISO                2   6.0
    uint SisoMiso(void) const { return (m_data[6] >> 6) & 0x3; }
    QString SisoMisoString(void) const
    {
        static const std::array<const QString,4> sm
            { "SISO", "MISO", "reserved", "reserved" };
        return sm[SisoMiso()];
    }

    // bandwidth                4   6.2
    enum : std::uint8_t
    {
        kBandwidth8Mhz = 0x0,
        kBandwidth7Mhz = 0x1,
        kBandwidth6Mhz = 0x2,
        kBandwidth5Mhz = 0x3,
        kBandwidth10Mhz = 0x4,
        kBandwidth1712kHz = 0x5,
    };
    uint Bandwidth(void) const { return (m_data[6] >> 2) & 0xF; }
    uint BandwidthHz(void) const
    {
        static constexpr std::array<const uint,6> bw
            { 8000000, 7000000, 6000000, 5000000, 10000000, 1712000 };
        return (Bandwidth() <= kBandwidth1712kHz ? bw[Bandwidth()] : 0);
    }
    QString BandwidthString(void) const
    {
        static const std::array<const QString,6> bs
            { "8", "7", "6", "5", "10", "1.712" };
        return (Bandwidth() <= kBandwidth1712kHz) ? bs[Bandwidth()] : "0";
    }

    // guard_interval           3   7.0
    enum : std::uint8_t
    {
        kGuardInterval_1_32    = 0x0,
        kGuardInterval_1_16    = 0x1,
        kGuardInterval_1_8     = 0x2,
        kGuardInterval_1_4     = 0x3,
        kGuardInterval_1_128   = 0x4,
        kGuardInterval_19_128  = 0x5,
        kGuardInterval_19_256  = 0x6,
    };
    uint GuardInterval(void) const { return (m_data[7]>>5) & 0x7; }
    QString GuardIntervalString(void) const
    {
        static const std::array<const QString,8> gi
            { "1/32", "1/16", "1/8", "1/4", "1/128", "19/128", "19/256", "reserved" };
        return gi[GuardInterval()];
    }

    // transmission_mode        3   7.3
    enum : std::uint8_t
    {
        kTransmissionMode2k  = 0x00,
        kTransmissionMode8k  = 0x01,
        kTransmissionMode4k  = 0x02,
        kTransmissionMode1k  = 0x03,
        kTransmissionMode16k = 0x04,
        kTransmissionMode32k = 0x05,
    };
    uint TransmissionMode(void) const { return (m_data[7]>>2) & 0x7; }
    QString TransmissionModeString(void) const
    {
        static const std::array<const QString,8> tm
            { "2k", "8k", "4k", "1k", "16k", "32k", "reserved", "reserved" };
        return tm[TransmissionMode()];
    }
    uint OtherFrequencyFlag(void) const { return (m_data[7]>>1) & 0x1; }
    uint TFSFlag(void) const { return m_data[7] & 0x1; }

  public:
    uint NumCells(void) const { return m_cellPtrs.size(); }
    uint CellID(uint i) const { return (m_cellPtrs[i][0] << 8) | m_cellPtrs[i][1]; }
    uint FrequencyLoopLength(uint i) const { return m_cellPtrs[i][2]; }

    uint CentreFrequency(uint i) const
    {
        return (m_cellPtrs[i][2] << 24) | (m_cellPtrs[i][3] << 16) | (m_cellPtrs[i][4] << 8) | m_cellPtrs[i][5];
    }

    uint CentreFrequency(int i, int j) const
    {
        return (m_cellPtrs[i][3+(4*j)] << 24) | (m_cellPtrs[i][4+(4*j)] << 16) | (m_cellPtrs[i][5+(4*j)] << 8) | m_cellPtrs[i][6+(4*j)];
    }
    uint SubcellInfoLoopLength(uint i) const { return m_subCellPtrs[i][0]; }
    uint CellIDExtension(uint i, uint j) const { return m_subCellPtrs[i][1+(5*j)]; }
    uint TransposerFrequency(uint i, uint j) const
    {
        return (m_subCellPtrs[i][1+(5*j)] << 24) | (m_subCellPtrs[i][2+(5*j)] << 16) | (m_subCellPtrs[i][3+(5*j)] << 8) | m_cellPtrs[i][4+(5*j)];
    }

    void Parse(void) const;
    QString toString(void) const override; // MPEGDescriptor

  private:
    mutable std::vector<const unsigned char*> m_cellPtrs; // used to parse
    mutable std::vector<const unsigned char*> m_subCellPtrs; // used to parse
};

// DVB Bluebook A038 (Feb 2019) p 100       0x7f 0x05
class SHDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit SHDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::sh_delivery_system))
        {
            m_data = nullptr;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x05      sh_delivery_system

    //
    // TBD
    //

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 115       0x7F 0x06
class SupplementaryAudioDescriptor : public MPEGDescriptor
{
  public:
    explicit SupplementaryAudioDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::supplementary_audio))
        {
            m_data = nullptr;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x06      supplementary_audio_descriptor

    // mix_type                 1   3.0
    uint MixType(void) const { return m_data[3] & 0x1; }

    // editorial_classification 5   3.1
    uint EditorialClassification(void) const { return (m_data[3] >> 1 ) & 0x1F; }

    // reserved_future_use      1   3.6
    uint ReservedFutureUse(void) const { return (m_data[3] >> 6 ) & 0x1; }

    // language_code_present    1   3.7
    uint LanguageCodePresent(void) const { return (m_data[3] >> 7 ) & 0x1; }

    // ISO_639_language_code   24   4.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[4]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 113       0x7F 0x07
class NetworkChangeNotifyDescriptor : public MPEGDescriptor
{
  public:
    explicit NetworkChangeNotifyDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::network_change_notify))
        {
            m_data = nullptr;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x07      network_change_notify_descriptor

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 112       0x7F 0x08
class MessageDescriptor : public MPEGDescriptor
{
  public:
    explicit MessageDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::supplementary_audio))
        {
            m_data = nullptr;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x7f      extension
    // descriptor_length        8   1.0
    // descriptor_tag_extension 8   2.0       0x08      message_descriptor

    // message_id               8   3.0
    uint MessageID(void) const { return m_data[3]; }

    // ISO_639_language_code   24   4.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[4]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }

    // text_char                8   7.0
    QString Message(void) const
        { return dvb_decode_text(m_data+7, DescriptorLength()-5); }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 117
class TargetRegionDescriptor : public MPEGDescriptor
{
  public:
    explicit TargetRegionDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::target_region))
        {
            m_data = nullptr;
        }
    }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x7f    extension
    // descriptor_length                8   1.0     4       or more
    // descriptor_tag_extension         8   2.0     0x09    target_region

    // country_code                    24   3.0     e.g. "GBR"
    uint CountryCode(void) const
    {
        return ((m_data[3] << 16) | (m_data[4] << 8) | m_data[5]);
    }
    QString CountryCodeString(void) const
    {
        std::array<QChar,3> code
            { QChar(m_data[3]), QChar(m_data[4]), QChar(m_data[5]) };
        return QString(code.data(), 3);
    }
    //
    // TBD
    //
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 118
class TargetRegionNameDescriptor : public MPEGDescriptor
{
  public:
    explicit TargetRegionNameDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::target_region_name))
        {
            m_data = nullptr;
        }
    }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x7f    extension
    // descriptor_length                8   1.0     7       or more
    // descriptor_tag_extension         8   2.0     0x0A    target_region_name

    // country_code                    24   3.0     e.g. "GBR"
    uint CountryCode(void) const
    {
        return ((m_data[3] << 16) | (m_data[4] << 8) | m_data[5]);
    }
    QString CountryCodeString(void) const
    {
        std::array<QChar,3> code
            { QChar(m_data[3]), QChar(m_data[4]), QChar(m_data[5]) };
        return QString(code.data(), 3);
    }

    // ISO_639_language_code           24   6.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[6]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }

    //
    // TBD
    //
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 115           0x7F 0x0B
class ServiceRelocatedDescriptor : public MPEGDescriptor
{
  public:
    explicit ServiceRelocatedDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::service_relocated))
        {
            m_data = nullptr;
        }
    }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x7f    extension
    // descriptor_length                8   1.0     7
    // descriptor_tag_extension         8   2.0     0x0B    service_relocated

    // old_original_network_id         16   3.0
    // old_transport_stream_id         16   5.0
    // old_service_id                  16   7.0
    uint OldOriginalNetworkID(void) const { return (m_data[3] << 8) | m_data[4]; }
    uint OldTransportID(void) const { return (m_data[5] << 8) | m_data[6]; }
    uint OldServiceID(void) const { return (m_data[7] << 8) | m_data[8]; }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 98
class C2DeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit C2DeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::c2_delivery_system))
        {
            m_data = nullptr;
        }
    }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x7f    extension
    // descriptor_length                8   1.0     9
    // descriptor_tag_extension         8   2.0     0x0D    c2_delivery_system

    // plp_id                           8   3.0
    uint PlpID(void) const
    {
        return m_data[3];
    }

    // data_slice_id                    8   4.0
    uint DataSliceID(void) const
    {
        return m_data[4];
    }

    // C2_System_tuning_frequency       32  5.0
    uint Frequency(void) const
    {
        return ((m_data[5]<<24) | (m_data[6]<<16) |
                (m_data[7]<<8)  | (m_data[8]));
    }

    //
    // TBD
    //

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Feb 2019) p 108
class S2XSatelliteDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit S2XSatelliteDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extension)
    {
        if (IsValid() && (DescriptorTagExtension() != DescriptorID::s2x_satellite_delivery_system))
        {
            m_data = nullptr;
        }
    }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x7f    extension
    // descriptor_length                8   1.0
    // descriptor_tag_extension         8   2.0     0x17    s2x_delivery_system

    //
    // TBD
    //

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 58
class DSNGDescriptor : public MPEGDescriptor
{
  public:
    explicit DSNGDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dsng) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x68
    // descriptor_length        8   1.0
    // for (i=0;i<N;i++) { byte 8 }
};

// DVB Bluebook A038 (Sept 2011) p 58
class MTV_PUBLIC ExtendedEventDescriptor : public MPEGDescriptor
{
  public:
    explicit ExtendedEventDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extended_event) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4e
    // descriptor_length        8   1.0

    // descriptor_number        4   2.0
    uint DescriptorNumber(void) const { return m_data[2] >> 4; }
    // last_number              4   2.4
    uint LastNumber(void)       const { return m_data[2] & 0xf; }
    // ISO_639_language_code   24   3.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[3]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // length_of_items          8   6.0
    uint LengthOfItems(void)    const { return m_data[6]; }
    // for ( i=0;i<N;i++)
    // {
    //   item_description_len   8   0.0+p
    //   for (j=0;j<N;j++) { item_desc_char 8 }
    //   item_length            8   1.0+p2
    //   for (j=0;j<N;j++) { item_char 8 }
    // }
    QMultiMap<QString,QString> Items(void) const;
    // text_length 8
    uint TextLength(void)       const { return m_data[7 + LengthOfItems()]; }
    // for (i=0; i<N; i++) { text_char 8 }
    QString Text(void) const
        { return dvb_decode_text(&m_data[8 + LengthOfItems()], TextLength()); }

    // HACK beg -- Pro7Sat is missing encoding
    QString Text(const enc_override &encoding_override) const
    {
        return dvb_decode_text(&m_data[8 + LengthOfItems()], TextLength(),
                               encoding_override);
    }
    // HACK end -- Pro7Sat is missing encoding
};

// DVB Bluebook A038 (Sept 2011) p 60
class FrequencyListDescriptor : public MPEGDescriptor
{
  public:
    explicit FrequencyListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::frequency_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x62
    // descriptor_length        8   1.0

    // reserved_future_use      6   2.0
    // coding_type              2   2.6
    enum : std::uint8_t
    {
        kCodingTypeNotDefined  = 0x0,
        kCodingTypeSatellite   = 0x1,
        kCodingTypeCable       = 0x2,
        kCodingTypeTerrestrial = 0x3,
    };
    uint CodingType(void)  const { return m_data[2] & 0x3; }
    // for (i=0;I<N;i++)
    // {
    //   centre_frequency      32
    // }
    uint FrequencyCount()  const { return DescriptorLength()>>2; }
    unsigned long long Frequency(uint i) const
    {
        if (kCodingTypeTerrestrial == CodingType())
        {
            return (((unsigned long long)m_data[(i*4)+3]<<24) |
                                        (m_data[(i*4)+4]<<16) |
                                        (m_data[(i*4)+5]<<8)  |
                                        (m_data[(i*4)+6]));
        }
        return byte4BCD2int(m_data[(i*4)+3], m_data[(i*4)+4],
                            m_data[(i*4)+5], m_data[(i*4)+6]);
    }
    unsigned long long FrequencyHz(uint i) const
    {
        return Frequency(i) *
            ((kCodingTypeTerrestrial == CodingType()) ? 10 : 100);
    }

    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 70
// ETSI EN 300 468 p 58
class MTV_PUBLIC LocalTimeOffsetDescriptor : public MPEGDescriptor
{
  public:
    explicit LocalTimeOffsetDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::local_time_offset) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x58
    // descriptor_length        8   1.0
    uint Count(void) const { return DescriptorLength() / 13; }
    // for(i=0;i<N;i++)
    // {
    //   country_code          24   0.0+p
    uint CountryCode(uint i) const
    {
        int o = 2 + (i*13);
        return ((m_data[o] << 16) | (m_data[o+1] << 8) | m_data[o+2]);
    }
    QString CountryCodeString(uint i) const
    {
        int o = 2 + (i*13);
        std::array<QChar,3> code
            { QChar(m_data[o]), QChar(m_data[o+1]), QChar(m_data[o+2]) };
        return QString(code.data(), 3);
    }
    //   country_region_id      6   3.0+p
    uint CountryRegionId(uint i) const { return m_data[2 + (i*13) + 3] >> 2; }
    //   reserved               1   3.6+p
    //   local_time_off_polarity 1   3.7+p
    /// -1 if true, +1 if false (behind utc, ahead of utc, resp).
    bool LocalTimeOffsetPolarity(uint i) const
        { return (m_data[2 + (i*13) + 3] & 0x01) != 0; }
    //   local_time_offset     16   4.0+p
    uint LocalTimeOffset(uint i) const
        { return (byteBCD2int(m_data[2 + (i*13) + 4]) * 60) +
                 byteBCD2int(m_data[2 + (i*13) + 5]); }
    int LocalTimeOffsetWithPolarity(uint i) const
        { return (LocalTimeOffsetPolarity(i) ? -1 : +1) * LocalTimeOffset(i); }
    //   time_of_change        40   6.0+p
    const unsigned char *TimeOfChangeData(uint i) const
        { return &m_data[2 + (i*13) + 6]; }
    QDateTime TimeOfChange(uint i)  const
        { return dvbdate2qt(TimeOfChangeData(i));   }
    time_t TimeOfChangeUnix(uint i) const
        { return dvbdate2unix(TimeOfChangeData(i)); }
    //   next_time_offset      16  11.0+p
    uint NextTimeOffset(uint i) const
        { return (byteBCD2int(m_data[2 + (i*13) + 11]) * 60) +
                 byteBCD2int(m_data[2 + (i*13) + 12]); }
    // }                           13.0
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 71
class MosaicDescriptor : public MPEGDescriptor
{
  public:
    explicit MosaicDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::mosaic) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x51
    // descriptor_length        8   1.0

    // mosaic_entry_point       1   2.0
    // num_horiz_elem_cells     3   2.1
    // reserved_future_use      1   2.4
    // num_vert_elem_cells      3   2.5
    // for (i=0;i<N; i++)
    // {
    //   logical_cell_id        6   0.0+p
    //   reserved_future_use    7   0.6+p
    //   logical_cell_pres_info 3   1.5+p
    //   elem_cell_field_len    8   2.0+p
    //   for (i=0; j<elementary_cell_field_length; j++)
    //   {
    //     reserved_future_use  2
    //     elementary_cell_id   6
    //   }
    //   cell_linkage_info      8
    //   if (cell_linkage_info == 0x01)
    //   {
    //     bouquet_id          16
    //   }
    //   if (cell_linkage_info == 0x02)
    //   {
    //     original_network_id 16
    //     transport_stream_id 16
    //     service_id          16
    //   }
    //   if (cell_linkage_info == 0x03)
    //   {
    //     original_network_id 16
    //     transport_stream_id 16
    //     service_id          16
    //   }
    //   if (cell_linkage_info == 0x04)
    //   {
    //     original_network_id 16
    //     transport_stream_id 16
    //     service_id          16
    //     event_id            16
    //   }
    // }
};

// DVB Bluebook A038 (Sept 2011) p 74
class MultilingualBouquetNameDescriptor : public MPEGDescriptor
{
  public:
    explicit MultilingualBouquetNameDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::multilingual_bouquet_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5c
    // descriptor_length        8   1.0

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   bouquet_name_length    8
    //   for (j=0;j<N;j++) { char 8 }
    // }
};

// DVB Bluebook A038 (Sept 2011) p 75
class MultilingualNetworkNameDescriptor : public MPEGDescriptor
{
  public:
    explicit MultilingualNetworkNameDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::multilingual_network_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5b
    // descriptor_length        8   1.0

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   network_name_length    8
    //   for (j=0;j<N;j++) { char 8 }
    // }
};

// DVB Bluebook A038 (Sept 2011) p 76
class MultilingualServiceNameDescriptor : public MPEGDescriptor
{
  public:
    explicit MultilingualServiceNameDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::multilingual_service_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5d
    // descriptor_length        8   1.0

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   service_provider_name_length 8
    //   for (j=0;j<N;j++) { char 8 }
    //   service_name_length    8
    //   for (j=0;j<N;j++) { char 8 }
    // }
};

// DVB Bluebook A038 (Sept 2011) p 76
class NVODReferenceDescriptor : public MPEGDescriptor
{
  public:
    explicit NVODReferenceDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::nvod_reference) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4b
    // descriptor_length        8   1.0
    uint Count(void) const { return DescriptorLength() / 6; }

    // for (i=0;i<N;i++)
    // {
    //   transport_stream_id   16
    uint TransportStreamId(uint i) const
        { return (m_data[(i * 6) + 2] << 8) | m_data[(i * 6) + 3]; }
    //   original_network_id   16
    uint OriginalNetworkId(uint i) const
        { return (m_data[(i * 6) + 4] << 8) |  m_data[(i * 6) + 5]; }
    //   service_id            16
    uint ServiceId(uint i) const
        { return (m_data[(i * 6) + 6] << 8) | m_data[(i * 6) + 7]; }
    // }
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 78
// ETSI EN 300 468
class ParentalRatingDescriptor : public MPEGDescriptor
{
  public:
    explicit ParentalRatingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::parental_rating) { }
    explicit ParentalRatingDescriptor(const std::vector<uint8_t> &data) :
        MPEGDescriptor(data, DescriptorID::parental_rating) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x55
    // descriptor_length        8   1.0
    uint Count(void) const { return DescriptorLength() / 4; }

    // for (i=0; i<N; i++)
    // {
    //   country_code          24
    //   rating                 8
    // }
    QString CountryCodeString(uint i) const
    {
        int o = 2 + (i*4);
        if (i >= Count())
            return {""};
        std::array<QChar,3> code
            { QChar(m_data[o]), QChar(m_data[o+1]), QChar(m_data[o+2]) };
        return QString(code.data(), 3);
    }
    int Rating(uint i) const
    {
        if (i >= Count())
        {
            return -1;
        }

        unsigned char rawRating = m_data[2 + 3 + (i*4)];
        if (rawRating == 0)
        {
            // 0x00 - undefined
            return -1;
        }
        if (rawRating <= 0x0F)
        {
            // 0x01 to 0x0F - minumum age = rating + 3 years
            return rawRating + 3;
        }

        // 0x10 to 0xFF - defined by the broadcaster
        return -1;
    }
};

// DVB Bluebook A038 (Sept 2011) p 78 (see also ETSI EN 300 231 PDC)
class PDCDescriptor : public MPEGDescriptor
{
  public:
    explicit PDCDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::pdc, 3) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x69
    // descriptor_length        8   1.0

    // reserved_future_use      4   2.0
    // program_id_label        20   2.4
    uint ProgramIdLabel(void) const
    { return  (m_data[2] & 0x0F) << 16 | m_data[3] << 8 |  m_data[4]; }
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("PDCDescriptor program_id_label(%1)")
            .arg(ProgramIdLabel());
    }
};

// DVB Bluebook A038 (Sept 2011) p 79 (see also ETSI TS 101 162)
class PrivateDataSpecifierDescriptor : public MPEGDescriptor
{
  public:
    explicit PrivateDataSpecifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::private_data_specifier) { }
    explicit PrivateDataSpecifierDescriptor(const std::vector<uint8_t> &data) :
        MPEGDescriptor(data, DescriptorID::private_data_specifier) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5f
    // descriptor_length        8   1.0

    // private_data_specifier  32   2.0
    uint32_t PrivateDataSpecifier (void) const
    {
        return (m_data[2] << 24 | m_data[3] << 16 | m_data[4] << 8 | m_data[5]);
    }
};

// DVB Bluebook A038 (Sept 2011) p 79
class ScramblingDescriptor : public MPEGDescriptor
{
  public:
    explicit ScramblingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::scrambling, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x65
    // descriptor_length        8   1.0

    // scrambling_mode          8   2.0
    uint ScramblingMode(void) const { return m_data[2]; }
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("ScramblingDescriptor scrambling_mode(%1)")
                .arg(ScramblingMode());
    }
};

// DVB Bluebook A038 (Feb 2019) p 83, Table 89: Service type coding
class ServiceDescriptorMapping
{
  public:
    explicit ServiceDescriptorMapping(const uint service_type) : m_serviceType(service_type) {}
    enum : std::uint8_t
    {
        kServiceTypeDigitalTelevision          = 0x01,
        kServiceTypeDigitalRadioSound          = 0x02,
        kServiceTypeTeletext                   = 0x03,
        kServiceTypeNVODReference              = 0x04,
        kServiceTypeNVODTimeShifted            = 0x05,
        kServiceTypeMosaic                     = 0x06,
        kServiceTypeFMRadio                    = 0x07,
        kServiceTypeDVB_SRM                    = 0x08,
        // reserved for future use             = 0x09,
        kServiceTypeAdvancedCodecDigitalRadioSound = 0x0A,
        kServiceTypeH264AVCMosaic              = 0x0B,
        kServiceTypeDataBroadcast              = 0x0C,
        kServiceTypeCommonInterface            = 0x0D,
        kServiceTypeRCS_Map                    = 0x0E,
        kServiceTypeRCS_FLS                    = 0x0F,
        kServiceTypeDVB_MHP                    = 0x10,
        kServiceTypeMPEG2HDDigitalTelevision   = 0x11,
        // reserved for future use             = 0x12 to 0x15
        kServiceTypeH264AVCSDDigitalTelevision = 0x16,
        kServiceTypeH264AVCSDNVODTimeShifted   = 0x17,
        kServiceTypeH264AVCSDNVODReference     = 0x18,
        kServiceTypeH264AVCHDDigitalTelevision = 0x19,
        kServiceTypeH264AVCHDNVODTimeShifted   = 0x1A,
        kServiceTypeH264AVCHDNVODReference     = 0x1B,
        kServiceTypeH264AVCFrameCompatiblePlanoStereoscopicHDDigitalTelevision = 0x1C,
        kServiceTypeH264AVCFrameCompatiblePlanoStereoscopicHDNVODTimeShifted   = 0x1D,
        kServiceTypeH264AVCFrameCompatiblePlanoStereoscopicHDNVODReference     = 0x1E,
        kServiceTypeHEVCDigitalTelevision      = 0x1F,
        kServiceTypeHEVCUHDDigitalTelevision   = 0x20,
        // reserved for future use             = 0x21 to 0x7F
        // user defined                        = 0x80 to 0xFE
        // reserved for future use             = 0xFF
        // User Defined descriptors for Dish network
        kServiceTypeEchoStarTV1                = 0x91,
        kServiceTypeEchoStarTV2                = 0x9a,
        kServiceTypeEchoStarTV3                = 0xa4,
        kServiceTypeEchoStarTV4                = 0xa6,
        kServiceTypeNimiqTV1                   = 0x81,
        kServiceTypeNimiqTV2                   = 0x85,
        kServiceTypeNimiqTV3                   = 0x86,
        kServiceTypeNimiqTV4                   = 0x89,
        kServiceTypeNimiqTV5                   = 0x8a,
        kServiceTypeNimiqTV6                   = 0x8d,
        kServiceTypeNimiqTV7                   = 0x8f,
        kServiceTypeNimiqTV8                   = 0x90,
        kServiceTypeNimiqTV9                   = 0x96,

    };
    uint ServiceType(void) const { return m_serviceType; }
    bool IsDTV(void) const
    {
        return ((ServiceType() == kServiceTypeDigitalTelevision) ||
                (ServiceType() == kServiceTypeH264AVCSDDigitalTelevision) ||
                IsHDTV() ||
                IsUHDTV() ||
                (ServiceType() == kServiceTypeEchoStarTV1) ||
                (ServiceType() == kServiceTypeEchoStarTV2) ||
                (ServiceType() == kServiceTypeEchoStarTV3) ||
                (ServiceType() == kServiceTypeEchoStarTV4) ||
                (ServiceType() == kServiceTypeNimiqTV1) ||
                (ServiceType() == kServiceTypeNimiqTV2) ||
                (ServiceType() == kServiceTypeNimiqTV3) ||
                (ServiceType() == kServiceTypeNimiqTV4) ||
                (ServiceType() == kServiceTypeNimiqTV5) ||
                (ServiceType() == kServiceTypeNimiqTV6) ||
                (ServiceType() == kServiceTypeNimiqTV7) ||
                (ServiceType() == kServiceTypeNimiqTV8) ||
                (ServiceType() == kServiceTypeNimiqTV9));
    }
    bool IsDigitalAudio(void) const
    {
        return
            (ServiceType() == kServiceTypeDigitalRadioSound) ||
            (ServiceType() == kServiceTypeAdvancedCodecDigitalRadioSound);
    }
    bool IsHDTV(void) const
    {
        return
            (ServiceType() == kServiceTypeMPEG2HDDigitalTelevision) ||
            (ServiceType() == kServiceTypeH264AVCHDDigitalTelevision) ||
            (ServiceType() == kServiceTypeH264AVCFrameCompatiblePlanoStereoscopicHDDigitalTelevision);
    }
    bool IsUHDTV(void) const
    {
        return
            (ServiceType() == kServiceTypeHEVCDigitalTelevision) ||
            (ServiceType() == kServiceTypeHEVCUHDDigitalTelevision);
    }
    bool IsTeletext(void) const
    {
        return ServiceType() == kServiceTypeDataBroadcast;
    }
    QString toString(void) const;

  private:
    uint m_serviceType;
};

// DVB Bluebook A038 (Feb 2019) p 82
class ServiceDescriptor : public MPEGDescriptor
{
  public:
    explicit ServiceDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x48
    // descriptor_length        8   1.0

    // service_type             8   2.0
    uint ServiceType(void) const { return m_data[2]; }
    // svc_provider_name_len    8   3.0
    uint ServiceProviderNameLength(void) const { return m_data[3]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceProviderName(void) const
        { return dvb_decode_text(m_data + 4, ServiceProviderNameLength()); }
    QString ServiceProviderShortName(void) const
    {
        return dvb_decode_short_name(m_data + 4, ServiceProviderNameLength());
    }
    // service_name_length      8
    uint ServiceNameLength(void) const
        { return m_data[4 + ServiceProviderNameLength()]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceName(void) const
    {
        return dvb_decode_text(m_data + 5 + ServiceProviderNameLength(),
                               ServiceNameLength());
    }
    QString ServiceShortName(void) const
    {
        return dvb_decode_short_name(m_data + 5 + ServiceProviderNameLength(),
                                     ServiceNameLength());
    }
    bool IsDTV(void) const
        { return ServiceDescriptorMapping(ServiceType()).IsDTV(); }
    bool IsDigitalAudio(void) const
        { return ServiceDescriptorMapping(ServiceType()).IsDigitalAudio(); }
    bool IsHDTV(void) const
        { return ServiceDescriptorMapping(ServiceType()).IsHDTV(); }
    bool IsTeletext(void) const
        { return ServiceDescriptorMapping(ServiceType()).IsTeletext(); }

    QString toString(void) const override // MPEGDescriptor
    {
        return QString("ServiceDescriptor: %1 %2(0x%3)")
            .arg(ServiceName(),
                 ServiceDescriptorMapping(ServiceType()).toString())
            .arg(ServiceType(),2,16,QChar('0'));
    }
};

// DVB Bluebook A038 (Feb 2019) p 84
class ServiceAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    explicit ServiceAvailabilityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service_availability) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x72
    // descriptor_length        8   1.0

    // availability_flag        1   2.0
    // reserved                 7   2.1
    // for (i=0;i<N;i++) { cell_id 16 }
};

// DVB Bluebook A038 (Feb 2019) p 84
class ServiceListDescriptor : public MPEGDescriptor
{
  public:
    explicit ServiceListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x41
    // descriptor_length        8   1.0

    // for (i=0;i<N;I++)
    // {
    //   service_id            16  0.0+p
    //   service_type           8  2.0+p
    // }
    uint ServiceCount(void) const { return DescriptorLength() / 3; }

    uint ServiceID(uint i) const
        { return (m_data[2+(i*3)] << 8) | m_data[3+(i*3)]; }

    uint ServiceType(uint i) const { return m_data[4+(i*3)]; }

    QString toString(void) const override // MPEGDescriptor
    {
        QString str = QString("ServiceListDescriptor: %1 Services")
            .arg(ServiceCount());
        for (uint i=0; i<ServiceCount(); i++)
        {
            str.append("\n");
            str.append(QString("      Service (%1) Type%2 (0x%3)")
                .arg(ServiceID(i))
                .arg(ServiceDescriptorMapping(ServiceType(i)).toString())
                .arg(ServiceType(i),2,16,QChar('0')));
        }
        return str;
    }
};

// DVB Bluebook A038 (Sept 2011) p 82
class ServiceMoveDescriptor : public MPEGDescriptor
{
  public:
    explicit ServiceMoveDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service_move) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x60
    // descriptor_length        8   1.0

    // new_original_network_id 16   2.0
    // new_transport_stream_id 16   4.0
    // new_service_id          16   6.0
};

// DVB Bluebook A038 (Sept 2011) p 83
class ShortEventDescriptor : public MPEGDescriptor
{
  public:
    explicit ShortEventDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::short_event) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4d
    // descriptor_length        8   1.0

    // ISO_639_language_code   24   2.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&m_data[2]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // event_name_length        8   5.0
    uint EventNameLength(void) const { return m_data[5]; }
    // for (i=0;i<event_name_length;i++) { event_name_char 8 }
    QString EventName(void) const
        { return dvb_decode_text(&m_data[6], m_data[5]); }
    QString EventShortName(void) const
        { return dvb_decode_short_name(&m_data[6], m_data[5]); }
    // text_length              8
    uint TextLength(void) const { return m_data[6 + m_data[5]]; }
    // for (i=0;i<text_length;i++) { text_char 8 }
    QString Text(void) const
        { return dvb_decode_text(&m_data[7 + m_data[5]], TextLength()); }

    // HACK beg -- Pro7Sat is missing encoding
    QString EventName(const enc_override& encoding_override) const
    {
        return dvb_decode_text(&m_data[6], m_data[5],
                               encoding_override);
    }

    QString Text(const enc_override& encoding_override) const
    {
        return dvb_decode_text(&m_data[7 + m_data[5]], TextLength(),
                               encoding_override);
    }
    // HACK end -- Pro7Sat is missing encoding

    QString toString(void) const override // MPEGDescriptor
        { return LanguageString() + " : " + EventName() + " : " + Text(); }
};

// DVB Bluebook A038 (Sept 2011) p 84
class ShortSmoothingBufferDescriptor : public MPEGDescriptor
{
  public:
    explicit ShortSmoothingBufferDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::short_smoothing_buffer) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x61
    // descriptor_length        8   1.0

    // sb_size                  2   2.0
    // sb_leak_rate             6   2.2
    // for (i=0; i<N; i++)
    // { DVB_reserved           8 }
};

// DVB Bluebook A038 (Sept 2011) p 85
/// This is used to label streams so the can be treated differently,
/// for instance each stream may get it's own cueing instructions.
class StreamIdentifierDescriptor : public MPEGDescriptor
{
  public:
    explicit StreamIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::stream_identifier, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x52
    // descriptor_length        8   1.0       0x01

    // component_tag            8   2.0
    uint ComponentTag(void) const { return m_data[2]; }
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("Stream Identifier Descriptor (0x52): ComponentTag=0x%1")
            .arg(ComponentTag(),1,16);
    }
};

// DVB Bluebook A038 (Sept 2011) p 86
class StuffingDescriptor : public MPEGDescriptor
{
  public:
    explicit StuffingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dvb_stuffing) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x42
    // descriptor_length        8   1.0
    // stuffing_byte            *   2.0
    QString toString(void) const override // MPEGDescriptor
    {
        return QString("Stuffing Descriptor (0x42) length(%1)")
            .arg(DescriptorLength());
    }
};

// DVB Bluebook A038 (Sept 2011) p 86
class SubtitlingDescriptor : public MPEGDescriptor
{
  public:
    explicit SubtitlingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::subtitling) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x59
    // descriptor_length        8   1.0

    uint StreamCount(void) const { return DescriptorLength() >> 3; }
    // for (i= 0;i<N;I++)
    // {
    //   ISO_639_language_code 24   0.0+(i*8)
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&m_data[2 + (i<<3)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }

    //   subtitling_type        8   3.0+(i*8)
    uint SubtitleType(uint i) const
        { return m_data[5 + (i<<3)]; }
    //   composition_page_id   16   4.0+(i*8)
    uint CompositionPageID(uint i) const
        { return (m_data[6 + (i<<3)] << 8) | m_data[7 + (i<<3)]; }
    //   ancillary_page_id     16   6.0+(i*8)
    uint AncillaryPageID(uint i) const
        { return (m_data[8 + (i<<3)] << 8) | m_data[9 + (i<<3)]; }
    // }                            8.0
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 87
class TelephoneDescriptor : public MPEGDescriptor
{
  public:
    explicit TelephoneDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::telephone) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x57
    // descriptor_length        8   1.0

    // reserved_future_use      2   2.0
    // foreign_availability     1   2.2
    // connection_type          5   2.3
    // reserved_future_use      1   3.0
    // country_prefix_length    2   3.1
    // i18n_area_code_len       3   3.4
    // operator_code_length     2   3.6
    // reserved_future_use      1   3.7
    // national_area_code_len   3   4.0
    // core_number_length       4   4.4
    //
    // for (i=0; i<N; i++)
    //   { country_prefix_char   8 }
    // for (i=0; i<N; i++)
    //   { international_area_code_char 8 }
    // for (i=0; i<N; i++)
    //   { operator_code_char    8 }
    // for (i=0; i<N; i++)
    //   { national_area_code_char 8 }
    // for (i=0; i<N; i++)
    //   { core_number_char 8 }
};

// DVB Bluebook A038 (Sept 2011) p 88
class TeletextDescriptor : public MPEGDescriptor
{
  public:
    explicit TeletextDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::teletext) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x56
    // descriptor_length        8   1.0

    uint StreamCount(void) const { return DescriptorLength() / 5; }

    // for (i=0; i<N; i++)
    // {
    //   ISO_639_language_code 24  0.0
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&m_data[2 + (i*5)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }
    //   teletext_type         5   3.0
    uint TeletextType(uint i) const
        { return m_data[5 + (i*5)] >> 3; }
    //   teletext_magazine_num 3   3.5
    uint TeletextMagazineNum(uint i) const
        { return m_data[5 + (i*5)] & 0x7; }
    //   teletext_page_num     8   4.0
    uint TeletextPageNum(uint i) const
        { return m_data[6 + (i*5)]; }
    // }                           5.0
    QString toString(void) const override; // MPEGDescriptor
};

// DVB Bluebook A038 (Sept 2011) p 89
class TimeShiftedEventDescriptor : public MPEGDescriptor
{
  public:
    explicit TimeShiftedEventDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::time_shifted_event) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4f
    // descriptor_length        8   1.0

    // reference_service_id    16   2.0
    // reference_event_id      16   4.0
};

// DVB Bluebook A038 (Sept 2011) p 90
class TimeShiftedServiceDescriptor : public MPEGDescriptor
{
  public:
    explicit TimeShiftedServiceDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dvb_time_shifted_service) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4c
    // descriptor_length        8   1.0

    // reference_service_id    16   2.0
};

// DVB Bluebook A038 (Sept 2011) p 90
class TransportStreamDescriptor : public MPEGDescriptor
{
  public:
    explicit TransportStreamDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::transport_stream) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x67
    // descriptor_length        8   1.0

    // for (i=0; i<N; i++) { byte 8 }
    QString Data(void) const
        { return dvb_decode_text(&m_data[2], DescriptorLength()); }
    QString toString(void) const override // MPEGDescriptor
        { return QString("TransportStreamDescriptor data(%1)").arg(Data()); }
};

// DVB Bluebook A038 (Sept 2011) p 91
class VBIDataDescriptor : public MPEGDescriptor
{
  public:
    explicit VBIDataDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::vbi_data) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x45
    // descriptor_length        8   1.0

    // for (i=0; i<N; i++)
    // {
    //   data_service_id        8  0.0+p
    //   data_service_length    8  1.0+p
    //   if ((data_service_id&0x7) && data_service_id!=0x3))
    //   {
    //     for (i=0; i<N; i++)
    //     {
    //       reserved           2  2.0+p2
    //       field_parity       1  2.2+p2
    //       line_offset        5  2.3+p2
    //     }
    //   }
    //   else
    //   {
    //     for (i=0; i<N; i++) { reserved 8 }
    //   }
    // }
};

// DVB Bluebook A038 (Sept 2011) p 92
class VBITeletextDescriptor : public MPEGDescriptor
{
  public:
    explicit VBITeletextDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::vbi_teletext) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x46
    // descriptor_length        8   1.0

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24   0.0+p
    //   teletext_type          5   3.0+p
    //   teletext_magazine_num  3   3.5+p
    //   teletext_page_num      8   4.0+p
    // }                            5.0
};

// DVB Bluebook A038 (Feb 2019) p 125
class PartialTransportStreamDescriptor : public MPEGDescriptor
{
  public:
    explicit PartialTransportStreamDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::partial_transport_stream) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x63
    // descriptor_length        8   1.0

    // DVB_reserved_future_use  2   2.0
    // peak_rate               22   2.2
    uint PeakRate(void) const
        { return (m_data[2] & 0x3f) << 16 | m_data[3] << 8 | m_data[4]; }
    // DVB_reserved_future_use            2   5.0
    // minimum_overall_smoothing_rate    22   5.2
    uint SmoothRate(void) const
        { return (m_data[5] & 0x3f) << 16 | m_data[6] << 8 | m_data[7]; }
    // DVB_reserved_future_use            2   8.0
    // maximum_overall_smoothing_buffer  14   8.2
    uint SmoothBuf(void) const { return ((m_data[8] & 0x3f) << 8) | m_data[9]; }
    QString toString(void) const override; // MPEGDescriptor
};


// a_52a.pdf p125 Table A7 (for DVB)
// DVB Bluebook A038 (Feb 2019) p 145
class AC3Descriptor : public MPEGDescriptor
{
  public:
    explicit AC3Descriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ac3) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6A
    // descriptor_length        8   1.0

    // component_type_flag      1   2.0
    bool HasComponentType(void) const { return ( m_data[2] & 0x80 ) != 0; }
    // bsid_flag                1   2.1
    bool HasBSID(void) const { return ( m_data[2] & 0x40 ) != 0; }
    // mainid_flag              1   2.2
    bool HasMainID(void) const { return ( m_data[2] & 0x20 ) != 0; }
    // asvc_flag                1   2.3
    bool HasASVC(void) const { return ( m_data[2] & 0x10 ) != 0; }
    // reserved_flags           4   2.4
    // if (component_type_flag == 1)
    //   { component_type       8 uimsbf }
    uint ComponentType(void) const { return m_data[3]; }
    // if (bsid_flag == 1)
    //   { bsid                 8 uimsbf }
    uint BSID(void) const
        { return (HasComponentType()) ? m_data[4] : m_data[3]; }
    // if (mainid_flag == 1)
    //   { mainid               8 uimsbf }
    uint MainID(void) const
    {
        int offset = 3;
        offset += (HasComponentType()) ? 1 : 0;
        offset += (HasBSID()) ? 1 : 0;
        return m_data[offset];
    }
    // if (asvc_flag==1)
    //   { asvc                 8 uimsbf }
    uint ASVC(void) const
    {
        int offset = 3;
        offset += (HasComponentType()) ? 1 : 0;
        offset += (HasBSID()) ? 1 : 0;
        offset += (HasMainID()) ? 1 : 0;
        return m_data[offset];
    }
    // for (I=0;I<N;I++)
    //   { additional_info[i] N*8 uimsbf }
    //};
    QString toString(void) const override; // MPEGDescriptor
};

static QString coderate_inner(uint coderate)
{
    switch (coderate)
    {
        case 0x0:  return "auto"; // not actually defined in spec
        case 0x1:  return "1/2";
        case 0x2:  return "2/3";
        case 0x3:  return "3/4";
        case 0x4:  return "5/6";
        case 0x5:  return "7/8";
        case 0x8:  return "8/9";
        case 0xf:  return "none";
        default:   return "auto"; // not actually defined in spec
    }
}

/**
 *  \brief DVB Logical Channel Descriptor
 *
 * NIT descriptor ID 0x83 (Private Extension)
 *
 * Provides the Logical Channel Number (LCN) for each channel. This isn't used
 * in all DVB Networks.
 */
class DVBLogicalChannelDescriptor : public MPEGDescriptor
{
  public:
    explicit DVBLogicalChannelDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dvb_logical_channel_descriptor) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x83
    // descriptor_length        8   1.0

    uint ChannelCount(void) const { return DescriptorLength() >> 2; }

    uint ServiceID(uint i) const
        { return (m_data[2 + (i<<2)] << 8) | m_data[3 + (i<<2)]; }

    uint ChannelNumber(uint i) const
        { return ((m_data[4 + (i<<2)] << 8) | m_data[5 + (i<<2)]) & 0x3ff; }

    QString toString(void) const override; // MPEGDescriptor
};

/**
 *  \brief DVB HD Simulcast Logical Channel Descriptor
 *
 * NIT descriptor ID 0x88 (Private Extension)
 *
 * Provides the Logical Channel Number (LCN) for each channel when the channel
 * is simultaneously broadcast in SD and HD.
 */
class DVBSimulcastChannelDescriptor : public MPEGDescriptor
{
  public:
    explicit DVBSimulcastChannelDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dvb_simulcast_channel_descriptor) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x88
    // descriptor_length        8   1.0

    uint ChannelCount(void) const { return DescriptorLength() >> 2; }

    uint ServiceID(uint i) const
        { return (m_data[2 + (i<<2)] << 8) | m_data[3 + (i<<2)]; }

    uint ChannelNumber(uint i) const
        { return ((m_data[4 + (i<<2)] << 8) | m_data[5 + (i<<2)]) & 0x3ff; }

    QString toString(void) const override; // MPEGDescriptor
};

/**
 *  \brief Freesat Logical Channel Number descriptor
 *
 * BAT descriptor ID 0xd3 (Private Extension)
 *
 * Provides the Logical Channel Number (LCN) for each channel.
 *
 * https://blog.nexusuk.org/2014/07/decoding-freesat-part-2.html
 */

class FreesatLCNDescriptor : public MPEGDescriptor
{
  public:
    explicit FreesatLCNDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::freesat_lcn_table)
    {
        assert(m_data && PrivateDescriptorID::freesat_lcn_table== DescriptorTag());
        if (nullptr == m_data) // assert is disabled when NDEBUG is defined
            return;

        const unsigned char *payload = &data[2];

        size_t offset = 0;
        while ((offset + 5 < DescriptorLength()) &&
               (offset + 5 + payload[offset+4] <= DescriptorLength()))
        {
            m_entries.push_back(&payload[offset]);
            offset += 5 + payload[offset+4];
        }
    }
    //       Name                 bits  loc  expected value
    // descriptor_tag               8   0.0       0xd3
    // descriptor_length            8   1.0
    // for (i=0;i<N;i++) {
    //   service_id                16   0.0+p
    //   chan_id                   15   2.1+p
    //   length                     8   4.0+p
    //   for (j=0;j<N;j++) {
    //     unknown                  4   0.0+p2
    //     logical_channel_number   12  0.4+p2
    //     region_id                16  2.0+p2
    //   }
    // }

    uint ServiceCount(void) const
        { return m_entries.size(); }

    uint ServiceID(size_t i) const
        { return *m_entries[i] << 8 | *(m_entries[i]+1); }

    uint ChanID(size_t i) const
        { return (*(m_entries[i] + 2) << 8 | *(m_entries[i] + 3)) & 0x7FFF; }

    uint LCNCount(size_t i) const
        { return *(m_entries[i] + 4) / 4; }

    uint LogicalChannelNumber(size_t i, size_t j) const
        { return (*(m_entries[i] + 5 + (j*4)) << 8 | *(m_entries[i] + 5 + (j*4) + 1)) & 0xFFF; }

    uint RegionID(size_t i, size_t j) const
        { return *(m_entries[i] + 5 + (j*4) + 2) << 8 | *(m_entries[i] + 5 + (j*4) + 3); }

    QString toString(void) const override; // MPEGDescriptor

  private:
    desc_list_t m_entries;
};

/**
 *  \brief Freesat Region descriptor
 *
 * BAT descriptor ID 0xd4 (Private Extension)
 *
 * Region table
 *
 * https://blog.nexusuk.org/2014/07/decoding-freesat-part-2.html
 */
class FreesatRegionDescriptor : public MPEGDescriptor
{
  public:
    explicit FreesatRegionDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::freesat_region_table)
    {
        assert(m_data && PrivateDescriptorID::freesat_region_table == DescriptorTag());
        if (nullptr == m_data) // assert is disabled when NDEBUG is defined
            return;

        const unsigned char *payload = &data[2];

        uint offset = 0;
        while ((offset + 6 < DescriptorLength()) &&
               (offset + 6 + payload[offset+5] <= DescriptorLength()))
        {
            m_entries.push_back(&payload[offset]);
            offset += 6 + payload[offset+5];
        }
    }
    //       Name                 bits  loc  expected value
    // descriptor_tag               8   0.0       0xd4
    // descriptor_length            8   1.0
    // for (i=0;i<N;i++) {
    //   region_id                 16   0.0+p
    //   language_code             24   2.0+p     eng
    //   text_length                8   5.0+p
    //   for (j=0;j<N;j++) {
    //      text_char               8
    //   }
    // }

    uint RegionCount(void) const
       { return m_entries.size(); }

    int RegionID(uint i) const
        { return *m_entries[i] << 8 | *(m_entries[i]+1); }

    QString Language(uint i) const
        { return QString::fromLatin1((char *) m_entries[i] + 2, 3); }

    QString RegionName(uint i) const
        { return QString::fromLatin1((char *) m_entries[i] + 6, *(m_entries[i] + 5)); }

    QString toString(void) const override; // MPEGDescriptor

  private:
    desc_list_t m_entries;
};

/**
 *  \brief Freesat Channel Callsign descriptor
 *
 * BAT descriptor 0xd9 (Private Extension)
 *
 * Provides the callsign for a channel.
 */
class FreesatCallsignDescriptor : public MPEGDescriptor
{
  public:
    explicit FreesatCallsignDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::freesat_callsign)
    {
        assert(m_data && PrivateDescriptorID::freesat_callsign == DescriptorTag());
    }

    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0xd8
    // descriptor_length        8   1.0

    // ISO_639_language_code   24   2.0
    // callsign_length          8   5.0
    // for (j=0;j<N;j++) {
    //    callsign_char         8

    QString Language(void) const
        { return QString::fromLatin1((char *) m_data +2, 3); }

    QString Callsign(void) const
        { return dvb_decode_short_name(&m_data[6], m_data[5]); }

    QString toString(void) const override; // MPEGDescriptor
};

/**
 *  \brief Sky Logical Channel Number descriptor
 *
 * BAT descriptor ID 0xb1 (Private Extension)
 *
 * Provides the Logical Channel Number (LCN) for each channel.
 *
 * Descriptor layout from tvheadend src/input/mpegts/dvb_psi.c
 * function dvb_bskyb_local_channels
 */

class SkyLCNDescriptor : public MPEGDescriptor
{
  public:
    explicit SkyLCNDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::sky_lcn_table)
    {
        assert(m_data && PrivateDescriptorID::sky_lcn_table== DescriptorTag());
    }
    //       Name                 bits  loc  expected value
    // descriptor_tag               8   0.0       0xd3
    // descriptor_length            8   1.0
    // region_id                   16   2.0
    // for (i=0;i<N;i++) {
    //   service_id                16   0.0+p
    //   service_type               8   2.0+p
    //   unknown                   16   3.0+p
    //   logical_channel_number    16   5.0+p2
    //   unknown                   16   7.0+p2
    // }

    uint RegionID(void) const
        { return (*(m_data + 3) != 0xFF) ? *(m_data + 3) : 0xFFFF;}

    uint RegionRaw(void) const
        { return *(m_data + 2) << 8 | *(m_data + 3);}

    uint ServiceCount(void) const
        { return (DescriptorLength() - 2) / 9; }

    uint ServiceID(size_t i) const
        { return *(m_data + 4 + (i*9)) << 8 | *(m_data + 5 + (i*9)); }

    uint ServiceType(size_t i) const
        { return *(m_data + 6 + (i*9)); }

    uint ChannelID(size_t i) const
        { return *(m_data + 7 + (i*9)) << 8 | *(m_data + 8 + (i*9)); }

    uint LogicalChannelNumber(size_t i) const
        { return *(m_data + 9 + (i*9)) << 8 | *(m_data + 10 + (i*9)); }

    uint Flags(size_t i) const
        { return *(m_data + 11 + (i*9)) << 8 | *(m_data + 12 + (i*9)); }

    QString toString(void) const override; // MPEGDescriptor
};

// Descriptor layout similar to SkyLCNDescriptor
class OpenTVChannelListDescriptor : public MPEGDescriptor
{
  public:
    explicit OpenTVChannelListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::opentv_channel_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0xB1
    // descriptor_length        8   1.0

    uint ChannelCount(void) const { return (DescriptorLength() - 2)/9; }

    uint RegionID() const
        { return (m_data[2] << 8) | m_data[3]; }

    uint ServiceID(uint i) const
        { return (m_data[4 + 0 + (i*9)] << 8) | m_data[4 + 1 + (i*9)]; }

    uint ChannelType(uint i) const
        { return m_data[4 + 2 + (i*9)]; }

    uint ChannelID(uint i) const
        { return ((m_data[4 + 3 + (i*9)] << 8) | m_data[4 + 4 + (i*9)]); }

    uint ChannelNumber(uint i) const
        { return ((m_data[4 + 5 + (i*9)] << 8) | m_data[4 + 6 + (i*9)]); }

    uint Flags(uint i) const
        { return ((m_data[4 + 7 + (i*9)] << 8) | m_data[4 + 8 + (i*9)]) & 0xf; }

    QString toString(void) const override; // MPEGDescriptor
};

// ETSI TS 102 323 (TV Anytime)
class DVBContentIdentifierDescriptor : public MPEGDescriptor
{
  public:
    explicit DVBContentIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dvb_content_identifier)
    {
        size_t count  = 0;

        m_crid.fill(nullptr);

        if (IsValid())
        {
            uint8_t position = 2; /// position points to the first byte of the "sub"descriptor
            while (m_data[1] >= position)
            {
                size_t length = m_data[position+1];
                m_crid[count] = &m_data[position];
                count++;
                position+=length+2;
            }
        }
        m_cridCount = count;
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x76
    // descriptor_length        8   1.0

    uint ContentType(size_t n=0) const { return m_crid[n][0] >> 2; }

    uint ContentEncoding(size_t n=0) const { return m_crid[n][0] & 0x03; }

    // A content identifier is a URI.  It may contain UTF-8 encoded using %XX.
    QString ContentId(size_t n=0) const
    {
        // Access the array in two steps so cppcheck doesn't get confused.
        const uint8_t* cridN = m_crid[n];
        int length = cridN[1];
        int positionOfHash = length-1;
        while (positionOfHash >= 0) {
            if (cridN[2 + positionOfHash] == '#') {
                length = positionOfHash; /* remove the hash and the following IMI */
                break;
            }
            positionOfHash--;
        }
        return QString::fromLatin1((const char *)&m_crid[n][2], length);
    }

    size_t CRIDCount() const
    {
        return m_cridCount;
    }

  private:
    size_t m_cridCount;
    std::array<const uint8_t*,8> m_crid {};
};

// ETSI TS 102 323 (TV Anytime)
class DefaultAuthorityDescriptor : public MPEGDescriptor
{
  public:
    explicit DefaultAuthorityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::default_authority) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x73
    // descriptor_length        8   1.0

    QString DefaultAuthority(void) const
    {
        return QString::fromLatin1((const char *)m_data+2, m_data[1]);
    }

    QString toString(void) const override // MPEGDescriptor
    {
        return QString("DefaultAuthorityDescriptor: Authority(%1)")
            .arg(DefaultAuthority());
    }
};

// Draft ETSI EN 300 468 V1.16.1 (2019-05)
// DVB Bluebook A038 (Feb 2019) p 59
// Table 42: S2 satellite delivery system descriptor
// 0x79
class S2SatelliteDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    explicit S2SatelliteDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::s2_satellite_delivery_system) { }
    //       Name                     bits  loc     expected value
    // descriptor_tag                   8   0.0     0x79
    // descriptor_length                8   1.0
    // scrambling_sequence_selector     1   2.0
    uint ScramblingSequenceSelector() const
        { return (m_data[2] >> 7) & 0x01; }

    // multiple_input_stream_flag       1   2.1
    uint MultipleInputStreamFlag() const
        { return (m_data[2] >> 6) & 0x01; }

    // reserved_zero_future_use         1   2.2
    // not_timeslice_flag               1   2.3
    uint NotTimesliceFlag() const
        { return (m_data[2] >> 4) & 0x01; }

    // reserved_future_use              2   2.4
    // TS_GS_mode                       2   2.6
    uint TSGSMode() const
        { return m_data[2] & 0x03; }

    // if (scrambling_sequence_selector == 1){
    //   reserved_future_use            6   3.0
    //   scrambling_sequence_index     18   3.6
    // }
    // if (multiple_input_stream_flag == 1){
    //   input_stream_identifier        8   6.0
    // }
    // if (not_timeslice_flag == 0){
    //   timeslice_number               8   7.0
    // }

    QString toString(void) const override; // MPEGDescriptor
};

/*
 * private UPC Cablecom (Austria) episode descriptor for Horizon middleware
 */
class PrivateUPCCablecomEpisodeTitleDescriptor : public MPEGDescriptor
{
    public:
     explicit PrivateUPCCablecomEpisodeTitleDescriptor(const unsigned char *data, int len = 300) :
         MPEGDescriptor(data, len, PrivateDescriptorID::upc_event_episode_title) { }
    explicit PrivateUPCCablecomEpisodeTitleDescriptor(const std::vector<uint8_t> &data) :
        MPEGDescriptor(data, PrivateDescriptorID::upc_event_episode_title) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0xa7
    // descriptor_length        8   1.0

    // ISO_639_language_code   24   2.0
    int LanguageKey(void) const
    {
        return iso639_str3_to_key(&m_data[2]);
    }
    QString LanguageString(void) const
    {
        return iso639_key_to_str3(LanguageKey());
    }
    int CanonicalLanguageKey(void) const
    {
        return iso639_key_to_canonical_key(LanguageKey());
    }
    QString CanonicalLanguageString(void) const
    {
        return iso639_key_to_str3(CanonicalLanguageKey());
    }

    uint TextLength(void) const
    {
        return m_data[1] - 3;
    }

    QString Text(void) const
    {
        return dvb_decode_text(&m_data[5], TextLength());
    }
};

#endif // DVB_DESCRIPTORS_H
