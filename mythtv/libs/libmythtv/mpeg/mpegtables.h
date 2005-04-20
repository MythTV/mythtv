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

namespace StreamID {
    enum {
        MPEG1Video = 0x01,
        MPEG2Video = 0x02,
        MPEG4Video = 0x10,
        H264Video  = 0x1b,

        MPEG1Audio = 0x03,
        MPEG2Audio = 0x04,
        AACAudio   = 0x0f,
        AC3Audio   = 0x81,
        DTSAudio   = 0x8a,

        PrivSec    = 0x05,
        PrivData   = 0x06,
    };
};

namespace TableID {
    enum {
        PAT = 0x00, PMT = 0x02,
        STUFFING = 0x80, CAPTION = 0x86, CENSOR = 0x87,
        ECN = 0xA0, SRVLOC = 0xA1, TSS = 0xA2, CMPNAME = 0xA3
    };
};

/** \class PSIPTable
 *  \brief A PSIP table is a special type of PES packet containing an
 *         MPEG, ATSC, or DVB table.
 */
class PSIPTable : public PESPacket {
  private:
    // creates non-clone version, for View
    PSIPTable(const PESPacket& pkt, bool) :
        PESPacket(reinterpret_cast<const TSPacket*>(pkt.tsheader()), false) { ; }
  public:
    PSIPTable(const PSIPTable& table) : PESPacket(table) { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }
    PSIPTable(const PESPacket& table) : PESPacket(table) { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }
    PSIPTable(const TSPacket& table) : PESPacket(table) { 
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }


    static const PSIPTable View(const TSPacket& tspacket) {
        return PSIPTable(PESPacket::View(tspacket), false);
    }

    static PSIPTable View(TSPacket& tspacket) {
        return PSIPTable(PESPacket::View(tspacket), false);
    }

    // Section            Bits   Start Byte sbit
    // -----------------------------------------
    // table_id             8       1.0       0
    unsigned int TableID() const { return StreamID(); }

    // section_syntax_ind   1       2.0       8   should always be 1
    // private_indicator    1       2.1       9   should always be 1
    // reserved             2       2.2      10

    // section_length      12       2.4      12   always less than 0xFFE
    // adds 3 to the total section length to account for 3 bytes
    // before the end of the section length field.
    unsigned int SectionLength() const { return Length() + 3; }

    // table_id_extension  16       4.0      24   table dependent
    unsigned int TableIDExtension() const {
        return (pesdata()[4]<<8) | pesdata()[5];
    }

    // reserved             2       6.0      40

    // version_number       5       6.2      42
    // incremented modulo 32 when table info changes
    unsigned int Version() const { return (pesdata()[6]>>2) & 0x1f; }

    // current_next_ind     1       6.7      47
    // if 0 this table is not yet valid, but will be the next psip
    // table with the same sectionNumber(), tableIDExtension() and
    // tableID() to become valid.
    bool IsCurrent() const { return bool(pesdata()[6]&1); }

    // section_number       8       7.0      48
    unsigned int Section() const { return pesdata()[7]; }

    // last_section_number  8       8.0      56
    unsigned int LastSection() const { return pesdata()[8]; }

    // this is only for real ATSC PSIP tables, not similar MPEG2 tables
    // protocol_version     8       9.0      64   should always be 0 for now
    unsigned int ProtocolVersion() const { return pesdata()[9]; }

    // PSIP_table_data      x       9.0      72 (incl. protocolVersion)
    const unsigned char* psipdata() const { return pesdata()+PSIP_OFFSET; }
    unsigned char* psipdata() { return pesdata()+PSIP_OFFSET; }

    // sets
    void SetTableID(unsigned int id) { SetStreamID(id); }
    // subtracts 3 from total section length to account for 3 bytes
    // before the end of the section length field.
    void SetSectionLength(unsigned int length) { SetLength(length-3); }
    void SetTableIDExtension(unsigned int len) {
        pesdata()[4] = (len>>8) & 0xff;
        pesdata()[5] = len & 0xff;
    }
    void SetVersionNumber(unsigned int ver) {
        pesdata()[6] = ((ver&0x1f)<<2) | (pesdata()[6] & 0x83);
    }
    void SetCurrent(bool cur) {
        pesdata()[6] = (pesdata()[6]&(0xff-0x80)) | (cur ? 0x80:0);
    }
    void SetSection(unsigned int num) { pesdata()[7] = num; }
    void SetLastSection(unsigned int num) { pesdata()[8] = num; }

    // only for real ATSC PSIP tables, not similar MPEG2 tables
    //void setProtocolVersion(int ver) { pesdata()[7] = ver; }

    const QString toString() const;

    static const unsigned int PSIP_OFFSET=9; // general PSIP header offset
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

class ProgramAssociationTable : public PSIPTable {
    static ProgramAssociationTable* CreateBlank() {
        TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
        memcpy(tspacket->data()+tspacket->AFCOffset(), init_patheader, PSIP_OFFSET);
        PSIPTable psip = PSIPTable::View(*tspacket);
        psip.SetLength(PSIP_OFFSET);
        ProgramAssociationTable *pat = new ProgramAssociationTable(psip);
        delete tspacket;
        return pat;
    }
  public:
    ProgramAssociationTable(const PSIPTable &table) : PSIPTable(table) {
        assert(0x00==TableID());
    }

    // transport stream ID, program ID, count
    static ProgramAssociationTable* Create(uint tsid, uint version,
                                           const vector<uint>& pnum,
                                           const vector<uint>& pid);

    unsigned int TransportStreamID() const {
        return TableIDExtension();
    }

    unsigned int ProgramCount() const {
        return (SectionLength()-PSIP_OFFSET-3)>>2;
    }

    unsigned int ProgramNumber(unsigned int i) const {
        return (psipdata()[(i<<2)] << 8) | psipdata()[(i<<2)+1];
    }

    unsigned int ProgramPID(unsigned int i) const {
        return (psipdata()[(i<<2)+2]&0x1f) << 8 | psipdata()[(i<<2)+3];
    }

    void SetTranportStreamID(unsigned int gtsid) {
        SetTableIDExtension(gtsid);
    }

    // helper function
    unsigned int FindPID(unsigned int progNum) const {
        for (unsigned int i=0; i<ProgramCount(); i++)
            if (progNum==ProgramNumber(i))
                return ProgramPID(i);
        return 0;
    }
    unsigned int FindAnyPID() const {
        for (unsigned int i=0; i<ProgramCount(); i++)
            if (0!=ProgramNumber(i))
                return ProgramPID(i);
        return 0;
    }
    unsigned int FindProgram(unsigned int pid) const {
        for (unsigned int i=0; i<ProgramCount(); i++)
            if (pid==ProgramPID(i))
                return ProgramNumber(i);
        return 0;
    }

    const QString toString() const;
};

extern const unsigned char DEFAULT_PMT_HEADER[9];

/** \class ProgramMapTable
 *  \brief A PMT table maps a program described in the ProgramAssociationTable
 *         to various PID's which describe the multimedia contents of the
 *         program.
 */
class ProgramMapTable : public PSIPTable {
    static const unsigned int pmt_header=4; // minimum PMT header offset
    mutable vector<unsigned char*> _ptrs; // used to parse

    static ProgramMapTable* CreateBlank() {
        TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
        memcpy(tspacket->data()+tspacket->AFCOffset(), DEFAULT_PMT_HEADER, PSIP_OFFSET);
        PSIPTable psip = PSIPTable::View(*tspacket);
        psip.SetLength(PSIP_OFFSET);
        ProgramMapTable *pmt = new ProgramMapTable(psip);
        delete tspacket;
        return pmt;
    }
  public:
    ProgramMapTable(const PSIPTable& table) :
        PSIPTable(table) {
        assert(TableID::PMT==TableID());
        Parse();
    }

    static ProgramMapTable* Create(uint programNumber, uint basepid,
                                   uint pcrpid, uint version,
                                   vector<uint> pids, vector<uint> types);
        
    /// stream that contrains program clock reference.
    unsigned int PCRPID() const {
        return ((psipdata()[0] << 8) | psipdata()[1]) & 0x1fff;
    }

    unsigned int ProgramNumber() const { return TableIDExtension(); }

    unsigned int ProgramInfoLength() const {
        return ((psipdata()[2]<<8) | psipdata()[3]) & 0x0fff;
    }

    const unsigned char* ProgramInfo() const { return pesdata()+13; }

    unsigned int StreamType(unsigned int i) const { return _ptrs[i][0]; }

    unsigned int StreamPID(unsigned int i) const {
        return ((_ptrs[i][1] << 8) | _ptrs[i][2]) & 0x1fff;
    }

    unsigned int StreamInfoLength(unsigned int i) const {
        return ((_ptrs[i][3] << 8) | _ptrs[i][4]) & 0x0fff;
    }

    const unsigned char* StreamInfo(unsigned int i) const {
        return _ptrs[i]+5;
    }

    unsigned int StreamCount() const {
        return (_ptrs.size()) ? _ptrs.size()-1 : 0;
    }

    // sets
    void SetPCRPID(unsigned int pid) {
        psipdata()[0] = ((pid >> 8) & 0x1F) | (psipdata()[0] & 0xE0);
        psipdata()[1] = (pid & 0xFF);
    }

    void SetProgramNumber(unsigned int num) { SetTableIDExtension(num); }

    void SetStreamPID(unsigned int i, unsigned int pid) {
        _ptrs[i][1] = ((pid>>8) & 0x1f) | (_ptrs[i][1] & 0xe0);
        _ptrs[i][2] = pid & 0xff;
    }

    void SetStreamType(unsigned int i, unsigned int type) {
        _ptrs[i][0] = type;
    }

    // helper methods
    const QString StreamTypeString(unsigned int i) const;
    uint FindPIDs(uint type, vector<uint>& pids) const {
        for (uint i=0; i < StreamCount(); i++) {
            if (type == StreamType(i)) {
                pids.push_back(StreamPID(i));
            }
        }
        return pids.size();
    }

    unsigned int FindPID(unsigned int pid) const {
        for (unsigned int i=0; i<StreamCount(); i++) {
            if (pid==StreamPID(i)) {
                return i;
            }
        }
        return static_cast<unsigned int>(-1);
    }

    void RemoveAllStreams() {
        memset(psipdata(), 0xff, pmt_header);
        SetProgramInfoLength(0);
        _ptrs.clear();
    }
    void AppendStream(unsigned int pid, unsigned int type,
                      unsigned char* streamInfo=0, unsigned int infoLength=0);

    void Parse() const;
    const QString toString() const;
    // unsafe sets
  private:
    void SetStreamInfoLength(unsigned int i, unsigned int length) {
        _ptrs[i][3] = ((length>>8) & 0x0f) |
            (_ptrs[i][3] & 0xf0);
        _ptrs[i][4] = length & 0xff;
    }

    void SetStreamProgramInfo(unsigned int i, unsigned char* streamInfo,
                              unsigned int infoLength) {
        SetStreamInfoLength(i, infoLength);
        memcpy(_ptrs[i]+5, streamInfo, infoLength);
    }

    void SetProgramInfoLength(unsigned int length) {
        psipdata()[2] = ((length<<8) & 0x0f) |
            (psipdata()[2] & 0xf0);
        psipdata()[3] = length & 0xff;
    }
};

/** \class AdaptationFieldControl
 *  \brief AdaptationFieldControl is used to transmit various important
 *         stream attributes.
 *   These include such things as the PCR and flags discontiunities in the
 *   program, such as when a commercial or another program begins. This is
 *   currently just passed through the MythTV recorders to the recorded
 *   stream.
 */
class AdaptationFieldControl {
  public:
    AdaptationFieldControl(const unsigned char* packet) : _data(packet) { ; }

    /** adaptation header length
     * (after which is payload data)         8   0.0
     */
    unsigned int Length() const { return _data[0]; }

    /** discontinuity_indicator
     *  (time base may change)               1   1.0
     */
    bool Discontinuity() const { return _data[1]&0x80; }
    // random_access_indicator (?)           1   1.1
    bool RandomAccess() const { return bool(_data[1]&0x40); }
    // elementary_stream_priority_indicator  1   1.2
    bool Priority() const { return bool(_data[1]&0x20); }

// Each of the following extends the adaptation header.  In order:

    /** PCR flag (we have PCR data)          1   1.3
     *  (adds 6 bytes after adaptation header)
     */
    bool PCR() const { return bool(_data[1]&0x10); }
    /** OPCR flag (we have OPCR data)        1   1.4
     *  (adds 6 bytes) ((Original) Program Clock Reference; used to time output)
     */
    bool OPCR() const { return bool(_data[1]&0x08); }
    /** splicing_point_flag                  1   1.5
     *  (adds 1 byte) (we have splice point data)
     *  Splice data is packets until a good splice point for
     *  e.g. commercial insertion -- if these are still set,
     *  might be a good way to recognize potential commercials
     *  for flagging.
     */
    bool SplicingPoint() const { return bool(_data[1]&0x04); }
    //  transport_private_data_flag          1   1.6
    // (adds 1 byte)
    bool PrivateTransportData() const { return bool(_data[1]&0x02); }
    // adaptation_field_extension_flag       1   1.7
    bool FieldExtension() const { return bool(_data[1]&0x1); }
    // extension length                      8   2.0
    // (adds extensionLength() bytes)
    unsigned int ExtensionLength() const { return _data[2]; }
    // ltw flag                              1   3.0
    // (adds 2 bytes)
    bool LTW() const { return bool(_data[3]&0x80); }
    // piecewise_rate_flag (adds 3 bytes)    1   3.1
    bool PiecewiseRate() const { return bool(_data[3]&0x40); }
    // seamless_splice_flag (adds 5 bytes)   1   3.2
    bool SeamlessSplice() const { return bool(_data[3]&0x20); }
    // unused flags                          5   3.3

  private:
    const unsigned char* _data;
};

#endif
