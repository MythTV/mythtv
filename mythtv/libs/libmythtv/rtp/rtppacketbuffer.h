/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _RTP_PACKET_BUFFER_H_
#define _RTP_PACKET_BUFFER_H_

#include <QList>
#include <QMap>

#include "udppacket.h"

class RTPDataPacket;
class RTPFECPacket;

class RTPPacketBuffer
{
  public:
    RTPPacketBuffer(unsigned int bitrate);

    /// Adds RFC 3550 RTP data packet
    void PushDataPacket(const RTPDataPacket&);

    /// Adds SMPTE 2022 Forward Error Correction Stream packet
    void PushFECPacket(const RTPFECPacket&, unsigned int fec_stream_num);

    /// Return true if there are ordered packets ready for processing
    bool HasAvailablePacket(void) const;

    /// Fetches an RTP Data Packet for processing.
    /// Call FreePacket() when done with the packet.
    RTPDataPacket PopDataPacket(void);

    /// Gets a packet for use in PushDataPacket/PushFECPacket
    UDPPacket GetEmptyPacket(void);

    /// Frees an RTPDataPacket returned by PopDataPacket.
    void FreePacket(const UDPPacket &);

  private:
    uint m_bitrate;

    /// Packets key to use for next empty packet
    uint64_t m_next_empty_packet_key;
    
    /// Packets ready for reuse
    QMap<uint64_t, UDPPacket> m_empty_packets;

    /// Ordered list of available packets
    QList<RTPDataPacket> m_available_packets;

    int m_large_sequence_number_seen_recently;
    uint64_t m_current_sequence;

    /// The key is the RTP sequence number + sequence if applicable
    QMap<uint64_t, RTPDataPacket> m_unordered_packets;
};

#endif // _RTP_PACKET_BUFFER_H_
