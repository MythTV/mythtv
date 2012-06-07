/* -*- Mode: c++ -*-
 * UDPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _UDP_PACKET_BUFFER_H_
#define _UDP_PACKET_BUFFER_H_

#include "packetbuffer.h"

class UDPPacketBuffer : public PacketBuffer
{
  public:
    UDPPacketBuffer(unsigned int bitrate) : PacketBuffer(bitrate) {}

    /// Adds Raw UDP data packet
    virtual void PushDataPacket(const UDPPacket &packet)
    {
        m_available_packets.push_back(packet);
    }

    /// Frees the packet, there is no FEC used by Raw UDP
    virtual void PushFECPacket(const UDPPacket &packet, unsigned int)
    {
        FreePacket(packet);
    }
};

#endif // _UDP_PACKET_BUFFER_H_
