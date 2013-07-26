/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

#ifndef _RTP_DATA_PACKET_H_
#define _RTP_DATA_PACKET_H_

#include <arpa/inet.h> // for ntohs()/ntohl()

#include "udppacket.h"
#include "mythlogging.h"

#ifdef _MSC_VER
#  include <WinSock2.h>
#endif

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
class RTPDataPacket : public UDPPacket
{
  public:
    RTPDataPacket(const RTPDataPacket &o) : UDPPacket(o), m_off(o.m_off) { }
    RTPDataPacket(const UDPPacket &o) : UDPPacket(o), m_off(0) { }
    RTPDataPacket(uint64_t key) : UDPPacket(key), m_off(0) { }
    RTPDataPacket(void) : UDPPacket(0ULL), m_off(0) { }

    bool IsValid(void) const
    {
        if (m_data.size() < 12)
        {
            return false;
        }
        if (2 != GetVersion())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("Version incorrect %1")
                .arg(GetVersion()));
            return false;
        }
        if (HasPadding() && (m_data.size() < 1328))
        {
            LOG(VB_GENERAL, LOG_INFO, QString("HasPadding && %1 < 1328")
                .arg(m_data.size()));
            return false;
        }

        int off = 12 + 4 * GetCSRCCount();
        if (off > m_data.size())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("off %1 > sz %2")
                .arg(off).arg(m_data.size()));
            return false;
        }
        if (HasExtension())
        {
            uint ext_size = m_data[off+2] << 8 | m_data[off+3];
            off += 4 * (1 + ext_size);
        }
        if (off > m_data.size())
        {
            LOG(VB_GENERAL, LOG_INFO, QString("off + ext %1 > sz %2")
                .arg(off).arg(m_data.size()));
            return false;
        }
        m_off = off;

        return true;
    }

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
        return ntohs(*reinterpret_cast<const uint16_t*>(m_data.data()+2));
    }

    uint GetTimeStamp(void) const
    {
        return ntohl(*reinterpret_cast<const uint32_t*>(m_data.data()+4));
    }

    uint GetSynchronizationSource(void) const
    {
        return ntohl(*reinterpret_cast<const uint32_t*>(m_data.data()+8));
    }

    uint GetContributingSource(uint i) const
    {
        const uint32_t tmp =
            *reinterpret_cast<const uint32_t*>(m_data.data() + 12 + 4 * i);
        return ntohl(tmp);
    }

    uint GetPayloadOffset(void) const { return m_off; }

    uint GetPaddingSize(void) const
    {
        if (!HasPadding())
            return 0;
        return m_data[1328];
    }

  protected:
    mutable uint m_off;
};

#endif // _RTP_DATA_PACKET_H_
