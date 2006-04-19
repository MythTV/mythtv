// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef SCANSTREAMDATA_H_
#define SCANSTREAMDATA_H_

#include "atscstreamdata.h"
#include "dvbstreamdata.h"

class ScanStreamData : public ATSCStreamData, public DVBStreamData
{
  public:
    ScanStreamData();
    virtual ~ScanStreamData();

    bool IsRedundant(uint pid, const PSIPTable&) const;
    bool HandleTables(uint pid, const PSIPTable &psip);

    void Reset(void);

    void ReturnCachedTable(const PSIPTable *psip) const;
    void ReturnCachedTables(pmt_vec_t&) const;
    void ReturnCachedTables(pmt_map_t&) const;
};

#endif // SCANSTREAMDATA_H_
