/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include "udppacket.h"

#ifndef RTP_FEC_PACKET_H
#define RTP_FEC_PACKET_H

/** \brief RTP FEC Packet
 */
class RTPFECPacket : public UDPPacket
{
  public:
    explicit RTPFECPacket(const UDPPacket &o) : UDPPacket(o) { }
    explicit RTPFECPacket(uint64_t key) : UDPPacket(key) { }
    RTPFECPacket(void) : UDPPacket(0ULL) { }

    // TODO
};

#endif // RTP_FEC_PACKET_H
