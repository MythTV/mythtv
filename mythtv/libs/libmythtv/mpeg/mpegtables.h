// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _MPEG_TABLES_H_
#define _MPEG_TABLES_H_

#include <cassert>
#include "pespacket.h"

/** \file mpegtables.h
 *  \code
 *   special ATSC restrictions on PMT,PAT tables
 *   max 400ms,100ms between PMTs,PATs, resp.
 *   no adaptation headers in PMT,PAT or PSIP packets
 *     (?unless version number is discontinuous?)
 *   PES Payload must not be scrambled (1999 Sarnoff)
 *   No Clock in PES, no MPEG-1 System Fields, no CRC or priv. data
 *   no more than one frame per PES packet
 *   program 0 is network information table in DVB streams
 *  \endcode
 */

/** \class TableID
 *  \brief Contains listing of Stream ID's for various A/V Stream types.
 */
class StreamID
{
  public:
    enum
    {
        // video
        MPEG1Video     = 0x01,
        MPEG2Video     = 0x02,
        MPEG4Video     = 0x10,
        H264Video      = 0x1b,
        OpenCableVideo = 0x80,

        // audio
        MPEG1Audio     = 0x03,
        MPEG2Audio     = 0x04,
        AACAudio       = 0x0f,
        AC3Audio       = 0x81,
        DTSAudio       = 0x8a,

        // other
        PrivSec        = 0x05,
        PrivData       = 0x06,

        // DSM-CC Object Carousel
        DSMCC_A        = 0x0A,      // Multi-protocol Encapsulation
        DSMCC_B        = 0x0B,      // Std DSMCC Data
        DSMCC_C        = 0x0C,      // NPT DSMCC Data
        DSMCC_D        = 0x0D,      // Any DSMCC Data

        // special id's, not actually ID's but can be used in FindPIDs
        AnyMask        = 0xFFFF0000,
        AnyVideo       = 0xFFFF0001,
        AnyAudio       = 0xFFFF0002,
    };
    /// Returns true iff video is an MPEG1/2/3, H264 or open cable video stream.
    static bool IsVideo(uint type)
    {
        return ((StreamID::MPEG1Video == type) ||
                (StreamID::MPEG2Video == type) ||
                (StreamID::MPEG4Video == type) ||
                (StreamID::H264Video  == type) || 
                (StreamID::OpenCableVideo == type));
    }
    /// Returns true iff audio is MPEG1/2, AAC, AC3 or DTS audio stream.
    static bool IsAudio(uint type)
    {
        return ((StreamID::MPEG1Audio == type) ||
                (StreamID::MPEG2Audio == type) ||
                (StreamID::AACAudio   == type) ||
                (StreamID::AC3Audio   == type) ||
                (StreamID::DTSAudio   == type));
    }
    /// Returns true iff stream contains DSMCC Object Carousel
    static bool IsObjectCarousel(uint type)
    {
        return ((StreamID::DSMCC_A == type) ||
                (StreamID::DSMCC_B == type) ||
                (StreamID::DSMCC_C == type) ||
                (StreamID::DSMCC_D == type));
    }
    static const char* toString(uint streamID);
};

enum
{
    MPEG_PAT_PID  = 0x0000,
    MPEG_CAT_PID  = 0x0001,
    MPEG_TSDT_PID = 0x0002,

    DVB_NIT_PID   = 0x0010,
    DVB_SDT_PID   = 0x0011,
    DVB_EIT_PID   = 0x0012,
    DVB_RST_PID   = 0x0013,
    DVB_TOT_PID   = 0x0013,

    ATSC_PSIP_PID = 0x1ffb,
};

/** \class TableID
 *  \brief Contains listing of Table ID's for various tables (PAT=0,PMT=2,etc).
 */
class TableID
{
  public:
    enum
    {
        PAT      = 0x00, // always on pid 0x00
        CAT      = 0x01, // always on pid 0x01
        PMT      = 0x02,
        TSDT     = 0x03, // always on pid 0x02

        // DVB manditory
        NIT      = 0x40, // always on pid 0x10
        SDT      = 0x42, // always on pid 0x11
        PF_EIT   = 0x4e, // always on pid 0x12
        TDT      = 0x70, // always on pid 0x14

        // DVB optional
        NITo     = 0x41, // always on pid 0x10
        SDTo     = 0x46, // always on pid 0x11
        BAT      = 0x4a, // always on pid 0x11
        PF_EITo  = 0x4f, // always on pid 0x12
        SC_EITbeg  = 0x50, // always on pid 0x12
        SC_EITend  = 0x5f, // always on pid 0x12
        SC_EITbego = 0x60, // always on pid 0x12
        SC_EITendo = 0x6f, // always on pid 0x12
        RST      = 0x71, // always on pid 0x13
        ST       = 0x72, // any pid 0x10-0x14
        TOT      = 0x73, // always on pid 0x14
        RNT      = 0x74, // always on pid 0x16
        CT       = 0x75,
        RCT      = 0x76,
        CIT      = 0x77, // always on pid 0x12
        MPEFEC   = 0x78,
        DIT      = 0x7e, // always on pid 0x1e
        SIT      = 0x7f, // always on pid 0x1f

        // ATSC
        STUFFING = 0x80,
        CAPTION  = 0x86,
        CENSOR   = 0x87,

        ECN      = 0xA0,
        SRVLOC   = 0xA1,
        TSS      = 0xA2,
        CMPNAME  = 0xA3,

        MGT      = 0xC7, // always on pid 0x1ffb
        TVCT     = 0xC8,
        CVCT     = 0xC9,
        RRT      = 0xCA,
        EIT      = 0xCB,
        ETT      = 0xCC,
        STT      = 0xCD,

        DCCT     = 0xD3,
        DCCSCT   = 0xD4,
    };
};

/** \class PSIPTable
 *  \brief A PSIP table is a special type of PES packet containing an
 *         MPEG, ATSC, or DVB table.
 */
class PSIPTable : public PESPacket
{
  private:
    // creates non-clone version, for View
    PSIPTable(const PESPacket& pkt, bool)
        : PESPacket(reinterpret_cast<const TSPacket*>(pkt.tsheader()), false)
        { ; }
  public:
    PSIPTable(const PSIPTable& table) : PESPacket(table)
    { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }
    PSIPTable(const PESPacket& table) : PESPacket(table)
    { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }
    PSIPTable(const TSPacket& table) : PESPacket(table)
    { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }


    static const PSIPTable View(const TSPacket& tspacket)
        { return PSIPTable(PESPacket::View(tspacket), false); }

    static PSIPTable View(TSPacket& tspacket)
        { return PSIPTable(PESPacket::View(tspacket), false); }

    // Section            Bits   Start Byte sbit
    // -----------------------------------------
    // table_id             8       1.0       0
    uint TableID(void) const { return StreamID(); }

    // section_syntax_ind   1       2.0       8   should always be 1
    // private_indicator    1       2.1       9   should always be 1
    // reserved             2       2.2      10

    // section_length      12       2.4      12   always less than 0xFFE
    // adds 3 to the total section length to account for 3 bytes
    // before the end of the section length field.
    uint SectionLength(void) const { return Length() + 3; }

    // table_id_extension  16       4.0      24   table dependent
    uint TableIDExtension(void) const
        { return (pesdata()[4]<<8) | pesdata()[5]; }

    // reserved             2       6.0      40

    // version_number       5       6.2      42
    // incremented modulo 32 when table info changes
    uint Version(void) const { return (pesdata()[6]>>2) & 0x1f; }

    // current_next_ind     1       6.7      47
    // if 0 this table is not yet valid, but will be the next psip
    // table with the same sectionNumber(), tableIDExtension() and
    // tableID() to become valid.
    bool IsCurrent(void) const { return bool(pesdata()[6]&1); }

    // section_number       8       7.0      48
    uint Section(void) const { return pesdata()[7]; }

    // last_section_number  8       8.0      56
    uint LastSection(void) const { return pesdata()[8]; }

    // this is only for real ATSC PSIP tables, not similar MPEG2 tables
    // protocol_version     8       9.0      64   should always be 0 for now
    uint ProtocolVersion(void) const { return pesdata()[9]; }

    // PSIP_table_data      x       9.0      72 (incl. protocolVersion)
    const unsigned char* psipdata(void) const
        { return pesdata() + PSIP_OFFSET; }
    unsigned char* psipdata(void)
        { return pesdata() + PSIP_OFFSET; }

    // sets
    void SetTableID(uint id) { SetStreamID(id); }
    // subtracts 3 from total section length to account for 3 bytes
    // before the end of the section length field.
    void SetSectionLength(uint length) { SetLength(length-3); }
    void SetTableIDExtension(uint len)
    {
        pesdata()[4] = (len>>8) & 0xff;
        pesdata()[5] = len & 0xff;
    }
    void SetVersionNumber(uint ver)
        { pesdata()[6] = ((ver & 0x1f)<<2) | (pesdata()[6] & 0x83); }
    void SetCurrent(bool cur)
        { pesdata()[6] = (pesdata()[6]&(0xff-0x80)) | (cur ? 0x80:0); }
    void SetSection(uint num) { pesdata()[7] = num; }
    void SetLastSection(uint num) { pesdata()[8] = num; }

    // only for real ATSC PSIP tables, not similar MPEG2 tables
    //void setProtocolVersion(int ver) { pesdata()[7] = ver; }

    const QString toString(void) const;

    static const uint PSIP_OFFSET = 9; // general PSIP header offset
};

/** \class ProgramAssociationTable
 *  \brief The Program Association Table lists all the programs
 *         in a stream, and is alwyas found on PID 0.
 *
 *   Based on info in this table and the ProgramMapTable 
 *   for the program we are interested in we should
 *   be able determine which PID to write to the ringbuffer
 *   when given the program stream to record.
 *
 *   NOTE: Broadcasters are encouraged to keep the
 *   subprogram:PID mapping constant.  If we store this data
 *   in the channel database, we can branch-predict which
 *   PIDs we are looking for, and can thus "tune" the
 *   subprogram more quickly.
 *
 *  \sa ProgramMapTable
 */

extern const unsigned char init_patheader[9];

class ProgramAssociationTable : public PSIPTable
{
    static ProgramAssociationTable* CreateBlank(void)
    {
        TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
        memcpy(tspacket->data() + tspacket->AFCOffset(),
               init_patheader, PSIP_OFFSET);
        PSIPTable psip = PSIPTable::View(*tspacket);
        psip.SetLength(PSIP_OFFSET);
        ProgramAssociationTable *pat = new ProgramAssociationTable(psip);
        delete tspacket;
        return pat;
    }
  public:
    ProgramAssociationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::PAT == TableID());
    }

    // transport stream ID, program ID, count
    static ProgramAssociationTable* Create(uint tsid, uint version,
                                           const vector<uint>& pnum,
                                           const vector<uint>& pid);

    uint TransportStreamID(void) const { return TableIDExtension(); }

    uint ProgramCount(void) const
        { return (SectionLength()-PSIP_OFFSET-3)>>2; }

    uint ProgramNumber(uint i) const
        { return (psipdata()[(i<<2)] << 8) | psipdata()[(i<<2) + 1]; }

    uint ProgramPID(uint i) const
    {
        return (((psipdata()[(i<<2) + 2] & 0x1f) << 8) |
                psipdata()[(i<<2) + 3]);
    }

    void SetTranportStreamID(uint gtsid) { SetTableIDExtension(gtsid); }

    // helper function
    uint FindPID(uint progNum) const
    {
        for (uint i = 0; i < ProgramCount(); i++)
            if (progNum==ProgramNumber(i))
                return ProgramPID(i);
        return 0;
    }
    uint FindAnyPID(void) const
    {
        for (uint i = 0; i < ProgramCount(); i++)
            if (0!=ProgramNumber(i))
                return ProgramPID(i);
        return 0;
    }
    uint FindProgram(uint pid) const
    {
        for (uint i = 0; i < ProgramCount(); i++)
            if (pid==ProgramPID(i))
                return ProgramNumber(i);
        return 0;
    }

    const QString toString(void) const;
};

extern const unsigned char DEFAULT_PMT_HEADER[9];

/** \class ProgramMapTable
 *  \brief A PMT table maps a program described in the ProgramAssociationTable
 *         to various PID's which describe the multimedia contents of the
 *         program.
 */
class ProgramMapTable : public PSIPTable
{
    static const uint pmt_header = 4; // minimum PMT header offset
    mutable vector<unsigned char*> _ptrs; // used to parse

    static ProgramMapTable* CreateBlank(void)
    {
        TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
        memcpy(tspacket->data() + tspacket->AFCOffset(),
               DEFAULT_PMT_HEADER, PSIP_OFFSET);
        PSIPTable psip = PSIPTable::View(*tspacket);
        psip.SetLength(PSIP_OFFSET);
        ProgramMapTable *pmt = new ProgramMapTable(psip);
        delete tspacket;
        return pmt;
    }
  public:
    ProgramMapTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::PMT == TableID());
        Parse();
    }

    static ProgramMapTable* Create(uint programNumber, uint basepid,
                                   uint pcrpid, uint version,
                                   vector<uint> pids, vector<uint> types);
        
    /// stream that contrains program clock reference.
    uint PCRPID(void) const
        { return ((psipdata()[0] << 8) | psipdata()[1]) & 0x1fff; }

    uint ProgramNumber(void) const
        { return TableIDExtension(); }

    uint ProgramInfoLength(void) const
        { return ((psipdata()[2]<<8) | psipdata()[3]) & 0x0fff; }

    const unsigned char* ProgramInfo(void) const
        { return pesdata() + 13; }

    uint StreamType(uint i) const
        { return _ptrs[i][0]; }

    uint StreamPID(uint i) const
        { return ((_ptrs[i][1] << 8) | _ptrs[i][2]) & 0x1fff; }

    uint StreamInfoLength(uint i) const
        { return ((_ptrs[i][3] << 8) | _ptrs[i][4]) & 0x0fff; }

    const unsigned char* StreamInfo(uint i) const
        { return _ptrs[i] + 5; }

    uint StreamCount(void) const
        { return (_ptrs.size()) ? _ptrs.size()-1 : 0; }

    // sets
    void SetPCRPID(uint pid)
    {
        psipdata()[0] = ((pid >> 8) & 0x1F) | (psipdata()[0] & 0xE0);
        psipdata()[1] = (pid & 0xFF);
    }

    void SetProgramNumber(uint num) { SetTableIDExtension(num); }

    void SetStreamPID(uint i, uint pid)
    {
        _ptrs[i][1] = ((pid>>8) & 0x1f) | (_ptrs[i][1] & 0xe0);
        _ptrs[i][2] = pid & 0xff;
    }

    void SetStreamType(uint i, uint type)
        { _ptrs[i][0] = type; }

    // helper methods
    /// Returns true iff StreamID::IsVideo(StreamType(i)) returns true.
    bool IsVideo(uint i) const { return StreamID::IsVideo(StreamType(i)); }
    bool IsAudio(uint i) const;
    /// Returns true iff PMT contains CA descriptor.
    bool IsEncrypted(void) const;
    /// Returns a string representation of type at stream index i
    const QString StreamTypeString(uint i) const;
    uint FindPIDs(uint type, vector<uint>& pids) const;
    uint FindPIDs(uint type, vector<uint>& pids, vector<uint>& types) const;

    /// \brief Locates stream index of pid.
    /// \return stream index if successful, -1 otherwise
    int FindPID(uint pid) const
    {
        for (uint i = 0; i < StreamCount(); i++)
            if (pid == StreamPID(i))
                return i;
        return -1;
    }

    void RemoveAllStreams(void)
    {
        memset(psipdata(), 0xff, pmt_header);
        SetProgramInfoLength(0);
        _ptrs.clear();
    }
    void AppendStream(uint pid, uint type, unsigned char* si = 0, uint il = 0);

    void Parse(void) const;
    const QString toString(void) const;
    // unsafe sets
  private:
    void SetStreamInfoLength(uint i, uint length)
    {
        _ptrs[i][3] = ((length>>8) & 0x0f) | (_ptrs[i][3] & 0xf0);
        _ptrs[i][4] = length & 0xff;
    }

    void SetStreamProgramInfo(uint i, unsigned char* streamInfo,
                              uint infoLength)
    {
        SetStreamInfoLength(i, infoLength);
        memcpy(_ptrs[i] + 5, streamInfo, infoLength);
    }

    void SetProgramInfoLength(uint length)
    {
        psipdata()[2] = ((length<<8) & 0x0f) | (psipdata()[2] & 0xf0);
        psipdata()[3] = length & 0xff;
    }
};

/** \class AdaptationFieldControl
 *  \brief AdaptationFieldControl is used to transmit various important
 *         stream attributes.
 *
 *   These include such things as the PCR and flags discontiunities in the
 *   program, such as when a commercial or another program begins. This is
 *   currently just passed through the MythTV recorders to the recorded
 *   stream.
 */
class AdaptationFieldControl
{
  public:
    AdaptationFieldControl(const unsigned char* packet) : _data(packet) { ; }

    /** adaptation header length
     * (after which is payload data)         8   0.0
     */
    uint Length(void) const             { return _data[0]; }

    /** discontinuity_indicator
     *  (time base may change)               1   1.0
     */
    bool Discontinuity(void) const      { return _data[1] & 0x80; }
    // random_access_indicator (?)           1   1.1
    bool RandomAccess(void) const       { return bool(_data[1] & 0x40); }
    // elementary_stream_priority_indicator  1   1.2
    bool Priority(void) const           { return bool(_data[1] & 0x20); }

// Each of the following extends the adaptation header.  In order:

    /** PCR flag (we have PCR data)          1   1.3
     *  (adds 6 bytes after adaptation header)
     */
    bool PCR(void) const                { return bool(_data[1] & 0x10); }
    /** OPCR flag (we have OPCR data)        1   1.4
     *  (adds 6 bytes) ((Original) Program Clock Reference; used to time output)
     */
    bool OPCR(void) const               { return bool(_data[1] & 0x08); }
    /** splicing_point_flag                  1   1.5
     *  (adds 1 byte) (we have splice point data)
     *  Splice data is packets until a good splice point for
     *  e.g. commercial insertion -- if these are still set,
     *  might be a good way to recognize potential commercials
     *  for flagging.
     */
    bool SplicingPoint(void) const      { return bool(_data[1] & 0x04); }
    //  transport_private_data_flag          1   1.6
    // (adds 1 byte)
    bool PrivateTransportData(void) const { return bool(_data[1] & 0x02); }
    // adaptation_field_extension_flag       1   1.7
    bool FieldExtension(void) const     { return bool(_data[1] & 0x1); }
    // extension length                      8   2.0
    uint ExtensionLength(void) const    { return _data[2]; }
    // ltw flag                              1   3.0
    // (adds 2 bytes)
    bool LTW(void) const                { return bool(_data[3] & 0x80); }
    // piecewise_rate_flag (adds 3 bytes)    1   3.1
    bool PiecewiseRate(void) const      { return bool(_data[3] & 0x40); }
    // seamless_splice_flag (adds 5 bytes)   1   3.2
    bool SeamlessSplice(void) const     { return bool(_data[3] & 0x20); }
    // unused flags                          5   3.3

  private:
    const unsigned char* _data;
};

#endif
