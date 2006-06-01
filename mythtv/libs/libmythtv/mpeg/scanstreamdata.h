// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef SCANSTREAMDATA_H_
#define SCANSTREAMDATA_H_

#include "atscstreamdata.h"
#include "dvbstreamdata.h"

class ScanStreamData :
    public virtual MPEGStreamData,
    public ATSCStreamData,
    public DVBStreamData
{
  public:
    ScanStreamData();
    virtual ~ScanStreamData();

    bool IsRedundant(uint pid, const PSIPTable&) const;
    bool HandleTables(uint pid, const PSIPTable &psip);

    void Reset(void);

    bool HasEITPIDChanges(const uint_vec_t& /*in_use_pids*/) const
        { return false; }
    bool GetEITPIDChanges(const uint_vec_t& /*in_use_pids*/,
                          uint_vec_t& /*add_pids*/,
                          uint_vec_t& /*del_pids*/) const { return false; }
};

#endif // SCANSTREAMDATA_H_
