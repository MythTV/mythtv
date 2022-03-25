/* -*- Mode: c++ -*-
 * PacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include <cstdint>

// MythTV headers
#include "packetbuffer.h"
#include "mythrandom.h"

PacketBuffer::PacketBuffer(unsigned int bitrate) :
    m_bitrate(bitrate),
#if QT_VERSION < QT_VERSION_CHECK(5,10,0)
    m_next_empty_packet_key(0ULL)
{
    while (!m_next_empty_packet_key)
    {
        m_next_empty_packet_key =
            (MythRandom() << 24) ^ (MythRandom() << 16) ^
            (MythRandom() << 8) ^ MythRandom();
    }
}
#else
    m_next_empty_packet_key(MythRandom64())
{
}
#endif

bool PacketBuffer::HasAvailablePacket(void) const
{
    return !m_available_packets.empty();
}

UDPPacket PacketBuffer::PopDataPacket(void)
{
    if (m_available_packets.empty())
        return UDPPacket(0);

    UDPPacket packet(m_available_packets.front());
    m_available_packets.pop_front();

    return packet;
}

UDPPacket PacketBuffer::GetEmptyPacket(void)
{
    QMap<uint64_t, UDPPacket>::iterator it = m_empty_packets.begin();
    if (it == m_empty_packets.end())
    {
        return UDPPacket(m_next_empty_packet_key++);
    }

    UDPPacket packet(*it);
    m_empty_packets.erase(it);

    return packet;
}

void PacketBuffer::FreePacket(const UDPPacket &packet)
{
    uint64_t top = packet.GetKey() & (0xFFFFFFFFULL<<32);
    if (top == (m_next_empty_packet_key & (0xFFFFFFFFULL<<32)))
        m_empty_packets[packet.GetKey()] = packet;
}
