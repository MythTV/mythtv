// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _DVB_DESCRIPTORS_H_
#define _DVB_DESCRIPTORS_H_

#include <cassert>
#include <vector>
#include <map>
#include <qmutex.h>
#include <qstring.h>
#include "mythcontext.h"
#include "mpegdescriptors.h"

using namespace std;

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

class NetworkNameDescriptor : public MPEGDescriptor
{
  public:
    NetworkNameDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x40
        assert(DescriptorID::network_name == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // for (i=0;i<N;i++){ char 8 uimsbf }
    QString Name() const
        { return dvb_decode_text(_data+2, DescriptorLength()); }
    QString toString() const
        { return QString("NetworkNameDescriptor: ")+Name(); }
};

class LinkageDescriptor : public MPEGDescriptor
{
  public:
    enum
    {
        lt_InformationService          = 0x01,
        lt_EPGService                  = 0x02, 
        lt_CAReplacementService        = 0x03,
        lt_TSContainingCompleteNetworkBouquetSI = 0x04,
        lt_ServiceReplacementService   = 0x05,
        lt_DataBroadcastService        = 0x06,
        lt_RCSMap                      = 0x07,
        lt_MobileHandOver              = 0x08,
        lt_SystemSoftwareUpdateService = 0x09,
        lt_TSContaining_SSU_BAT_NIT    = 0x0A,
        lt_IP_MACNotificationService   = 0x0B,
        lt_TSContaining_INT_BAT_NIT    = 0x0C,
    };

    LinkageDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4A
        assert(DescriptorID::linkage == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // transport_stream_id 16 uimsbf
    uint TSID()                const { return (_data[2]<<8) | _data[3]; }
    // original_network_id 16 uimsbf
    uint OriginalNetworkID()   const { return (_data[4]<<8) | _data[5]; }
    // service_id 16 uimsbf
    uint ServiceID()           const { return (_data[6]<<8) | _data[7]; }
    // linkage_type 8 uimsbf
    uint LinkageType()         const { return _data[8]; }
    
    // if (linkage_type != 0x08)
    //    { for (i=0;i<N;i++) { private_data_byte 8 bslbf } }
    uint PrivateDataLength_N8()   const { return DescriptorLength() - 7; }
    const unsigned char* PrivateData_N8() const { return _data+9; }
    // if (linkage_type == 0x08)
    //   {
    //      hand-over_type 4 bslbf 
    uint HandOverType()        const { return _data[10]>>4; }
    //      reserved_future_use 3 bslbf 
    //      origin_type 1 bslbf 
    bool OriginType()          const { return _data[10]&0x1; }
    //      if (hand-over_type == 0x01 || hand-over_type == 0x02 || 
    //          hand-over_type == 0x03)
    //        { network_id 16 uimsbf } 
    bool HasNetworkID()        const { return bool(HandOverType() & 0x3); }
    uint NetworkID()           const { return (_data[11]<<8) | _data[12]; }
    //      if (origin_type ==0x00)
    //        { initial_service_id 16 uimsbf } 
    bool HasInitialServiceID() const { return !OriginType(); }
    uint InitialServiceID()    const
        { return HasNetworkID() ? (_data[13]<<8) | _data[14] : NetworkID(); }

    //      for (i=0;i<N;i++)
    //        { private_data_byte 8 bslbf }
    uint PrivateDataOffset_8() const
    {
        return 11 + (HasNetworkID() ? 2 : 0) +
            (HasInitialServiceID() ? 2 : 0);
    }
    uint PrivateDataLength_8() const
        { return DescriptorLength() - (PrivateDataOffset_8()-2); }
    const unsigned char* PrivateData_8() const
        { return _data + PrivateDataOffset_8(); }

    QString toString() const { return QString("LinkageDescriptor(stub)"); }
};

class AdaptationFieldDataDescriptor : public MPEGDescriptor
{
  public:
    AdaptationFieldDataDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x70
        assert(DescriptorID::adaptation_field_data == DescriptorTag());
    // descriptor_length        8   1.0
    }

    /// adapt_field_data_id     8   2.0
    uint AdaptationFieldDataID() const { return _data[2]; }

    QString toString() const { return QString("AdaptationFieldDataDescriptor(stub)"); }
};

class AncillaryDataDescriptor : public MPEGDescriptor
{
  public:
    AncillaryDataDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6b
        assert(DescriptorID::ancillary_data == DescriptorTag());
    // descriptor_length        8   1.0
    }

    /// ancillary_data_id       8   2.0
    uint AncillaryDataID() const { return _data[2]; }

    QString toString() const { return QString("AncillaryDataDescriptor(stub)"); }
};

class AnnouncementSupportDescriptor : public MPEGDescriptor
{
  public:
    AnnouncementSupportDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6e
        assert(DescriptorID::announcement_support == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("AnnouncementSupportDescriptor(stub)"); }  
};

class BouquetNameDescriptor : public MPEGDescriptor
{
  public:
    BouquetNameDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x47
        assert(DescriptorID::bouquet_name == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for(i=0;i<N;i++) { char 8 } 
    QString toString() const { return QString("BouquetNameDescriptor(stub)"); }
};

class CAIdentifierDescriptor : public MPEGDescriptor
{
  public:
    CAIdentifierDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x53
        assert(DescriptorID::CA_identifier == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // 
    // for (i=0; i<N; i++)
    //   { CA_system_id 16 }
    QString toString() const { return QString("CAIdentifierDescriptor(stub)"); }
};

class CellFrequencyLinkDescriptor : public MPEGDescriptor
{
  public:
    CellFrequencyLinkDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6d
        assert(DescriptorID::cell_frequency_link == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("CellFrequencyLinkDescriptor(stub)"); }
};

class CellListDescriptor : public MPEGDescriptor
{
  public:
    CellListDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6c
        assert(DescriptorID::cell_list == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // 
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
    QString toString() const { return QString("CellListDescriptor(stub)"); }
};

class ComponentDescriptor : public MPEGDescriptor
{
  public:
    ComponentDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x50
        assert(DescriptorID::component == DescriptorTag());
    // descriptor_length        8   1.0
    }

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

    bool IsVideo(void)    const { return 0x1 == StreamContent(); }
    bool IsAudio(void)    const { return 0x2 == StreamContent(); }
    bool IsSubtitle(void) const { return 0x3 == StreamContent(); }

    bool IsHDTV(void) const
    {
        return IsVideo() && 0x9 <= ComponentType() && ComponentType() <= 0x10;
    }

    bool IsStereo(void) const
    {
        return IsAudio() &&
            ((0x3 == ComponentType()) || (0x5 == ComponentType()));
    }

    bool IsReallySubtitled(void) const
    {
        if (!IsSubtitle())
            return false;
        switch (ComponentType())
        {
            case 0x1:
            case 0x3:
            case 0x10 ... 0x13:
            case 0x20 ... 0x23:
                return true;
            default:
                return false;
        }
    }

    QString toString() const { return QString("ComponentDescriptor(stub)"); }
};

typedef enum
{
    kCategoryNone = 0,
    kCategoryMovie,
    kCategorySeries,
    kCategorySports,
    kCategoryTVShow,
    kCategoryLast,
} MythCategoryType;

QString myth_category_type_to_string(uint category_type);
MythCategoryType string_to_myth_category_type(const QString &type);

class ContentDescriptor : public MPEGDescriptor
{
  public:
    ContentDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x54
        assert(DescriptorID::content == DescriptorTag());
    // descriptor_length        8   1.0
    }

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

    MythCategoryType GetMythCategory(uint i) const;
    QString GetDescription(uint i) const;
    QString toString() const;

  private:
    static void Init(void);

  private:
    static QMutex            categoryLock;
    static map<uint,QString> categoryDesc;
    static bool              categoryDescExists;
};

class CountryAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    CountryAvailabilityDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x
        assert(DescriptorID::country_availability == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // country_avail_flag       1   2.0
    // reserved_future_use      7   2.1
    // 
    // for (i=0; i<N; i++)
    //   { country_code        24 } 
    QString toString() const { return QString("CountryAvailabilityDescriptor(stub)"); }
};

class DataBroadcastDescriptor : public MPEGDescriptor
{
  public:
    DataBroadcastDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x64
        assert(DescriptorID::data_broadcast == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // data_broadcast_id       16   2.0
    // component_tag            8   4.0
    // selector_length          8   5.0
    // for (i=0; i<selector_length; i++)
    // {
    //   selector_byte          8
    // }
    // ISO_639_language_code   24
    // text_length              8
    // for (i=0; i<text_length; i++) { text_char 8 } 
    QString toString() const { return QString("DataBroadcastDescriptor(stub)"); }
};

class DataBroadcastIdDescriptor : public MPEGDescriptor
{
  public:
    DataBroadcastIdDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x66
        assert(DescriptorID::data_broadcast_id == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // data_broadcast_id       16   2.0
    // for(i=0; i < N;i++ )
    // { id_selector_byte       8 } 
    QString toString() const { return QString("DataBroadcastIdDescriptor(stub)"); }
};

class CableDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    CableDeliverySystemDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x44
        assert(DescriptorID::cable_delivery_system == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // frequency               32   2.0
    uint FrequencyRaw() const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    unsigned long long FrequencyHz() const
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
    uint FECOuter() const { return _data[7] & 0xf; }
    QString FECOuterString() const
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
    uint Modulation() const { return _data[8]; }
    QString ModulationString() const
    {
        static QString ms[] =
            { "auto", "qam_16", "qam_32", "qam_64", "qam_128", "qam_256" };
        return (Modulation() <= kModulationQAM256) ?
            ms[Modulation()] : QString("auto");
    }
    // symbol_rate             28   9.0
    uint SymbolRateRaw() const
    {
        return ((_data[9]<<20) | (_data[10]<<12) |
                (_data[11]<<4) | (_data[12]>>4));
    }
    uint SymbolRateHz() const
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
    uint FECInner() const { return _data[12] & 0xf; }
    QString FECInnerString() const { return coderate_inner(FECInner()); }
    QString toString() const { return QString("CableDeliverySystemDescriptor(stub)"); }
};

class SatelliteDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    SatelliteDeliverySystemDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x43
        assert(DescriptorID::satellite_delivery_system == DescriptorTag());
    // descriptor_length        8   1.0
    }

    /// frequency              32   2.0
    uint FrequencyRaw() const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    unsigned long long FrequencyHz() const
    {
        return byte4BCD2int(_data[2], _data[3], _data[4], _data[5]) * 10;
    }
    /// orbital_position       16   6.0
    uint OrbitalPosition() const
        { return byte2BCD2int(_data[6], _data[7]); }
    QString OrbitalPositionString() const
    {
        uint num = OrbitalPosition();
        return QString("%1.%2 %3").arg(num / 10).arg(num % 10)
            .arg((IsEast()) ? "East" : "West");
    }
    double OrbitalPositionFloat()  const
        { return ((double) OrbitalPosition()) / 10.0; }
    /// west_east_flag          1   8.0
    bool IsEast()                 const { return (_data[8]&0x80); }
    bool IsWest()                 const { return !IsEast(); }
    // polarization             2   8.1
    uint Polarization()           const { return (_data[8]>>5)&0x3; }
    QString PolarizationString()  const
    {
        static QString ps[] = { "h", "v", "l", "r" };
        return ps[Polarization()];
    }
    bool IsCircularPolarization() const { return (_data[8]>>6)&0x1; }
    bool IsLinearPolarization()   const { return !((_data[8]>>6)&0x1); }
    bool IsHorizontalLeftPolarization() const { return (_data[8]>>5)&0x1; }
    bool IsVerticalRightPolarization() const { return !((_data[8]>>5)&0x1); }
    // modulation               5   8.3
    enum
    {
        kModulationQPSK_NS = 0x0, // Non standard QPSK for Bell ExpressVu
        kModulationQPSK   = 0x1,
        kModulation8PSK   = 0x2,
        kModulationQAM16  = 0x3,
    };
    uint Modulation() const { return _data[8]&0x1f; }
    QString ModulationString() const
    {
        static QString ms[] = { "qpsk", "qpsk", "qpsk_8", "qam_16" };
        return (Modulation() <= kModulationQAM16) ? ms[Modulation()] : "auto";
    }
    // symbol_rate             28   9.0
    uint SymbolRate() const
    {
        return ((_data[9]<<20) | (_data[10]<<12) |
                (_data[11]<<4) | (_data[12]>>4));
    }
    uint SymbolRateHz() const
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
    uint FECInner() const { return _data[12] & 0xf; }
    QString FECInnerString() const { return coderate_inner(FECInner()); }

    QString toString() const { return QString("SatelliteDeliverySystemDescriptor(stub)"); }

};

class TerrestrialDeliverySystemDescriptor : public MPEGDescriptor
{
  public:
    TerrestrialDeliverySystemDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5a
        assert(DescriptorID::terrestrial_delivery_system == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // centre_frequency        32   2.0
    uint Frequency() const
    {
        return ((_data[2]<<24) | (_data[3]<<16) |
                (_data[4]<<8)  | (_data[5]));
    }
    uint FrequencyHz() const { return Frequency() * 10; }

    // bandwidth                3   6.0
    enum
    {
        kBandwidth8Mhz = 0x0,
        kBandwidth7Mhz = 0x1,
        kBandwidth6Mhz = 0x2,
        kBandwidth5Mhz = 0x3,
    };
    uint Bandwidth()            const { return _data[6]>>5; }
    uint BandwidthHz()          const { return (8 - Bandwidth()) * 1000000; }
    QString BandwidthString()   const
    {
        static QString bs[] = { "8", "7", "6", "5" };
        return (Bandwidth() <= kBandwidth5Mhz) ? bs[Bandwidth()] : "auto";
    }
    // priority                 1   6.3
    bool HighPriority()         const { return _data[6] & 0x10; }
    // time_slicing_indicator   1   6.4
    bool IsTimeSlicingIndicatorUsed() const { return !(_data[6] & 0x08); }
    // MPE-FEC_indicator        1   6.5
    bool IsMPE_FECUsed()        const { return !(_data[6] & 0x04); }
    // reserved_future_use      2   6.6
    // constellation            2   7.0
    enum
    {
        kConstellationQPSK  = 0x0,
        kConstellationQAM16 = 0x1,
        kConstellationQAM64 = 0x2,
    };
    uint Constellation()        const { return _data[7]>>6; }
    QString ConstellationString() const
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
    uint Hierarchy()            const { return (_data[7]>>3) & 0x7; }

    /// \bug returns "a" for values >= 4 for compatibility with siparser.cpp
    QString HierarchyString() const
    {
        static QString hs[] = { "n", "1", "2", "4", "a", "a", "a", "a" };
        return hs[Hierarchy()];
    }
    bool NativeInterleaver()    const { return _data[7] & 0x20; }
    uint Alpha()                const
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
    uint CodeRateHP()           const { return _data[7] & 0x7; }
    QString CodeRateHPString()  const
    {
        static QString cr[] = {
            "1/2", "2/3", "3/4", "5/6", "7/8", "auto", "auto", "auto"
        };
        return cr[CodeRateHP()];
    }
    // code_rate-LP_stream      3   8.0
    uint CodeRateLP()           const { return (_data[8]>>5) & 0x7; }
    QString CodeRateLPString()  const
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
    uint GuardInterval()        const { return (_data[8]>>3) & 0x3; }
    QString GuardIntervalString() const
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
    uint TransmissionMode()     const { return (_data[8]>>1) & 0x3; }
    QString TransmissionModeString() const
    {
        static QString tm[] = { "2", "8", "4", "auto" };
        return tm[TransmissionMode()];
    }
    // other_frequency_flag     1   8.7
    bool OtherFrequencyInUse()  const { return _data[8] & 0x1; }
    // reserved_future_use     32   9.0

    QString toString() const { return QString("TerrestrialDeliverySystemDescriptor(stub)"); }
};

class DSNGDescriptor : public MPEGDescriptor
{
  public:
    DSNGDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x68
        assert(DescriptorID::DSNG == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++) { byte 8 } 
    QString toString() const { return QString("DSNGDescriptor(stub)"); }
};

class ExtendedEventDescriptor : public MPEGDescriptor
{
  public:
    ExtendedEventDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4e
        assert(DescriptorID::extended_event == DescriptorTag());
    // descriptor_length        8   1.0
    }

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

    QString toString() const { return QString("ExtendedEventDescriptor(stub)"); }
};

class FrequencyListDescriptor : public MPEGDescriptor
{
  public:
    FrequencyListDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x62
        assert(DescriptorID::frequency_list == DescriptorTag());
    // descriptor_length        8   1.0
    }

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

    QString toString() const;
};

class LocalTimeOffsetDescriptor : public MPEGDescriptor
{
  public:
    LocalTimeOffsetDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x58
        assert(DescriptorID::local_time_offset == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for(i=0;i<N;i++)
    // {
    //   country_code          24   0.0+p
    //   country_region_id      6   3.0+p
    //   reserved               1   3.6+p
    //   local_time_off_polarit 1   3.7+p
    //   local_time_offset     16   4.0+p
    //   time_of_change        40   6.0+p
    //   next_time_offset      16  11.0+p
    // }                           13.0
    QString toString() const { return QString("LocalTimeOffsetDescriptor(stub)"); }
};

class MosaicDescriptor : public MPEGDescriptor
{
  public:
    MosaicDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x51
        assert(DescriptorID::mosaic == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("MosaicDescriptor(stub)"); }
};

class MultilingualBouquetNameDescriptor : public MPEGDescriptor
{
  public:
    MultilingualBouquetNameDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5c
        assert(DescriptorID::multilingual_bouquet_name == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   bouquet_name_length    8 
    //   for (j=0;j<N;j++) { char 8 }
    // } 
    QString toString() const { return QString("MultilingualBouguetNameDescriptor(stub)"); }
};

class MultilingualNetworkNameDescriptor : public MPEGDescriptor
{
  public:
    MultilingualNetworkNameDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5b
        assert(DescriptorID::multilingual_network_name == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   network_name_length    8
    //   for (j=0;j<N;j++) { char 8 }
    // }
    QString toString() const { return QString("MultilingualNetworkNameDescriptor(stub)"); }
};

class MultilingualServiceNameDescriptor : public MPEGDescriptor
{
  public:
     MultilingualServiceNameDescriptor(const unsigned char* data)
         : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5d
        assert(DescriptorID::multilingual_service_name == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24
    //   service_provider_name_length 8 
    //   for (j=0;j<N;j++) { char 8 }
    //   service_name_length    8
    //   for (j=0;j<N;j++) { char 8 }
    // } 
    QString toString() const { return QString("MultiLingualServiceNameDescriptor(stub)"); }
};

class NVODReferenceDescriptor : public MPEGDescriptor
{
  public:
    NVODReferenceDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4b
        assert(DescriptorID::NVOD_reference == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++)
    // {
    //   transport_stream_id   16
    //   original_network_id   16
    //   service_id            16
    // }
    QString toString() const { return QString("NVODReferenceDescriptor(stub)"); }
};

class ParentalRatingDescriptor : public MPEGDescriptor
{
  public:
    ParentalRatingDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x55
        assert(DescriptorID::parental_rating == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0; i<N; i++)
    // {
    //   country_code          24
    //   rating                 8
    // } 
    QString toString() const { return QString("ParentalRatingDescriptor(stub)"); }
};

class PDCDescriptor : public MPEGDescriptor
{
  public:
    PDCDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x69
        assert(DescriptorID::PDC == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // reserved_future_use      4   2.0
    // program_id_label        20   2.4
    QString toString() const { return QString("PDCDescriptor(stub)"); }
};

class PrivateDataSpecifierDescriptor : public MPEGDescriptor
{
  public:
    PrivateDataSpecifierDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x5f
        assert(DescriptorID::private_data_specifier == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // private_data_specifier  32   2.0
    QString toString() const { return QString("PrivateDataSpecifierDescriptor(stub)"); }
};

class ScramblingDescriptor : public MPEGDescriptor
{
  public:
    ScramblingDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x65
        assert(DescriptorID::scrambling == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // scrambling_mode          8   2.0

    QString toString() const { return QString("ScramblingDescriptor"); }
};

class ServiceDescriptor : public MPEGDescriptor
{
  public:
    ServiceDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x48
        assert(DescriptorID::service == DescriptorTag());
    // descriptor_length        8   1.0
    }
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
        kServiceTypeFMRadio                  = 0x0A,
        kServiceTypeNTSCCodedSignal          = 0x0B,
        kServiceTypeDataBroadcast            = 0x0C,
        kServiceTypeCommonInterface          = 0x0D,
        kServiceTypeRCS_Map                  = 0x0E,
        kServiceTypeRCS_FLS                  = 0x0F,
        kServiceTypeDVB_MHP                  = 0x10,
    };
    // service_type             8   2.0
    uint ServiceType(void) const { return _data[2]; }
    // svc_provider_name_len    8   3.0
    uint ServiceProviderNameLength(void) const { return _data[3]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceProviderName(void) const
        { return dvb_decode_text(_data + 4, ServiceProviderNameLength()); }
    // service_name_length      8
    uint ServiceNameLength(void) const
        { return _data[4 + ServiceProviderNameLength()]; }
    // for (i=0;i<N;I++) { char 8 }
    QString ServiceName(void) const
    {
        return dvb_decode_text(_data + 5 + ServiceProviderNameLength(),
                               ServiceNameLength());
    }
    bool IsDTV(void) const
        { return ServiceType() ==  kServiceTypeDigitalTelevision; }
    bool IsDigitalAudio(void) const
        { return ServiceType() ==  kServiceTypeDigitalRadioSound; }

    QString toString() const;
};

class ServiceAvailabilityDescriptor : public MPEGDescriptor
{
  public:
    ServiceAvailabilityDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x72
        assert(DescriptorID::service_availability == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // availability_flag        1   2.0
    // reserved                 7   2.1
    // for (i=0;i<N;i++) { cell_id 16 } 
    QString toString() const { return QString("ServiceAvailabilityDescriptor(stub)"); }
};

class ServiceListDescriptor : public MPEGDescriptor
{
  public:
    ServiceListDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x41
        assert(DescriptorID::service_list == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;I++)
    // {
    //   service_id            16  0.0+p
    //   service_type           8  2.0+p
    // } 
    QString toString() const { return QString("ServiceListDescriptor(stub)"); }
};

class ServiceMoveDescriptor : public MPEGDescriptor
{
  public:
    ServiceMoveDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x60
        assert(DescriptorID::service_move == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // new_original_network_id 16   2.0
    // new_transport_stream_id 16   4.0
    // new_service_id          16   6.0
    QString toString() const { return QString("ServiceMoveDescriptor(stub)"); }
};

class ShortEventDescriptor : public MPEGDescriptor
{
  public:
    ShortEventDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4d
        assert(DescriptorID::short_event == DescriptorTag());
    // descriptor_length        8   1.0
    }

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

    QString toString() const
        { return LanguageString() + " : " + EventName() + " : " + Text(); }
};

class ShortSmoothingBufferDescriptor : public MPEGDescriptor
{
  public:
    ShortSmoothingBufferDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x61
        assert(DescriptorID::short_smoothing_buffer == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // sb_size                  2   2.0
    // sb_leak_rate             6   2.2
    // for (i=0; i<N; i++)
    // { DVB_reserved           8 } 
    QString toString() const { return QString("ShortSmoothingBufferDescriptor(stub)"); }
};

class StreamIdentifierDescriptor : public MPEGDescriptor
{
  public:
    StreamIdentifierDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x52
        assert(DescriptorID::stream_identifier == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // component_tag            8   2.0
    QString toString() const { return QString("StreamIdentifierDescriptor(stub)"); }
};

class SubtitlingDescriptor : public MPEGDescriptor
{
  public:
    SubtitlingDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x59
        assert(DescriptorID::subtitling == DescriptorTag());
    // descriptor_length        8   1.0
    }

    uint StreamCount(void) const { return DescriptorLength() >> 3; }
    // for (i= 0;i<N;I++)
    // {
    //   ISO_639_language_code 24   0.0+p
    int LanguageKey(uint i) const
        { return iso639_str3_to_key(&_data[2 + (i<<3)]); }
    QString LanguageString(uint i) const
        { return iso639_key_to_str3(LanguageKey(i)); }
    int CanonicalLanguageKey(uint i) const
        { return iso639_key_to_canonical_key(LanguageKey(i)); }
    QString CanonicalLanguageString(uint i) const
        { return iso639_key_to_str3(CanonicalLanguageKey(i)); }

    //   subtitling_type        8   3.0+p
    uint SubtitleType(uint i) const
        { return _data[5 + (i<<3)]; }
    //   composition_page_id   16   4.0+p
    uint CompositionPageID(uint i) const
        { return (_data[6 + (i<<3)] << 8) | _data[7 + (i<<3)]; }
    //   ancillary_page_id     16   6.0+p
    uint AncillaryPageID(uint i) const
        { return (_data[8 + (i<<3)] << 8) | _data[9 + (i<<3)]; }
    // }                            8.0
    QString toString() const { return QString("SubtitlingDescriptor(stub)"); }
};

class TelephoneDescriptor : public MPEGDescriptor
{
  public:
    TelephoneDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x57
        assert(DescriptorID::telephone == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("TelephoneDescriptor(stub)"); }
};

class TeletextDescriptor : public MPEGDescriptor
{
  public:
    TeletextDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x56
        assert(DescriptorID::teletext == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("TeletextDescriptor(stub)"); }
};

class TimeShiftedEventDescriptor : public MPEGDescriptor
{
  public:
    TimeShiftedEventDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x
        assert(DescriptorID::time_shifted_event == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // reference_service_id    16   2.0
    // reference_event_id      16   4.0
    QString toString() const { return QString("TimeShiftedEventDescriptor(stub)"); }
};

class TimeShiftedServiceDescriptor : public MPEGDescriptor
{
  public:
    TimeShiftedServiceDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x4c
        assert(DescriptorID::dvb_time_shifted_service == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // reference_service_id    16   2.0
    QString toString() const { return QString("TimeShiftedServiceDescriptor(stub)"); }
};

class TransportStreamDescriptor : public MPEGDescriptor
{
  public:
    TransportStreamDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x67
        assert(DescriptorID::transport_stream == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0; i<N; i++) { byte 8 } 
    QString toString() const { return QString("TransportStreamDescriptor(stub)"); }
};

class VBIDataDescriptor : public MPEGDescriptor
{
  public:
    VBIDataDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x45
        assert(DescriptorID::VBI_data == DescriptorTag());
    // descriptor_length        8   1.0
    }

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
    QString toString() const { return QString("VBIDataDescriptor(stub)"); }
};

class VBITeletextDescriptor : public MPEGDescriptor
{
  public:
    VBITeletextDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x
        assert(DescriptorID::VBI_teletext == DescriptorTag());
    // descriptor_length        8   1.0
    }

    // for (i=0;i<N;i++)
    // {
    //   ISO_639_language_code 24   0.0+p
    //   teletext_type          5   3.0+p
    //   teletext_magazine_num  3   3.5+p
    //   teletext_page_num      8   4.0+p
    // }                            5.0
    QString toString() const { return QString("VBITeletextDescriptor(stub)"); }
};

class PartialTransportStreamDescriptor : public MPEGDescriptor
{
  public:
    PartialTransportStreamDescriptor(const unsigned char* data)
        : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x63
        assert(DescriptorID::partial_transport_stream == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // DVB_reserved_future_use  2   2.0
    // peak_rate               22   2.2
    // DVB_reserved_future_use  2   5.0
    // min_overall_smooth_rate 22   5.2
    // DVB_reserved_future_use  2   8.0
    // max_overall_smooth_buf  14   8.2
    QString toString() const { return QString("PartialTransportStreamDescriptor(stub)"); }
};


// a_52a.pdf p125 Table A7 (for DVB)
class AC3Descriptor : public MPEGDescriptor
{
  public:
    AC3Descriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x6A
        assert(DescriptorID::AC3 == DescriptorTag());
    // descriptor_length        8   1.0
    }
    // component_type_flag      1   2.0
    // bsid_flag                1   2.1
    // mainid_flag              1   2.2
    // asvc_flag                1   2.3
    // reserved_flags           4   2.4
    // if (component_type_flag == 1)
    //   { component_type       8 uimsbf }
    // if (bsid_flag == 1)
    //   { bsid                 8 uimsbf }
    // if (mainid_flag == 1)
    //   { mainid               8 uimsbf }
    // if (asvc_flag==1)
    //   { asvc                 8 uimsbf }
    // for (I=0;I<N;I++)
    //   { additional_info[i] N*8 uimsbf }
    //};
    QString toString() const { return QString("AC3DescriptorDescriptor(stub)"); }
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
    UKChannelListDescriptor(const unsigned char* data) : MPEGDescriptor(data)
    {
    //       Name             bits  loc  expected value
    // descriptor_tag           8   0.0       0x83
        assert(DescriptorID::dvb_uk_channel_list == DescriptorTag());
    // descriptor_length        8   1.0
    }  

    uint ChannelCount() const { return DescriptorLength() >> 2; }

    uint ServiceID(uint i) const
        { return (_data[2 + (i<<2)] << 8) | _data[3 + (i<<2)]; }

    uint ChannelNumber(uint i) const
        { return ((_data[4 + (i<<2)] << 8) | _data[5 + (i<<2)]) & 0x3ff; }

    QString toString() const { return QString("UKChannelListDescriptor(stub)"); }
};

#endif
