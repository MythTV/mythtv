/* -*- Mode: c++ -*-
 * PacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include <cstdint>

// MythTV headers
#include "libmythbase/mythrandom.h"
#include "packetbuffer.h"

PacketBuffer::PacketBuffer(unsigned int bitrate) :
    m_bitrate(bitrate),
    m_next_empty_packet_key(static_cast<uint64_t>(MythRandom()) << 32)
{
}

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

    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    UDPPacket packet(*it);
    m_empty_packets.erase(it);

    return packet;
}

void PacketBuffer::FreePacket(const UDPPacket &packet)
{
    static constexpr uint64_t k_mask_upper_32 = ~((UINT64_C(1) << (64 - 32)) - 1);
    uint64_t top = packet.GetKey() & k_mask_upper_32;
    if (top == (m_next_empty_packet_key & k_mask_upper_32))
        m_empty_packets[packet.GetKey()] = packet;
}
