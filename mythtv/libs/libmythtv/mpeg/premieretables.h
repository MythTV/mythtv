// -*- Mode: c++ -*-
#ifndef _PRIVATE_TABLES_H_
#define _PRIVATE_TABLES_H_

#include <QString>
#include <cstdint>  // uint32_t
#include "mpegtables.h"
#include "dvbdescriptors.h"

class PremiereContentInformationTable : public PSIPTable
{
  public:
    explicit PremiereContentInformationTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(IsEIT(TableID()));
    }

    // content_id              32   0.0
    uint ContentID(void) const
        { return ( psipdata()[0]<<24) | (psipdata()[1]<<16) |
                  (psipdata()[2]<< 8) |  psipdata()[3]; }

    // event_duration          24   4.0
    uint DurationInSeconds() const
        { return ((byteBCD2int(psipdata()[4]) * 3600) +
                  (byteBCD2int(psipdata()[5]) * 60) +
                  (byteBCD2int(psipdata()[6]))); }
    // descriptor length        8   8.0
    uint DescriptorsLength() const
        { return ((psipdata()[7] & 0x0F) << 8) | psipdata()[8]; }

    uint EventCount(void) const
        { return 1; }

    // descriptor length        x   9.0
    const unsigned char* Descriptors() const
        { return &psipdata()[9]; }

    static bool IsEIT(uint table_id);

  private:
    mutable vector<const unsigned char*> _ptrs; // used to parse
};

class PremiereContentPresentationTable : public PSIPTable
{
  public:
    explicit PremiereContentPresentationTable(const PSIPTable& table) : PSIPTable(table)
    {
    }
};
#endif // _PRIVATE_TABLES_H_
