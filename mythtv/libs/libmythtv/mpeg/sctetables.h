// -*- Mode: c++ -*-
/**
 *   SCTE System information tables.
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef SCTE_TABLES_H
#define SCTE_TABLES_H

#include <cassert>
#include <cstdint>
#include <ctime>

using uint = unsigned int;

#include <QDateTime>
#include <QString>

// MythTV
#include "libmythbase/iso639.h"
#include "libmythtv/mythtvexp.h"

#include "mpegtables.h"

// NOTE: MasterGuideTable defined in atsctables.h
// NOTE: VirtualChannelTable defined in atsctables.h

// NOTE: Section 4.4.1 specifies that we should not attempt to
//       use tables carrying a protocol_version greater than
//       the one we are familiar with, as of the SCTE65-2002
//       standard 0 is the only recognized version.

class MTV_PUBLIC CarrierDefinitionSubtable
{
  public:
    CarrierDefinitionSubtable(
        const unsigned char *beg, const unsigned char *end) :
        m_beg(beg), m_end(end) { }

    //     number_of_carriers   8  0.0+m_beg
    uint NumberOfCarriers(void) const { return m_beg[0]; }
    //     spacing_unit         1  1.0+m_beg
    enum : std::uint8_t // Table 5.4 Spacing Unit & Table 5.5 frequency unit
    {
        k10000Hz  = 0x0,
        k125000Hz = 0x1,
    };
    uint SpacingUnit(void) const { return m_beg[1]>>7; }
    uint SpacingUnitHz(void) const { return SpacingUnit() ? 125000 : 1000; }
    //     zero                 1  1.1+m_beg
    //     frequency_spacing   14  1.2+m_beg
    uint FrequencySpacing(void) const
        { return ((m_beg[1] & 0x3) << 8) | m_beg[2]; }
    uint FrequencySpacingHz(void) const
        { return FrequencySpacing() * SpacingUnitHz(); }
    //     frequency_unit       1  3.0+m_beg
    uint FrequencyUnit(void) const { return m_beg[3]>>7; }
    uint FrequencyUnitHz(void) const { return FrequencyUnit() ? 125000 : 1000; }
    //     first_carrier_frequency 15 3.1+m_beg
    uint FirstCarrierFrequency(void) const
        { return ((m_beg[3] & 0x3) << 8) | m_beg[4]; }
    uint64_t FirstCarrierFrequencyHz(void) const
        { return (uint64_t)FirstCarrierFrequency() * FrequencyUnitHz(); }

    //   descriptors_count     8    5.0+m_beg
    uint DescriptorsCount(void) const { return m_beg[5]; }
    //   for (i=0; i<descriptors_count; i++) {
    //     descriptor()        ?    ?.0
    //   }
    uint DescriptorsLength(void) const { return m_end - m_beg - 6; }
    const unsigned char *Descriptors(void) const { return m_beg + 6; }

    QString toString(void) const;
    QString toStringXML(uint indent_level) const;

  private:
    const unsigned char *m_beg;
    const unsigned char *m_end;
};

class ModulationModeSubtable
{
  public:
    ModulationModeSubtable(const unsigned char *beg, const unsigned char *end) :
        m_beg(beg), m_end(end) { }
    //     transmission_system 4   0.0+m_beg
    enum : std::uint8_t // Table 5.7 TransmissionSystem
    {
        kTSUnknown       = 0,
        kTSITUAnnexA     = 1, ///< Specified in Annex A of ITU Rec. J.83
        kTSITUAnnexB     = 2, ///< Specified in Annex B of ITU Rec. J.83
        kTSITUQPSK       = 3, ///< ITU-R Rec. BO.1211:1995 (QPSK)
        kTSATSC          = 4,
        kTSDigiCipher    = 5, ///< from SCTE 57 -- DigiCipher II
        // all other values are reserved
        // ITU Rec. J.83 Annex A is the 8Mhz global standard,
        // Annex B is the 6Mhz North American standard and
        // Annex C is the 6Mhz Japanese standard. (QAM)
    };
    uint TransmissionSystem(void) const { return m_beg[0] >> 4; }
    QString TransmissionSystemString(void) const;
    //     inner_coding_mode   4   0.4+m_beg
    enum : std::uint8_t // Table 5.8
    {
        kRate5_11Coding =  0,
        kRate1_2Coding  =  1,
        // reserved     =  2,
        kRate3_5Coding  =  3,
        // reserved     =  4,
        kRate2_3Coding  =  5,
        // reserved     =  6,
        kRate3_4Coding  =  7,
        kRate4_5Coding  =  8,
        kRate5_6Coding  =  9,
        // reserved     = 10,
        kRate7_8Coding  = 11,
        // reserved     = 12,
        // reserved     = 13,
        // reserved     = 14,
        kNone           = 15,
        // all other values are reserved
    };
    uint InnerCodingMode(void) const { return m_beg[0] & 0xf; }
    QString InnerCodingModeString(void) const;
    //     split_bitstream_mode 1   1.0+m_beg
    bool SplitBitstreamMode(void) const { return ( m_beg[1] & 0x80 ) != 0; }
    //     zero                2    1.1+m_beg
    //     modulation_format   5    1.3+m_beg
    enum : std::uint8_t // Table 5.9
    {
        kUnknown   =  0,
        kQPSK      =  1,
        kBPSK      =  2,
        kOQPSK     =  3,
        kVSB8      =  4,
        kVSB16     =  5,
        kQAM16     =  6,
        kQAM32     =  7,
        kQAM64     =  8,
        kQAM80     =  9, // last modulation format that 65 & 57 agree on
        kQAM96     = 10,
        kQAM112    = 11,
        kQAM128    = 12,
        kQAM160    = 13,
        kQAM192    = 14,
        kQAM224    = 15,
        kQAM256    = 16,
        kQAM320    = 17,
        kQAM384    = 18,
        kQAM448    = 19,
        kQAM512    = 20,
        kQAM640    = 21,
        kQAM768    = 22,
        kQAM896    = 23,
        kQAM1024   = 24,
        // all other values are reserved
    };
    uint ModulationFormat(void) const { return m_beg[1] & 0x1f; }
    QString ModulationFormatString(void) const;
    //     zero                4    2.0+m_beg
    //     symbol_rate        28    2.4+m_beg
    uint SymbolRate(void) const
    {
        return (((m_beg[2]&0xf)<<24) | (m_beg[3]<<16) |
                (m_beg[4]<<8) | (m_beg[5]));
    }
    //   descriptors_count     8    6.0+m_beg
    uint DescriptorsCount(void) const { return m_beg[6]; }
    //   for (i=0; i<descriptors_count; i++) {
    //     descriptor()        ?    ?.0
    //   }
    uint DescriptorsLength(void) const { return m_end - m_beg - 7; }
    const unsigned char *Descriptors(void) const { return m_beg + 7; }

    static QString toString(void);
    QString toStringXML(uint indent_level) const;

  private:
    const unsigned char *m_beg;
    const unsigned char *m_end;
};

class MTV_PUBLIC SCTENetworkInformationTable : public PSIPTable
{
  public:
    SCTENetworkInformationTable(const SCTENetworkInformationTable &table) :
        PSIPTable(table)
    {
        assert(TableID::NITscte == TableID());
        Parse();
    }
    explicit SCTENetworkInformationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::NITscte == TableID());
        Parse();
    }
    ~SCTENetworkInformationTable() override { ; }
    // SCTE65-2002, page 15, Table 5.1
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC2
    // zero                     2   1.0       0
    // reserved                 2   1.2       3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0       0
    // protocol_version         5   3.3       0
    // first_index              8   4.0 (value of 0 is illegal)
    uint FirstIndex(void) const { return pesdata()[4]; }
    // number_of_records        8   5.0
    uint NumberOfRecords(void) const { return pesdata()[5]; }
    // transmission_medium      4   6.0       0
    uint TransmissionMedium(void) const { return pesdata()[6] >> 4; }
    // table_subtype            4   6.4
    enum : std::uint8_t // Table 5.2 NIT Subtype
    {
        kInvalid                   = 0x0,
        kCarrierDefinitionSubtable = 0x1,
        kModulationModeSubtable    = 0x2,
        // all other values are reserved
    };
    uint TableSubtype(void) const { return pesdata()[6] & 0xf; }
    // for (i = 0; i < number_of_records; i++) {
    //   if (kCarrierDefinitionSubtable == table_subtype) {
    CarrierDefinitionSubtable CarrierDefinition(uint i) const
        { return {m_ptrs[i], m_ptrs[i+1]}; }
    //   if (kModulationModeSubtable == table_subtype) {
    ModulationModeSubtable ModulationMode(uint i) const
        { return {m_ptrs[i], m_ptrs[i+1]}; }

    // }
    // for (i=0; i<N; i++)
    //   descriptor()          ?    ?.0   optional (determined by looking
    //                                              at section_length)
    uint DescriptorsLength(void) const
        { return SectionLength() - (m_ptrs.back() - pesdata()) - 4/*CRC*/; }
    const unsigned char * Descriptors(void) const { return m_ptrs.back(); }
    // CRC_32                  32

    bool Parse(void);
    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable

  private:
    std::vector<const unsigned char*> m_ptrs;
};

class MTV_PUBLIC NetworkTextTable : public PSIPTable
{
  public:
    NetworkTextTable(const NetworkTextTable &table) :
        PSIPTable(table)
    {
        assert(TableID::NTT == TableID());
        Parse();
    }
    explicit NetworkTextTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::NTT == TableID());
        Parse();
    }
    ~NetworkTextTable() override { ; }
    // SCTE65-2002, page 20, Table 5.10
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC3
    // zero                     2   1.0       0
    // reserved                 2   1.2       3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0       0
    // protocol_version         5   3.3       0
    // iso_639_language_code   24   4.0   see ISO 639.2/B
    //  * the value 0xFFFFFFFF shall match any language when text
    //    is only available in one language
    int LanguageKey(void) const
        { return iso639_str3_to_key(pesdata() + 4); }
    QString LanguageString(void) const
        { return iso639_key_to_str3(LanguageKey()); }
    int CanonicalLanguageKey(void) const
        { return iso639_key_to_canonical_key(LanguageKey()); }
    QString CanonicalLanguageString(void) const
        { return iso639_key_to_str3(CanonicalLanguageKey()); }
    // transmission_medium      4   7.0       0
    uint TransmissionMedium(void) const { return pesdata()[7] >> 4; }
    // table_subtype            4   7.4   see Table 5.11
    enum : std::uint8_t
    {
        kInvalid = 0x0,
        kSourceNameSubtable = 0x6,
        // all other values are reserved
    };
    uint TableSubtype(void) const { return pesdata()[7] & 0xf; }
    // if (kSourceNameSubtable == table_subtype) {
    //   number_of_SNS_records  8   8.0
    //   for (i=0; i<number_of_SNS_records; i++) {
    //     application_type     1   0.0+_ptrs[i]
    //     zero                 7   0.1+_ptrs[i] 0
    //     if (application_type)
    //       application_id    16   1.0+_ptrs[i]
    //     else
    //       source_id         16   1.0+_ptrs[i]
    //     name_length          8   3.0+_ptrs[i]
    //     source_name() name_length*8 4.0+_ptrs[i]
    //     sns_descriptors_count 8  4.0+ptrs[i]+name_length
    //     for (i = 0; i < sns_descriptors_count; i++)
    //       descriptor         ?   ?.0
    //   }
    // }
    // for (i=0; i<N; i++)
    //   descriptor()           ?   ?.0   optional (determined by looking
    //                                              at section_length)
    // CRC_32                  32

    void Parse(void) const;
    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
};

class MTV_PUBLIC DefinedChannelsMapSubtable
{
  public:
    explicit DefinedChannelsMapSubtable(const unsigned char *data) : m_data(data) {}
    //   zero                   4   7.0       0
    //   first_virtual_channel 12   7.4
    uint FirstVirtualChannel(void) const
        { return ((m_data[7]<<8) | m_data[8]) & 0xfff; }
    //   zero                   1   9.0
    //   DCM_data_length        7   9.1
    uint DCMDataLength(void) const { return m_data[9] & 0x7f; }
    //   for (i=0; i<DCM_data_length; i++) {
    //      range_defined       1  10.0+i
    bool RangeDefined(uint i) const { return ( m_data[10+i] & 0x80 ) != 0; }
    //      channels_count      7  10.1+i
    uint ChannelsCount(uint i) const { return m_data[10+i] & 0x7f; }
    //   }

    QString toStringXML(uint indent_level) const;
    uint Size(void) const { return DCMDataLength() + 3; }

  private:
    const unsigned char *m_data;
};

class VirtualChannelMapSubtable
{
  public:
    VirtualChannelMapSubtable(
        const unsigned char *data, const std::vector<const unsigned char*> &ptrs) :
        m_data(data), _ptrs(ptrs) {}

    //   zero                   2  7.0
    //   descriptors_included   1  7.2
    bool DescriptorsIncluded(void) const { return ( m_data[7] & 0x20 ) != 0; }
    //   zero                   5  7.3
    //   splice                 1  8.0
    bool Splice(void) const { return ( m_data[8] & 0x80 ) != 0; }
    //   zero                   7  8.1
    //   activation_time       32  9.0
    uint ActivationTimeRaw(void) const
    {
        return ((m_data[9] << 24) | (m_data[10] << 24) |
                (m_data[11] << 24) | m_data[12]);
    }
    /// \note If the GPS_UTC_offset in the SystemTimeTable is zero
    /// this includes the correction for leap seconds. Otherwise
    /// the offset must be passed as a parameter.
    QDateTime ActivationTimeUTC(uint offset = 0) const
    {
        QDateTime dt;
        dt.setSecsSinceEpoch(GPS_EPOCH + offset + ActivationTimeRaw());
        return dt;
    }
    //   number_of_VC_records   8 13.0
    uint NumberOfVCRecords(void) const { return m_data[13]; }

    //   for (i = 0; i < number_of_VC_records; i++) {
    //     zero                 4 0.0+_ptrs[i]
    //     virtual_channel_number 12 0.4+_ptrs[i]
    uint VirtualChannelNumber(uint i) const
        { return ((_ptrs[i][0]<<8) | _ptrs[i][1]) & 0xfff; }
    //     application_virtual_channel 1 2.0+_ptrs[i]
    bool ApplicationVirtualChannel(uint i) const { return ( _ptrs[i][1] & 0x80 ) != 0; }
    //     zero                 1 2.1+_ptrs[i]
    //     path_select          1 2.2+_ptrs[i]
    enum : std::uint8_t // Table 5.18 path select
    {
        kPath1 = 0x0,
        kPath2 = 0x1,
    };
    uint PathSelect(uint i) const { return (_ptrs[i][2]>>5) & 0x1; }
    QString PathSelectString(uint i) const
        { return PathSelect(i) ? "Path 2" : "Path 1"; }
    //     transport_type       1 2.3+_ptrs[i]
    enum : std::uint8_t // Table 5.19 transport type
    {
        kMPEG2Transport    = 0x0,
        kNonMPEG2Transport = 0x1,
    };
    uint TransportType(uint i) const { return (_ptrs[i][2]>>4) & 0x1; }
    QString TransportTypeString(uint i) const
        { return TransportType(i) ? "Non-MPEG-2" : "MPEG-2"; }
    //     channel_type         4 2.4+_ptrs[i]
    enum : std::uint8_t // Table 5.20 channel type
    {
        kNormalChannel     = 0x0,
        kHiddenChannel     = 0x1,
        // all other values are reserved
    };
    uint ChannelType(uint i) const { return _ptrs[i][2] & 0xf; }
    QString ChannelTypeString(uint i) const
        { return ChannelType(i) ? "Hidden" : "Normal"; }
    //     if (application_virtual_channel) {
    //       application_id    16 3.0+_ptrs[i]
    uint ApplicationID(uint i) const { return (_ptrs[i][3]<<8) | _ptrs[i][4]; }
    //     } else {
    //       source_id         16 3.0+_ptrs[i]
    uint SourceID(uint i) const { return (_ptrs[i][3]<<8) | _ptrs[i][4]; }
    //     }
    //     if (transport_type==MPEG_2) {
    //       CDS_reference      8 5.0+_ptrs[i]
    uint CDSReference(uint i) const { return _ptrs[i][5]; }
    //       program_number    16 6.0+_ptrs[i]
    uint ProgramNumber(uint i) const { return (_ptrs[i][6]<<8) | _ptrs[i][7]; }
    //       MMS_reference      8 8.0+_ptrs[i]
    uint MMSReference(uint i) const { return _ptrs[i][8]; }
    //     } else {
    //       CDS_reference      8 5.0+_ptrs[i]
    //       scrambled          1 6.0+_ptrs[i]
    bool Scrambled(uint i) const { return ( _ptrs[i][6] & 0x80 ) != 0; }
    //       zero               3 6.1+_ptrs[i]
    //       video_standard     4 6.4+_ptrs[i]
    enum : std::uint8_t // Table 5.21 video standard
    {
        kNTSC   = 0x0,
        kPAL625 = 0x1,
        kPAL525 = 0x2,
        kSECAM  = 0x3,
        kMAC    = 0x4,
        // all other values are reserved
    };
    uint VideoStandard(uint i) const { return _ptrs[i][6] & 0xf; }
    QString VideoStandardString(uint i) const;
    //       zero              16 7.0+_ptrs[i] 0
    //     }
    //     if (descriptors_included) {
    //       descriptors_count  8 9.0+_ptrs[i]
    uint DescriptorsCount(uint i) const { return _ptrs[i][9]; }
    //       for (i = 0; i < descriptors_count; i++)
    //         descriptor()
    uint DescriptorsLength(uint i) const { return _ptrs[i+1] - _ptrs[i] - 10; }
    const unsigned char *Descriptors(uint i) const { return _ptrs[i] + 10; }
    //     }
    //   }

    QString toStringXML(uint indent_level) const;
    uint Size(void) const { return _ptrs.back() - m_data; }

    const unsigned char *m_data;
    const std::vector<const unsigned char*> &_ptrs;
};

class MTV_PUBLIC InverseChannelMapSubtable
{
  public:
    explicit InverseChannelMapSubtable(const unsigned char *data) : m_data(data) {}
    //   zero                   4 7.0
    //   first_map_index       12 7.4
    uint FirstMapIndex(void) const { return ((m_data[7]<<8)|m_data[8]) & 0xfff; }
    //   zero                   1 9.0
    //   record_count           7 9.1
    uint RecordCount(void) const { return m_data[9] & 0x7f; }
    //   for (i=0; i<record_count; i++) {
    //     source_id           16 10.0+i*4
    uint SourceID(uint i) const { return (m_data[10+(i*4)]<<8) | m_data[11+(i*4)]; }
    //     zero                 4 12.0+i*4
    //     virtual_channel_number 12 12.4+i*4
    uint VirtualChannelNumber(uint i) const
        { return ((m_data[12+(i*4)]<<8) | m_data[13+(i*4)]) & 0xfff; }
    //   }

    QString toStringXML(uint indent_level) const;
    uint Size(void) const { return (RecordCount() * 4) + 3; }

  private:
    const unsigned char *m_data;
};

// AKA Short-form Virtual Channel Table
class MTV_PUBLIC ShortVirtualChannelTable : public PSIPTable
{
  public:
    ShortVirtualChannelTable(const ShortVirtualChannelTable &table) :
        PSIPTable(table)
    {
        assert(TableID::SVCTscte == TableID());
        Parse();
    }
    explicit ShortVirtualChannelTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::SVCTscte == TableID());
        Parse();
    }
    ~ShortVirtualChannelTable() override { ; }
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC4
    // zero                     2   1.0       0
    // reserved                 2   1.2       3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0       0
    // protocol_version         5   3.3       0
    // transmission_medium      4   4.0       0
    uint TransmissionMedium(void) const { return pesdata()[4] >> 4; }
    // table_subtype            4   4.4
    enum : std::uint8_t // Table 5.14 Table Subtype
    {
        kVirtualChannelMap  = 0x0,
        kDefinedChannelsMap = 0x1,
        kInverseChannelMap  = 0x2,
        // all other values are reserved
    };
    uint TableSubtype(void) const { return pesdata()[4] & 0xf; }
    QString TableSubtypeString(void) const;
    // vct_id                  16   5.0
    uint ID(void) const { return (pesdata()[5]<<8) | pesdata()[6]; }
    // if (table_subtype==kDefinedChannelsMap) {
    DefinedChannelsMapSubtable DefinedChannelsMap(void) const
        { return DefinedChannelsMapSubtable(pesdata()); }
    // }
    // if (table_subtype==kVirtualChannelMap) {
    VirtualChannelMapSubtable VirtualChannelMap(void) const
    { return { pesdata(), m_ptrs}; }
    // }
    // if (table_subtype==kInverseChannelMap) {
    InverseChannelMapSubtable InverseChannelMap(void) const
        { return InverseChannelMapSubtable(pesdata()); }
    // }
    // for (i=0; i<N; i++)
    //   descriptor()           ?   ?.0   optional (determined by looking
    //                                              at section_length)
    uint DescriptorsLength(void) const
        { return SectionLength() - (m_ptrs.back() - pesdata()) - 4/*CRC*/; }
    const unsigned char * Descriptors(void) const { return m_ptrs.back(); }
    // }
    // CRC_32                  32

    bool Parse(void);
    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable

  private:
    std::vector<const unsigned char*> m_ptrs;
};

/** \class SCTESystemTimeTable
 *  \brief This table contains the GPS time at the time of transmission.
 */
class MTV_PUBLIC SCTESystemTimeTable : public PSIPTable
{
  public:
    SCTESystemTimeTable(const SCTESystemTimeTable &table) : PSIPTable(table)
    {
        assert(TableID::STTscte == TableID());
    }
    explicit SCTESystemTimeTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::STTscte == TableID());
    }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC5
    // zero                     2   1.0          0
    // reserved                 2   1.2          3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0          0
    // protocol_version         5   3.3          0
    // zero                     8   4.0          0
    // system_time             32   5.0
    uint32_t SystemTimeRaw(void) const
    {
        return ((pesdata()[5] <<24) | (pesdata()[6]<<16) |
                (pesdata()[7]<< 8) |  pesdata()[8]);
    }
    QDateTime SystemTimeGPS(void) const
    {
        QDateTime dt;
        dt.setSecsSinceEpoch(GPSUnix());
        return dt;
    }
    QDateTime SystemTimeUTC(void) const
    {
        QDateTime dt;
        dt.setSecsSinceEpoch(UTCUnix());
        return dt;
    }
    time_t GPSUnix(void) const
        { return GPS_EPOCH + SystemTimeRaw(); }
    time_t UTCUnix(void) const
        { return GPSUnix() - GPSUTCOffset(); }
    // GPS_UTC_offset           8   9.0 (leap seconds since 1980)
    uint GPSUTCOffset(void) const { return pesdata()[9]; }
    // for (I = 0;I< N;I++) { descriptor() }
    uint DescriptorsLength(void) const
        { return SectionLength() - 10/*bytes before desc*/ - 4/*CRC*/; }
    const unsigned char * Descriptors(void) const { return pesdata() + 10; }

    // CRC_32                  32

    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
};

// PIM = 0xC0 -- Program Information Message (57 2003) PMT PID
class MTV_PUBLIC ProgramInformationMessageTable : public PSIPTable
{
  public:
    ProgramInformationMessageTable(
        const ProgramInformationMessageTable &table) : PSIPTable(table)
    {
        assert(TableID::PIM == TableID());
    }
    explicit ProgramInformationMessageTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::PIM == TableID());
    }

    QString toString(void) const override // PSIPTable
        { return "Program Information Message\n"; }
    QString toStringXML(uint /*indent_level*/) const override // PSIPTable
        { return "<ProgramInformationMessage />"; }
};

// PNM = 0xC1 -- Program Name Message (57 2003) PMT PID
class MTV_PUBLIC ProgramNameMessageTable : public PSIPTable
{
  public:
    ProgramNameMessageTable(
        const ProgramNameMessageTable &table) : PSIPTable(table)
    {
        assert(TableID::PNM == TableID());
    }
    explicit ProgramNameMessageTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::PNM == TableID());
    }

    QString toString(void) const override // PSIPTable
        { return "Program Name Message\n"; }
    QString toStringXML(uint /*indent_level*/) const override // PSIPTable
        { return "<ProgramNameMessage />"; }
};

// ADET = 0xD9 -- Aggregate Data Event Table (80 2002)
class MTV_PUBLIC AggregateDataEventTable : public PSIPTable
{
  public:
    AggregateDataEventTable(
        const AggregateDataEventTable &table) : PSIPTable(table)
    {
        assert(TableID::ADET == TableID());
    }
    explicit AggregateDataEventTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::ADET == TableID());
    }

    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
};


#endif // SCTE_TABLES_H
