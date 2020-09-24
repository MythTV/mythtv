// -*- Mode: c++ -*-
#ifndef PREMIERE_TABLES_H
#define PREMIERE_TABLES_H

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

    static uint EventCount(void)
        { return 1; }

    // descriptor length        x   9.0
    const unsigned char* Descriptors() const
        { return &psipdata()[9]; }

    static bool IsEIT(uint table_id);

  private:
    mutable std::vector<const unsigned char*> m_ptrs; // used to parse
};

class PremiereContentPresentationTable : public PSIPTable
{
  public:
    explicit PremiereContentPresentationTable(const PSIPTable& table) : PSIPTable(table)
    {
    }
};
#endif // PREMIERE_TABLES_H
