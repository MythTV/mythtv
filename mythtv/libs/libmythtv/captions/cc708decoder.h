// -*- Mode: c++ -*-
// Copyright (c) 2003-2005, Daniel Kristjansson

#ifndef CC708DECODER_H_
#define CC708DECODER_H_

#include <cstdint>

#include "libmythbase/compat.h"
#include "libmythbase/mythchrono.h"

using cc708_seen_flags = std::array<bool,64>;
using cc708_seen_times = std::array<SystemTime,64>;

/** EIA-708-A closed caption packet */
struct CaptionPacket
{
    std::array<uint8_t,128+16> data {0};
    int size {0};
};

class CC708Reader;

class CC708Decoder
{
  public:
    explicit CC708Decoder(CC708Reader *ccr) : m_reader(ccr) {}
   ~CC708Decoder() = default;

    void decode_cc_data(uint cc_type, uint data1, uint data2);

    /// \return Services seen in last few seconds as specified.
    void services(std::chrono::seconds seconds, cc708_seen_flags & seen) const;

  private:
    CaptionPacket  m_partialPacket {};
    CC708Reader   *m_reader        {nullptr};
    cc708_seen_times m_lastSeen    {};
};

#endif // CC708DECODER_H_
