/* -*- Mode: c++ -*-
 * PacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include <QList>
#include <QMap>

#include "udppacket.h"

class PacketBuffer
{
  public:
    explicit PacketBuffer(unsigned int bitrate);
    virtual ~PacketBuffer() = default;

    virtual void PushDataPacket(const UDPPacket&) = 0;

    virtual void PushFECPacket(const UDPPacket&, unsigned int) = 0;

    /// \brief Returns true if there are ordered packets ready for processing.
    bool HasAvailablePacket(void) const;

    /// \brief Fetches a data packet for processing.
    /// Call FreePacket() when done with the packet.
    UDPPacket PopDataPacket(void);

    /// Gets a packet for use in PushDataPacket/PushFECPacket.
    UDPPacket GetEmptyPacket(void);

    /** \brief Frees an RTPDataPacket returned by PopDataPacket.
     *  \note Since we return UDPPackets and not pointers to packets
     *        we don't leak if this isn't called, but by preventing
     *        the reference count on the QByteArrays from going to
     *        0 we can reuse the byte array and avoid mallocs.
     */
    void FreePacket(const UDPPacket &packet);

  protected:
    uint m_bitrate;

    /** @brief Packets key to use for next empty packet.
    The upper 32 bits are random and the lower 32 bits are incremented from 0.
    */
    uint64_t m_next_empty_packet_key;
    
    /// Packets ready for reuse
    QMap<uint64_t, UDPPacket> m_empty_packets;

    /// Ordered list of available packets
    QList<UDPPacket> m_available_packets;
};

#endif // PACKET_BUFFER_H
