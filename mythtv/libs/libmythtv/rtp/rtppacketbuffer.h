/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _RTP_PACKET_BUFFER_H_
#define _RTP_PACKET_BUFFER_H_

#include <QMap>

#include "rtpdatapacket.h"
#include "packetbuffer.h"

class RTPPacketBuffer : public PacketBuffer
{
  public:
    RTPPacketBuffer(unsigned int bitrate) :
        PacketBuffer(bitrate),
        m_large_sequence_number_seen_recently(0),
        m_current_sequence(0ULL)
    {
    }

    /// Adds RFC 3550 RTP data packet
    virtual void PushDataPacket(const UDPPacket&);

    /// Adds SMPTE 2022 Forward Error Correction Stream packet
    virtual void PushFECPacket(const UDPPacket&, unsigned int fec_stream_num);

  private:
    int m_large_sequence_number_seen_recently;
    uint64_t m_current_sequence;

    /// The key is the RTP sequence number + sequence if applicable
    QMap<uint64_t, RTPDataPacket> m_unordered_packets;
};

#endif // _RTP_PACKET_BUFFER_H_
