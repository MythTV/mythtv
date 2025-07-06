// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef ATSC_TABLES_H
#define ATSC_TABLES_H

#include "libmythbase/mythconfig.h"

#include <cstdint>  // uint32_t
#include <ctime>
#include <limits> // workaround QTBUG-90395

using uint = unsigned int;

#include <QString>
#include <QtEndian>

#include "libmythbase/mythdate.h"
#include "libmythbase/mythmiscutil.h" // for xml_indent
#include "libmythtv/mpeg/atscdescriptors.h"
#include "libmythtv/mpeg/mpegtables.h"
#include "libmythtv/mythtvexp.h"

// Some sample code is in pcHDTV's dtvscan.c,
// accum_sect/dequeue_buf/atsc_tables.  We should stuff
// this data back into the channel's guide data, though if
// it's unreliable we will need to be able to prefer the
// downloaded XMLTV data.

/** \file atsctables.h
 *   Overall structure
 *   \code
 *     base_PID ---> STT (Time),
 *                   RRT (Censorship),
 *                   MGT (Guide management),
 *                   VCT (Channel Guide)
 *
 *     MGT ---> (PID_A,EIT[0],version_number),
 *              (PID_B,EIT[1],version_number),      etc,
 *              (PID_X,RTT,version_number),
 *              (PID_Y,RTT,version_number),         etc,
 *              (PID_1,Event ETT,version_number),
 *              (PID_2,Event ETT,version_number),   etc,
 *              (PID_m,Channel ETT,version_number),
 *              (PID_n,Channel ETT,version_number), etc.
 *       Each EIT[i] is valid for 3 hours,
 *       EIT[0] is always the current programming.
 *       Other tables are described by descriptors,
 *       such as Subtitle Descriptors.
 *
 *     VCT ---> (channel-A,source-id[0]),
 *              (channel-B,source-id[1]), etc.
 *   \endcode
 *
 *   New VCTs can be sent with a new version number to update the programming
 *   in an EIT, then when the data is complete a new MGT is sent with an
 *   updated version_number for that EIT, updating the data.
 */

class MTV_PUBLIC TableClass
{
  public:
    enum kTableTypes : std::int8_t
    {
        UNKNOWN = -1,
        TVCTc   =  0,
        TVCTnc  =  1,
        CVCTc   =  2,
        CVCTnc  =  3,
        ETTc    =  4,
        DCCSCT  =  5,
        EIT     =  6,
        ETTe    =  7,
        DCCT    =  8,
        RRT     =  9,
    };
};

/** \class MasterGuideTable
 *  \brief This table tells the decoder on which PIDs to find other tables,
 *         and their sizes and each table's current version number.
 */
class MTV_PUBLIC MasterGuideTable : public PSIPTable
{
  public:
    MasterGuideTable(const MasterGuideTable& table) : PSIPTable(table)
    {
        assert(TableID::MGT == TableID());
        Parse();
    }
    explicit MasterGuideTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::MGT == TableID());
        Parse();
    }
    ~MasterGuideTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC7
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4     0-4093
    // table_id_extension      16   3.0     0x0000

    /* Each Map ID corresponds to one set of channel mappings. Each STB
     * is expected to ignore any Map ID's other than the one corresponding
     * to it's head-end.
     *
     * Note: This is only defined for SCTE streams, it is always 0 in ATSC streams
     */
    uint SCTEMapId() const
    {
        return (pesdata()[3]<<8) | pesdata()[4];
    }

    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // tables_defined          16   9.0, 6-370 valid OTA, 2-370 valid w/Cable
    uint TableCount()    const { return (pesdata()[9]<<8) | pesdata()[10]; }
    uint TableCountRaw() const { return (pesdata()[9]<<8) | pesdata()[10]; }
    // for (i=0; i<tableCount(); i++) {
    //   table_type                    16  0.0
    uint TableType(uint i) const
    {
        return (m_ptrs[i][0]<<8) | m_ptrs[i][1];
    }
    int TableClass(uint i) const;
    QString TableClassString(uint i) const;
    //   reserved                       3  2.0    0x7
    //   table_type_PID                13  2.3
    uint TablePID(uint i) const
    {
        return ((m_ptrs[i][2]<<8) | (m_ptrs[i][3])) & 0x1fff;
    }
    //   reserved                       3  4.0    0x7
    //   table_type_version_number      5  4.3
    uint TableVersion(uint i) const
    {
        return (m_ptrs[i][4]) & 0x1f;
    }
    //   number_bytes for table desc.  32  5.0
    uint TableDescriptorsBytes(uint i) const
    {
        return ((m_ptrs[i][5]<<24) | (m_ptrs[i][6]<<16) |
                (m_ptrs[i][7]<<8)  | (m_ptrs[i][8]));
    }
    //   reserved                       4  9.0    0xf
    //   table_type_descriptors_length 12  9.4
    uint TableDescriptorsLength(uint i) const
    {
        return ((m_ptrs[i][9]<<8) | (m_ptrs[i][10])) & 0xfff;
    }

    //   for (I = 0; I<M; I++)
    //     descriptor = 11 + (offset)
    //   }
    // }
    const unsigned char* TableDescriptors(uint i) const
    {
        return m_ptrs[i]+11;
    }
    // reserved                         4         0xf
    // descriptors_length              12
    uint GlobalDescriptorsLength() const
    {
        return ((m_ptrs[TableCount()][0]<<8) |
                (m_ptrs[TableCount()][1])) & 0xfff;
    }

    const unsigned char* GlobalDescriptors() const
    {
        return m_ptrs[TableCount()]+2;
    }
    // for (I=0; I<N; I++) {
    //   descriptor()
    // }
    // CRC_32                          32

    void Parse(void) const;
    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
  private:
    mutable std::vector<unsigned char*> m_ptrs; // used to parse
};

/** \class VirtualChannelTable
 *  \brief This table contains information about the
 *         channels transmitted on this multiplex.
 *  \sa TerrestrialVirtualChannelTable,
 *      CableVirtualChannelTable
 */
class MTV_PUBLIC VirtualChannelTable : public PSIPTable
{
  public:
    VirtualChannelTable(const VirtualChannelTable &table) : PSIPTable(table)
    {
        assert(TableID::TVCT == TableID() || TableID::CVCT == TableID());
        Parse();
    }
    explicit VirtualChannelTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::TVCT == TableID() || TableID::CVCT == TableID());
        Parse();
    }

    ~VirtualChannelTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0      0xC8/0xC9
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // transport_stream_id     16   3.0
    uint TransportStreamID() const { return TableIDExtension(); }

    // num_channels_in_section  8   9.0
    uint ChannelCount()      const { return pesdata()[9]; }
    uint ChannelCountRaw()   const { return pesdata()[9]; }

    // for(i=0; i<num_channels_in_section; i++) {
    //   short_name          7*16   0.0 (7 UCS-2 chars padded by 0x0000)
    QString ShortChannelName(uint i) const
    {
        if (i >= ChannelCount())
            return {};

        QString str;
        const auto* ustr = reinterpret_cast<const unsigned short*>(m_ptrs[i]);
        for (int j=0; j<7; j++)
        {
            QChar c(qFromBigEndian(ustr[j]));
            if (c != QChar('\0')) str.append(c);
        }
        return str.simplified();
    }
    //   reserved               4  13.0        0xf

    //   major_channel_number  10  13.4
    // 14 RRRR JJJJ 15 jjjj jjmm  16 MMMM MMMM
    //              JJ JJjj jjjj  mm MMMM MMMM
    uint MajorChannel(uint i) const
    {
        return (((m_ptrs[i][14])<<6)&0x3c0) | (m_ptrs[i][15]>>2);
    }
    //   minor_channel_number  10  14.6
    uint MinorChannel(uint i) const
    {
        return (((m_ptrs[i][15])<<8)&0x300) | m_ptrs[i][16];
    }
    //   modulation_mode        8  16.0
    uint ModulationMode(uint i) const
    {
        return m_ptrs[i][17];
    }
    QString ModulationModeString(uint i) const;
    //   carrier_frequency     32  17.0 deprecated
    //   channel_TSID          16  21.0
    uint ChannelTransportStreamID(uint i) const
    {
        return ((m_ptrs[i][22]<<8) | m_ptrs[i][23]);
    }
    //   program_number        16  23.0
    uint ProgramNumber(uint i) const
    {
        return ((m_ptrs[i][24]<<8) | m_ptrs[i][25]);
    }
    //   ETM_location           2  25.0
    uint ETMlocation(uint i) const
    {
        return (m_ptrs[i][26]>>6) & 0x03;
    }
    //   access_controlled      1  25.2
    bool IsAccessControlled(uint i) const
    {
        return ( m_ptrs[i][26] & 0x20 ) != 0;
    }
    //   hidden                 1  25.3
    bool IsHidden(uint i) const
    {
        return ( m_ptrs[i][26] & 0x10 ) != 0;
    }
    //   reserved               2  25.4          3
    //   hide_guide             1  25.6
    bool IsHiddenInGuide(uint i) const
    {
        return ( m_ptrs[i][26] & 0x02 ) != 0;
    }
    //   reserved               6  25.7       0x3f
    //   service_type           6  26.2
    uint ServiceType(uint i) const
    {
        return m_ptrs[i][27] & 0x3f;
    }
    QString ServiceTypeString(uint i) const;
    //   source_id             16  27.0
    uint SourceID(uint i) const
    {
        return ((m_ptrs[i][28]<<8) | m_ptrs[i][29]);
    }
    //   reserved               6  29.0       0xfb
    //   descriptors_length    10  29.6
    uint DescriptorsLength(uint i) const
    {
        return ((m_ptrs[i][30]<<8) | m_ptrs[i][31]) & 0x03ff;
    }
    //   for (i=0;i<N;i++) { descriptor() }
    const unsigned char* Descriptors(uint i) const
    {
        return m_ptrs[i]+32;
    }
    // }
    // reserved                 6             0xfb
    // additional_descriptors_length 10
    uint GlobalDescriptorsLength() const
    {
        uint i = ChannelCount();
        return ((m_ptrs[i][0]<<8) | m_ptrs[i][1]) & 0x03ff;
    }
    // for (j=0; j<N; j++) { additional_descriptor() }
    const unsigned char* GlobalDescriptors() const
    {
        return m_ptrs[ChannelCount()]+2;
    }
    // CRC_32                  32
    void Parse() const;
    int Find(int major, int minor) const;
    QString GetExtendedChannelName(uint idx) const;
    QString toString(void) const override; // PSIPTable
    virtual QString ChannelString(uint channel) const = 0;
    QString toStringXML(uint indent_level) const override; // PSIPTable
    virtual QString ChannelStringXML(uint indent_level, uint channel) const;
    virtual QString XMLChannelValues(uint indent_level, uint channel) const;
  protected:
    mutable std::vector<unsigned char*> m_ptrs;
};

/** \class TerrestrialVirtualChannelTable
 *  \brief This table contains information about the terrestrial
 *         channels transmitted on this multiplex.
 *  \sa CableVirtualChannelTable
 */
class MTV_PUBLIC TerrestrialVirtualChannelTable : public VirtualChannelTable
{
  public:
    TerrestrialVirtualChannelTable(const TerrestrialVirtualChannelTable &table)
        : VirtualChannelTable(table)
    {
        assert(TableID::TVCT == TableID());
    }
    explicit TerrestrialVirtualChannelTable(const PSIPTable &table)
        : VirtualChannelTable(table)
    {
        assert(TableID::TVCT == TableID());
    }
    ~TerrestrialVirtualChannelTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC8
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // transport_stream_id     16   3.0
    // num_channels_in_section  8   9.0

    // for (i=0; i<num_channels_in_section; i++)
    // {
    //   short_name          7*16   0.0 (7 UTF-16 chars padded by 0x0000)
    //   reserved               4  14.0        0xf

    //   major_channel_number  10  14.4
    // 14 RRRR JJJJ 15 jjjj jjmm  16 MMMM MMMM
    //              JJ JJjj jjjj  mm MMMM MMMM
    //   minor_channel_number  10  15.6
    //   modulation_mode        8  17.0
    //   carrier_frequency     32  18.0 deprecated
    //   channel_TSID          16  22.0
    //   program_number        16  24.0
    //   ETM_location           2  26.0
    //   access_controlled      1  26.2
    //   hidden                 1  26.3
    //   reserved               2  26.4          3
    //   hide_guide             1  26.6
    //   reserved               6  26.7       0x3f
    //   service_type           6  27.2
    //   source_id             16  28.0
    //   reserved               6  30.0       0xfb
    //   descriptors_length    10  30.6-31
    //   for (i=0;i<N;i++) { descriptor() }
    // }
    // reserved                 6             0xfb
    // additional_descriptors_length 10
    // for (j=0; j<N; j++) { additional_descriptor() }
    // CRC_32                  32
    QString ChannelString(uint channel) const override; // VirtualChannelTable
    QString XMLChannelValues(uint indent_level, uint channel) const override; // VirtualChannelTable
};


/** \class CableVirtualChannelTable
 *  \brief This table contains information about the cable
 *         channels transmitted on this multiplex.
 *  \sa TerrestrialVirtualChannelTable
 */
class MTV_PUBLIC CableVirtualChannelTable : public VirtualChannelTable
{
  public:
    CableVirtualChannelTable(const CableVirtualChannelTable &table)
        : VirtualChannelTable(table)
    {
        assert(TableID::CVCT == TableID());
    }
    explicit CableVirtualChannelTable(const PSIPTable &table)
        : VirtualChannelTable(table)
    {
        assert(TableID::CVCT == TableID());
    }
    ~CableVirtualChannelTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC9
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000

    /* Each Map ID corresponds to one set of channel mappings. Each STB
     * is expected to ignore any Map ID's other than the one corresponding
     * to it's head-end.
     *
     * Note: This is only defined for SCTE streams,
     *       it is always 0 in ATSC streams.
     */
    uint SCTEMapId() const
    {
        return (pesdata()[3]<<8) | pesdata()[4];
    }

    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // for (i=0; i<num_channels_in_section; i++)
    // {
    //   short_name          7*16   0.0 (7 UTF-16 chars padded by 0x0000)
    //   reserved               4  14.0        0xf

    //   major_channel_number  10  14.4
    // 14 RRRR JJJJ 15 jjjj jjmm  16 MMMM MMMM
    //              JJ JJjj jjjj  mm MMMM MMMM
    //   minor_channel_number  10  15.6

    bool SCTEIsChannelNumberOnePart(uint i) const
    {
        return (MajorChannel(i) >> 4) == 0x3f;
    }
    bool SCTEIsChannelNumberTwoPart(uint i) const
    {
        return MajorChannel(i) < 1000;
    }
    // Note: In SCTE streams, the channel number is undefined if
    //       the two functions above return false. As of 2002 spec.

    uint SCTEOnePartChannel(uint i) const
    {
        if (SCTEIsChannelNumberOnePart(i))
            return ((MajorChannel(i) & 0xf) << 10) | MinorChannel(i);
        return 0;
    }

    //   modulation_mode        8  17.0
    //   carrier_frequency     32  18.0 deprecated
    //   channel_TSID          16  22.0
    //   program_number        16  24.0
    //   ETM_location           2  26.0
    //   access_controlled      1  26.2
    //   hidden                 1  26.3
    //   path_select            1  26.4
    bool IsPathSelect(uint i) const
    {
        return ( m_ptrs[i][26] & 0x08 ) != 0;
    }
    //   out_of_band            1  26.5
    bool IsOutOfBand(uint i) const
    {
        return ( m_ptrs[i][26] & 0x04 ) != 0;
    }
    //   hide_guide             1  26.6
    //   reserved               3  26.7          7
    //   service_type           6  27.2
    //   source_id             16  28.0
    //   reserved               6  30.0       0xfb
    //   descriptors_length    10  30.6-31
    //   for (i=0;i<N;i++) { descriptor() }
    // }
    // reserved                 6             0xfb
    // additional_descriptors_length 10
    // for (j=0; j<N; j++) { additional_descriptor() }
    // CRC_32                  32
    QString ChannelString(uint channel) const override; // VirtualChannelTable
    QString XMLChannelValues(uint indent_level, uint channel) const override; // VirtualChannelTable
};

/** \class EventInformationTable
 *  \brief EventInformationTables contain program titles, start times,
 *         and channel information.
 *  \sa ExtendedTextTable, TerrestrialVirtualChannelTable, CableVirtualChannelTable
 */
class MTV_PUBLIC EventInformationTable : public PSIPTable
{
  public:
    EventInformationTable(const EventInformationTable &table)
        : PSIPTable(table)
    {
        assert(TableID::EIT == TableID());
        Parse();
    }
    explicit EventInformationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::EIT == TableID());
        Parse();
    }
    ~EventInformationTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCB
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // source_id               16   3.0     0x0000
    uint SourceID() const { return TableIDExtension(); }

    // num_events_in_section    8   9.0
    uint EventCount()    const { return psipdata()[1]; }
    uint EventCountRaw() const { return psipdata()[1]; }
    // for (j = 0; j< num_events_in_section;j++)
    // {
    //   reserved               2   0.0    3
    //   event_id              14   0.2
    uint EventID(uint i) const
    {
        return ((m_ptrs[i][0]<<8) | m_ptrs[i][1])&0x3fff;
    }
    //   start_time            32   2.0
    uint StartTimeRaw(uint i) const
    {
        return ((m_ptrs[i][2]<<24) | (m_ptrs[i][3]<<16) |
                (m_ptrs[i][4]<<8)  | (m_ptrs[i][5]));
    }
    QDateTime StartTimeGPS(uint i) const
    {
        // Time in GPS seconds since 00:00:00 on January 6th, 1980 UTC
        return MythDate::fromSecsSinceEpoch(GPS_EPOCH + StartTimeRaw(i));
    }
    //   reserved               2   6.0    3
    //   ETM_location           2   6.2
    uint ETMLocation(uint i) const
    {
        return (m_ptrs[i][6]>>4)&3;
    }
    //   length_in_seconds     20   6.4
    uint LengthInSeconds(uint i) const
    {
        return ((m_ptrs[i][6]<<16) | (m_ptrs[i][7]<<8) |
                (m_ptrs[i][8])) & 0xfffff;
    }
    //   title_length           8   9.0
    uint TitleLength(uint i) const
    { return m_ptrs[i][9]; }
    //   title_text() var       *  10.0
    MultipleStringStructure title(int i) const
    {
        return MultipleStringStructure(m_ptrs[i]+10);
    }
    //   reserved               4        0xf
    //   descriptors_length    12
    uint DescriptorsLength(uint i) const
    {
        unsigned char *desc=m_ptrs[i]+10+TitleLength(i);
        return ((desc[0]<<8)|(desc[1]))&0xfff;
    }
    //   for (i=0;i<N;i++)
    //   {
    //      descriptor()
    const unsigned char* Descriptors(uint i) const
    {
        return m_ptrs[i]+12+TitleLength(i);
    }
    //   }
    // }
    // CRC_32 32
    void Parse() const;
    QString toString() const override; // PSIPTable
  private:
    mutable std::vector<unsigned char*> m_ptrs;
};

/** \class ExtendedTextTable
 *  \brief ExtendedTextTable contain additional text not
 *         contained in EventInformationTables.
 *  \sa EventInformationTable
 */
class MTV_PUBLIC ExtendedTextTable : public PSIPTable
{
  public:
    ExtendedTextTable(const ExtendedTextTable &table) : PSIPTable(table)
    {
        assert(TableID::ETT == TableID());
    }
    explicit ExtendedTextTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::ETT == TableID());
    }
    ~ExtendedTextTable() override { ; }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCC
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4     0-4093
    // ETT_table_id_extension  16   3.0  unique per pid
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    uint ExtendedTextTableID() const { return TableIDExtension(); }
    void SetExtendedTextTableID(uint id)
        { SetTableIDExtension(id); }

    // ETM_id                  32  10.0
    //                    31..16      15..2 iff  1..0
    // channel ETM_id   source_id       0         00
    // event   ETM_id   source_id   event_id      10
    bool IsChannelETM(void)    const { return 0 == (psipdata()[4] & 3); }
    bool IsEventETM(void)      const { return 2 == (psipdata()[4] & 3); }
    uint SourceID(void) const
        { return (psipdata()[1] << 8) | psipdata()[2]; }
    uint EventID(void) const
        { return (psipdata()[3] << 6) | (psipdata()[4] >> 2); }

    // extended_text_message    *  14.0  multiple string structure a/65b p81
    MultipleStringStructure ExtendedTextMessage() const
    {
        return MultipleStringStructure(psipdata() + 5);
    }

    QString toString() const override; // PSIPTable
    // CRC_32                  32
};

/** \class SystemTimeTable
 *  \brief This table contains the GPS time at the time of transmission.
 *
 *   It can we used to detect drift between GPS and UTC time, this
 *   is currently at 14 seconds.
 *   See also: a_65b.pdf page 23
 */
class MTV_PUBLIC SystemTimeTable : public PSIPTable
{
  public:
    SystemTimeTable(const SystemTimeTable &table) : PSIPTable(table)
    {
        assert(TableID::STT == TableID());
    }
    explicit SystemTimeTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::STT == TableID());
    }

    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCD
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4     0-4093
    // table_id_extension      16   3.0          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now

    // system_time             32   9.0
    uint32_t GPSRaw(void) const
    {
        return ((pesdata()[9] <<24) | (pesdata()[10]<<16) |
                (pesdata()[11]<< 8) |  pesdata()[12]);
    }
    QDateTime SystemTimeGPS(void) const
    {
        return MythDate::fromSecsSinceEpoch(GPS_EPOCH + GPSRaw());
    }
    time_t GPSUnix(void) const
        { return GPS_EPOCH + GPSRaw(); }
    time_t UTCUnix(void) const
        { return GPSUnix() - GPSOffset(); }

    // GPS_UTC_offset           8  13.0
    uint GPSOffset() const { return pesdata()[13]; }
    // daylight_savings        16  14.0
    //   DS_status              1  14.0
    //   reserved               2  14.1          3
    //   DS_day_of_month        5  14.3
    //   DS_hour                8  15.0
    bool InDaylightSavingsTime()     const { return ( pesdata()[14]&0x80 ) != 0; }
    uint DayDaylightSavingsStarts()  const { return pesdata()[14]&0x1f; }
    uint HourDaylightSavingsStarts() const { return pesdata()[15]; }
    // for (I = 0;I< N;I++) { descriptor() }
    // CRC_32                  32

    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
};

/** \class RatingRegionTable
 *  \brief No one has had time to decode this table yet...
 */
class MTV_PUBLIC RatingRegionTable : public PSIPTable
{
  public:
    RatingRegionTable(const RatingRegionTable &table) : PSIPTable(table)
    {
        assert(TableID::RRT == TableID());
    }
    explicit RatingRegionTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::RRT == TableID());
    }
};

/** \class DirectedChannelChangeTable
 *  \brief No one has had time to decode this table yet...
 */
class MTV_PUBLIC DirectedChannelChangeTable : public PSIPTable
{
  public:
    DirectedChannelChangeTable(const DirectedChannelChangeTable &table)
        : PSIPTable(table)
    {
        assert(TableID::DCCT == TableID());
    }
    explicit DirectedChannelChangeTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::DCCT == TableID());
    }

    // Name                               bits  loc  expected value
    // table_id                             8   0.0       0xD3
    // section_syntax_indicator             1   1.0          1
    // private_indicator                    1   1.1          1
    // reserved                             2   1.2          3
    // section_length                      12   1.4     0-4093
    // dcc_subtype                          8   3.0
    // dcc_id                               8   4.0
    // reserved                             2   5.0          3
    // version_number                       5   5.2       0-31
    // current_next_indicator               1   5.7          1
    // section_number                       8   6.0       0x00
    // last_section_number                  8   7.0       0x00
    // protocol_version                     8   8.0       0x00 for now

    // dcc_test_count                       8   9.0
    // for (i = 0; i < num_dcc_test_count; i++)
    // {
    //   dcc_context                        1   0.0
    //   reserved                           3   0.1         7
    //   dcc_from_major_channel_number     10   0.4
    //   dcc_from_minor_channel_number     10   1.6
    //   reserved                           4   3.0        15
    //   dcc_to_major_channel_number       10   3.4
    //   dcc_to_minor_channel_number       10   4.6
    //   dcc_start_time                    32   5.0
    //   dcc_end_time                      32   9.0
    //   dcc_term_count                     8  10.0
    //   for (j=0; j< dcc_term_count; j++) {
    //     dcc_selection_type               8   0.0
    //     dcc_selection_id                64   1.0
    //     reserved                         6   9.0
    //     dcc_term_descriptors_length     10   9.6
    //     for (k=0; k<N; k++) {
    //       dcc_term_descriptor()
    //     }
    //   }
    //   reserved                           6
    //   dcc_test_descriptors_length       10
    //   for (j=0; j<N; j++) {
    //     dcc_test_descriptor()
    //   }
    // Reserved                             6
    // dcc_additional_descriptors_length   10
    // for (i=0; i<N; i++) {
    //   dcc_additional_descriptor()
    // }
    // CRC_32
    // }
};

/** \class DirectedChannelChangeSelectionCodeTable
 *  \brief No one has had time to decode this table yet...
 */
class MTV_PUBLIC DirectedChannelChangeSelectionCodeTable : public PSIPTable
{
  public:
    DirectedChannelChangeSelectionCodeTable(
        const DirectedChannelChangeSelectionCodeTable &table)
        : PSIPTable(table)
    {
        assert(TableID::DCCSCT == TableID());
    }
    explicit DirectedChannelChangeSelectionCodeTable(const PSIPTable &table)
        : PSIPTable(table)
    {
        assert(TableID::DCCSCT == TableID());
    }

    // Name                                  bits  loc  expected value
    // table_id                                8   0.0       0xD4
    // section_syntax_indicator                1   1.0          1
    // private_indicator                       1   1.1          1
    // reserved                                2   1.2          3
    // section_length                         12   1.4     0-4093
    // dccsct_type                            16   3.0
    // reserved                                2   5.0          3
    // version_number                          5   5.2
    // current_next_indicator                  1   5.7          1
    // section_number                          8   6.0       0x00
    // last_section_number                     8   7.0       0x00
    // protocol_version                        8   8.0

    // updates_defined                         8   9.0
    // for (i=0; i< updates_defined; i++) {
    //   update_type                           8   0.0
    //   update_data_length                    8   1.0
    //   if (update_type == new_genre_category) {
    //     genre_category_code                 8
    //     genre_category_name_text()         var
    //   }
    //   if (update_type == new_state) {
    //     dcc_state_location_code             8
    //     dcc_state_location_code_text()     var
    //   }
    //   if (update_type == new_county) {
    //     state_code                          8   0.0
    //     reserved                            6   1.0         31
    //     dcc_county_location_code           10   1.6
    //     dcc_county_location_code_text()    var
    //   }
    //   reserved                              6               31
    //   dccsct_descriptors_length            10
    //   for (j=0; j<N; j++) {
    //     dccsct_descriptors()
    //   }
    // }
    // reserved                                6               31
    // dccsct_additional_descriptors_length   10
    // for (i=0; i<N; i++) {
    //   dccsct_additional_descriptors()
    // }
    // CRC_32                                 32
};

/// SCTE 65 & ATSC/81 0xD6
class MTV_PUBLIC AggregateEventInformationTable : public PSIPTable
{
  public:
    AggregateEventInformationTable(
        const AggregateEventInformationTable &table) : PSIPTable(table)
    {
        assert(TableID::AEIT == TableID());
    }
    explicit AggregateEventInformationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::AEIT == TableID());
    }

    QString toString(void) const override // PSIPTable
        { return "AggregateEventInformationTable\n"; }
    QString toStringXML(uint /*indent_level*/) const override // PSIPTable
        { return "<AggregateEventInformationTable />"; }

    // Name                                  bits  loc  expected value
    // table_id                               8    0.0       0xD6
    // section_syntax_indicator               1    1.0          1
    // private_indicator                      1    1.1          1
    // reserved                               2    1.2          3
    // section_length                        12    1.4     0-4093
    // AEIT_subtype                           8    3.0       0x00
    // MGT_tag                                8    4.0      0-255
    // reserved                               2    5.0          3
    // version_number                         5    5.2       0-31
    // current_next_indicator                 1    5.7          1
    // section_number                         8    6.0       0x00
    // last_section_number                    8    7.0       0x00

    // if (AEIT_subtype == 0) {
    //   num_sources_in_section               8    8.0
    //   for (j = 0; j< num_sources_in_section;j++) {
    //     source_id                         16    9.0
    //     num_events                         8   11.0
    //     for (j = 0; j< num_events;j++) {
    //       off_air                          1    0.0        T/F
    //       reserved                         1    0.1          1
    //       event_id                        14    0.2
    //       start_time                      32    2.0
    //       reserved                         4    6.0         15
    //       duration                        20    6.4
    //       title_length                     8    9.0
    //       title_text()                    var
    //       reserved                         4                15
    //       descriptors_length              12
    //       for (i=0;i<N;i++) {
    //         descriptor()
    //       }
    //     }
    //   }
    // else
    //   reserved
    // CRC_32                                32
};

/// SCTE 65 & ATSC/81 0xD7
class MTV_PUBLIC AggregateExtendedTextTable : public PSIPTable
{
  public:
    AggregateExtendedTextTable(
        const AggregateExtendedTextTable &table) : PSIPTable(table)
    {
        assert(TableID::AETT == TableID());
    }
    explicit AggregateExtendedTextTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::AETT == TableID());
    }

    QString toString(void) const override // PSIPTable
        { return "AggregateExtendedTextTable\n"; }
    QString toStringXML(uint /*indent_level*/) const override // PSIPTable
        { return "<AggregateExtendedTextTable />"; }
    // Name                                  bits  loc  expected value
    // table_id                                8   0.0       0xD7
    // section_syntax_indicator                1   1.0          1
    // private_indicator                       1   1.1          1
    // reserved                                2   1.2          3
    // section_length                        12    1.4     0-4093
    // AEIT_subtype                           8    3.0       0x00
    // MGT_tag                                8    4.0      0-255
    // reserved                               2    5.0          3
    // version_number                         5    5.2       0-31
    // current_next_indicator                 1    5.7          1
    // section_number                         8    6.0       0x00
    // last_section_number                    8    7.0       0x00

    // if (AETT_subtype == 0) {
    //   num_blocks_in_section                8    8.0
    //   for (j = 0; j< num_blocks_in_section;j++) {
    //     ETM_id                            32    0.0
    //     reserved                           4    4.0         15
    //     extended_text_length              12    4.4
    //     extended_text_message()          var
    //     }
    //   }
    // else
    //   reserved
    // CRC_32                                32

};

#endif // ATSC_TABLES_H
