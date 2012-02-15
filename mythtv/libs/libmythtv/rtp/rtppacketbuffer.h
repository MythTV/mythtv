/* -*- Mode: c++ -*-
 * RTPPacketBuffer
 * Copyright (c) 2012 Digital Nirvana, Inc.
 * Distributed as part of MythTV under GPL v2 and later.
 */

class RTPPacketBuffer
{
    RTPPacketBuffer(uint bitrate);
    ~RTPPacketBuffer();

    /// Adds RFC 3550 RTP data packet
    void PushDataPacket(unsigned char *data, uint size);

    /// Adds SMPTE 2022 Forward Error Correction Stream packet
    void PushFECPacket(unsigned char *data, uint size, int fec_stream_num);

    /// Returns number of re-ordered packets ready for reading
    uint AvailablePackets(void) const;

    /// Fetches an RTP Data Packet for processing
    RTPDataPacket *PopDataPacket(void);

    /// Frees an RTPDataPacket returned by PopDataPacket.
    void FreeDataPacket(RTPDataPacket*);
};
