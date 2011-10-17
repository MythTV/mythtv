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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _SCTE_TABLES_H_
#define _SCTE_TABLES_H_

#include <cassert>
#include "mpegtables.h"
#include "mythtvexp.h"

// NOTE: MasterGuideTable defined in atsctables.h
// NOTE: VirtualChannelTable defined in atsctables.h

// NOTE: Section 4.4.1 specifies that we should not attempt to
//       use tables carrying a protocol_version greater than
//       the one we are familiar with, as of the SCTE65-2002
//       standard 0 is the only recognized version.

class MTV_PUBLIC SCTENetworkInformationTable : public PSIPTable
{
  public:
    SCTENetworkInformationTable(const SCTENetworkInformationTable &table) :
        PSIPTable(table)
    {
        assert(TableID::NITscte == TableID());
        Parse();
    }
    SCTENetworkInformationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::NITscte == TableID());
        Parse();
    }
    ~SCTENetworkInformationTable() { ; }
    // SCTE65-2002, page 15, Table 5.1
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC2
    // zero                     2   1.0       0
    // reserved                 2   1.2       3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0       0
    // protocol_version         5   3.3       0
    // first_index              8   4.0
    // number_of_records        8   5.0
    // transmission_medium      4   6.0       0
    // table_subtype            4   6.4
    enum // Table 5.2 NIT Subtype
    {
        kInvalid                   = 0x0,
        kCarrierDefinitionSubtable = 0x1,
        kModulationModeSubtable    = 0x2,
        // all other values are reserved
    };
    // for (i = 0; i < number_of_records; i++) {
    //   if (kCarrierDefinitionSubtable == table_subtype) {
    //     number_of_carriers   8  0.0+_ptrs[i]
    //     spacing_unit         1  1.0+_ptrs[i]
    enum // Table 5.4 Spacing Unit & Table 5.5 frequency unit
    {
        k10000Hz  = 0x0,
        k125000Hz = 0x1,
    };
    //     zero                 1  1.1+_ptrs[i]
    //     frequency_spacing   14  1.2+_ptrs[i]
    //     frequency_unit       1  3.0+_ptrs[i]
    //     first_carrier_frequency 15 3.1+_ptrs[i]
    //   }
    //   if (kModulationModeSubtable == table_subtype) {
    //     transmission_system 4   0.0+_ptrs[i]
    enum // Table 5.7 TransmissionSystem
    {
        kTSUnknown       = 0,
        kTSReservedETSI  = 1,
        kTSITUAnnexB     = 2, // Specified in Annex B of ITU Rec. J.83
        kTSReservedOther = 3,
        kTSATSC          = 4,
        // all other values are reserved
    };
    //     inner_coding_mode   4   0.4+_ptrs[i]
    enum // Table 5.8
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
    //     split_bitstream_mode 1   1.0+_ptrs[i]
    //     zero                2    1.1+_ptrs[i]
    //     modulation_format   5    1.3+_ptrs[i]
    enum // Table 5.9
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
        kQAM80     =  9,
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
    //     zero                4    2.0+_ptrs[i]
    //     symbol_rate        28    2.4+_ptrs[i]
    //   }
    //   descriptors_count     8    (CDS==subtype?5:6)+_ptrs[i]
    //   for (i=0; i<descriptors_count; i++) {
    //     descriptor()        ?    ?.0
    //   }
    // }
    // for (i=0; i<N; i++)
    //   descriptor()          ?    ?.0   optional (determined by looking
    //                                              at section_length)
    // CRC_32                  32

    void Parse(void) const;
    QString toString(void) const;
    QString toStringXML(void) const;
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
    NetworkTextTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::NTT == TableID());
        Parse();
    }
    ~NetworkTextTable() { ; }
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
    // transmission_medium      4   7.0       0
    // table_subtype            4   7.4   see Table 5.11
    enum
    {
        kInvalid = 0x0,
        kSourceNameSubtable = 0x6,
        // all other values are reserved
    };
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
    QString toString(void) const;
    QString toStringXML(void) const;
};

// AKA Short-form Virtual Channel Table
class MTV_PUBLIC ShortVirtualChannelMap : public PSIPTable
{
    ShortVirtualChannelMap(const ShortVirtualChannelMap &table) :
        PSIPTable(table)
    {
        assert(TableID::SVCTscte == TableID());
        Parse();
    }
    ShortVirtualChannelMap(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::SVCTscte == TableID());
        Parse();
    }
    ~ShortVirtualChannelMap() { ; }
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC4
    // zero                     2   1.0       0
    // reserved                 2   1.2       3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // zero                     3   3.0       0
    // protocol_version         5   3.3       0
    // transmission_medium      4   4.0       0
    // table_subtype            4   4.4
    enum // Table 5.14 Table Subtype
    {
        kVirtualChannelMap  = 0x0,
        kDefinedChannelsMap = 0x1,
        kInverseChannelMap  = 0x2,
        // all other values are reserved
    };
    // vct_id                  16   5.0
    // if (table_subtype==kDefinedChannelsMap) {
    //   zero                   4   7.0       0
    //   first_virtual_channel 12   7.4
    //   zero                   1   9.0
    //   DCM_data_length        7   9.1
    //   for (i=0; i<DCM_data_length; i++) {
    //      range_defined       1  10.0+i
    //      channels_count      7  10.1+i
    //   }
    // }
    // if (table_subtype==kVirtualChannelMap)
    //   zero                   2  7.0
    //   descriptors_included   1  7.2
    //   zero                   5  7.3
    //   splice                 1  8.0
    //   zero                   7  8.1
    //   activation_time       32  9.0
    //   number_of_VC_records   8 13.0
    //   for (i = 0; i < number_of_VC_records; i++) {
    //     zero                 4 0.0+_ptrs[i]
    //     virtual_channel_number 12 0.4+_ptrs[i]
    //     application_virtual_channel 1 2.0+_ptrs[i]
    //     zero                 1 2.1+_ptrs[i]
    //     path_select          1 2.2+_ptrs[i]
    enum // Table 5.18 path select
    {
        kPath1 = 0x0,
        kPath2 = 0x1,
    };
    //     transport_type       1 2.3+_ptrs[i]
    enum // Table 5.19 transport type
    {
        kMPEG2Transport    = 0x0,
        kNonMPEG2Transport = 0x1,
    };
    //     channel_type         4 2.4+_ptrs[i]
    enum // Table 5.20 channel type
    {
        kNormalChannel     = 0x0,
        kHiddenChannel     = 0x1,
        // all other values are reserved
    };
    //     if (application_virtual_channel) {
    //       application_id    16 3.0+_ptrs[i]
    //     } else {
    //       source_id         16 3.0+_ptrs[i]
    //     }
    //     if (transport_type==MPEG_2) {
    //       CDS_reference      8 5.0+_ptrs[i]
    //       program_number    16 6.0+_ptrs[i]
    //       MMS_reference      8 8.0+_ptrs[i]
    //     } else {
    //       CDS_reference      8 5.0+_ptrs[i]
    //       scrambled          1 6.0+_ptrs[i]
    //       zero               3 6.1+_ptrs[i]
    //       video_standard     4 6.4+_ptrs[i]
    enum // Table 5.21 video standard
    {
        kNTSC   = 0x0,
        kPAL625 = 0x1,
        kPAL525 = 0x2,
        kSECAM  = 0x3,
        kMAC    = 0x4,
        // all other values are reserved
    };
    //       zero              16 7.0+_ptrs[i] 0
    //     }
    //     if (descriptors_included) {
    //       descriptors_count  8 9.0+_ptrs[i]
    //       for (i = 0; i < descriptors_count; i++)
    //         descriptor()
    //     }
    //   }
    // }
    // if (table_subtype==kInverseChannelMap)
    //   zero                   4 7.0
    //   first_map_index       12 7.4
    //   zero                   1 9.0
    //   record_count           7 9.1
    //   for (i=0; i<record_count; i++) {
    //     source_id           16 10.0+i*4
    //     zero                 4 12.0+i*4
    //     virtual_channel_number 12 12.4+i*4
    //   }
    // }
    // for (i=0; i<N; i++)
    //   descriptor()           ?   ?.0   optional (determined by looking
    //                                              at section_length)
    // }
    // CRC_32                  32

    void Parse(void) const;
    QString toString(void) const;
    QString toStringXML(void) const;
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
    SCTESystemTimeTable(const PSIPTable &table) : PSIPTable(table)
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
    uint32_t GPSRaw(void) const
    {
        return ((pesdata()[5] <<24) | (pesdata()[6]<<16) |
                (pesdata()[7]<< 8) |  pesdata()[8]);
    }
    QDateTime SystemTimeGPS(void) const
    {
        QDateTime dt;
        dt.setTime_t(GPS_EPOCH + GPSRaw());
        return dt;
    }
    time_t GPSUnix(void) const
        { return GPS_EPOCH + GPSRaw(); }
    time_t UTCUnix(void) const
        { return GPSUnix() - GPSOffset(); }
    // GPS_UTC_offset           8   9.0
    uint GPSOffset(void) const { return pesdata()[9]; }
    // for (I = 0;I< N;I++) { descriptor() }
    // CRC_32                  32

    void Parse(void) const;
    QString toString(void) const;
    QString toStringXML(void) const;
};

#endif // _SCTE_TABLES_H_
