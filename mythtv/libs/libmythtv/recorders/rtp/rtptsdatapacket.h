/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _RTP_TS_DATA_PACKET_H_
#define _RTP_TS_DATA_PACKET_H_

#include <algorithm>
using namespace std;

#include "rtpdatapacket.h"

/** \brief RTP Transport Stream Data Packet
 *
 */
class RTPTSDataPacket : public RTPDataPacket
{
  public:
    RTPTSDataPacket(const RTPDataPacket &o) : RTPDataPacket(o) { }
    RTPTSDataPacket(const UDPPacket &o) : RTPDataPacket(o) { }
    RTPTSDataPacket(uint64_t key) : RTPDataPacket(key) { }
    RTPTSDataPacket(void) : RTPDataPacket(0ULL) { }

    const unsigned char *GetTSData(void) const
    {
        return reinterpret_cast<const unsigned char*>(m_data.data()) + GetTSOffset();
    }

    unsigned int GetTSDataSize(void) const
    {
        return max(1328 - (int)GetTSOffset() - (int)GetPaddingSize(), 0);
    }

  private:
    uint GetTSOffset(void) const { return m_off; }
};

#endif // _RTP_TS_DATA_PACKET_H_
