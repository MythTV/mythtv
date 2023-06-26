/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#include <algorithm>

#include "rtppacketbuffer.h"
#include "rtpdatapacket.h"
#include "rtpfecpacket.h"

void RTPPacketBuffer::PushDataPacket(const UDPPacket &udp_packet)
{
    RTPDataPacket packet(udp_packet);

    uint key = packet.GetSequenceNumber();

    bool large_was_seen_recently = m_largeSequenceNumberSeenRecently > 0;
    m_largeSequenceNumberSeenRecently = 
        (key > (1U<<15)) ? 500 : m_largeSequenceNumberSeenRecently - 1;
    m_largeSequenceNumberSeenRecently =
        std::max(m_largeSequenceNumberSeenRecently, 0);

    if (m_largeSequenceNumberSeenRecently > 0)
    {
        if (key < 500) 
            key += 1ULL<<16;
    }
    else if (large_was_seen_recently)
    {
        m_currentSequence += 1ULL<<16;
    }

    key += m_currentSequence;

/*
    LOG(VB_RECORD, LOG_DEBUG, QString("Pushing %1 as %2 (lr %3)")
        .arg(packet.GetSequenceNumber()).arg(key)
        .arg(m_largeSequenceNumberSeenRecently));
*/

    m_unorderedPackets[key] = packet;

    // TODO pushing packets onto the ordered list should be based on
    // the bitrate and the M+N of the FEC.. but for now...
    const int kHighWaterMark = 500;
    const int kLowWaterMark  = 100;
    if (m_unorderedPackets.size() > kHighWaterMark)
    {
        while (m_unorderedPackets.size() > kLowWaterMark)
        {
            QMap<uint64_t, RTPDataPacket>::iterator it =
                m_unorderedPackets.begin();
/*
            LOG(VB_RECORD, LOG_DEBUG, QString("Popping %1 as %2")
                .arg((*it).GetSequenceNumber()).arg(it.key()));
*/
            m_available_packets.push_back(*it);
            m_unorderedPackets.erase(it);
        }
    }
}

void RTPPacketBuffer::PushFECPacket(
    const UDPPacket &packet,
    [[maybe_unused]] uint fec_stream_num)
{
    // TODO IMPLEMENT
    // for now just free the packet for immediate reuse
    FreePacket(packet);
}
