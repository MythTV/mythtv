// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _TS_PACKET_H_
#define _TS_PACKET_H_

#include <cstdlib>
#include "mythcontext.h"
using namespace std;

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
class TSHeader {
  public:
    TSHeader() { _tsdata[0] = SYNC_BYTE; }
    TSHeader(int cc) {
        _tsdata[0] = SYNC_BYTE;
        SetContinuityCounter(cc);
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
    unsigned int ScramplingControl() const { return (_tsdata[3] >> 6) & 0x3; }
    //3.2  2 bit adaptation_field_control
    //       (01-no adptation field,payload only
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
    bool Scrampled() const { return bool(_tsdata[3]&0x80); }
    bool HasAdaptationField() const { return bool(_tsdata[3] & 0x20); }
    bool HasPayload() const { return bool(_tsdata[3] & 0x10); }

    unsigned int AFCOffset() const { // only works if AFC fits in TSPacket
        return HasAdaptationField() ? _tsdata[4]+1+4 : 4;
    }

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

    static const unsigned int HEADER_SIZE;
    static const unsigned char PAYLOAD_ONLY_HEADER[4];
  private:
    unsigned char _tsdata[4];
};

/** \class TSPacket
 *  \brief Used to access the data of a Transport Stream packet.
 *
 *  \sa TSHeader, PESPacket, HDTVRecorder
 */
class TSPacket : public TSHeader
{
    friend class PESPacket;
  public:
    TSPacket() : TSHeader() {}
    static TSPacket* CreatePayloadOnlyPacket()
    {
        TSPacket *pkt = new TSPacket();
        pkt->InitHeader(PAYLOAD_ONLY_HEADER);
        memset(pkt->_tspayload, 0xFF, PAYLOAD_SIZE);
        return pkt;
    }
    
    inline TSPacket* CreateClone() const {
        TSPacket *pkt = new TSPacket();
        memcpy(&pkt, this, SIZE);
        return pkt;
    }

    void InitPayload(const unsigned char* payload)
    {
        if (payload)
            memcpy(_tspayload, payload, PAYLOAD_SIZE);
    }

    //4.0  8 bits, iff payloadStart(), points to start of field
    unsigned int StartOfFieldPointer() const { return data()[AFCOffset()]; }

    QString toString() const;

    static const unsigned int SIZE;
    static const unsigned int PAYLOAD_SIZE;
    static const TSPacket    *NULL_PACKET;
  private:
    unsigned char _tspayload[184];
};

#endif
