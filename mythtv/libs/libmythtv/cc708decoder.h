// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef CC708DECODER_H_
#define CC708DECODER_H_

#include <stdint.h>
#include <time.h>

#include "format.h"
#include "compat.h"

#ifndef __CC_CALLBACKS_H__
/** EIA-708-A closed caption packet */
typedef struct CaptionPacket
{
    unsigned char data[128+16];
    int size;
} CaptionPacket;
#endif

class CC708Reader;

class CC708Decoder
{
  public:
    CC708Decoder(CC708Reader *ccr) : reader(ccr)
    {
        memset(&partialPacket, 0, sizeof(CaptionPacket));
        memset(last_seen,      0, sizeof(last_seen));
    }
   ~CC708Decoder() {}

    void decode_cc_data(uint cc_type, uint data1, uint data2);

    /// \return Services seen in last few seconds as specified.
    void services(uint seconds, bool[64]) const;

  private:
    CaptionPacket  partialPacket;
    CC708Reader   *reader;
    time_t         last_seen[64];
};

#endif // CC708DECODER_H_
