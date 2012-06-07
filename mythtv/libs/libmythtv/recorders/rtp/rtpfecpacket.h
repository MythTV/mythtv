/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include "udppacket.h"

#ifndef _RTP_FEC_PACKET_H_
#define _RTP_FEC_PACKET_H_

/** \brief RTP FEC Packet
 */
class RTPFECPacket : public UDPPacket
{
  public:
    RTPFECPacket(const UDPPacket &o) : UDPPacket(o) { }
    RTPFECPacket(uint64_t key) : UDPPacket(key) { }
    RTPFECPacket(void) : UDPPacket(0ULL) { }

    // TODO
};

#endif // _RTP_FEC_PACKET_H_
