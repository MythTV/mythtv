// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef PES_PACKET_H
#define PES_PACKET_H

/*
  max length of PSI table = 1024 bytes
  max length of private_section = 4096 bytes
*/

#include <vector>

using AspectArray = std::array<float,16>;

#include "libmythbase/mythlogging.h"

#include "tspacket.h"

MTV_PUBLIC unsigned char *pes_alloc(uint size);
MTV_PUBLIC void pes_free(unsigned char *ptr);

/** \class PESPacket
 *  \brief Allows us to transform TS packets to PES packets, which
 *         are used to hold multimedia streams and very similar to PSIP tables.
 *  \sa PSIPTable, TSPacket
 */
class MTV_PUBLIC PESPacket
{
  protected:
    /** noop constructor, only for use by derived classes */
    PESPacket() = default;

  public:
    // does not create it's own data
    explicit PESPacket(const unsigned char *pesdata)
        : m_pesData(const_cast<unsigned char*>(pesdata)),
          m_fullBuffer(const_cast<unsigned char*>(pesdata)),
          m_badPacket(!VerifyCRC())
    {
        m_pesDataSize = std::max(((int)Length())-1 + (PESPacket::HasCRC() ? 4 : 0), 0);
    }
    explicit PESPacket(const std::vector<uint8_t> &pesdata)
      : m_pesData(const_cast<unsigned char*>(pesdata.data())),
        m_fullBuffer(const_cast<unsigned char*>(pesdata.data())),
        m_badPacket(!VerifyCRC())
    {
        m_pesDataSize = std::max(((int)Length())-1 + (PESPacket::HasCRC() ? 4 : 0), 0);
    }

    // Deleted functions should be public.
    //const PESPacket& operator=(const PESPacket& pkt);
    PESPacket& operator=(const PESPacket& pkt) = delete;

    // may be modified
    PESPacket(const PESPacket& pkt)
        : m_psiOffset(pkt.m_psiOffset),
          m_ccLast(pkt.m_ccLast),
          m_pesDataSize(pkt.m_pesDataSize),
          m_allocSize(pkt.m_allocSize),
          m_badPacket(pkt.m_badPacket)
    { // clone
        if (!m_allocSize)
            m_allocSize = pkt.m_pesDataSize + (pkt.m_pesData - pkt.m_fullBuffer);

        m_fullBuffer = pes_alloc(m_allocSize);
        memcpy(m_fullBuffer, pkt.m_fullBuffer, m_allocSize);
        m_pesData = m_fullBuffer + (pkt.m_pesData - pkt.m_fullBuffer);
    }

    // At this point we should have the entire VCT table in buffer
    // at (buffer - 8), and without the tspacket 4 byte header

        //if (TableID::TVCT == table_id)
        //VirtualChannelTable vct;

    virtual ~PESPacket()
    {
        if (IsClone())
            pes_free(m_fullBuffer);

        m_fullBuffer = nullptr;
        m_pesData    = nullptr;
    }

    bool IsClone() const { return bool(m_allocSize); }

    // return true if complete or broken
    bool AddTSPacket(const TSPacket* tspacket, int cardid, bool &broken);

    bool IsGood() const { return !m_badPacket; }

    const TSHeader* tsheader() const
        { return reinterpret_cast<const TSHeader*>(m_fullBuffer); }
    TSHeader* tsheader()
        { return reinterpret_cast<TSHeader*>(m_fullBuffer); }

    void GetAsTSPackets(std::vector<TSPacket> &output, uint cc) const;

    // m_pesData[-3] == 0, m_pesData[-2] == 0, m_pesData[-1] == 1
    uint StreamID()   const { return m_pesData[0]; }
    uint Length()     const
        { return (m_pesData[1] & 0x0f) << 8 | m_pesData[2]; }
    // 2 bits "10"
    // 2 bits "PES_scrambling_control (0 not scrambled)
    uint ScramblingControl() const
        { return (m_pesData[3] & 0x30) >> 4; }
    /// 1 bit  Indicates if this is a high priority packet
    bool HighPriority()       const { return ((m_pesData[3] & 0x8) >> 3) != 0; }
    /// 1 bit  Data alignment indicator (must be 0 for video)
    bool DataAligned()        const { return ((m_pesData[3] & 0x4) >> 2) != 0; }
    /// 1 bit  If true packet may contain copy righted material and is
    ///        known to have once contained materiale with copy rights.
    ///        If false packet may contain copy righted material but is
    ///        not known to have ever contained materiale with copy rights.
    bool CopyRight()          const { return ((m_pesData[3] & 0x2) >> 1) != 0; }
    /// 1 bit  Original Recording
    bool OriginalRecording()  const { return (m_pesData[3] & 0x1) != 0; }

    /// 1 bit  Presentation Time Stamp field is present
    bool HasPTS()             const { return ((m_pesData[4] & 0x80) >> 7) != 0; }
    /// 1 bit  Decoding Time Stamp field is present
    bool HasDTS()             const { return ((m_pesData[4] & 0x40) >> 6) != 0; }
    /// 1 bit  Elementary Stream Clock Reference field is present
    bool HasESCR()            const { return ((m_pesData[4] & 0x20) >> 5) != 0; }
    /// 1 bit  Elementary Stream Rate field is present
    bool HasESR()             const { return ((m_pesData[4] & 0x10) >> 4) != 0; }
    /// 1 bit  DSM field present (should always be false for broadcasts)
    bool HasDSM()             const { return ((m_pesData[4] & 0x8) >> 3) != 0; }
    /// 1 bit  Additional Copy Info field is present
    bool HasACI()             const { return ((m_pesData[4] & 0x4) >> 2) != 0; }
    /// 1 bit  Cyclic Redundancy Check present
    virtual bool HasCRC()     const { return ((m_pesData[4] & 0x2) >> 1) != 0; }
    /// 1 bit  Extension flags are present
    bool HasExtensionFlags()  const { return (m_pesData[4] & 0x1) != 0; }

    /// Presentation Time Stamp, present if HasPTS() is true
    uint64_t PTS(void) const
    {
        int i = 6;
        return
            (uint64_t(m_pesData[i+0] & 0x0e) << 29) |
            (uint64_t(m_pesData[i+1]       ) << 22) |
            (uint64_t(m_pesData[i+2] & 0xfe) << 14) |
            (uint64_t(m_pesData[i+3]       ) <<  7) |
            (uint64_t(m_pesData[i+4] & 0xfe) >> 1);
    }
    /// Decode Time Stamp, present if HasDTS() is true
    uint64_t DTS(void) const
    {
        int i = 6+(HasPTS()?5:0);
        return
            (uint64_t(m_pesData[i+0] & 0x0e) << 29) |
            (uint64_t(m_pesData[i+1]       ) << 22) |
            (uint64_t(m_pesData[i+2] & 0xfe) << 14) |
            (uint64_t(m_pesData[i+3]       ) <<  7) |
            (uint64_t(m_pesData[i+4] & 0xfe) >> 1);
    }

    // 8 bits PES Header Length
    // variable length -- pes header fields
    // variable length -- pes data block

    uint TSSizeInBuffer()     const { return m_pesDataSize; }
    uint PSIOffset()          const { return m_psiOffset; }

    const unsigned char* pesdata() const { return m_pesData; }
    unsigned char* pesdata()             { return m_pesData; }

    const unsigned char* data() const { return m_fullBuffer; }
    unsigned char* data() { return m_fullBuffer; }

    void SetStreamID(uint id) { m_pesData[0] = id; }
    void SetLength(uint len)
    {
        m_pesData[1] = (m_pesData[1] & 0xf0) | ((len>>8) & 0x0f);
        m_pesData[2] = len & 0xff;
    }
    void SetTotalLength(uint len)
    {
        len += 4 /* for CRC */;
        len -= 3 /* for data before data last byte of length */;
        SetLength(len);
    }

    void SetPSIOffset(uint offset)
    {
        m_psiOffset = offset;
        m_pesData = m_fullBuffer + m_psiOffset + 1;
    }

    uint CRC(void) const
    {
        if (!HasCRC() || (Length() < 1))
            return kTheMagicNoCRCCRC;
        uint offset = Length() - 1;
        return ((m_pesData[offset+0]<<24) |
                (m_pesData[offset+1]<<16) |
                (m_pesData[offset+2]<<8) |
                (m_pesData[offset+3]));
    }

    void SetCRC(uint crc)
    {
        if (Length() < 1)
            return;

        uint offset = Length() - 1;
        m_pesData[offset+0] = (crc & 0xff000000) >> 24;
        m_pesData[offset+1] = (crc & 0x00ff0000) >> 16;
        m_pesData[offset+2] = (crc & 0x0000ff00) >> 8;
        m_pesData[offset+3] = (crc & 0x000000ff);
    }

    uint CalcCRC(void) const;
    bool VerifyCRC(void) const;
    bool VerifyCRC(int cardid, int pid) const;

  protected:
    void Finalize() { SetCRC(CalcCRC()); }

    unsigned char *m_pesData     { nullptr }; ///< Pointer to PES data in full buffer
    unsigned char *m_fullBuffer  { nullptr }; ///< Pointer to allocated data

    uint           m_psiOffset   {     0 }; ///< AFCOffset + StartOfFieldPointer
    uint           m_ccLast      {   255 }; ///< Continuity counter of last inserted TS Packet
    uint           m_pesDataSize {     0 }; ///< Number of data bytes (TS header + PES data)
    uint           m_allocSize   {     0 }; ///< Total number of bytes we allocated
    bool           m_badPacket   { false }; ///< true if a CRC is not good yet

    // FIXME re-read the specs and follow all negations to find out the
    // initial value of the CRC function when its being returned
    static const uint kTheMagicNoCRCCRC = 0xFFFFFFFF;

  public:
    static constexpr uint kMpegCRCSize { 4 };
};

class SequenceHeader
{
  public:
    uint width(void)     const { return (m_data[0]        <<4) | (m_data[1]>>4); }
    uint height(void)    const { return ((m_data[1] & 0xf)<<8) |  m_data[2];     }
    uint aspectNum(void) const { return m_data[3] >> 4;                          }
    uint fpsNum(void)    const { return m_data[3] & 0xf;                         }
    float fps(void)      const { return kMpeg2Fps[fpsNum()];                     }
    float aspect(bool mpeg1) const;

  private:
    SequenceHeader() {;} // only used via reinterpret cast
    ~SequenceHeader() {;}

    std::array<unsigned char,11> m_data {};
    static const AspectArray kMpeg1Aspect;
    static const AspectArray kMpeg2Aspect;
    static const AspectArray kMpeg2Fps;
};

#endif // PES_PACKET_H
