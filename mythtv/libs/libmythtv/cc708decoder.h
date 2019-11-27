// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef CC708DECODER_H_
#define CC708DECODER_H_

#include <cstdint>
#include <ctime>

#include "format.h"
#include "compat.h"

#ifndef __CC_CALLBACKS_H__
/** EIA-708-A closed caption packet */
struct CaptionPacket
{
    unsigned char data[128+16];
    int size;
};
#endif

class CC708Reader;

class CC708Decoder
{
  public:
    explicit CC708Decoder(CC708Reader *ccr) : m_reader(ccr)
    {
        memset(&m_partialPacket, 0, sizeof(CaptionPacket));
        memset(m_lastSeen,       0, sizeof(m_lastSeen));
    }
   ~CC708Decoder() = default;

    void decode_cc_data(uint cc_type, uint data1, uint data2);
    void decode_cc_null(void);

    /// \return Services seen in last few seconds as specified.
    void services(uint seconds, bool[64]) const;

  private:
    CaptionPacket  m_partialPacket;
    CC708Reader   *m_reader {nullptr};
    time_t         m_lastSeen[64];
};

#endif // CC708DECODER_H_
