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
    m_nextEmptyPacketKey(static_cast<uint64_t>(MythRandom()) << 32)
{
}

bool PacketBuffer::HasAvailablePacket(void) const
{
    return !m_availablePackets.empty();
}

UDPPacket PacketBuffer::PopDataPacket(void)
{
    if (m_availablePackets.empty())
        return UDPPacket(0);

    UDPPacket packet(m_availablePackets.front());
    m_availablePackets.pop_front();

    return packet;
}

UDPPacket PacketBuffer::GetEmptyPacket(void)
{
    QMap<uint64_t, UDPPacket>::iterator it = m_emptyPackets.begin();
    if (it == m_emptyPackets.end())
    {
        return UDPPacket(m_nextEmptyPacketKey++);
    }

    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    UDPPacket packet(*it);
    m_emptyPackets.erase(it);

    return packet;
}

void PacketBuffer::FreePacket(const UDPPacket &packet)
{
    static constexpr uint64_t k_mask_upper_32 = ~((UINT64_C(1) << (64 - 32)) - 1);
    uint64_t top = packet.GetKey() & k_mask_upper_32;
    if (top == (m_nextEmptyPacketKey & k_mask_upper_32))
        m_emptyPackets[packet.GetKey()] = packet;
}
