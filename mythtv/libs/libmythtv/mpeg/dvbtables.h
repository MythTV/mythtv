// -*- Mode: c++ -*-
// Copyright (c) 2005, Daniel Thor Kristjansson
#ifndef _DVB_TABLES_H_
#define _DVB_TABLES_H_

#include <qstring.h>
#include "mpegtables.h"

/** \class NetworkInformationTable
 *  \brief This table tells the decoder on which PIDs to find other tables.
 *  \todo This is just a stub.
 */
class NetworkInformationTable : PSIPTable
{
  public:
    NetworkInformationTable(const PSIPTable& table) : PSIPTable(table)
    {
    // start_code_prefix        8   0.0          0
    // table_id                 8   1.0       0x40
        assert(TableID::NIT == TableID());
        Parse();
    // section_syntax_indicator 1   2.0          1
    // private_indicator        1   2.1          1
    // reserved                 2   2.2          3
    // table_id_extension      16   4.0     0x0000
    // reserved                 2   6.0          3
    // current_next_indicator   1   6.7          1
    // section_number           8   7.0       0x00
    // last_section_number      8   8.0       0x00
    }
    ~NetworkInformationTable() { ; }

    void Parse(void) const;
    QString toString(void) const;
  private:
    mutable vector<unsigned char*> _ptrs; // used to parse
};

/** \class ServiceDescriptionTable
 *  \brief This table tells the decoder on which PIDs to find A/V data.
 *  \todo This is just a stub.
 */
class ServiceDescriptionTable: PSIPTable
{
  public:
    ServiceDescriptionTable(const PSIPTable& table) : PSIPTable(table)
    {
    // start_code_prefix        8   0.0          0
    // table_id                 8   1.0       0x42
        assert(TableID::SDT == TableID());
        Parse();
    // section_syntax_indicator 1   2.0          1
    // private_indicator        1   2.1          1
    // reserved                 2   2.2          3
    // table_id_extension      16   4.0     0x0000
    // reserved                 2   6.0          3
    // current_next_indicator   1   6.7          1
    // section_number           8   7.0       0x00
    // last_section_number      8   8.0       0x00
    }
    ~ServiceDescriptionTable() { ; }

    void Parse(void) const;
    QString toString(void) const;
  private:
    mutable vector<unsigned char*> _ptrs; // used to parse
};

#endif // _DVB_TABLES_H_
