/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef RTP_PACKET_BUFFER_H
#define RTP_PACKET_BUFFER_H

#include <QMap>

#include "rtpdatapacket.h"
#include "packetbuffer.h"

class RTPPacketBuffer : public PacketBuffer
{
  public:
    explicit RTPPacketBuffer(unsigned int bitrate) :
        PacketBuffer(bitrate) {}

    /// Adds RFC 3550 RTP data packet
    void PushDataPacket(const UDPPacket &udp_packet) override; // PacketBuffer

    /// Adds SMPTE 2022 Forward Error Correction Stream packet
    void PushFECPacket(const UDPPacket &packet, unsigned int fec_stream_num) override; // PacketBuffer

  private:
    int      m_largeSequenceNumberSeenRecently { 0   };
    uint64_t m_currentSequence                 { 0LL };

    /// The key is the RTP sequence number + sequence if applicable
    QMap<uint64_t, RTPDataPacket> m_unorderedPackets;
};

#endif // RTP_PACKET_BUFFER_H
