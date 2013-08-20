// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _DVB_DESCRIPTORS_H_
#define _DVB_DESCRIPTORS_H_

#include <QMutex>
#include <QString>

#include "mythtvexp.h" // MTV_PUBLIC - Symbol Visibility
#include "mpegdescriptors.h"
#include "programinfo.h" // for subtitle types and audio and video properties

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

extern QString dvb_decode_text(const unsigned char *src, uint length,
                               const unsigned char *encoding_override,
                               uint encoding_override_length);

inline QString dvb_decode_text(const unsigned char *src, uint length)
{
    return dvb_decode_text(src, length, NULL, 0);
}

QString dvb_decode_short_name(const unsigned char *src, uint raw_length);

#define byteBCDH2int(i) (i >> 4)
#define byteBCDL2int(i) (i & 0x0f)
#define byteBCD2int(i) (byteBCDH2int(i) * 10 + byteBCDL2int(i))
#define byte2BCD2int(i, j) \
  (byteBCDH2int(i) * 1000     + byteBCDL2int(i) * 100       + \
   byteBCDH2int(j) * 10       + byteBCDL2int(j))
#define byte3BCD2int(i, j, k) \
  (byteBCDH2int(i) * 100000   + byteBCDL2int(i) * 10000     + \
   byteBCDH2int(j) * 1000     + byteBCDL2int(j) * 100       + \
   byteBCDH2int(k) * 10       + byteBCDL2int(k))
#define byte4BCD2int(i, j, k, l) \
  (byteBCDH2int(i) * 10000000LL + byteBCDL2int(i) * 1000000 + \
   byteBCDH2int(j) * 100000     + byteBCDL2int(j) * 10000   + \
   byteBCDH2int(k) * 1000       + byteBCDL2int(k) * 100     + \
   byteBCDH2int(l) * 10         + byteBCDL2int(l))

// DVB Bluebook A038 (Sept 2011) p 77
class NetworkNameDescriptor : public MPEGDescriptor
{
  public:
    NetworkNameDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::network_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x40
    // descriptor_length        8   1.0
    // for (i=0;i<N;i++){ char 8 uimsbf }
    QString Name(void) const
        { return dvb_decode_text(_data+2, DescriptorLength()); }
    QString ShortName(void) const
        { return dvb_decode_short_name(_data+2, DescriptorLength()); }
    QString toString(void) const
        { return QString("NetworkNameDescriptor: ")+Name(); }
};

// DVB Bluebook A038 (Sept 2011) p 63
class LinkageDescriptor : public MPEGDescriptor
{
  public:
    LinkageDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::linkage)
    {
        if (!_data)
            return;
        if (DescriptorLength() < 7)
        {
            _data = NULL;
        }
        else if (kMobileHandOver == LinkageType())
        {
            uint end = 8;
            if (DescriptorLength() < end)
            {
                _data = NULL;
                return;
            }
            end += (HasMobileNetworkID()) ? 2 : 0;
            end += (HasMobileInitialServiceID()) ? 2 : 0;
            if (DescriptorLength() < end)
                _data = NULL;
            m_offset = end + 2;
        }
        else if (kEventLinkage == LinkageType())
        {
            if (DescriptorLength() < 10)
                _data = NULL;
            m_offset = 12;
        }
    }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4A
    // descriptor_length        8   1.0
    // transport_stream_id     16   2.0
    uint TSID(void)            const { return (_data[2]<<8) | _data[3]; }
    // original_network_id     16   4.0
    uint OriginalNetworkID()   const { return (_data[4]<<8) | _data[5]; }
    // service_id              16   6.0
    uint ServiceID(void)       const { return (_data[6]<<8) | _data[7]; }
    // linkage_type             8   8.0
    enum
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
    uint LinkageType(void)     const { return _data[8]; }
    QString LinkageTypeString(void) const;

    // if (linkage_type == 0x08)
    // {
    //    hand-over_type        4   9.0
    enum
    {
        kHandOverIdentical         = 0x01,
        kHandOverLocalVariation    = 0x02,
        kHandOverAssociatedService = 0x03,
    };
    uint MobileHandOverType(void) const { return _data[9]>>4; }
    QString MobileHandOverTypeString(void) const;
    //    reserved_future_use   3   9.4
    //    origin_type           1   9.7
    enum
    {
        kOriginNIT = 0x0,
        kOriginSDT = 0x1,
    };
    uint MobileOriginType(void) const { return _data[9]&0x1; }
    QString MobileOriginTypeString(void) const;
    //    if (hand-over_type == 0x01 || hand-over_type == 0x02 ||
    //        hand-over_type == 0x03)
    //    { network_id         16  10.0 }
    bool HasMobileNetworkID(void) const
        { return bool(MobileHandOverType() & 0x3); }
    uint MobileNetworkID(void) const { return (_data[10]<<8) | _data[11]; }
    //    if (origin_type ==0x00)
    //    { initial_service_id 16  HasNetworkID()?10.0:12.0 }
    bool HasMobileInitialServiceID(void) const
        { return kOriginNIT == MobileOriginType(); }
    uint MobileInitialServiceID(void) const
    {
        return HasMobileNetworkID() ?
            ((_data[12]<<8) | _data[13]) : ((_data[10]<<8) | _data[11]);
    }
    // }
    // if (linkage_type == 0x0D)
    // {
    //    target_event_id      16   9.0
    uint TargetEventID(void) const { return (_data[9]<<8) | _data[10]; }
    //    target_listed         1  11.0
    bool IsTargetListed(void) const { return _data[11]&0x80; }
    //    event_simulcast       1  11.1
    bool IsEventSimulcast(void) const { return _data[11]&0x40; }
    //    reserved              6  11.2
    // }
    //      for (i=0;i<N;i++)
    //        { private_data_byte 8 bslbf }
    const unsigned char *PrivateData(void) const
        { return &_data[m_offset]; }
    uint PrivateDataLength(void) const
        { return DescriptorLength() + 2 - m_offset; }

  private:
    uint m_offset;
};

// DVB Bluebook A038 (Sept 2011) p 38
class AdaptationFieldDataDescriptor : public MPEGDescriptor
{
  public:
    AdaptationFieldDataDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::adaptation_field_data, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x70
    // descriptor_length        8   1.0
    // adapt_field_data_id      8   2.0
    uint AdaptationFieldDataID(void) const { return _data[2]; }
    QString toString(void) const
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
    AncillaryDataDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ancillary_data, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6b
    // descriptor_length        8   1.0
    // ancillary_data_id        8   2.0
    uint AncillaryDataID(void) const { return _data[2]; }
    QString toString(void) const
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
    AnnouncementSupportDescriptor(const unsigned char *data, int len = 300) :
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
    BouquetNameDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::bouquet_name) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x47
    // descriptor_length        8   1.0
    // for(i=0;i<N;i++) { char 8 }
    QString BouquetName(void) const
         { return dvb_decode_text(_data+2, _data[1]); }
    QString BouquetShortName(void) const
         { return dvb_decode_short_name(_data+2, _data[1]); }

    QString toString(void) const
    {
        return QString("BouquetNameDescriptor: Bouquet Name(%1)")
            .arg(BouquetName());
    }
};

// DVB Bluebook A038 (Sept 2011) p 41
class CAIdentifierDescriptor : public MPEGDescriptor
{
  public:
    CAIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ca_identifier) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x53
    // descriptor_length        8   1.0
    //
    uint CASystemCount(void) const { return DescriptorLength() >> 1; }
    // for (i=0; i<N; i++)
    //   { CA_system_id 16 }
    int CASystemId(uint i) const
        { return (_data[2 + i*2] << 8) | _data[3 + i*2]; }
    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 42
class CellFrequencyLinkDescriptor : public MPEGDescriptor
{
  public:
    CellFrequencyLinkDescriptor(const unsigned char *data, int len = 300) :
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
    CellListDescriptor(const unsigned char *data, int len = 300) :
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
    ComponentDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::component) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x50
    // descriptor_length        8   1.0
    // reserved_future_use      4   2.0
    // stream_content           4   2.4
    uint StreamContent(void) const { return _data[2] & 0xf; }
    // component_type           8   3.0
    uint ComponentType(void) const { return _data[3]; }
    // component_tag            8   4.0
    uint ComponentTag(void)  const { return _data[4]; }
    // ISO_639_language_code   24   5.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&_data[5]); }
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
        return 0x1 == StreamContent() ||
               0x5 == StreamContent();
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

    unsigned char VideoProperties(void) const
    {
        if (0x1 == StreamContent())
            return MPEG2Properties();
        if (0x5 == StreamContent())
            return VID_AVC | AVCProperties();

        return VID_UNKNOWN;
    }

    unsigned char MPEG2Properties(void) const
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

    unsigned char AVCProperties(void) const
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

    unsigned char AudioProperties(void) const
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

    unsigned char MP2Properties(void) const
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

    unsigned char AC3Properties(void) const
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

    unsigned char HEAACProperties(void) const
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

    unsigned char SubtitleType(void) const
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

    QString toString(void) const
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
    ContentDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::content) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x54
    // descriptor_length        8   1.0

    uint Count(void)         const { return DescriptorLength() >> 1; }
    // for (i=0;i<N;i++)
    // {
    //   content_nibble_level_1 4   0.0+p
    uint Nibble1(uint i)     const { return _data[2 + (i<<1)] >> 4; }
    //   content_nibble_level_2 4   0.4+p
    uint Nibble2(uint i)     const { return _data[2 + (i<<1)] & 0xf; }

    uint Nibble(uint i)      const { return _data[2 + (i<<1)]; }

    //   user_nibble            4   1.0+p
    uint UserNibble1(uint i) const { return _data[3 + (i<<1)] >> 4; }
    //   user_nibble            4   1.4+p
    uint UserNibble2(uint i) const { return _data[3 + (i<<1)] & 0xf; }
    uint UserNibble(uint i)  const { return _data[3 + (i<<1)]; }
    // }                            2.0

    ProgramInfo::CategoryType GetMythCategory(uint i) const;
    QString GetDescription(uint i) const;
    QString toString(void) const;

  protected:
    static void Init(void);

  protected:
    static QMutex             categoryLock;
    static QMap<uint,QString> categoryDesc;
    static volatile bool      categoryDescExists;
};

// DVB Bluebook A038 (Sept 2011) p 49
class CountryAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    CountryAvailabilityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::country_availability) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x49
    // descriptor_length        8   1.0

    uint CountryCount(void) const { return ((DescriptorLength() - 1) / 3); }

    // country_avail_flag       1   2.0
    bool IsAvailable(void) const { return (_data[2] & 0x1); }
    // reserved_future_use      7   2.1
    //
    // for (i=0; i<N; i++)
    //   { country_code        24 }
    QString CountryNames(void) const
    {
        QString countries="";
        for (uint i=0; i<CountryCount(); i++)
        {
            if (i!=0) countries.append(" ");
            countries.append(QString::fromLatin1(
                                 (const char *)_data+(3*(i+1)), 3));
        };
        return countries;
    }

    QString toString(void) const
    {
        return QString("CountryAvailabilityDescriptor: Available(%1) in (%2)")
            .arg(IsAvailable()).arg(CountryNames());
    }
};

// DVB Bluebook A038 (Sept 2011) p 50
class DataBroadcastDescriptor : public MPEGDescriptor
{
  public:
    DataBroadcastDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::data_broadcast) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x64
    // descriptor_length        8   1.0

    // data_broadcast_id       16   2.0
    uint DataBroadcastId(void) const { return _data[2] << 8 | _data[3]; }
    // component_tag            8   4.0
    uint DataComponentTag(void) const { return _data[4]; }
    // selector_length          8   5.0
    uint SelectorLength(void) const { return _data[5]; }
    // for (i=0; i<selector_length; i++)
    // {
    //   selector_byte          8
    const unsigned char *Selector(void) const { return &_data[6]; }
    // }
    // ISO_639_language_code   24
    int LanguageKey(void) const
        { return iso639_str3_to_key(&_data[6 + SelectorLength()]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // text_length              8
    uint TextLength(void) const { return _data[6 + SelectorLength() + 3]; }
    // for (i=0; i<text_length; i++) { text_char 8 }
    QString Text(void) const
    {
        return dvb_decode_text(&_data[6 + SelectorLength() + 4], TextLength());
    }

    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 51
class DataBroadcastIdDescriptor : public MPEGDescriptor
{
  public:
    DataBroadcastIdDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::data_broadcast_id) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x66
    // descriptor_length        8   1.0

    // data_broadcast_id       16   2.0
    uint DataBroadCastId(void) const { return _data[2] << 8 | _data[3]; }
    // for(i=0; i < N;i++ )
    // { id_selector_byte       8 }
};

// DVB Bluebook A038 (Sept 2011) p 51
class CableDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    CableDeliverySystemDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::cable_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x44
    // descriptor_length        8   1.0

    // frequency               32   2.0
    uint FrequencyRaw(void) const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    unsigned long long FrequencyHz(void) const
    {
        return byte4BCD2int(_data[2], _data[3], _data[4], _data[5]) * 100;
    }
    // reserved_future_use     12   6.0
    // FEC_outer                4   7.4
    enum
    {
        kOuterFEC_None        = 0x1,
        kOuterFEC_RS204_RS188 = 0x2,
    };
    uint FECOuter(void) const { return _data[7] & 0xf; }
    QString FECOuterString(void) const
    {
        return (FECOuter() == kOuterFEC_None) ? "None" :
            ((FECOuter() == kOuterFEC_RS204_RS188) ? "RS(204/188)" : "unknown");
    }
    // modulation               8   8.0
    enum
    {
        kModulationQAM16  = 0x01,
        kModulationQAM32  = 0x02,
        kModulationQAM64  = 0x03,
        kModulationQAM128 = 0x04,
        kModulationQAM256 = 0x05,
    };
    uint Modulation(void) const { return _data[8]; }
    QString ModulationString(void) const
    {
        static QString ms[] =
            { "auto", "qam_16", "qam_32", "qam_64", "qam_128", "qam_256" };
        return (Modulation() <= kModulationQAM256) ?
            ms[Modulation()] : QString("auto");
    }
    // symbol_rate             28   9.0
    uint SymbolRateRaw(void) const
    {
        return ((_data[9]<<20) | (_data[10]<<12) |
                (_data[11]<<4) | (_data[12]>>4));
    }
    uint SymbolRateHz(void) const
    {
        return ((byte3BCD2int(_data[9], _data[10], _data[11]) * 1000) +
                (byteBCDH2int(_data[12]) * 100));
    }
    // FEC_inner                4  12.4
    enum
    {
        kInnerFEC_1_2_ConvolutionCodeRate = 0x1,
        kInnerFEC_2_3_ConvolutionCodeRate = 0x2,
        kInnerFEC_3_4_ConvolutionCodeRate = 0x3,
        kInnerFEC_5_6_ConvolutionCodeRate = 0x4,
        kInnerFEC_7_8_ConvolutionCodeRate = 0x5,
        kInnerFEC_8_9_ConvolutionCodeRate = 0x6,
        kInnerFEC_None                    = 0xF,
    };
    uint FECInner(void) const { return _data[12] & 0xf; }
    QString FECInnerString(void) const { return coderate_inner(FECInner()); }
    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 53
class SatelliteDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    SatelliteDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::satellite_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x43
    // descriptor_length        8   1.0

    /// frequency              32   2.0
    uint FrequencyRaw(void) const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    unsigned long long FrequencyHz(void) const
    {
        return byte4BCD2int(_data[2], _data[3], _data[4], _data[5]) * 10;
    }
    /// orbital_position       16   6.0
    uint OrbitalPosition(void) const
        { return byte2BCD2int(_data[6], _data[7]); }
    QString OrbitalPositionString(void) const
    {
        uint num = OrbitalPosition();
        return QString("%1.%2 %3").arg(num / 10).arg(num % 10)
            .arg((IsEast()) ? "East" : "West");
    }
    double OrbitalPositionFloat()  const
        { return ((double) OrbitalPosition()) / 10.0; }
    /// west_east_flag          1   8.0
    bool IsEast(void)             const { return (_data[8]&0x80); }
    bool IsWest(void)             const { return !IsEast(); }
    // polarization             2   8.1
    uint Polarization(void)       const { return (_data[8]>>5)&0x3; }
    QString PolarizationString()  const
    {
        static QString ps[] = { "h", "v", "l", "r" };
        return ps[Polarization()];
    }
    bool IsCircularPolarization(void) const { return (_data[8]>>6)&0x1; }
    bool IsLinearPolarization(void) const { return !((_data[8]>>6)&0x1); }
    bool IsHorizontalLeftPolarization(void) const { return (_data[8]>>5)&0x1; }
    bool IsVerticalRightPolarization(void) const
        { return !((_data[8]>>5)&0x1); }
    // roll off                 2   8.3
    enum
    {
        kRollOff_35,
        kRollOff_20,
        kRollOff_25,
        kRollOff_Auto,
    };
    uint RollOff(void) const { return (_data[8]>>3)&0x3; }
    QString RollOffString(void) const
    {
        static QString ro[] = { "0.35", "0.20", "0.25", "auto" };
        return ro[RollOff()];
    }
    // modulation system        1   8.5
    uint ModulationSystem(void) const { return (_data[8]>>2)&0x1; }
    QString ModulationSystemString(void) const
    {
        return ModulationSystem() ? "DVB-S2" : "DVB-S";
    }
    // modulation               2   8.6
    enum
    {
        kModulationQPSK_NS = 0x0, // Non standard QPSK for Bell ExpressVu
        // should be "auto" according to DVB SI standard
        kModulationQPSK   = 0x1,
        kModulation8PSK   = 0x2,
        kModulationQAM16  = 0x3,
    };
    uint Modulation(void) const { return _data[8]&0x03; }
    QString ModulationString(void) const
    {
        static QString ms[] = { "qpsk", "qpsk", "8psk", "qam_16" };
        return ms[Modulation()];
    }
    // symbol_rate             28   9.0
    uint SymbolRate(void) const
    {
        return ((_data[9]<<20) | (_data[10]<<12) |
                (_data[11]<<4) | (_data[12]>>4));
    }
    uint SymbolRateHz(void) const
    {
        return ((byte3BCD2int(_data[9], _data[10], _data[11]) * 1000) +
                (byteBCDH2int(_data[12]) * 100));
    }
    // FEC_inner                4  12.4
    enum
    {
        kInnerFEC_1_2_ConvolutionCodeRate = 0x1,
        kInnerFEC_2_3_ConvolutionCodeRate = 0x2,
        kInnerFEC_3_4_ConvolutionCodeRate = 0x3,
        kInnerFEC_5_6_ConvolutionCodeRate = 0x4,
        kInnerFEC_7_8_ConvolutionCodeRate = 0x5,
        kInnerFEC_8_9_ConvolutionCodeRate = 0x6,
        kInnerFEC_None                    = 0xF,
    };
    uint FECInner(void) const { return _data[12] & 0xf; }
    QString FECInnerString(void) const { return coderate_inner(FECInner()); }

    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 55
class TerrestrialDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    TerrestrialDeliverySystemDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::terrestrial_delivery_system) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5a
    // descriptor_length        8   1.0

    // centre_frequency        32   2.0
    uint Frequency(void) const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    uint64_t FrequencyHz(void) const { return uint64_t(Frequency()) * 10ULL; }

    // bandwidth                3   6.0
    enum
    {
        kBandwidth8Mhz = 0x0,
        kBandwidth7Mhz = 0x1,
        kBandwidth6Mhz = 0x2,
        kBandwidth5Mhz = 0x3,
    };
    uint Bandwidth(void) const { return _data[6]>>5; }
    uint BandwidthHz(void) const { return (8 - Bandwidth()) * 1000000; }
    QString BandwidthString(void) const
    {
        static QString bs[] = { "8", "7", "6", "5" };
        return (Bandwidth() <= kBandwidth5Mhz) ? bs[Bandwidth()] : "auto";
    }
    // priority                 1   6.3
    bool HighPriority(void) const { return _data[6] & 0x10; }
    // time_slicing_indicator   1   6.4
    bool IsTimeSlicingIndicatorUsed(void) const { return !(_data[6] & 0x08); }
    // MPE-FEC_indicator        1   6.5
    bool IsMPE_FECUsed(void) const { return !(_data[6] & 0x04); }
    // reserved_future_use      2   6.6
    // constellation            2   7.0
    enum
    {
        kConstellationQPSK  = 0x0,
        kConstellationQAM16 = 0x1,
        kConstellationQAM64 = 0x2,
    };
    uint Constellation(void) const { return _data[7]>>6; }
    QString ConstellationString(void) const
    {
        static QString cs[] = { "qpsk", "qam_16", "qam_64" };
        return (Constellation() <= kConstellationQAM64) ?
            cs[Constellation()] : "auto";
    }
    // hierarchy_information    3   7.2
    enum
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
    uint Hierarchy(void) const { return (_data[7]>>3) & 0x7; }

    /// \bug returns "a" for values >= 4 for compatibility with siparser.cpp
    QString HierarchyString(void) const
    {
        static QString hs[] = { "n", "1", "2", "4", "a", "a", "a", "a" };
        return hs[Hierarchy()];
    }
    bool NativeInterleaver(void) const { return _data[7] & 0x20; }
    uint Alpha(void) const
    {
        uint i = (_data[7]>>3) & 0x3;
        return (0x2 == i) ? 4 : i;
    }
    // code_rate-HP_stream      3   7.5
    enum
    {
        kCodeRate_1_2 = 0x0,
        kCodeRate_2_3 = 0x1,
        kCodeRate_3_4 = 0x2,
        kCodeRate_5_6 = 0x3,
        kCodeRate_7_8 = 0x4,
    };
    uint CodeRateHP(void) const { return _data[7] & 0x7; }
    QString CodeRateHPString(void) const
    {
        static QString cr[] = {
            "1/2", "2/3", "3/4", "5/6", "7/8", "auto", "auto", "auto"
        };
        return cr[CodeRateHP()];
    }
    // code_rate-LP_stream      3   8.0
    uint CodeRateLP(void) const { return (_data[8]>>5) & 0x7; }
    QString CodeRateLPString(void) const
    {
        static QString cr[] = {
            "1/2", "2/3", "3/4", "5/6", "7/8", "auto", "auto", "auto"
        };
        return cr[CodeRateLP()];
    }
    // guard_interval           2   8.3
    enum
    {
        kGuardInterval_1_32 = 0x0,
        kGuardInterval_1_16 = 0x1,
        kGuardInterval_1_8  = 0x2,
        kGuardInterval_1_4  = 0x3,
    };
    uint GuardInterval(void) const { return (_data[8]>>3) & 0x3; }
    QString GuardIntervalString(void) const
    {
        static QString gi[] = { "1/32", "1/16", "1/8", "1/4" };
        return gi[GuardInterval()];
    }
    // transmission_mode        2   8.5
    enum
    {
        kTransmissionMode2k = 0x00,
        kTransmissionMode8k = 0x01,
        kTransmissionMode4k = 0x02,
    };
    uint TransmissionMode(void) const { return (_data[8]>>1) & 0x3; }
    QString TransmissionModeString(void) const
    {
        static QString tm[] = { "2", "8", "4", "auto" };
        return tm[TransmissionMode()];
    }
    // other_frequency_flag     1   8.7
    bool OtherFrequencyInUse(void) const { return _data[8] & 0x1; }
    // reserved_future_use     32   9.0

    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 58
class DSNGDescriptor : public MPEGDescriptor
{
  public:
    DSNGDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dsng) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x68
    // descriptor_length        8   1.0
    // for (i=0;i<N;i++) { byte 8 }
};

// DVB Bluebook A038 (Sept 2011) p 58
class ExtendedEventDescriptor : public MPEGDescriptor
{
  public:
    ExtendedEventDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::extended_event) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4e
    // descriptor_length        8   1.0

    // descriptor_number        4   2.0
    uint DescriptorNumber(void) const { return _data[2] >> 4; }
    // last_number              4   2.4
    uint LastNumber(void)       const { return _data[2] & 0xf; }
    // ISO_639_language_code   24   3.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&_data[3]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // length_of_items          8   6.0
    uint LengthOfItems(void)    const { return _data[6]; }
    // for ( i=0;i<N;i++)
    // {
    //   item_description_len   8   0.0+p
    //   for (j=0;j<N;j++) { item_desc_char 8 }
    //   item_length            8   1.0+p2
    //   for (j=0;j<N;j++) { item_char 8 }
    // }
    // text_length 8
    uint TextLength(void)       const { return _data[7 + _data[6]]; }
    // for (i=0; i<N; i++) { text_char 8 }
    QString Text(void) const
        { return dvb_decode_text(&_data[8 + _data[6]], TextLength()); }

    // HACK beg -- Pro7Sat is missing encoding
    QString Text(const unsigned char *encoding_override,
                 uint encoding_length) const
    {
        return dvb_decode_text(&_data[8 + _data[6]], TextLength(),
                               encoding_override, encoding_length);
    }
    // HACK end -- Pro7Sat is missing encoding
};

// DVB Bluebook A038 (Sept 2011) p 60
class FrequencyListDescriptor : public MPEGDescriptor
{
  public:
    FrequencyListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::frequency_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x62
    // descriptor_length        8   1.0

    // reserved_future_use      6   2.0
    // coding_type              2   2.6
    enum
    {
        kCodingTypeNotDefined  = 0x0,
        kCodingTypeSatellite   = 0x1,
        kCodingTypeCable       = 0x2,
        kCodingTypeTerrestrial = 0x3,
    };
    uint CodingType(void)  const { return _data[2] & 0x3; }
    // for (i=0;I<N;i++)
    // {
    //   centre_frequency      32
    // }
    uint FrequencyCount()  const { return DescriptorLength()>>2; }
    unsigned long long Frequency(uint i) const
    {
        if (kCodingTypeTerrestrial == CodingType())
            return ((_data[3 + (i<<2)]<<24) | (_data[4 + (i<<2)]<<16) |
                    (_data[5 + (i<<2)]<<8)  | (_data[6 + (i<<2)]));
        else
            return byte4BCD2int(_data[3 + (i<<2)], _data[4 + (i<<2)],
                                _data[5 + (i<<2)], _data[6 + (i<<2)]);
    }
    unsigned long long FrequencyHz(uint i) const
    {
        return Frequency(i) *
            ((kCodingTypeTerrestrial == CodingType()) ? 10 : 100);
    }

    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 70
// ETSI EN 300 468 p 58
class LocalTimeOffsetDescriptor : public MPEGDescriptor
{
  public:
    LocalTimeOffsetDescriptor(const unsigned char *data, int len = 300) :
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
        int o = 2 + i*13;
        return ((_data[o] << 16) | (_data[o+1] << 8) | _data[o+2]);
    }
    QString CountryCodeString(uint i) const
    {
        int o = 2 + i*13;
        return QString(_data[o]) + QChar(_data[o+1]) + QChar(_data[o+2]);
    }
    //   country_region_id      6   3.0+p
    uint CountryRegionId(uint i) const { return _data[2 + i*13 + 3] >> 2; }
    //   reserved               1   3.6+p
    //   local_time_off_polarity 1   3.7+p
    /// -1 if true, +1 if false (behind utc, ahead of utc, resp).
    bool LocalTimeOffsetPolarity(uint i) const
        { return _data[2 + i*13 + 3] & 0x01; }
    //   local_time_offset     16   4.0+p
    uint LocalTimeOffset(uint i) const
        { return (_data[2 + i*13 + 4] << 8) | _data[2 + i*13 + 5]; }
    int LocalTimeOffsetWithPolarity(uint i) const
        { return (LocalTimeOffsetPolarity(i) ? -1 : +1) * LocalTimeOffset(i); }
    //   time_of_change        40   6.0+p
    // TODO decode this
    //   next_time_offset      16  11.0+p
    uint NextTimeOffset(uint i) const
        { return (_data[2 + i*13 + 11]<<8) | _data[2 + i*13 + 12]; }
    // }                           13.0
    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 71
class MosaicDescriptor : public MPEGDescriptor
{
  public:
    MosaicDescriptor(const unsigned char *data, int len = 300) :
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
    MultilingualBouquetNameDescriptor(
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
    MultilingualNetworkNameDescriptor(
        const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::multilingual_network_name)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5b
    // descriptor_length        8   1.0
    }

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
    MultilingualServiceNameDescriptor(
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
    NVODReferenceDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::nvod_reference) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4b
    // descriptor_length        8   1.0
    uint Count(void) const { return DescriptorLength() / 6; }

    // for (i=0;i<N;i++)
    // {
    //   transport_stream_id   16
    uint TransportStreamId(uint i) const
        { return (_data[i * 6 + 2] << 8) | _data[i * 6 + 3]; }
    //   original_network_id   16
    uint OriginalNetworkId(uint i) const
        { return (_data[i * 6 + 4] << 8) |  _data[i * 6 + 5]; }
    //   service_id            16
    uint ServiceId(uint i) const
        { return (_data[i * 6 + 6] << 8) | _data[i * 6 + 7]; }
    // }
    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 78
class ParentalRatingDescriptor : public MPEGDescriptor
{
  public:
    ParentalRatingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::parental_rating) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x55
    // descriptor_length        8   1.0
    uint Count(void) const { return DescriptorLength() / 4; }

    // for (i=0; i<N; i++)
    // {
    //   country_code          24
    //   rating                 8
    // }
};

// DVB Bluebook A038 (Sept 2011) p 78 (see also ETSI EN 300 231 PDC)
class PDCDescriptor : public MPEGDescriptor
{
  public:
    PDCDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::pdc, 3) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x69
    // descriptor_length        8   1.0

    // reserved_future_use      4   2.0
    // program_id_label        20   2.4
    uint ProgramIdLabel(void) const
    { return  (_data[2] & 0x0F) << 16 | _data[3] << 8 |  _data[4]; }
    QString toString(void) const
    {
        return QString("PDCDescriptor program_id_label(%1)")
            .arg(ProgramIdLabel());
    }
};

// DVB Bluebook A038 (Sept 2011) p 79 (see also ETSI TS 101 162)
class PrivateDataSpecifierDescriptor : public MPEGDescriptor
{
  public:
    PrivateDataSpecifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::private_data_specifier) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5f
    // descriptor_length        8   1.0

    // private_data_specifier  32   2.0
};

// DVB Bluebook A038 (Sept 2011) p 79
class ScramblingDescriptor : public MPEGDescriptor
{
  public:
    ScramblingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::scrambling, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x65
    // descriptor_length        8   1.0

    // scrambling_mode          8   2.0
    uint ScramblingMode(void) const { return _data[2]; }
    QString toString(void) const
    {
        return QString("ScramblingDescriptor scrambling_mode(%1)")
                .arg(ScramblingMode());
    }
};

// Map serviceid's to their types
class ServiceDescriptorMapping
{
  public:
    ServiceDescriptorMapping(const uint serviceid) { m_serviceid = serviceid; }
    enum
    {
        kServiceTypeDigitalTelevision        = 0x01,
        kServiceTypeDigitalRadioSound        = 0x02,
        kServiceTypeTeletext                 = 0x03,
        kServiceTypeNVODReference            = 0x04,
        kServiceTypeNVODTimeShifted          = 0x05,
        kServiceTypeMosaic                   = 0x06,
        kServiceTypePALCodedSignal           = 0x07,
        kServiceTypeSECAMCodedSignal         = 0x08,
        kServiceTypeD_D2_MAC                 = 0x09,
        kServiceTypeAdvancedCodecDigitalRadioSound        = 0x0A,
        kServiceTypeNTSCCodedSignal          = 0x0B,
        kServiceTypeDataBroadcast            = 0x0C,
        kServiceTypeCommonInterface          = 0x0D,
        kServiceTypeRCS_Map                  = 0x0E,
        kServiceTypeRCS_FLS                  = 0x0F,
        kServiceTypeDVB_MHP                  = 0x10,
        kServiceTypeHDTV                     = 0x11,
        kServiceTypeAdvancedCodecSDDigitalTelevision       = 0x16,
        kServiceTypeAdvancedCodecHDDigitalTelevision       = 0x19,
        kServiceTypeAdvancedCodecFrameCompatiblePlanoStereoscopicHDTelevisionService = 0x1c,
        kServiceTypeEchoStarTV1              = 0x91,
        kServiceTypeEchoStarTV2              = 0x9a,
        kServiceTypeEchoStarTV3              = 0xa4,
        kServiceTypeEchoStarTV4              = 0xa6,
        kServiceTypeNimiqTV1                 = 0x81,
        kServiceTypeNimiqTV2                 = 0x85,
        kServiceTypeNimiqTV3                 = 0x86,
        kServiceTypeNimiqTV4                 = 0x89,
        kServiceTypeNimiqTV5                 = 0x8a,
        kServiceTypeNimiqTV6                 = 0x8d,
        kServiceTypeNimiqTV7                 = 0x8f,
        kServiceTypeNimiqTV8                 = 0x90,
        kServiceTypeNimiqTV9                 = 0x96,

    };
    uint ServiceType(void) const { return m_serviceid; }
    bool IsDTV(void) const
    {
        return ((ServiceType() == kServiceTypeDigitalTelevision) ||
                (ServiceType() ==
                 kServiceTypeAdvancedCodecSDDigitalTelevision) ||
                IsHDTV() ||
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
        return ((ServiceType() == kServiceTypeDigitalRadioSound) ||
                (ServiceType() == kServiceTypeAdvancedCodecDigitalRadioSound));
    }
    bool IsHDTV(void) const
    {
        return
            (ServiceType() == kServiceTypeHDTV) ||
            (ServiceType() == kServiceTypeAdvancedCodecHDDigitalTelevision) ||
            (ServiceType() == kServiceTypeAdvancedCodecFrameCompatiblePlanoStereoscopicHDTelevisionService);
    }
    bool IsTeletext(void) const
    {
        return ServiceType() == kServiceTypeDataBroadcast;
    }
    QString toString(void) const;

  private:
    uint m_serviceid;
};

// DVB Bluebook A038 (Sept 2011) p 80
class ServiceDescriptor : public MPEGDescriptor
{
  public:
    ServiceDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x48
    // descriptor_length        8   1.0

    // service_type             8   2.0
    uint ServiceType(void) const { return _data[2]; }
    // svc_provider_name_len    8   3.0
    uint ServiceProviderNameLength(void) const { return _data[3]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceProviderName(void) const
        { return dvb_decode_text(_data + 4, ServiceProviderNameLength()); }
    QString ServiceProviderShortName(void) const
    {
        return dvb_decode_short_name(_data + 4, ServiceProviderNameLength());
    }
    // service_name_length      8
    uint ServiceNameLength(void) const
        { return _data[4 + ServiceProviderNameLength()]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceName(void) const
    {
        return dvb_decode_text(_data + 5 + ServiceProviderNameLength(),
                               ServiceNameLength());
    }
    QString ServiceShortName(void) const
    {
        return dvb_decode_short_name(_data + 5 + ServiceProviderNameLength(),
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

    QString toString(void) const
    {
        return QString("ServiceDescriptor: %1 %2").arg(ServiceName())
            .arg(ServiceDescriptorMapping(ServiceType()).toString());
    }
};

// DVB Bluebook A038 (Sept 2011) p 82
class ServiceAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    ServiceAvailabilityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::service_availability) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x72
    // descriptor_length        8   1.0

    // availability_flag        1   2.0
    // reserved                 7   2.1
    // for (i=0;i<N;i++) { cell_id 16 }
};

// DVB Bluebook A038 (Sept 2011) p 82
class ServiceListDescriptor : public MPEGDescriptor
{
  public:
    ServiceListDescriptor(const unsigned char *data, int len = 300) :
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
        { return (_data[2+i*3] << 8) | _data[3+i*3]; }

    uint ServiceType(uint i) const { return _data[4+i*3]; }

    QString toString(void) const
    {
        QString str = QString("ServiceListDescriptor: %1 Services\n")
            .arg(ServiceCount());
        for (uint i=0; i<ServiceCount(); i++)
        {
            if (i!=0) str.append("\n");
            str.append(QString("      Service (%1) Type%2").arg(ServiceID(i))
                .arg(ServiceDescriptorMapping(ServiceType(i)).toString()));
        }
        return str;
    }
};

// DVB Bluebook A038 (Sept 2011) p 82
class ServiceMoveDescriptor : public MPEGDescriptor
{
  public:
    ServiceMoveDescriptor(const unsigned char *data, int len = 300) :
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
    ShortEventDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::short_event) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4d
    // descriptor_length        8   1.0

    // ISO_639_language_code   24   2.0
    int LanguageKey(void) const
        { return iso639_str3_to_key(&_data[2]); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // event_name_length        8   5.0
    uint EventNameLength(void) const { return _data[5]; }
    // for (i=0;i<event_name_length;i++) { event_name_char 8 }
    QString EventName(void) const
        { return dvb_decode_text(&_data[6], _data[5]); }
    QString EventShortName(void) const
        { return dvb_decode_short_name(&_data[6], _data[5]); }
    // text_length              8
    uint TextLength(void) const { return _data[6 + _data[5]]; }
    // for (i=0;i<text_length;i++) { text_char 8 }
    QString Text(void) const
        { return dvb_decode_text(&_data[7 + _data[5]], TextLength()); }

    // HACK beg -- Pro7Sat is missing encoding
    QString EventName(const unsigned char *encoding_override,
                      uint encoding_length) const
    {
        return dvb_decode_text(&_data[6], _data[5],
                               encoding_override, encoding_length);
    }

    QString Text(const unsigned char *encoding_override,
                 uint encoding_length) const
    {
        return dvb_decode_text(&_data[7 + _data[5]], TextLength(),
                               encoding_override, encoding_length);
    }
    // HACK end -- Pro7Sat is missing encoding

    QString toString(void) const
        { return LanguageString() + " : " + EventName() + " : " + Text(); }
};

// DVB Bluebook A038 (Sept 2011) p 84
class ShortSmoothingBufferDescriptor : public MPEGDescriptor
{
  public:
    ShortSmoothingBufferDescriptor(const unsigned char *data, int len = 300) :
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
    StreamIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::stream_identifier, 1) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x52
    // descriptor_length        8   1.0       0x01

    // component_tag            8   2.0
    uint ComponentTag(void) const { return _data[2]; }
    QString toString(void) const
    {
        return QString("Stream Identifier Descriptor (0x52): ComponentTag=0x%1")
            .arg(ComponentTag(),1,16);
    }
};

// DVB Bluebook A038 (Sept 2011) p 86
class StuffingDescriptor : public MPEGDescriptor
{
  public:
    StuffingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dvb_stuffing) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x42
    // descriptor_length        8   1.0
    // stuffing_byte            *   2.0
    QString toString(void) const
    {
        return QString("Stuffing Descriptor (0x42) length(%1)")
            .arg(DescriptorLength());
    }
};

// DVB Bluebook A038 (Sept 2011) p 86
class SubtitlingDescriptor : public MPEGDescriptor
{
  public:
    SubtitlingDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::subtitling) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x59
    // descriptor_length        8   1.0

    uint StreamCount(void) const { return DescriptorLength() >> 3; }
    // for (i= 0;i<N;I++)
    // {
    //   ISO_639_language_code 24   0.0+(i*8)
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&_data[2 + (i<<3)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }

    //   subtitling_type        8   3.0+(i*8)
    uint SubtitleType(uint i) const
        { return _data[5 + (i<<3)]; }
    //   composition_page_id   16   4.0+(i*8)
    uint CompositionPageID(uint i) const
        { return (_data[6 + (i<<3)] << 8) | _data[7 + (i<<3)]; }
    //   ancillary_page_id     16   6.0+(i*8)
    uint AncillaryPageID(uint i) const
        { return (_data[8 + (i<<3)] << 8) | _data[9 + (i<<3)]; }
    // }                            8.0
};

// DVB Bluebook A038 (Sept 2011) p 87
class TelephoneDescriptor : public MPEGDescriptor
{
  public:
    TelephoneDescriptor(const unsigned char *data, int len = 300) :
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
    TeletextDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::teletext) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x56
    // descriptor_length        8   1.0

    uint StreamCount(void) const { return DescriptorLength() / 5; }

    // for (i=0; i<N; i++)
    // {
    //   ISO_639_language_code 24  0.0
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&_data[2 + (i*5)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }
    //   teletext_type         5   3.0
    uint TeletextType(uint i) const
        { return _data[5 + (i*5)] >> 3; }
    //   teletext_magazine_num 3   3.5
    uint TeletextMagazineNum(uint i) const
        { return _data[5 + (i*5)] & 0x7; }
    //   teletext_page_num     8   4.0
    uint TeletextPageNum(uint i) const
        { return _data[6 + (i*5)]; }
    // }                           5.0
    QString toString(void) const;
};

// DVB Bluebook A038 (Sept 2011) p 89
class TimeShiftedEventDescriptor : public MPEGDescriptor
{
  public:
    TimeShiftedEventDescriptor(const unsigned char *data, int len = 300) :
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
    TimeShiftedServiceDescriptor(const unsigned char *data, int len = 300) :
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
    TransportStreamDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::transport_stream) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x67
    // descriptor_length        8   1.0

    // for (i=0; i<N; i++) { byte 8 }
    QString Data(void) const
        { return dvb_decode_text(&_data[2], DescriptorLength()); }
    QString toString(void) const
        { return QString("TransportStreamDescriptor data(%1)").arg(Data()); }
};

// DVB Bluebook A038 (Sept 2011) p 91
class VBIDataDescriptor : public MPEGDescriptor
{
  public:
    VBIDataDescriptor(const unsigned char *data, int len = 300) :
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
    VBITeletextDescriptor(const unsigned char *data, int len = 300) :
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

// DVB Bluebook A038 (Sept 2011) p 119
class PartialTransportStreamDescriptor : public MPEGDescriptor
{
  public:
    PartialTransportStreamDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::partial_transport_stream) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x63
    // descriptor_length        8   1.0

    // DVB_reserved_future_use  2   2.0
    // peak_rate               22   2.2
    uint PeakRate(void) const
        { return (_data[2] & 0x3f) << 16 | _data[3] | _data[4]; }
    // DVB_reserved_future_use  2   5.0
    // min_overall_smooth_rate 22   5.2
    uint SmoothRate(void) const
        { return (_data[5] & 0x3f) << 16 | _data[6] | _data[7]; }
    // DVB_reserved_future_use  2   8.0
    // max_overall_smooth_buf  14   8.2
    uint SmoothBuf(void) const { return ((_data[8] & 0x3f) << 8) | _data[9]; }
    QString toString(void) const;
};


// a_52a.pdf p125 Table A7 (for DVB)
class AC3Descriptor : public MPEGDescriptor
{
  public:
    AC3Descriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::ac3) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6A
    // descriptor_length        8   1.0

    // component_type_flag      1   2.0
    bool HasComponentType(void) const { return _data[2] & 0x80; }
    // bsid_flag                1   2.1
    bool HasBSID(void) const { return _data[2] & 0x40; }
    // mainid_flag              1   2.2
    bool HasMainID(void) const { return _data[2] & 0x20; }
    // asvc_flag                1   2.3
    bool HasASVC(void) const { return _data[2] & 0x10; }
    // reserved_flags           4   2.4
    // if (component_type_flag == 1)
    //   { component_type       8 uimsbf }
    uint ComponentType(void) const { return _data[3]; }
    // if (bsid_flag == 1)
    //   { bsid                 8 uimsbf }
    uint BSID(void) const
        { return (HasComponentType()) ? _data[4] : _data[3]; }
    // if (mainid_flag == 1)
    //   { mainid               8 uimsbf }
    uint MainID(void) const
    {
        int offset = 3;
        offset += (HasComponentType()) ? 1 : 0;
        offset += (HasBSID()) ? 1 : 0;
        return _data[offset];
    }
    // if (asvc_flag==1)
    //   { asvc                 8 uimsbf }
    uint ASVC(void) const
    {
        int offset = 3;
        offset += (HasComponentType()) ? 1 : 0;
        offset += (HasBSID()) ? 1 : 0;
        offset += (HasMainID()) ? 1 : 0;
        return _data[offset];
    }
    // for (I=0;I<N;I++)
    //   { additional_info[i] N*8 uimsbf }
    //};
    QString toString(void) const;
};

static QString coderate_inner(uint cr)
{
    switch (cr)
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

class UKChannelListDescriptor : public MPEGDescriptor
{
  public:
    UKChannelListDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, PrivateDescriptorID::dvb_uk_channel_list) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x83
    // descriptor_length        8   1.0

    uint ChannelCount(void) const { return DescriptorLength() >> 2; }

    uint ServiceID(uint i) const
        { return (_data[2 + (i<<2)] << 8) | _data[3 + (i<<2)]; }

    uint ChannelNumber(uint i) const
        { return ((_data[4 + (i<<2)] << 8) | _data[5 + (i<<2)]) & 0x3ff; }

    QString toString(void) const;
};

// ETSI TS 102 323 (TV Anytime)
class DVBContentIdentifierDescriptor : public MPEGDescriptor
{
  public:
    DVBContentIdentifierDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::dvb_content_identifier) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x76
    // descriptor_length        8   1.0

    uint ContentType(void) const { return _data[2] >> 2; }

    uint ContentEncoding(void) const { return _data[2] & 0x03; }

    // A content identifier is a URI.  It may contain UTF-8 encoded using %XX.
    QString ContentId(void) const
    {
        int length = _data[3];
        int positionOfHash = length-1;
        while (positionOfHash >= 0) {
            if (_data[4 + positionOfHash] == '#') {
                length = positionOfHash; /* remove the hash and the following IMI */
                break;
            }
            positionOfHash--;
        }
        return QString::fromLatin1((const char *)&_data[4], length);
    }
};

// ETSI TS 102 323 (TV Anytime)
class DefaultAuthorityDescriptor : public MPEGDescriptor
{
  public:
    DefaultAuthorityDescriptor(const unsigned char *data, int len = 300) :
        MPEGDescriptor(data, len, DescriptorID::default_authority) { }
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x73
    // descriptor_length        8   1.0

    QString DefaultAuthority(void) const
    {
        return QString::fromLatin1((const char *)_data+2, _data[1]);
    }

    QString toString(void) const
    {
        return QString("DefaultAuthorityDescriptor: Authority(%1)")
            .arg(DefaultAuthority());
    }
};

#endif
