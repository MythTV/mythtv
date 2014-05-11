//
//  rtcpdatapacket.h
//  MythTV
//
//  Created by Jean-Yves Avenard on 6/05/2014.
//  Copyright (c) 2014 Bubblestuff Pty Ltd. All rights reserved.
//

#ifndef MythTV_rtcpdatapacket_h
#define MythTV_rtcpdatapacket_h

#include <QHostAddress>
#include <QByteArray>
#include <QtEndian>

#include "udppacket.h"
#include "mythlogging.h"

#define RTP_VERSION 2
#define RTCP_RR     201
#define RTCP_SDES   202

/** \brief RTCP Data Packet
 *
 *  The RTCP Header exists for all RTP packets, it contains a payload
 *  type, timestamp and a sequence number for packet reordering.
 *
 *  Different RTP Data Packet types have their own sub-classes for
 *  accessing the data portion of the packet.
 *
 *  The data is stored in a QByteArray which is a reference counted
 *  shared data container, so an RTPDataPacket can be assigned to a
 *  subclass efficiently.
 */

class RTCPDataPacket : public UDPPacket
{
public:
    RTCPDataPacket(const RTCPDataPacket &o)
  : UDPPacket(o),
    m_timestamp(o.m_timestamp), m_last_timestamp(o.m_last_timestamp),
    m_sequence(o.m_sequence),   m_last_sequence(o.m_last_sequence),
    m_lost(o.m_lost),           m_ssrc(o.m_ssrc)
    { }

    RTCPDataPacket(uint32_t timestamp, uint32_t last_timestamp,
                   uint32_t sequence, uint32_t last_sequence,
                   uint32_t m_lost, uint32_t lost_interval,
                   uint32_t ssrc)
  : m_timestamp(timestamp),     m_last_timestamp(last_timestamp),
    m_sequence(sequence),       m_last_sequence(last_sequence),
    m_lost(m_lost),             m_lost_interval(lost_interval),
    m_ssrc(ssrc) { }

    QByteArray GetData(void) const
    {
        QByteArray buffer;

        if (m_sequence == 0)
        {
            // No packet received yet, send an empty RTPC RR packet
            uchar rtcp[10];

            rtcp[0] = RTP_VERSION << 6;         // RTP version
            rtcp[1] = RTCP_RR;                  // RTCP_RR
            qToBigEndian((qint16)1, &rtcp[2]);  // length in words - 1
            qToBigEndian((qint32)0, &rtcp[4]);  // our own SSRC
            buffer = QByteArray((char *)rtcp, 10);
        }
        else
        {
            static char *hostname = (char *)"MythTV";
            uint32_t len = strlen(hostname);
            uchar *rtcp = new uchar[46 + len + 1];

            rtcp[0] = (RTP_VERSION << 6) + 1;   // 1 report block
            rtcp[1] = RTCP_RR;                  // RTCP_RR)
            qToBigEndian((qint16)7, &rtcp[2]);  // length in words - 1
                                                // our own SSRC: we use the server's SSRC + 1 to avoid conflicts
            qToBigEndian((quint32)m_ssrc + 1, &rtcp[4]);
            qToBigEndian((quint32)m_ssrc, &rtcp[8]);
            // some placeholders we should really fill...
            // RFC 1889/p 27

//            extended_max          = stats->cycles + stats->max_seq;
//            expected              = extended_max - stats->base_seq;
//            lost                  = expected - stats->received;
//            lost                  = FFMIN(lost, 0xffffff); // clamp it since it's only 24 bits...
//            expected_interval     = expected - stats->expected_prior;
//            stats->expected_prior = expected;
//            received_interval     = stats->received - stats->received_prior;
//            stats->received_prior = stats->received;
//            lost_interval         = expected_interval - received_interval;
//            if (expected_interval == 0 || lost_interval <= 0)
//            fraction = 0;
//            else
//            fraction = (lost_interval << 8) / expected_interval;
//
//            fraction = (fraction << 24) | lost;
//            avio_wb32(pb, fraction); /* 8 bits of fraction, 24 bits of total packets lost */
//            avio_wb32(pb, extended_max); /* max sequence received */
//            avio_wb32(pb, stats->jitter >> 4); /* jitter */
//
//            if (s->last_rtcp_ntp_time == AV_NOPTS_VALUE) {
//                avio_wb32(pb, 0); /* last SR timestamp */
//                avio_wb32(pb, 0); /* delay since last SR */
//            } else {
//                uint32_t middle_32_bits   = s->last_rtcp_ntp_time >> 16; // this is valid, right? do we need to handle 64 bit values special?
//                uint32_t delay_since_last = av_rescale(av_gettime() - s->last_rtcp_reception_time,
//                                                       65536, AV_TIME_BASE);
//
//                avio_wb32(pb, middle_32_bits); /* last SR timestamp */
//                avio_wb32(pb, delay_since_last); /* delay since last SR */
//            }

            qToBigEndian((quint32) 0, &rtcp[12]); /* 8 bits of fraction, 24 bits of total packets lost */
            qToBigEndian((quint32) 0, &rtcp[16]); /* max sequence received */
            qToBigEndian((quint32) 0, &rtcp[20]); /* jitter */

            qToBigEndian((quint32) m_timestamp, &rtcp[24]); /* last SR timestamp */
            qToBigEndian((quint32) m_last_timestamp, &rtcp[28]); /* delay since last SR timestamp */

            // CNAME
            rtcp[32] = (RTP_VERSION << 6) + 1;  // 1 report block
            rtcp[33] = RTCP_SDES;               // RTCP_SDES)
            qToBigEndian((qint16)((7 + len + 3) / 4), &rtcp[34]); /* length in words - 1 */
            qToBigEndian((quint32)m_ssrc, &rtcp[36]);
            qToBigEndian((quint32)m_ssrc + 1, &rtcp[40]);

            buffer = QByteArray((char *)rtcp, 44);

            buffer.append((char)0x01); //44
            buffer.append((char)len);  //45
            buffer.append(hostname, len); //46
            buffer.append((char)0); // 46 + len   END

            // padding
            for (len = (7 + len) % 4; len % 4; len++)
            {
                buffer.append((char)0);
            }
            delete[] rtcp;
        }
        return buffer;
    }

    uint GetSequenceNumber(void) const
    {
        return m_sequence;
    }

protected:
    uint32_t m_timestamp, m_last_timestamp;
    uint32_t m_sequence, m_last_sequence, m_lost, m_lost_interval;
    uint32_t m_ssrc;
};
#endif
