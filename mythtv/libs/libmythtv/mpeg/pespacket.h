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
        {
            _pesdata = tspacket.data() + tspacket.AFCOffset() + 
                tspacket.StartOfFieldPointer();
        }
        else
        {
            cerr<<"Started PESPacket, but !payloadStart()"<<endl;
            _pesdata = tspacket.data() + tspacket.AFCOffset();
        }

        _badPacket = true;
        if ((_pesdata - tspacket.data()) < (188-3-4))
        {
            _badPacket = !VerifyCRC();
        }
    }

  protected:
    // does not create it's own data
    PESPacket(const TSPacket* tspacket, bool)
        : _pesdata(0), _fullbuffer(0), _allocSize(0)
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
          _ccLast(pkt._ccLast), _cnt(pkt._cnt),
          _allocSize((pkt._allocSize) ? pkt._allocSize : 188),
          _badPacket(pkt._badPacket)
    { // clone
        _fullbuffer = pes_alloc(_allocSize);
        memcpy(_fullbuffer, pkt._fullbuffer, _allocSize);
        _pesdata = _fullbuffer + (pkt._pesdata - pkt._fullbuffer);
    }

    // may be modified
    PESPacket(const TSPacket& tspacket)
        : _ccLast(tspacket.ContinuityCounter()), _cnt(1)
    { // clone
        InitPESPacket(const_cast<TSPacket&>(tspacket));

        int len     = (4*1024) - 256; /* ~4KB */
        int offset  = _pesdata - tspacket.data();
        _allocSize  = ((len+offset+187)/188)*188; /* make it multiple of 188 */
        _fullbuffer = pes_alloc(_allocSize);
        _pesdata    = _fullbuffer + offset;
        memcpy(_fullbuffer, tspacket.data(), 188);
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

    unsigned int StartCodePrefix() const { return _pesdata[0]; }
    unsigned int StreamID() const { return _pesdata[1]; }
    unsigned int Length() const
        { return (pesdata()[2] & 0x0f) << 8 | pesdata()[3]; }

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
    uint _ccLast;       ///< Continuity counter of last inserted TS Packet
    uint _cnt;          ///< Number of tspackets contained in PES Packet
    uint _allocSize;    ///< number of bytes we allocated
    bool _badPacket;    ///< true if a CRC is not good yet
};

#endif // _PES_PACKET_H_
