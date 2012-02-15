/* -*- Mode: c++ -*-
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

/** \brief RTP Fixed Header for all packets.
 *
 *  The RTP Header exists for all RTP packets, it contains a payload
 *  type, timestamp and a sequence number for packet reordering.
 */
class RTPFixedHeader
{
  public:
    RTPFixedHeader(const char *data, uint sz) :
        m_data(data)
    {
        // TODO verify packet is big enough to contain the data
        m_valid = 2 == GetVersion();
    }

    bool IsValid(void) const { return m_valid; }
    uint GetVersion(void) const { return (m_data[0] >> 6); }
    uint GetExtension(void) const { return (m_data[0] >> 4) & 0x1; }
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
        return 0; // TODO -- return data and ensure proper byte order.
    }

    uint GetTimeStamp(void) const
    {
        return 0; // TODO -- return data and ensure proper byte order.
    }

    uint GetSynchronizationSource(void) const
    {
        return 0; // TODO -- return data and ensure proper byte order.
    }

    uint GetContributingSource(uint i) const
    {
        return 0; // TODO -- return data and ensure proper byte order.
    }

  private:
    unsigned char *m_data;
    bool m_valid;
};
