// -*- Mode: c++ -*-
// Copyright (c) 2015, Digital Nirvana
#ifndef TSSTREAMDATA_H_
#define TSSTREAMDATA_H_

#include "mpegstreamdata.h"

/* Specialized version of MPEGStreamData which is used to 'blindly'
 * record the entire MPTS transport from an input */

class MTV_PUBLIC TSStreamData : public MPEGStreamData
{
  public:
    explicit TSStreamData(int cardnum);
    virtual ~TSStreamData() { ; }

    virtual bool ProcessTSPacket(const TSPacket& tspacket);

    using MPEGStreamData::Reset;
    virtual void Reset(int /* desiredProgram */) { ; }
    virtual bool HandleTables(uint /* pid */, const PSIPTable & /* psip */) { return true; }
};

#endif
