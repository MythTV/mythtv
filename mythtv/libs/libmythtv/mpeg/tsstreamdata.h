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
    ~TSStreamData() override { ; }

    bool ProcessTSPacket(const TSPacket& tspacket) override; // MPEGStreamData

    using MPEGStreamData::Reset;
    void Reset(int /* desiredProgram */) override { ; } // MPEGStreamData
    bool HandleTables(uint /* pid */, const PSIPTable & /* psip */) override // MPEGStreamData
        { return true; }
};

#endif
