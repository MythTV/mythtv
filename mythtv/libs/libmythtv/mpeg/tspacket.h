// -*- Mode: c++ -*-
// Copyright (c) 2003-2004,2010 Daniel Thor Kristjansson
#ifndef TS_PACKET_H
#define TS_PACKET_H

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>

#include "libmyth/mythcontext.h"
#include "libmythtv/mythtvexp.h"

// n.b. these PID relationships are only a recommendation from ATSC,
// but seem to be universal
//static constexpr uint16_t VIDEO_PID(uint16_t bp) { return bp+1; };
//static constexpr uint16_t AUDIO_PID(uint16_t bp) { return bp+4; };
static constexpr uint8_t SYNC_BYTE { 0x47 };

using TSHeaderArray = std::array<uint8_t,4>;

// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)

/** \class TSHeader
 *  \brief Used to access header of a TSPacket.
 *
 *  This class is also used to determine which PID a PESPacket arrived on.
 *
 *  \warning Be very, very careful when modifying this data structure.
 *  This class is used in the core of the received video data path, and
 *  for performance reasons overlaid on the buffer of bytes received
 *  from the network. It therefore can only contain (per-instance)
 *  variables that directly correspond to the data bytes received from
 *  the broadcaster.  There may be static variables in this class, but
 *  absolutely no per-instance variables that are not part of the
 *  overlaid byte stream.  There also must not be any virtual
 *  functions in this class, as the vtable is implemented as a
 *  per-instance variable in the class structure, and would mess up
 *  the overlaying of this structure on the received byte stream.
 *
 *  \sa TSPacket, PESPacket, HDTVRecorder
 */
class MTV_PUBLIC TSHeader
{
  public:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock, std::chrono::microseconds>;

    TSHeader(void)
    {
        m_tsData[0] = SYNC_BYTE;
        /*
          no need to init rest of header, this is only a part of a larger
          packets which initialize the rest of the data differently.
        */
    }
    explicit TSHeader(int cc)
    {
        m_tsData[0] = SYNC_BYTE;
        SetContinuityCounter(cc);
        /*
          no need to init rest of header, this is only a part of a larger
          packets which initialize the rest of the data differently.
        */
    }
    void InitHeader(const unsigned char* header) {
        if (header)
        {
            m_tsData[0]=header[0];
            m_tsData[1]=header[1];
            m_tsData[2]=header[2];
            m_tsData[3]=header[3];
        }
    }

    // Decode the link header (bytes 0-4):
    // gets

    //0.0  8 bits SYNC_BYTE
    bool HasSync(void) const { return SYNC_BYTE == m_tsData[0]; }
    //1.0  1 bit transport_packet_error (if set discard immediately:
    //       modem error)
    bool TransportError(void) const { return ( m_tsData[1] & 0x80 ) != 0; }
    //1.1  1 bit payload_unit_start_indicator
    //  (if set this packet starts a section, and has pointerField)
    bool PayloadStart(void) const { return ( m_tsData[1] & 0x40 ) != 0; }
       //1.2  1 bit transport_priority (ignore)
    bool Priority(void) const { return ( m_tsData[1] & 0x20 ) != 0; }
    //1.3  13 bit PID (packet ID, which transport stream)
    unsigned int PID(void) const {
        return ((m_tsData[1] << 8) + m_tsData[2]) & 0x1fff;
    }
    //3.0  2 bit transport_scrambling_control (00,01 OK; 10,11 scrambled)
    unsigned int ScramblingControl(void) const { return (m_tsData[3] >> 6) & 0x3; }
    //3.2  2 bit adaptation_field_control
    //       (01-no adaptation field,payload only
    //        10-adaptation field only,no payload
    //        11-adaptation field followed by payload
    //        00-reserved)
    unsigned int AdaptationFieldControl(void) const {
        return (m_tsData[3] >> 4) & 0x3;
    }
    //3.4  4 bit continuity counter (should cycle 0->15 in sequence
    //       for each PID; if skip, we lost a packet; if dup, we can
    //       ignore packet)
    unsigned int ContinuityCounter(void) const { return m_tsData[3] & 0xf; }

    // shortcuts
    bool Scrambled(void) const { return ( m_tsData[3] & 0x80 ) != 0; }
    bool HasAdaptationField(void) const { return ( m_tsData[3] & 0x20 ) != 0; }
    size_t AdaptationFieldSize(void) const
    { return (HasAdaptationField() ? static_cast<size_t>(data()[4]) : 0); }
    bool HasPayload(void) const { return ( m_tsData[3] & 0x10 ) != 0; }

    bool GetDiscontinuityIndicator(void) const
    { return (AdaptationFieldSize() > 0) && ((data()[5] & 0x80 ) != 0); }

    bool HasPCR(void) const { return (AdaptationFieldSize() > 0) &&
                              ((data()[5] & 0x10 ) != 0); }

    /*
      The PCR field is a 42 bit field in the adaptation field of the
      Transport Stream.  The PCR field consists of a 9 bit part that
      increments at a 27MHz rate and a 33 bit part that increments at
      a 90kHz rate (when the 27MHz part rolls over).
    */
    // The high-order 33bits (of the 48 total) are the 90kHz clock
    int64_t GetPCRbase(void) const
    { return ((static_cast<int64_t>(data()[6]) << 25) |
              (static_cast<int64_t>(data()[7]) << 17) |
              (static_cast<int64_t>(data()[8]) << 9)  |
              (static_cast<int64_t>(data()[9]) << 1)  |
              (data()[10] >> 7)); }

    // The low-order 9 bits (of the 48 total) are the 27MHz clock
    int32_t GetPCRext(void) const
    { return (((static_cast<int32_t>(data()[10]) & 0x1) << 8) |
              static_cast<int32_t>(data()[11])); }

    // PCR in a 27MHz clock
    int64_t GetPCRraw(void) const
    { return ((GetPCRbase() * 300) + GetPCRext()); }

    // PCR as a time point
    TimePoint GetPCR(void) const
    { return TimePoint(std::chrono::microseconds(GetPCRraw() / 27)); }

    void SetTransportError(bool err) {
        if (err) m_tsData[1] |= 0x80; else m_tsData[1] &= (0xff-(0x80));
    }
    void SetPayloadStart(bool start) {
        if (start) m_tsData[1] |= 0x40; else m_tsData[1] &= (0xff-0x40);
    }
    void SetPriority(bool priority) {
        if (priority) m_tsData[1] |= 0x20; else m_tsData[1] &= (0xff-0x20);
    }
    void SetPID(unsigned int pid) {
        m_tsData[1] = ((pid >> 8) & 0x1F) | (m_tsData[1] & 0xE0);
        m_tsData[2] = (pid & 0xFF);
    }
    void SetScrambled(unsigned int scr) {
        m_tsData[3] = (m_tsData[3] & (0xff-(0x3<<6))) | (scr<<6);
    }
    void SetAdaptationFieldControl(unsigned int afc) {
        m_tsData[3] = (m_tsData[3] & 0xcf) | (afc&0x3)<<4;
    }
    void SetContinuityCounter(unsigned int cc) {
        m_tsData[3] = (m_tsData[3] & 0xf0) | (cc & 0xf);
    }

    const unsigned char* data(void) const { return m_tsData.data(); }
    unsigned char* data(void) { return m_tsData.data(); }

    static constexpr unsigned int kHeaderSize {4};
    static const TSHeaderArray kPayloadOnlyHeader;
  private:
    TSHeaderArray m_tsData;         // Intentionally no initialization
};

static_assert(sizeof(TSHeader) == 4,
              "The TSHeader class must be 4 bytes in size.  It must "
              "have only one non-static variable (m_tsData) of 4 "
              "bytes, and it must not have any virtual functions.");



/** \class TSPacket
 *  \brief Used to access the data of a Transport Stream packet.
 *
 *  \warning Be very, very careful when modifying this data structure.
 *  This class is used in the core of the received video data path, and
 *  for performance reasons overlaid on the buffer of bytes received
 *  from the network. It therefore can only contain (per-instance)
 *  variables that directly correspond to the data bytes received from
 *  the broadcaster.  There may be static variables in this class, but
 *  absolutely no per-instance variables that are not part of the
 *  overlaid byte stream.  There also must not be any virtual
 *  functions in this class, as the vtable is implemented as a
 *  per-instance variable in the class structure, and would mess up
 *  the overlaying of this structure on the received byte stream.
 *
 *  \sa TSHeader, PESPacket, HDTVRecorder
 */
class MTV_PUBLIC TSPacket : public TSHeader
{
    friend class PESPacket;
  public:
    /* note: payload is intentionally left uninitialized */
    // cppcheck-suppress uninitMemberVar
    TSPacket(void) = default;

    static TSPacket* CreatePayloadOnlyPacket(void)
    {
        auto *pkt = new TSPacket();
        pkt->InitHeader(kPayloadOnlyHeader.data());
        pkt->m_tsPayload.fill(0xFF);
        pkt->SetStartOfFieldPointer(0);
        return pkt;
    }

    TSPacket* CreateClone(void) const {
        auto *pkt = new TSPacket();
        memcpy(pkt, this, kSize);
        return pkt;
    }

    void InitPayload(const unsigned char *payload)
    {
        if (payload)
            std::copy(payload, payload+kPayloadSize, m_tsPayload.data());
    }

    void InitPayload(const unsigned char *payload, uint size)
    {
        if (payload)
            std::copy(payload, payload+size, m_tsPayload.data());
        else
            size = 0;

        if (size < TSPacket::kPayloadSize)
            std::fill_n(&m_tsPayload[size], TSPacket::kPayloadSize - size, 0xff);
    }

    // This points outside the TSHeader data, but is declared here because
    // it is used for the different types of packets that employ a TS Header
    unsigned int AFCOffset(void) const { // only works if AFC fits in TSPacket
        return HasAdaptationField() ? m_tsPayload[0]+1+4 : 4;
    }

    //4.0  8 bits, iff payloadStart(), points to start of field
    unsigned int StartOfFieldPointer(void) const
        { return m_tsPayload[AFCOffset()-4]; }
    void SetStartOfFieldPointer(uint sof)
        { m_tsPayload[AFCOffset()-4] = sof; }

    QString toString(void) const;

    static constexpr unsigned int kSize             {188};
    static constexpr unsigned int kPayloadSize      {188-4};
    static constexpr unsigned int kDVBEmissionSize  {204};
    static constexpr unsigned int kISDBEmissionSize {204};
    static constexpr unsigned int k8VSBEmissionSize {208};
    static const TSPacket    *kNullPacket;
  private:
    std::array<uint8_t,184> m_tsPayload;    // Intentionally no initialization
};

static_assert(sizeof(TSPacket) == 188,
              "The TSPacket class must be 188 bytes in size.  It must "
              "have only one non-static variable (m_tsPayload) of 184 "
              "bytes, and it must not have any virtual functions.");

// NOLINTEND(cppcoreguidelines-pro-type-member-init)

#if 0 /* not used yet */
/** \class TSDVBEmissionPacket
 *  \brief Adds DVB forward error correction data to size of packet.
 */
class MTV_PUBLIC TSDVBEmissionPacket : public TSPacket
{
  private:
    unsigned char m_tsFec[16];
};

/** \class TSISDBEmissionPacket
 *  \brief Adds ISDB forward error correction data to size of packet.
 */
class MTV_PUBLIC TSISDBEmissionPacket : public TSPacket
{
  private:
    unsigned char m_tsFec[16];
};

/** \class TS8VSBEmissionPacket
 *  \brief Adds 8-VSB forward error correction data to size of packet.
 */
class MTV_PUBLIC TS8VSBEmissionPacket : public TSPacket
{
  private:
    unsigned char m_tsFec[20];
};
#endif

#endif // TS_PACKET_H
