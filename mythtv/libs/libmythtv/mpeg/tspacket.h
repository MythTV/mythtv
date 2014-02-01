// -*- Mode: c++ -*-
// Copyright (c) 2003-2004,2010 Daniel Thor Kristjansson
#ifndef _TS_PACKET_H_
#define _TS_PACKET_H_

#include <cstdlib>
#include "mythcontext.h"
#include "mythtvexp.h"

// n.b. these PID relationships are only a recommendation from ATSC,
// but seem to be universal
#define VIDEO_PID(bp) ((bp)+1)
#define AUDIO_PID(bp) ((bp)+4)
#define SYNC_BYTE     0x0047

/** \class TSHeader
 *  \brief Used to access header of a TSPacket.
 *
 *  This class is also used to determine which PID a PESPacket arrived on.
 *
 *  \sa TSPacket, PESPacket, HDTVRecorder
 */
class MTV_PUBLIC TSHeader
{
  public:
    TSHeader()
    {
        _tsdata[0] = SYNC_BYTE;
        /*
          no need to init rest of header, this is only a part of a larger
          packets which initialize the rest of the data differently.
        */
    }
    TSHeader(int cc)
    {
        _tsdata[0] = SYNC_BYTE;
        SetContinuityCounter(cc);
        /*
          no need to init rest of header, this is only a part of a larger
          packets which initialize the rest of the data differently.
        */
    }
    void InitHeader(const unsigned char* header) {
        if (header)
        {
            _tsdata[0]=header[0];
            _tsdata[1]=header[1];
            _tsdata[2]=header[2];
            _tsdata[3]=header[3];
        }
    }

    // Decode the link header (bytes 0-4):
    // gets

    //0.0  8 bits SYNC_BYTE
    bool HasSync() const { return SYNC_BYTE == _tsdata[0]; }
    //1.0  1 bit transport_packet_error (if set discard immediately:
    //       modem error)
    bool TransportError() const { return bool(_tsdata[1]&0x80); }
    //1.1  1 bit payload_unit_start_indicator
    //  (if set this packet starts a section, and has pointerField)
    bool PayloadStart() const { return bool(_tsdata[1]&0x40); }
       //1.2  1 bit transport_priority (ignore)
    bool Priority() const { return bool(_tsdata[1]&0x20); }
    //1.3  13 bit PID (packet ID, which transport stream)
    inline unsigned int PID() const {
        return ((_tsdata[1] << 8) + _tsdata[2]) & 0x1fff;
    }
    //3.0  2 bit transport_scrambling_control (00,01 OK; 10,11 scrambled)
    unsigned int ScramblingControl() const { return (_tsdata[3] >> 6) & 0x3; }
    //3.2  2 bit adaptation_field_control
    //       (01-no adaptation field,payload only
    //        10-adaptation field only,no payload
    //        11-adaptation field followed by payload
    //        00-reserved)
    unsigned int AdaptationFieldControl() const {
        return (_tsdata[3] >> 4) & 0x3;
    }
    //3.4  4 bit continuity counter (should cycle 0->15 in sequence
    //       for each PID; if skip, we lost a packet; if dup, we can
    //       ignore packet)
    unsigned int ContinuityCounter() const { return _tsdata[3] & 0xf; }

    // shortcuts
    bool Scrambled() const { return bool(_tsdata[3]&0x80); }
    bool HasAdaptationField() const { return bool(_tsdata[3] & 0x20); }
    bool HasPayload() const { return bool(_tsdata[3] & 0x10); }

    void SetTransportError(bool err) {
        if (err) _tsdata[1] |= 0x80; else _tsdata[1] &= (0xff-(0x80));
    }
    void SetPayloadStart(bool start) {
        if (start) _tsdata[1] |= 0x40; else _tsdata[1] &= (0xff-0x40);
    }
    void SetPriority(bool priority) {
        if (priority) _tsdata[1] |= 0x20; else _tsdata[1] &= (0xff-0x20);
    }
    void SetPID(unsigned int pid) {
        _tsdata[1] = ((pid >> 8) & 0x1F) | (_tsdata[1] & 0xE0);
        _tsdata[2] = (pid & 0xFF);
    }
    void SetScrambled(unsigned int scr) {
        _tsdata[3] = (_tsdata[3] & (0xff-(0x3<<6))) | (scr<<6);
    }
    void SetAdaptationFieldControl(unsigned int afc) {
        _tsdata[3] = (_tsdata[3] & 0xcf) | (afc&0x3)<<4;
    }
    void SetContinuityCounter(unsigned int cc) {
        _tsdata[3] = (_tsdata[3] & 0xf0) | (cc & 0xf);
    }

    const unsigned char* data() const { return _tsdata; }
    unsigned char* data() { return _tsdata; }

    static const unsigned int kHeaderSize;
    static const unsigned char kPayloadOnlyHeader[4];
  private:
    unsigned char _tsdata[4];
};

/** \class TSPacket
 *  \brief Used to access the data of a Transport Stream packet.
 *
 *  \sa TSHeader, PESPacket, HDTVRecorder
 */
class MTV_PUBLIC TSPacket : public TSHeader
{
    friend class PESPacket;
  public:
    /* note: payload is intenionally left uninitialized */
    TSPacket() : TSHeader() { }

    static TSPacket* CreatePayloadOnlyPacket()
    {
        TSPacket *pkt = new TSPacket();
        pkt->InitHeader(kPayloadOnlyHeader);
        memset(pkt->_tspayload, 0xFF, kPayloadSize);
        pkt->SetStartOfFieldPointer(0);
        return pkt;
    }

    inline TSPacket* CreateClone() const {
        TSPacket *pkt = new TSPacket();
        memcpy(pkt, this, kSize);
        return pkt;
    }

    void InitPayload(const unsigned char *payload)
    {
        if (payload)
            memcpy(_tspayload, payload, kPayloadSize);
    }

    void InitPayload(const unsigned char *payload, uint size)
    {
        if (payload)
            memcpy(_tspayload, payload, size);
        else
            size = 0;

        if (size < TSPacket::kPayloadSize)
            memset(_tspayload + size, 0xff, TSPacket::kPayloadSize - size);
    }

    // This points outside the TSHeader data, but is declared here because
    // it is used for the different types of packets that employ a TS Header
    unsigned int AFCOffset() const { // only works if AFC fits in TSPacket
        return HasAdaptationField() ? _tspayload[0]+1+4 : 4;
    }

    //4.0  8 bits, iff payloadStart(), points to start of field
    unsigned int StartOfFieldPointer() const 
        { return _tspayload[AFCOffset()-4]; }
    void SetStartOfFieldPointer(uint sof) 
        { _tspayload[AFCOffset()-4] = sof; }

    QString toString() const;

    static const unsigned int kSize;
    static const unsigned int kPayloadSize;
    static const unsigned int kDVBEmissionSize;
    static const unsigned int kISDBEmissionSize;
    static const unsigned int k8VSBEmissionSize;
    static const TSPacket    *kNullPacket;
  private:
    unsigned char _tspayload[184];
};

#if 0 /* not used yet */
/** \class TSDVBEmissionPacket
 *  \brief Adds DVB forward error correction data to size of packet.
 */
class MTV_PUBLIC TSDVBEmissionPacket : public TSPacket
{
  private:
    unsigned char _tsfec[16];
};

/** \class TSISDBEmissionPacket
 *  \brief Adds ISDB forward error correction data to size of packet.
 */
class MTV_PUBLIC TSISDBEmissionPacket : public TSPacket
{
  private:
    unsigned char _tsfec[16];
};

/** \class TS8VSBEmissionPacket
 *  \brief Adds 8-VSB forward error correction data to size of packet.
 */
class MTV_PUBLIC TS8VSBEmissionPacket : public TSPacket
{
  private:
    unsigned char _tsfec[20];
};
#endif

#endif // _TS_PACKET_H_
