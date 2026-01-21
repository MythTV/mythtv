/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef RTP_DATA_PACKET_H
#define RTP_DATA_PACKET_H

#include <limits> // workaround QTBUG-90395

#include <QtEndian>

#include "libmythtv/mythtvexp.h"
#include "udppacket.h"

/** \brief RTP Data Packet
 *
 *  The RTP Header exists for all RTP packets, it contains a payload
 *  type, timestamp and a sequence number for packet reordering.
 *
 *  Different RTP Data Packet types have their own sub-classes for
 *  accessing the data portion of the packet.
 *
 *  The data is stored in a QByteArray which is a reference counted
 *  shared data container, so an RTPDataPacket can be assigned to a
 *  subclass efficiently.
 */
class MTV_PUBLIC RTPDataPacket : public UDPPacket
{
  public:
    RTPDataPacket(const RTPDataPacket&)  = default;
    explicit RTPDataPacket(const UDPPacket &o) : UDPPacket(o), m_off(0) { }
    explicit RTPDataPacket(uint64_t key) : UDPPacket(key), m_off(0) { }
    RTPDataPacket(void) : UDPPacket(0ULL) { }

    RTPDataPacket& operator=(const RTPDataPacket&) = default;

    bool IsValid(void) const override; // UDPPacket

    uint GetVersion(void) const { return (m_data[0] >> 6) & 0x3; }
    bool HasPadding(void) const { return (m_data[0] >> 5) & 0x1; }
    bool HasExtension(void) const { return (m_data[0] >> 4) & 0x1; }
    uint GetCSRCCount(void) const { return m_data[0] & 0xf; }

    enum {
        kPayLoadTypePCMAudio   = 8,
        kPayLoadTypeMPEGAudio  = 12,
        kPayLoadTypeH261Video  = 31,
        kPayLoadTypeMPEG2Video = 32,
        kPayLoadTypeTS         = 33,
        kPayLoadTypeH263Video  = 34,
    };

    uint GetPayloadType(void) const
    {
        return m_data[1] & 0x7f;
    }

    uint GetSequenceNumber(void) const
    {
        return qFromBigEndian(*reinterpret_cast<const uint16_t*>(m_data.data()+2));
    }

    uint GetTimeStamp(void) const
    {
        return qFromBigEndian(*reinterpret_cast<const uint32_t*>(m_data.data()+4));
    }

    uint GetSynchronizationSource(void) const
    {
        return qFromBigEndian(*reinterpret_cast<const uint32_t*>(m_data.data()+8));
    }

    uint GetContributingSource(uint i) const
    {
        const uint32_t tmp =
            *reinterpret_cast<const uint32_t*>(m_data.data() + 12 + 4 * i);
        return qFromBigEndian(tmp);
    }

    uint GetPayloadOffset(void) const { return m_off; }

    uint GetPaddingSize(void) const
    {
        if (!HasPadding())
            return 0;
        return m_data[m_data.size()-1];
    }

  protected:
    mutable uint m_off { 0 };
};

#endif // RTP_DATA_PACKET_H
