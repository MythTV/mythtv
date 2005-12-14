// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _PES_PACKET_H_
#define _PES_PACKET_H_

/*
  max length of PSI table = 1024 bytes
  max length of private_section = 4096 bytes
*/

#include "tspacket.h"
extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

unsigned char *pes_alloc(uint size);
void pes_free(unsigned char *ptr);
inline unsigned int pes_length(const unsigned char* pesbuf)
    { return (pesbuf[2] & 0x0f) << 8 | pesbuf[3]; }

/** \class PESPacket
 *  \brief Allows us to transform TS packets to PES packets, which
 *         are used to hold PSIP tables as well as multimedia streams.
 *  \sa PSIPTable, TSPacket
 */
class PESPacket
{
    /// Only handles single TS packet PES packets, for PMT/PAT tables basically
    void InitPESPacket(TSPacket& tspacket)
    {
        if (tspacket.PayloadStart())
            _psiOffset = tspacket.AFCOffset() + tspacket.StartOfFieldPointer();
        else
        {
            cerr<<"Started PESPacket, but !payloadStart()"<<endl;
            _psiOffset = tspacket.AFCOffset();
        }
        _pesdata = tspacket.data() + tspacket.AFCOffset();

        _badPacket = true;
        if ((_pesdata - tspacket.data()) < (188-3-4))
        {
            _badPacket = !VerifyCRC();
        }
    }

  protected:
    // does not create it's own data
    PESPacket(const TSPacket* tspacket, bool)
        : _pesdata(0), _fullbuffer(0), _pesdataSize(188), _allocSize(0)
    {
        InitPESPacket(const_cast<TSPacket&>(*tspacket));
        _fullbuffer = const_cast<unsigned char*>(tspacket->data());
    }

  private:
    //const PESPacket& operator=(const PESPacket& pkt);
    PESPacket& operator=(const PESPacket& pkt);

  public:
    // may be modified
    PESPacket(const PESPacket& pkt)
        : _pesdata(0),
          _psiOffset(pkt._psiOffset),
          _ccLast(pkt._ccLast),
          _pesdataSize(pkt._pesdataSize),
          _allocSize((pkt._allocSize) ? pkt._allocSize : 188),
          _badPacket(pkt._badPacket)
    { // clone
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
        /* make alloc size multiple of 188 */
        _allocSize  = ((len+_psiOffset+187)/188)*188;
        _fullbuffer = pes_alloc(_allocSize);
        _pesdata    = _fullbuffer + _psiOffset;
        memcpy(_fullbuffer, tspacket.data(), 188);
    }

    // At this point we should have the entire VCT table in buffer
    // at (buffer - 8), and without the tspacket 4 byte header
    
        //if (TableID::TVCT == table_id)
        //VirtualChannelTable vct;

    // may be modified
    PESPacket(const TSPacket &tspacket, int start_code_prefix,
              const unsigned char *pesdata_plus_one, uint pes_size)
        : _ccLast(tspacket.ContinuityCounter()), _pesdataSize(pes_size)
    { // clone
        InitPESPacket(const_cast<TSPacket&>(tspacket)); // sets _psiOffset
        int len     = pes_size+4;
        /* make alloc size multiple of 188 */
        _allocSize  = ((len+_psiOffset+187)/188)*188;
        _fullbuffer = pes_alloc(_allocSize);
        _pesdata    = _fullbuffer + _psiOffset;
        memcpy(_fullbuffer, tspacket.data(), 188);
        _pesdata[0] = start_code_prefix;
        memcpy(_fullbuffer + _psiOffset + 1, pesdata_plus_one, pes_size-1);
    }

    virtual ~PESPacket()
    {
        if (IsClone())
            pes_free(_fullbuffer);
        _fullbuffer = 0;
        _pesdata = 0;
    }

    static const PESPacket View(const TSPacket& tspacket)
        { return PESPacket(&tspacket, false); }

    static PESPacket View(TSPacket& tspacket)
        { return PESPacket(&tspacket, false); }

    bool IsClone() const { return bool(_allocSize); }

    // return true if complete or broken
    bool AddTSPacket(const TSPacket* tspacket);

    bool IsGood() const { return !_badPacket; }

    const TSHeader* tsheader() const 
        { return reinterpret_cast<const TSHeader*>(_fullbuffer); }
    TSHeader* tsheader()
        { return reinterpret_cast<TSHeader*>(_fullbuffer); }

    // _pesdata[-2] == 0, _pesdata[-1] == 0, _pesdata[0] == 1
    unsigned int StartCodePrefix() const { return _pesdata[0]; }
    unsigned int StreamID() const { return _pesdata[1]; }
    unsigned int Length() const
        { return (pesdata()[2] & 0x0f) << 8 | pesdata()[3]; }
    // 2 bits "10"
    // 2 bits "PES_scrambling_control (0 not scrambled)
    unsigned int ScramblingControl() const
        { return (pesdata()[4] & 0x30) >> 4; }
    /// 1 bit  Indicates if this is a high priority packet
    bool HighPriority() const { return (pesdata()[4] & 0x8) >> 3; }
    /// 1 bit  Data alignment indicator (must be 0 for video)
    bool DataAligned()  const { return (pesdata()[4] & 0x4) >> 2; }
    /// 1 bit  If true packet may contain copy righted material and is
    ///        known to have once contained materiale with copy rights.
    ///        If false packet may contain copy righted material but is
    ///        not known to have ever contained materiale with copy rights.
    bool CopyRight()    const { return (pesdata()[4] & 0x2) >> 1; }
    /// 1 bit  Original Recording
    bool OriginalRecording() const { return pesdata()[4] & 0x1; }

    /// 1 bit  Presentation Time Stamp field is present
    bool HasPTS()       const { return (pesdata()[5] & 0x80) >> 7; }
    /// 1 bit  Decoding Time Stamp field is present
    bool HasDTS()       const { return (pesdata()[5] & 0x40) >> 6; }
    /// 1 bit  Elementary Stream Clock Reference field is present
    bool HasESCR()      const { return (pesdata()[5] & 0x20) >> 5; }
    /// 1 bit  Elementary Stream Rate field is present
    bool HasESR()       const { return (pesdata()[5] & 0x10) >> 4; }
    /// 1 bit  DSM field present (should always be false for broadcasts)
    bool HasDSM()       const { return (pesdata()[5] & 0x8) >> 3; }
    /// 1 bit  Additional Copy Info field is present
    bool HasACI() const { return (pesdata()[5] & 0x4) >> 2; }
    /// 1 bit  Cyclic Redundancy Check present
    bool HasCRC()        const { return (pesdata()[5] & 0x2) >> 1; }
    /// 1 bit  Extension flags are present
    bool HasExtensionFlags() const { return pesdata()[5] & 0x1; }

    // 8 bits PES Header Length
    // variable length -- pes header fields
    // variable length -- pes data block

    unsigned int TSSizeInBuffer() const { return _pesdataSize; }
    unsigned int PSIOffset() const { return _psiOffset; }

    const unsigned char* pesdata() const { return _pesdata; }
    unsigned char* pesdata() { return _pesdata; }

    const unsigned char* dvbParseableData() const { return _pesdata+1; }
    /// \brief You can call SIParser::ParseTable(data, size, pid)
    ///        where data = pes.dvbParseableData(), size = pes.length()+12,
    ///        and pid = pes.tspacket()->PID().
    unsigned char* dvbParseableData() { return _pesdata+1; }

    void SetStartCodePrefix(unsigned int val) { _pesdata[0]=val&0xff; }
    void SetStreamID(unsigned int id) { _pesdata[1]=id; }
    void SetLength(unsigned int len)
    {
        pesdata()[2] = (pesdata()[2] & 0xf0) | ((len>>8) & 0x0f);
        pesdata()[3] = len & 0xff;
    }

    unsigned int CRC() const 
    {
        unsigned int offset = Length();
        return ((_pesdata[offset+0]<<24) |
                (_pesdata[offset+1]<<16) |
                (_pesdata[offset+2]<<8) |
                (_pesdata[offset+3]));
    }

    unsigned int CalcCRC() const
        { return mpegts_crc32(dvbParseableData(), Length()-1); }

    void SetCRC(unsigned int crc)
    {
        _pesdata[Length()+0] = (crc & 0xff000000) >> 24;
        _pesdata[Length()+1] = (crc & 0x00ff0000) >> 16;
        _pesdata[Length()+2] = (crc & 0x0000ff00) >> 8;
        _pesdata[Length()+3] = (crc & 0x000000ff);
    }

    void SetPSIOffset(unsigned int offset)
    {
        _psiOffset = offset;
        _pesdata = _fullbuffer + _psiOffset;
    }

    bool VerifyCRC() const { return CalcCRC() == CRC(); }

    /*
    // for testing
    unsigned int t_CRC32(int offset) const {
        return ((_pesdata[offset+0]<<24) |
                (_pesdata[offset+1]<<16) |
                (_pesdata[offset+2]<<8) |
                (_pesdata[offset+3]));
    }
    // for testing
    unsigned int t_calcCRC32(int offset) const {
        return mpegts_crc32(_pesdata+1, offset-1);
    }
    */
  protected:
    void Finalize() { SetCRC(CalcCRC()); }

    unsigned char *_pesdata;    ///< Pointer to PES data in full buffer
    unsigned char *_fullbuffer; ///< Pointer to allocated data
  private:
    uint _psiOffset;    ///< AFCOffset + StartOfFieldPointer
    uint _ccLast;       ///< Continuity counter of last inserted TS Packet
    uint _pesdataSize;  ///< Number of bytes containing PES data
    uint _allocSize;    ///< Total number of bytes we allocated
    bool _badPacket;    ///< true if a CRC is not good yet
};

class SequenceHeader
{
  public:
    SequenceHeader() {;}
    ~SequenceHeader() {;}

    uint width(void)     const { return (data[0]        <<4) | (data[1]>>4); }
    uint height(void)    const { return ((data[1] & 0xf)<<8) |  data[2];     }
    uint aspectNum(void) const { return data[3] >> 4;                        }
    uint fpsNum(void)    const { return data[3] & 0xf;                       }
    float fps(void)      const { return mpeg2_fps[fpsNum()];                 }
    float aspect(bool mpeg1) const;

  private:
    unsigned char data[11];
    static const float mpeg1_aspect[16];
    static const float mpeg2_aspect[16];
    static const float mpeg2_fps[16];
};

#endif // _PES_PACKET_H_
