// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson
#ifndef _DVB_TABLES_H_
#define _DVB_TABLES_H_

#include <qstring.h>
#include "mpegtables.h"
#include "dvbdescriptors.h"

/** \class NetworkInformationTable
 *  \brief This table tells the decoder on which PIDs to find other tables.
 *  \todo This is just a stub.
 */
class NetworkInformationTable : public PSIPTable
{
  public:
    NetworkInformationTable(const PSIPTable& table)
        : PSIPTable(table), _cached_network_name(QString::null)
    {
    // table_id                 8   0.0       0x40/0x41
        assert(TableID::NIT == TableID() || TableID::NITo == TableID());
        Parse();
    // section_syntax_indicator 1   1.0          1
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    }
    ~NetworkInformationTable() { ; }

    /// network_id             16   3.0     0x0000
    uint NetworkID() const { return TableIDExtension(); }

    // reserved_future_use      4   8.0        0xf
    /// network_desc_length    12   8.4          0
    uint NetworkDescriptorsLength() const
        { return ((pesdata()[8]<<8) | pesdata()[9]) & 0xfff; }

    /// for(i=0; i<N; i++)      x  10.0
    ///   { descriptor() }
    const unsigned char* NetworkDescriptors() const { return pesdata()+10; }

    // reserved_future_use      4  0.0+ndl     0xf
    /// trans_stream_loop_len  12  0.4+ndl
    uint TransportStreamCount() const
        { return ((_tsc_ptr[0]<<8) | _tsc_ptr[1]) & 0xfff; }
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

    void Parse(void) const;
    QString toString(void) const;
    QString NetworkName() const;

  private:
    mutable QString _cached_network_name;
    mutable const unsigned char* _tsc_ptr;
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

/** \class ServiceDescriptionTable
 *  \brief This table tells the decoder on which PIDs to find A/V data.
 *  \todo This is just a stub.
 */
class ServiceDescriptionTable: public PSIPTable
{
  public:
    ServiceDescriptionTable(const PSIPTable& table) : PSIPTable(table)
    {
    // table_id                 8   0.0       0x42/0x46
        assert(TableID::SDT == TableID() || TableID::SDTo == TableID());
        Parse();
    // section_syntax_indicator 1   1.0          1
    // reserved_future_use      1   1.1          1
    // reserved                 2   1.2          3
    // section_length          12   1.4          0
    // reserved                 2   5.0          3
    // version_number           5   5.2          0
    // current_next_indicator   1   5.7          1
    // section_number           8   6.0       0x00
    // last_section_number      8   7.0       0x00
    }
    ~ServiceDescriptionTable() { ; }

    /// transport_stream_id    16   3.0     0x0000
    uint TSID() const { return TableIDExtension(); }

    /// original_network_id    16   8.0
    // TODO check this
    uint OriginalNetworkID() const { return (pesdata()[3]<<8) | pesdata()[4]; }

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
    //   running_status         3  3.0+p
    ///  free_CA_mode           1  3.3+p
    bool HasFreeCA(uint i) const { return bool(_ptrs[i][3] & 0x10); }
    ///  desc_loop_length      12  3.4+p
    uint ServiceDescriptorsLength(uint i) const
        { return ((_ptrs[i][3]<<8) | (_ptrs[i][4])) & 0xfff; }
    ///  for (j=0;j<N;j++)      x  5.0+p
    ///    { descriptor() }
    const unsigned char* ServiceDescriptors(uint i) const
        { return _ptrs[i]+5; }
    // }
    ServiceDescriptor *GetServiceDescriptor(uint i) const;

    void Parse(void) const;
    QString toString(void) const;

  private:
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

class DiscontinuityInformationTable : public PSIPTable
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

class SelectionInformationTable : public PSIPTable
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

#endif // _DVB_TABLES_H_
