/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include <algorithm>
using namespace std;

#include "rtppacketbuffer.h"
#include "rtpdatapacket.h"
#include "rtpfecpacket.h"

RTPPacketBuffer::RTPPacketBuffer(unsigned int bitrate) :
    m_bitrate(bitrate),
    m_next_empty_packet_key(0ULL),
    m_large_sequence_number_seen_recently(0),
    m_current_sequence(0ULL)
{
    while (!m_next_empty_packet_key)
    {
        m_next_empty_packet_key =
            (random() << 24) ^ (random() << 16) ^
            (random() << 8) ^ random();
    }
}

void RTPPacketBuffer::PushDataPacket(const RTPDataPacket &packet)
{
    uint key = packet.GetSequenceNumber();

    bool large_was_seen_recently = m_large_sequence_number_seen_recently > 0;
    m_large_sequence_number_seen_recently = 
        (key > (1U>>31)) ? 500 : m_large_sequence_number_seen_recently - 1;
    m_large_sequence_number_seen_recently =
        max(m_large_sequence_number_seen_recently, 0);

    if (m_large_sequence_number_seen_recently > 0)
    {
        if (key < (1U>>20)) 
            key += 1ULL<<32;
    }
    else if (large_was_seen_recently)
    {
        m_current_sequence += 1ULL<<32;
    }

    key += m_current_sequence;

    m_unordered_packets[key] = packet;

    // TODO pushing packets onto the ordered list should be based on
    // the bitrate and the M+N of the FEC.. but for now...
    const int kHighWaterMark = 500;
    const int kLowWaterMark  = 100;
    if (m_unordered_packets.size() > kHighWaterMark)
    {
        while (m_unordered_packets.size() > kLowWaterMark)
        {
            QMap<uint64_t, RTPDataPacket>::iterator it = m_unordered_packets.begin();
            m_available_packets.push_back(*it);
            m_unordered_packets.erase(it);
        }
    }
}

void RTPPacketBuffer::PushFECPacket(const RTPFECPacket &packet, uint fec_stream_num)
{
    (void) fec_stream_num;
    // TODO IMPLEMENT
    // for now just free the packet for immediate reuse
    FreePacket(packet);
}

bool RTPPacketBuffer::HasAvailablePacket(void) const
{
    return !m_available_packets.empty();
}

RTPDataPacket RTPPacketBuffer::PopDataPacket(void)
{
    if (m_available_packets.empty())
        return RTPDataPacket(0);

    RTPDataPacket packet(m_available_packets.front());
    m_available_packets.pop_front();

    return packet;
}

UDPPacket RTPPacketBuffer::GetEmptyPacket(void)
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

void RTPPacketBuffer::FreePacket(const UDPPacket &packet)
{
    uint64_t top = packet.GetKey() & (0xFFFFFFFFULL<<32);
    if (top == (m_next_empty_packet_key & (0xFFFFFFFFULL<<32)))
        m_empty_packets[packet.GetKey()] = packet;
}
