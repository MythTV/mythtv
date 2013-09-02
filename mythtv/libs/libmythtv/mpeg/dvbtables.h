// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson
#ifndef _DVB_TABLES_H_
#define _DVB_TABLES_H_

#include <QString>
#include <stdint.h>  // uint32_t

#include "dvbdescriptors.h"
#include "mpegtables.h"
#include "mythtvexp.h"

MTV_PUBLIC QDateTime dvbdate2qt(const unsigned char*);
MTV_PUBLIC time_t dvbdate2unix(const unsigned char*);
uint32_t dvbdate2key(const unsigned char*);

/** \class NetworkInformationTable
 *  \brief This table tells the decoder on which PIDs to find other tables.
 *  \todo This is just a stub.
 */
class MTV_PUBLIC NetworkInformationTable : public PSIPTable
{
  public:
    NetworkInformationTable(const NetworkInformationTable& table)
        : PSIPTable(table), _cached_network_name(QString::null)
    {
        assert(TableID::NIT == TableID() || TableID::NITo == TableID());
        Parse();
    }
    NetworkInformationTable(const PSIPTable& table)
        : PSIPTable(table), _cached_network_name(QString::null)
    {
        assert(TableID::NIT == TableID() || TableID::NITo == TableID());
        Parse();
    }
    ~NetworkInformationTable() { ; }

    // table_id                 8   0.0       0x40/0x41
    // section_syntax_indicator 1   1.0          1
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00

    /// network_id             16   3.0     0x0000
    uint NetworkID(void) const { return TableIDExtension(); }

    // reserved_future_use      4   8.0        0xf
    /// network_desc_length    12   8.4          0
    uint NetworkDescriptorsLength(void) const
        { return ((psipdata()[0]<<8) | psipdata()[1]) & 0xfff; }

    /// for(i=0; i<N; i++)      x  10.0
    ///   { descriptor() }
    const unsigned char* NetworkDescriptors(void) const
        { return psipdata() + 2; }

    // reserved_future_use      4  0.0+ndl     0xf
    /// trans_stream_loop_len  12  0.4+ndl
    uint TransportStreamDataLength(void) const
        { return ((_tsc_ptr[0]<<8) | _tsc_ptr[1]) & 0xfff; }
    uint TransportStreamCount(void) const
        { return _ptrs.size() - 1; }
    // for(i=0; i<N; i++) {
    ///  transport_stream_id   16  0.0+p
    uint TSID(uint i) const { return (_ptrs[i][0]<<8) | _ptrs[i][1]; }
    ///  original_network_id   16  2.0+p
    uint OriginalNetworkID(uint i) const
        { return (_ptrs[i][2]<<8) | _ptrs[i][3]; }
    //   reserved_future_use    4  4.0+p
    ///  trans_desc_length     12  4.4+p
    uint TransportDescriptorsLength(uint i) const
        { return ((_ptrs[i][4]<<8) | _ptrs[i][5]) & 0xfff; }
    ///    for(j=0;j<N;j++)     x  6.0+p
    ///      { descriptor() }
    const unsigned char* TransportDescriptors(uint i) const
        { return _ptrs[i]+6; }
    // }

    /// mutates a NITo into a NITa (vice versa) and recalculates the CRC
    bool Mutate(void);

    void Parse(void) const;
    QString toString(void) const;
    QString NetworkName(void) const;

  private:
    mutable QString _cached_network_name;
    mutable const unsigned char* _tsc_ptr;
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

/** \class ServiceDescriptionTable
 *  \brief This table tells the decoder on which PIDs to find A/V data.
 *  \todo This is just a stub.
 */
class MTV_PUBLIC ServiceDescriptionTable : public PSIPTable
{
  public:
    ServiceDescriptionTable(const ServiceDescriptionTable& table)
        : PSIPTable(table)
    {
        assert(TableID::SDT == TableID() || TableID::SDTo == TableID());
        Parse();
    }
    ServiceDescriptionTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::SDT == TableID() || TableID::SDTo == TableID());
        Parse();
    }
    ~ServiceDescriptionTable() { ; }

    // table_id                 8   0.0       0x42/0x46
    // section_syntax_indicator 1   1.0          1
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00

    /// transport_stream_id    16   3.0     0x0000
    uint TSID() const { return TableIDExtension(); }

    /// original_network_id    16   8.0
    uint OriginalNetworkID() const
        { return (psipdata()[0]<<8) | psipdata()[1]; }

    /// Number of services
    uint ServiceCount() const { return _ptrs.size()-1; }

    // reserved_future_use      8  10.0
    // for (i=0;i<N;i++) {
    ///  service_id            16  0.0+p
    uint ServiceID(uint i) const { return (_ptrs[i][0]<<8) | (_ptrs[i][1]); }
    //   reserved_future_use    6  2.0+p
    //   EIT_schedule_flag      1  2.6+p
    bool HasEITSchedule(uint i) const { return bool(_ptrs[i][2] & 0x2); }
    //   EIT_present_following  1  2.7+p
    bool HasEITPresentFollowing(uint i) const
        { return bool(_ptrs[i][2] & 0x1); }
    ///   running_status        3  3.0+p
    uint RunningStatus(uint i) const { return (_ptrs[i][3] & 0xE0) >> 5; }
    ///  free_CA_mode           1  3.3+p
    bool IsEncrypted(uint i) const { return bool(_ptrs[i][3] & 0x10); }
    ///  desc_loop_length      12  3.4+p
    uint ServiceDescriptorsLength(uint i) const
        { return ((_ptrs[i][3]<<8) | (_ptrs[i][4])) & 0xfff; }
    ///  for (j=0;j<N;j++)      x  5.0+p
    ///    { descriptor() }
    const unsigned char* ServiceDescriptors(uint i) const
        { return _ptrs[i]+5; }
    // }
    ServiceDescriptor *GetServiceDescriptor(uint i) const;

    /// mutates a SDTo into a SDTa (vice versa) and recalculates the CRC
    bool Mutate(void);

    void Parse(void) const;
    QString toString(void) const;

  private:
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

/** \class BouquetAssociationTable
 *  \brief Tells what channels can be found on each transponder
 *  for one bouquet (a bunch of channels from one provider)
 *  Note: French Provider Canal+ uses this to associate channels
 *  with an index used in their proprietary EIT
 */

class MTV_PUBLIC BouquetAssociationTable : public PSIPTable
{
  public:
    BouquetAssociationTable(const PSIPTable& table) : PSIPTable(table)
    {
       assert(TableID::BAT == TableID());
       Parse();
    }
    ~BouquetAssociationTable() { ; }

    // table_id                 8   0.0       0x4a
    // section_syntax_indicator 1   1.0          1
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4          0
    // Bouquet ID              16   3.0          0
    uint BouquetID() const { return TableIDExtension(); }

    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    // reserved                 4   8         0x0d
    uint BouquetDescriptorsLength(void) const
        { return ((psipdata()[0]<<8) | psipdata()[1]) & 0xfff; }

    // Bouquet descriptors len 12   8.4
    // for (i=0;i<N;i++)
    // Descriptor();
    const unsigned char* BouquetDescriptors(void) const
        { return psipdata() + 2; }
    // reserved                4 10+N.0

    // Transport stream loop len 12
    uint TransportStreamDataLength(void) const
        { return ((_tsc_ptr[0]<<8) | _tsc_ptr[1]) & 0xfff; }
    uint TransportStreamCount(void) const
        { return _ptrs.size() - 1; }

    // for (i=0;i<N;i++) {
    //   transport_stream_id    16
    uint TSID(uint i) const { return (_ptrs[i][0] << 8) | _ptrs[i][1]; }
    //   original_network_id    16
    uint OriginalNetworkID(uint i) const
        { return (_ptrs[i][2] << 8) | _ptrs[i][3]; }
    //   reserved                4
    //   transport descriptor len 12
    uint TransportDescriptorsLength(uint i) const
        { return ((_ptrs[i][4]<<8) | _ptrs[i][5]) & 0xfff; }
    ///    for(j=0;j<N;j++)     x  6.0+p
    ///      { descriptor() }
    const unsigned char* TransportDescriptors(uint i) const
        { return _ptrs[i]+6; }
    // }

    void Parse(void) const;
    QString toString(void) const;

  private:
    mutable const unsigned char* _tsc_ptr;
    mutable vector<const unsigned char*> _ptrs;
};

class MTV_PUBLIC DiscontinuityInformationTable : public PSIPTable
{
    DiscontinuityInformationTable(const PSIPTable& table) : PSIPTable(table)
        { ; }
    // table_id 8
    // section_syntax_indicator 1
    // reserved_future_use      1
    // reserved                 2
    // section_length          12
    // transition_flag          1
    // reserved_future_use      7
};

class MTV_PUBLIC SelectionInformationTable : public PSIPTable
{
    SelectionInformationTable(const PSIPTable& table) : PSIPTable(table)
        { ; }
    // table_id 8
    // section_syntax_indicator 1
    // DVB_reserved_future_use  1
    // ISO_reserved             2
    // section_length          12
    // DVB_reserved_future_use 16
    // ISO_reserved             2
    // version_number           5
    // current_next_indicator   1
    // section_number           8
    // last_section_number      8
    // DVB_reserved_for_future_use 4
    // transmission_info_loop_length 12
    // for (i =0;i<N;i++) { descriptor() }
    // for (i=0;i<N;i++)
    // {
    //   service_id 16
    //   DVB_reserved_future_use  1
    //   running_status           3
    //   service_loop_length     12
    //   for(j=0;j<N;j++) { descriptor() }
    // }
    // CRC_32 32 rpchof
};

class MTV_PUBLIC DVBEventInformationTable : public PSIPTable
{
  public:
    DVBEventInformationTable(const PSIPTable& table) : PSIPTable(table)
    {
    // table_id                 8   0.0       0xC7
        assert(IsEIT(TableID()));
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4
    // reserved                 2   5.0          3
    // version_number           5   5.2
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0
    // last_section_number      8   7.0
        Parse();
    }

    // service_id              16   3.0
    uint ServiceID(void) const { return TableIDExtension(); }

    // transport_stream_id     16   8.0
    uint TSID(void) const
        { return (psipdata()[0]<<8) | psipdata()[1]; }

    // original_network_id     16  10.0
    uint OriginalNetworkID(void) const
        { return (psipdata()[2]<<8) | psipdata()[3]; }

    // segment_last_section_num 8  12.0
    uint SegmentLastSectionNumber(void) const
        { return psipdata()[4]; }
    // last_table_id            8  13.0
    uint LastTableID(void) const
        { return psipdata()[5]; }

    uint EventCount() const { return _ptrs.size()-1; }

    // for(i=0;i<N;i++) {
    //   event_id              16   0.0+x
    uint EventID(uint i) const
        { return (_ptrs[i][0]<<8) | _ptrs[i][1]; }
    //   start_time            40   2.0+x
    const unsigned char *StartTime(uint i) const
        { return _ptrs[i]+2; }
    QDateTime StartTimeUTC(uint i) const
        { return dvbdate2qt(StartTime(i)); }
    time_t StartTimeUnixUTC(uint i) const
        { return dvbdate2unix(StartTime(i)); }
    time_t EndTimeUnixUTC(uint i) const
        { return StartTimeUnixUTC(i) + DurationInSeconds(i); }
    uint32_t StartTimeKey(uint i) const
        { return dvbdate2key(StartTime(i)); }
    //   duration              24   7.0+x
    const unsigned char *Duration(uint i) const
        { return _ptrs[i]+7; }
    uint DurationInSeconds(uint i) const
    {
        return ((byteBCD2int(Duration(i)[0]) * 3600) +
                (byteBCD2int(Duration(i)[1]) * 60) +
                (byteBCD2int(Duration(i)[2])));
    }
    //   running_status         3  10.0+x
    uint RunningStatus(uint i) const { return _ptrs[i][10] >> 5; }
    //   free_CA_mode           1  10.3+x
    bool IsScrambled(uint i) const { return bool(_ptrs[i][10] & 0x10); }
    //   descriptors_loop_len  12  10.4+x
    uint DescriptorsLength(uint i) const
        { return ((_ptrs[i][10]<<8) | (_ptrs[i][11])) & 0xfff; }
    //   for(i=0;i<N;i++)       y  12.0+x
    //     { descriptor() }
    const unsigned char* Descriptors(uint i) const
        { return _ptrs[i] + 12; }
    //   }
    //CRC_32 32 rpchof

    void Parse(void) const;

    static bool IsEIT(uint table_id);

  private:
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

/** \class TimeDateTable
 *  \brief This table gives the current DVB stream time
 */
class MTV_PUBLIC TimeDateTable : public PSIPTable
{
  public:
    TimeDateTable(const PSIPTable& table)
        : PSIPTable(table)
    {
        assert(TableID::TDT == TableID());
    }
    ~TimeDateTable() { ; }

    // table_id                 8   0.0       0x70
    // section_syntax_indicator 1   1.0          0
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4         40
    // UTC_time                40   3.0          0
    const unsigned char *UTCdata(void) const
        { return pesdata() + 3; }

    QDateTime UTC(void)  const { return dvbdate2qt(UTCdata());   }
    time_t UTCUnix(void) const { return dvbdate2unix(UTCdata()); }
};

#endif // _DVB_TABLES_H_
