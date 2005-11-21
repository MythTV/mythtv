// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _ATSC_TABLES_H_
#define _ATSC_TABLES_H_

#include "mpegtables.h"
#include "atscdescriptors.h"
#include <qstring.h>

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

#define secs_Between_1Jan1970_6Jan1980 315982800

class TableClass
{
  public:
    typedef enum
    {
        UNKNOWN=-1,
        TVCTc=0,
        TVCTnc,
        CVCTc,
        CVCTnc,
        ETTc,
        DCCSCT,
        EIT,
        ETTe,
        DCCT,
        RRT,
    };
};

/** \class MasterGuideTable
 *  \brief This table tells the decoder on which PIDs to find other tables,
 *         and their sizes and each table's current version number.
 */
class MasterGuideTable : public PSIPTable
{
  public:
    MasterGuideTable(const PSIPTable& table) : PSIPTable(table)
    {
    // table_id                 8   0.0       0xC7
        assert(TableID::MGT == TableID());
        Parse();
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }
    ~MasterGuideTable() { ; }

    // tables_defined          16   9.0, 6-370 valid OTA, 2-370 valid w/Cable
    uint TableCount() const
    {
         return (PESData()[9]<<8) | PESData()[10];
    }
    // for (i=0; i<tableCount(); i++) {
    //   table_type                    16  0.0
    uint TableType(uint i) const
    {
        return (_ptrs[i][0]<<8) | _ptrs[i][1];
    }
    int TableClass(uint i) const;
    QString TableClassString(uint i) const;
    //   reserved                       3  2.0    0x7
    //   table_type_PID                13  2.3
    uint TablePID(uint i) const
    {
        return ((_ptrs[i][2]<<8) | (_ptrs[i][3])) & 0x1fff;
    }
    //   reserved                       3  4.0    0x7
    //   table_type_version_number      5  4.3
    uint TableVersion(uint i) const
    {
        return (_ptrs[i][4]) & 0x1f;
    }
    //   number_bytes for table desc.  32  5.0
    uint TableDescriptorsBytes(uint i) const
    {
        return ((_ptrs[i][5]<<24) | (_ptrs[i][6]<<16) |
                (_ptrs[i][7]<<8)  | (_ptrs[i][8]));
    }
    //   reserved                       4  9.0    0xf
    //   table_type_descriptors_length 12  9.4
    uint TableDescriptorsLength(uint i) const
    {
        return ((_ptrs[i][9]<<8) | (_ptrs[i][10])) & 0xfff;
    }

    //   for (I = 0; I<M; I++)
    //     descriptor = 11 + (offset)
    //   }
    // }
    const unsigned char* TableDescriptors(uint i) const
    {
        return _ptrs[i]+11;
    }
    // reserved                         4         0xf
    // descriptors_length              12
    uint GlobalDescriptorsLength() const
    {
        return ((_ptrs[TableCount()][0]<<8) |
                (_ptrs[TableCount()][1])) & 0xfff;
    }

    const unsigned char* GlobalDescriptors() const
    {
        return _ptrs[TableCount()]+2;
    }
    // for (I=0; I<N; I++) {
    //   descriptor()
    // }
    // CRC_32                          32

    void Parse() const;
    QString toString() const;
  private:
    mutable vector<unsigned char*> _ptrs; // used to parse
};

/** \class VirtualChannelTable
 *  \brief This table contains information about the
 *         channels transmitted on this multiplex.
 *  \sa TerrestrialVirtualChannelTable,
 *      CableVirtualChannelTable
 */
class VirtualChannelTable : public PSIPTable
{
  public:
    VirtualChannelTable(const PSIPTable& table) : PSIPTable(table)
    {
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
        assert(TableID::TVCT == TableID() || TableID::CVCT == TableID());
        Parse();
    }

    ~VirtualChannelTable() { ; }

    // transport_stream_id     16   3.0
    uint TransportStreamID() const { return TableIDExtension(); }

    // num_channels_in_section  8   9.0
    uint ChannelCount()      const { return PESData()[9]; }

    // for(i=0; i<num_channels_in_section; i++) {
    //   short_name          7*16   0.0 (7 UTF-16 chars padded by 0x0000)
    const QString ShortChannelName(uint i) const
    {
        QString str;
        const unsigned short* ustr =
            reinterpret_cast<const unsigned short*>(_ptrs[i]);
        for (int j=0; j<7; j++)
        {
            QChar c((ustr[j]<<8) | (ustr[j]>>8));
            if (c != QChar('\0')) str.append(c);
        }
        return str;
    }
    //   reserved               4  13.0        0xf

    //   major_channel_number  10  13.4
    // 14 RRRR JJJJ 15 jjjj jjmm  16 MMMM MMMM
    //              JJ JJjj jjjj  mm MMMM MMMM
    uint MajorChannel(uint i) const
    {
        return (((_ptrs[i][14])<<6)&0x3c0) | (_ptrs[i][15]>>2);
    }
    //   minor_channel_number  10  14.6
    uint MinorChannel(uint i) const
    {
        return (((_ptrs[i][15])<<8)&0x300) | _ptrs[i][16];
    }
    //   modulation_mode        8  16.0
    uint ModulationMode(uint i) const
    {
        return _ptrs[i][17];
    }
    //   carrier_frequency     32  17.0 deprecated
    //   channel_TSID          16  21.0
    uint ChannelTransportStreamID(uint i) const
    {
        return ((_ptrs[i][22]<<8) | _ptrs[i][23]);
    }
    //   program_number        16  23.0
    uint ProgramNumber(uint i) const
    {
        return ((_ptrs[i][24]<<8) | _ptrs[i][25]);
    }
    //   ETM_location           2  25.0
    uint ETMlocation(uint i) const
    {
        return (_ptrs[i][26]>>6) & 0x03;
    }
    //   access_controlled      1  25.2
    bool IsAccessControlled(uint i) const
    {
        return bool(_ptrs[i][26] & 0x20);
    }
    //   hidden                 1  25.3
    bool IsHidden(uint i) const
    {
        return bool(_ptrs[i][26] & 0x10);
    }
    //   reserved               2  25.4          3
    //   hide_guide             1  25.6
    bool IsHiddenInGuide(uint i) const
    {
        return bool(_ptrs[i][26] & 0x2);
    }
    //   reserved               6  25.7       0x3f 
    //   service_type           6  26.2
    uint ServiceType(uint i) const
    {
        return _ptrs[i][27] & 0x3f;
    }
    //   source_id             16  27.0
    uint SourceID(uint i) const
    {
        return ((_ptrs[i][28]<<8) | _ptrs[i][29]);
    }
    //   reserved               6  29.0       0xfb
    //   descriptors_length    10  29.6
    uint DescriptorsLength(uint i) const
    {
        return ((_ptrs[i][30]<<8) | _ptrs[i][31]) & 0x03ff;
    }
    //   for (i=0;i<N;i++) { descriptor() }
    const unsigned char* Descriptors(uint i) const
    {
        return _ptrs[i]+32;
    }
    // }
    // reserved                 6             0xfb
    // additional_descriptors_length 10
    uint GlobalDescriptorsLength() const
    {
        uint i = ChannelCount();
        return ((_ptrs[i][0]<<8) | _ptrs[i][1]) & 0x03ff;
    }
    // for (j=0; j<N; j++) { additional_descriptor() }
    const unsigned char* GlobalDescriptors() const
    {
        return _ptrs[ChannelCount()]+2;
    }
    // CRC_32                  32
    void Parse() const;
    int Find(int major, int minor) const;
    QString GetExtendedChannelName(uint idx) const;
  protected:
    mutable vector<unsigned char*> _ptrs;
};

/** \class TerrestrialVirtualChannelTable
 *  \brief This table contains information about the terrestrial
 *         channels transmitted on this multiplex.
 *  \sa CableVirtualChannelTable
 */
class TerrestrialVirtualChannelTable : public VirtualChannelTable
{
  public:
    TerrestrialVirtualChannelTable(const PSIPTable& table)
        : VirtualChannelTable(table)
    {
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC8
        assert(TableID::TVCT == TableID());
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }
    ~TerrestrialVirtualChannelTable() { ; }

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
    QString toString() const;
    QString toString(int) const;
};


/** \class CableVirtualChannelTable
 *  \brief This table contains information about the cable
 *         channels transmitted on this multiplex.
 *  \sa TerrestrialVirtualChannelTable
 */
class CableVirtualChannelTable : public VirtualChannelTable
{
  public:
    CableVirtualChannelTable(const PSIPTable& table)
        : VirtualChannelTable(table)
    {
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xC9
        assert(TableID::CVCT == TableID());
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }
    ~CableVirtualChannelTable() { ; }

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
    //   path_select            1  26.4
    bool IsPathSelect(uint i) const
    {
        return bool(_ptrs[i][26] & 0x8);
    }
    //   out_of_band            1  26.5
    bool IsOutOfBand(uint i) const
    {
        return bool(_ptrs[i][26] & 0x4);
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
    QString toString() const;
    QString toString(int) const;
};

/** \class EventInformationTable
 *  \brief EventInformationTables contain program titles, start times,
 *         and channel information.
 *  \sa ExtendedTextTable, TerrestrialVirtualChannelTable, CableVirtualChannelTable
 */
class EventInformationTable : public PSIPTable
{
  public:
    EventInformationTable(const PSIPTable& table) : PSIPTable(table)
    {
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCB
        assert(TableID::EIT == TableID());
        Parse();
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // table_id_extension      16   3.0     0x0000
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }
    ~EventInformationTable() { ; }
    // source_id               16   3.0     0x0000
    uint SourceID() const { return TableIDExtension(); }

    // num_events_in_section    8   9.0
    uint EventCount() const { return PESData()[9]; }
    // for (j = 0; j< num_events_in_section;j++)
    // {
    //   reserved               2   0.0    3
    //   event_id              14   0.2
    uint EventID(uint i) const
    {
        return ((_ptrs[i][0]<<8) | _ptrs[i][1])&0x3fff;
    }
    //   start_time            32   2.0
    QDateTime StartTimeGPS(uint i) const
    {
        // Time in GPS seconds since 00:00:00 on January 6th, 1980 UTC
        uint t = ((_ptrs[i][2]<<24) | (_ptrs[i][3]<<16) |
                  (_ptrs[i][4]<<8)  | (_ptrs[i][5]));
        QDateTime dt;
        dt.setTime_t(secs_Between_1Jan1970_6Jan1980 + t);
        return dt;
    }
    //   reserved               2   6.0    3
    //   ETM_location           2   6.2
    uint ETMLocation(uint i) const
    {
        return (_ptrs[i][6]>>4)&3;
    }
    //   length_in_seconds     20   6.4
    uint LengthInSeconds(uint i) const
    {
        return ((_ptrs[i][6]<<16) | (_ptrs[i][7]<<8) |
                (_ptrs[i][8])) & 0xfffff;
    }
    //   title_length           8   9.0
    uint TitleLength(uint i) const
    { return _ptrs[i][9]; }
    //   title_text() var       *  10.0
    MultipleStringStructure title(int i) const
    {
        return MultipleStringStructure(_ptrs[i]+10);
    }
    //   reserved               4        0xf
    //   descriptors_length    12
    uint DescriptorsLength(uint i) const
    {
        unsigned char *desc=_ptrs[i]+10+TitleLength(i);
        return ((desc[0]<<8)|(desc[1]))&0xfff;
    }
    //   for (i=0;i<N;i++)
    //   {
    //      descriptor()
    const unsigned char* Descriptors(uint i) const
    {
        return _ptrs[i]+12+TitleLength(i);
    }
    //   }
    // }
    // CRC_32 32
    void Parse() const;
    QString toString() const;
  private:
    mutable vector<unsigned char*> _ptrs;
};

/** \class ExtendedTextTable
 *  \brief ExtendedTextTable contain additional text not
 *         contained in EventInformationTables.
 *  \sa EventInformationTable
 */
class ExtendedTextTable : public PSIPTable
{
  public:
    ExtendedTextTable(const PSIPTable& table) : PSIPTable(table)
    {
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCC
        assert(TableID::ETT == TableID());
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4
    // ETT_table_id_extension  16   3.0  unique per pid
    // reserved                 2   5.0          3
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }
    ~ExtendedTextTable() { ; }

    uint ExtendedTextTableID() const { return TableIDExtension(); }
    void SetExtendedTextTableID(uint id) { SetTableIDExtension(id); }

    // ETM_id                  32  10.0
    //                    31..16      15..2 iff  1..0
    // channel ETM_id   source_id       0         00
    // event   ETM_id   source_id   event_id      10
    bool IsChannelETM() const { return 0==(PESData()[12]&3); }
    bool IsEventETM() const   { return 2==(PESData()[12]&3); }
    int SourceID() const { return (PESData()[9]<<8) | PESData()[10]; }
    int EventID() const  { return (PESData()[11]<<6) | (PESData()[12]>>2); }

    // extended_text_message    *  14.0  multiple string structure a/65b p81
    const MultipleStringStructure ExtendedTextMessage() const
    {
        return MultipleStringStructure(PESData()+13);
    }

    QString toString() const;
    // CRC_32                  32
};

/** \class SystemTimeTable
 *  \brief This table contains the GPS time at the time of transmission.
 *
 *   It can we used to detect drift between GPS and UTC time, this
 *   is currently at 13 seconds.
 *   See also: a_65b.pdf page 23
 */
class SystemTimeTable : public PSIPTable
{
  public:
    SystemTimeTable(const PSIPTable& table) : PSIPTable(table)
    {
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xCD
        assert(TableID::STT == TableID());
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4
    // table_id_extension      16   3.0          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0  
    // current_next_indicator   1   5.7          1  
    // section_number           8   6.0       0x00 
    // last_section_number      8   7.0       0x00
    // protocol_version         8   8.0       0x00 for now
    }

    // system_time             32   9.0
    QDateTime SystemTimeGPS() const
    {
        uint t = ((PESData()[9]<<24)  | (PESData()[10]<<16) |
                  (PESData()[11]<< 8) |  PESData()[12]);
        QDateTime dt;
        dt.setTime_t(secs_Between_1Jan1970_6Jan1980 + t);
        return dt;
    }
    // GPS_UTC_offset           8  13.0 
    uint GPSOffset() const { return PESData()[13]; }
    // daylight_savings        16  14.0
    //   DS_status              1  14.0
    //   reserved               2  14.1          3
    //   DS_day_of_month        5  14.3
    //   DS_hour                8  15.0
    bool InDaylightSavingsTime()     const { return PESData()[14]&0x80; }
    uint DayDaylightSavingsStarts()  const { return PESData()[14]&0x1f; }
    uint HourDaylightSavingsStarts() const { return PESData()[15]; }
    // for (I = 0;I< N;I++) { descriptor() } 
    // CRC_32                  32

    QString toString() const
    {
        QString str =
            QString("    SystemTimeTable  GPSTime(%1) GPS2UTC_Offset(%2) ")
            .arg(SystemTimeGPS().toString(Qt::LocalDate)).arg(GPSOffset());
        str.append(QString("DS(%3) Day(%4) Hour(%5)")
                   .arg(InDaylightSavingsTime())
                   .arg(DayDaylightSavingsStarts())
                   .arg(HourDaylightSavingsStarts()));
        return str;
    }
};

/** \class RatingRegionTable
 *  \brief No one has had time to decode this table yet...
 */
class RatingRegionTable : public PSIPTable
{
  public:
    RatingRegionTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::RRT == TableID());
    }
};

/** \class DirectedChannelChangeTable
 *  \brief No one has had time to decode this table yet...
 */
class DirectedChannelChangeTable : public PSIPTable
{
  public:
    DirectedChannelChangeTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::DCCT == TableID());
    }
};

/** \class DirectedChannelChangeSelectionCodeTable
 *  \brief No one has had time to decode this table yet...
 */
class DirectedChannelChangeSelectionCodeTable : public PSIPTable
{
  public:
    DirectedChannelChangeSelectionCodeTable(const PSIPTable& table)
        : PSIPTable(table)
    {
        assert(TableID::DCCSCT == TableID());
    }
};

#endif // _ATSC_TABLES_H_
