// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _PES_PACKET_H_
#define _PES_PACKET_H_

/*
  max length of PSI table = 1024 bytes
  max length of private_section = 4096 bytes
*/

#include <vector>
using namespace std;

#include "tspacket.h"
#include "mythlogging.h"

MTV_PUBLIC unsigned char *pes_alloc(uint size);
MTV_PUBLIC void pes_free(unsigned char *ptr);

/** \class PESPacket
 *  \brief Allows us to transform TS packets to PES packets, which
 *         are used to hold PSIP tables as well as multimedia streams.
 *  \sa PSIPTable, TSPacket
 */
class MTV_PUBLIC PESPacket
{
    /// Only handles single TS packet PES packets, for PMT/PAT tables basically
    void InitPESPacket(TSPacket& tspacket)
    {
        if (tspacket.PayloadStart())
            _psiOffset = tspacket.AFCOffset() + tspacket.StartOfFieldPointer();
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Started PESPacket, but !payloadStart()");
            _psiOffset = tspacket.AFCOffset();
        }
        _pesdata = tspacket.data() + _psiOffset + 1;

        _badPacket = true;
        // first check if Length() will return something useful and
        // than check if the packet ends in the first TSPacket
        if ((_pesdata - tspacket.data()) <= (188-3) &&
            (_pesdata + Length() - tspacket.data()) <= (188-3))
        {
            _badPacket = !VerifyCRC();
        }
    }

  protected:
    // does not create it's own data
    PESPacket(const TSPacket* tspacket, bool)
        : _pesdata(NULL),    _fullbuffer(NULL),
          _ccLast(tspacket->ContinuityCounter()), _allocSize(0)
    {
        InitPESPacket(const_cast<TSPacket&>(*tspacket));
        _fullbuffer = const_cast<unsigned char*>(tspacket->data());
        _pesdataSize = TSPacket::kSize - (_pesdata - _fullbuffer);
    }
    // does not create it's own data
    PESPacket(const unsigned char *pesdata, bool)
        : _pesdata(const_cast<unsigned char*>(pesdata)),
          _fullbuffer(const_cast<unsigned char*>(pesdata)),
          _psiOffset(0), _ccLast(255), _allocSize(0)
    {
        _badPacket = !VerifyCRC();
        _pesdataSize = max(((int)Length())-1 + (HasCRC() ? 4 : 0), (int)0);
    }

  private:
    //const PESPacket& operator=(const PESPacket& pkt);
    PESPacket& operator=(const PESPacket& pkt);

  public:
    // may be modified
    PESPacket(const PESPacket& pkt)
        : _pesdata(NULL),
          _psiOffset(pkt._psiOffset),
          _ccLast(pkt._ccLast),
          _pesdataSize(pkt._pesdataSize),
          _allocSize(pkt._allocSize),
          _badPacket(pkt._badPacket)
    { // clone
        if (!_allocSize)
            _allocSize = pkt._pesdataSize + (pkt._pesdata - pkt._fullbuffer);

        _fullbuffer = pes_alloc(_allocSize);
        memcpy(_fullbuffer, pkt._fullbuffer, _allocSize);
        _pesdata = _fullbuffer + (pkt._pesdata - pkt._fullbuffer);
    }

    // may be modified
    PESPacket(const TSPacket& tspacket)
        : _ccLast(tspacket.ContinuityCounter()), _pesdataSize(188)
    { // clone
        InitPESPacket(const_cast<TSPacket&>(tspacket)); // sets _psiOffset

        int len     = (4*1024) - 256; /* ~4KB */
        _allocSize  = len + _psiOffset;
        _fullbuffer = pes_alloc(_allocSize);
        _pesdata    = _fullbuffer + _psiOffset + 1;
        memcpy(_fullbuffer, tspacket.data(), TSPacket::kSize);
    }

    // At this point we should have the entire VCT table in buffer
    // at (buffer - 8), and without the tspacket 4 byte header

        //if (TableID::TVCT == table_id)
        //VirtualChannelTable vct;

    // may be modified
    PESPacket(const TSPacket &tspacket,
              const unsigned char *pesdata, uint pes_size)
        : _ccLast(tspacket.ContinuityCounter()), _pesdataSize(pes_size)
    { // clone
        InitPESPacket(const_cast<TSPacket&>(tspacket)); // sets _psiOffset
        int len     = pes_size+4;
        /* make alloc size multiple of 188 */
        _allocSize  = ((len+_psiOffset+187)/188)*188;
        _fullbuffer = pes_alloc(_allocSize);
        _pesdata    = _fullbuffer + _psiOffset + 1;
        memcpy(_fullbuffer, tspacket.data(), 188);
        memcpy(_pesdata, pesdata, pes_size-1);
    }

    virtual ~PESPacket()
    {
        if (IsClone())
            pes_free(_fullbuffer);

        _fullbuffer = NULL;
        _pesdata    = NULL;
    }

    static const PESPacket View(const TSPacket& tspacket)
        { return PESPacket(&tspacket, false); }

    static PESPacket View(TSPacket& tspacket)
        { return PESPacket(&tspacket, false); }

    bool IsClone() const { return bool(_allocSize); }

    // return true if complete or broken
    bool AddTSPacket(const TSPacket* tspacket, bool &broken);

    bool IsGood() const { return !_badPacket; }

    const TSHeader* tsheader() const
        { return reinterpret_cast<const TSHeader*>(_fullbuffer); }
    TSHeader* tsheader()
        { return reinterpret_cast<TSHeader*>(_fullbuffer); }

    void GetAsTSPackets(vector<TSPacket> &pkts, uint cc) const;

    // _pesdata[-3] == 0, _pesdata[-2] == 0, _pesdata[-1] == 1
    uint StreamID()   const { return _pesdata[0]; }
    uint Length()     const
        { return (_pesdata[1] & 0x0f) << 8 | _pesdata[2]; }
    // 2 bits "10"
    // 2 bits "PES_scrambling_control (0 not scrambled)
    uint ScramblingControl() const
        { return (_pesdata[3] & 0x30) >> 4; }
    /// 1 bit  Indicates if this is a high priority packet
    bool HighPriority()       const { return (_pesdata[3] & 0x8) >> 3; }
    /// 1 bit  Data alignment indicator (must be 0 for video)
    bool DataAligned()        const { return (_pesdata[3] & 0x4) >> 2; }
    /// 1 bit  If true packet may contain copy righted material and is
    ///        known to have once contained materiale with copy rights.
    ///        If false packet may contain copy righted material but is
    ///        not known to have ever contained materiale with copy rights.
    bool CopyRight()          const { return (_pesdata[3] & 0x2) >> 1; }
    /// 1 bit  Original Recording
    bool OriginalRecording()  const { return _pesdata[3] & 0x1; }

    /// 1 bit  Presentation Time Stamp field is present
    bool HasPTS()             const { return (_pesdata[4] & 0x80) >> 7; }
    /// 1 bit  Decoding Time Stamp field is present
    bool HasDTS()             const { return (_pesdata[4] & 0x40) >> 6; }
    /// 1 bit  Elementary Stream Clock Reference field is present
    bool HasESCR()            const { return (_pesdata[4] & 0x20) >> 5; }
    /// 1 bit  Elementary Stream Rate field is present
    bool HasESR()             const { return (_pesdata[4] & 0x10) >> 4; }
    /// 1 bit  DSM field present (should always be false for broadcasts)
    bool HasDSM()             const { return (_pesdata[4] & 0x8) >> 3; }
    /// 1 bit  Additional Copy Info field is present
    bool HasACI()             const { return (_pesdata[4] & 0x4) >> 2; }
    /// 1 bit  Cyclic Redundancy Check present
    virtual bool HasCRC()     const { return (_pesdata[4] & 0x2) >> 1; }
    /// 1 bit  Extension flags are present
    bool HasExtensionFlags()  const { return _pesdata[4] & 0x1; }

    /// Presentation Time Stamp, present if HasPTS() is true
    uint64_t PTS(void) const
    {
        int i = 6;
        return
            (uint64_t(_pesdata[i+0] & 0x0e) << 29) |
            (uint64_t(_pesdata[i+1]       ) << 22) |
            (uint64_t(_pesdata[i+2] & 0xfe) << 14) |
            (uint64_t(_pesdata[i+3]       ) <<  7) |
            (uint64_t(_pesdata[i+4] & 0xfe) >> 1);
    }
    /// Decode Time Stamp, present if HasDTS() is true
    uint64_t DTS(void) const
    {
        int i = 6+(HasPTS()?5:0);
        return
            (uint64_t(_pesdata[i+0] & 0x0e) << 29) |
            (uint64_t(_pesdata[i+1]       ) << 22) |
            (uint64_t(_pesdata[i+2] & 0xfe) << 14) |
            (uint64_t(_pesdata[i+3]       ) <<  7) |
            (uint64_t(_pesdata[i+4] & 0xfe) >> 1);
    }

    // 8 bits PES Header Length
    // variable length -- pes header fields
    // variable length -- pes data block

    uint TSSizeInBuffer()     const { return _pesdataSize; }
    uint PSIOffset()          const { return _psiOffset; }

    const unsigned char* pesdata() const { return _pesdata; }
    unsigned char* pesdata()             { return _pesdata; }

    const unsigned char* data() const { return _fullbuffer; }
    unsigned char* data() { return _fullbuffer; }

    void SetStreamID(uint id) { _pesdata[0] = id; }
    void SetLength(uint len)
    {
        _pesdata[1] = (_pesdata[1] & 0xf0) | ((len>>8) & 0x0f);
        _pesdata[2] = len & 0xff;
    }
    void SetTotalLength(uint len)
    {
        len += 4 /* for CRC */;
        len -= 3 /* for data before data last byte of length */;
        SetLength(len);
    }

    void SetPSIOffset(uint offset)
    {
        _psiOffset = offset;
        _pesdata = _fullbuffer + _psiOffset + 1;
    }

    uint CRC(void) const
    {
        if (!HasCRC() || (Length() < 1))
            return 0x0;
        uint offset = Length() - 1;
        return ((_pesdata[offset+0]<<24) |
                (_pesdata[offset+1]<<16) |
                (_pesdata[offset+2]<<8) |
                (_pesdata[offset+3]));
    }

    void SetCRC(uint crc)
    {
        if (Length() < 1)
            return;

        uint offset = Length() - 1;
        _pesdata[offset+0] = (crc & 0xff000000) >> 24;
        _pesdata[offset+1] = (crc & 0x00ff0000) >> 16;
        _pesdata[offset+2] = (crc & 0x0000ff00) >> 8;
        _pesdata[offset+3] = (crc & 0x000000ff);
    }

    uint CalcCRC(void) const;
    bool VerifyCRC(void) const;

  protected:
    void Finalize() { SetCRC(CalcCRC()); }

    unsigned char *_pesdata;    ///< Pointer to PES data in full buffer
    unsigned char *_fullbuffer; ///< Pointer to allocated data

    uint _psiOffset;    ///< AFCOffset + StartOfFieldPointer
    uint _ccLast;       ///< Continuity counter of last inserted TS Packet
    uint _pesdataSize;  ///< Number of data bytes (TS header + PES data)
    uint _allocSize;    ///< Total number of bytes we allocated
    bool _badPacket;    ///< true if a CRC is not good yet
};

class SequenceHeader
{
  public:
    uint width(void)     const { return (data[0]        <<4) | (data[1]>>4); }
    uint height(void)    const { return ((data[1] & 0xf)<<8) |  data[2];     }
    uint aspectNum(void) const { return data[3] >> 4;                        }
    uint fpsNum(void)    const { return data[3] & 0xf;                       }
    float fps(void)      const { return mpeg2_fps[fpsNum()];                 }
    float aspect(bool mpeg1) const;

  private:
    SequenceHeader() {;} // only used via reinterpret cast
    ~SequenceHeader() {;}

    unsigned char data[11];
    static const float mpeg1_aspect[16];
    static const float mpeg2_aspect[16];
    static const float mpeg2_fps[16];
};

#endif // _PES_PACKET_H_
