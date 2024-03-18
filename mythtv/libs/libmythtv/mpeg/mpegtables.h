// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef MPEG_TABLES_H
#define MPEG_TABLES_H

#include <cassert>
#include <utility>

// MythTV headers
#include "libmythbase/stringutil.h" // for xml_indent
#include "libmythtv/mpeg/mpegdescriptors.h"
#include "libmythtv/mpeg/pespacket.h"
#include "libmythtv/mythtvexp.h"

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

/** Seconds between start of GPS time and the start of UNIX time.
 *  i.e. from Jan 1st, 1970 UTC to Jan 6th, 1980 UTC
 */
static constexpr qint64 GPS_EPOCH { 315964800 };

/** Leap seconds as of June 30th, 2022. */
static constexpr uint8_t GPS_LEAP_SECONDS { 18 };
// Note: You can obtain this number by adding one
// for every leap second added to UTC since Jan 6th, 1980
// and subtracting one for every leap second removed.
// See http://en.wikipedia.org/wiki/Leap_second


/** \class PESStreamID
 *  \brief Contains a listing of PES Stream ID's for various PES Packet types.
 */
class MTV_PUBLIC PESStreamID
{
  public:
    enum : std::uint8_t
    {
        PictureStartCode        = 0x00,
        SliceStartCodeBegin     = 0x01,
        SliceStartCodeEnd       = 0xaf,
        DVBECMData              = 0xb0,
        DVBEMMData              = 0xb1,
        UserData                = 0xb2,
        /// Sequence (SEQ) start code contains frame size, aspect ratio and fps
        SequenceStartCode       = 0xb3,
        SequenceError           = 0xb4,
        /// Followed by an extension byte, not documented here
        MPEG2ExtensionStartCode = 0xb5,
        MPEGReservedB6          = 0xb6,
        SEQEndCode              = 0xb7,
        /// Group of Pictures (GOP) start code. Tells us how to
        /// reorder frames from transmitted order to display order.
        /// Required in MPEG-1, but optional in MPEG-2.
        GOPStartCode            = 0xb8,
        ProgramEndCode          = 0xb9,
        PackHeader              = 0xba,
        SystemHeader            = 0xbb,
        ProgramStreamMap        = 0xbc,
        /// Non-MPEG audio & subpictures (w/ext hdr)
        NonMPEGAudioVideo       = 0xbd,
        PaddingStream           = 0xbe,
        /// DVD navigation data
        DVDNavigation           = 0xbf,
        /// First MPEG-1/2 audio stream (w/ext hdr)
        MPEGAudioStreamBegin    = 0xc0,
        /// Last MPEG-1/2 audio stream (w/ext hdr)
        MPEGAudioStreamEnd      = 0xdf,
        /// First MPEG-1/2 video stream (w/ext hdr)
        MPEGVideoStreamBegin    = 0xe0,
        /// Last MPEG-1/2 video stream (w/ext hdr)
        MPEGVideoStreamEnd      = 0xef,
        ECMData                 = 0xf0,
        EMMData                 = 0xf1,
        DSMCCData               = 0xf2,
        /// Data as defined in ISO and IEC standard number 13522
        Data13522               = 0xf3,
        /// First Data as defined in ITU-T recomendation H.222.1
        DataH2221Begin          = 0xf4,
        /// Last Data as defined in ITU-T recomendation H.222.1
        DataH2221End            = 0xf8,
        AncillaryData           = 0xf9,
        MPEGReservedFA          = 0xfa,
        FlexMux                 = 0xfb, // MPEG-2 13818-1
        MPEGReservedFC          = 0xfc,
        MPEGReservedFD          = 0xfd,
        MPEGReservedFE          = 0xfe,
        ProgramStreamDirectory  = 0xff, // MPEG-2 13818-1
    };
};


/** \class StreamID
 *  \brief Contains listing of PMT Stream ID's for various A/V Stream types.
 *
 *   This is used by the Program Map Table (PMT), and often used by other
 *   tables, such as the DVB SDT table.
 */
class MTV_PUBLIC StreamID
{
  public:
    enum
    {
        // video
        MPEG1Video     = 0x01, ///< ISO 11172-2 (aka MPEG-1)
        MPEG2Video     = 0x02, ///< ISO 13818-2 & ITU H.262 (aka MPEG-2)
        MPEG4Video     = 0x10, ///< ISO 14492-2 (aka MPEG-4)
        H264Video      = 0x1b, ///< ISO 14492-10 & ITU H.264 (aka MPEG-4-AVC)
        H265Video      = 0x24, ///< ISO 23008-2 & ITU H.265 (aka HEVC, Ultra HD)
        OpenCableVideo = 0x80, ///< Always MPEG-2??
        VC1Video       = 0xea, ///< SMPTE 421M video codec (aka VC1) in Blu-Ray

        // audio
        MPEG1Audio     = 0x03, ///< ISO 11172-3
        MPEG2Audio     = 0x04, ///< ISO 13818-3
        MPEG2AACAudio  = 0x0f, ///< ISO 13818-7 Audio w/ADTS syntax
        MPEG2AudioAmd1 = 0x11, ///< ISO 13818-3/AMD-1 Audio using LATM syntax
        AC3Audio       = 0x81, ///< A/53 Part 3:2009 6.7.1
        EAC3Audio      = 0x87, ///< A/53 Part 3:2009 6.7.3
        DTSAudio       = 0x8a,

        // DSM-CC Object Carousel
        DSMCC          = 0x08, ///< ISO 13818-1 Annex A DSM-CC & ITU H.222.0
        DSMCC_A        = 0x0a, ///< ISO 13818-6 type A Multi-protocol Encap
        DSMCC_B        = 0x0b, ///< ISO 13818-6 type B Std DSMCC Data
        DSMCC_C        = 0x0c, ///< ISO 13818-6 type C NPT DSMCC Data
        DSMCC_D        = 0x0d, ///< ISO 13818-6 type D Any DSMCC Data
        DSMCC_DL       = 0x14, ///< ISO 13818-6 Download Protocol
        MetaDataPES    = 0x15, ///< Meta data in PES packets
        MetaDataSec    = 0x16, ///< Meta data in metadata_section's
        MetaDataDC     = 0x17, ///< ISO 13818-6 Metadata in Data Carousel
        MetaDataOC     = 0x18, ///< ISO 13818-6 Metadata in Object Carousel
        MetaDataDL     = 0x19, ///< ISO 13818-6 Metadata in Download Protocol

        // other
        PrivSec        = 0x05, ///< ISO 13818-1 private tables   & ITU H.222.0
        PrivData       = 0x06, ///< ISO 13818-1 PES private data & ITU H.222.0

        MHEG           = 0x07, ///< ISO 13522 MHEG
        H222_1         = 0x09, ///< ITU H.222.1

        MPEG2Aux       = 0x0e, ///< ISO 13818-1 auxiliary & ITU H.222.0

        FlexMuxPES     = 0x12, ///< ISO 14496-1 SL/FlexMux in PES packets
        FlexMuxSec     = 0x13, ///< ISO 14496-1 SL/FlexMux in 14496_sections

        MPEG2IPMP      = 0x1a, ///< ISO 13818-10 Digital Restrictions Mangment
        MPEG2IPMP2     = 0x7f, ///< ISO 13818-10 Digital Restrictions Mangment

        Splice         = 0x86, ///< ANSI/SCTE 35 2007

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
                (StreamID::H265Video  == type) ||
                (StreamID::VC1Video   == type) ||
                (StreamID::OpenCableVideo == type));
    }
    /// Returns true iff audio is MPEG1/2, AAC, AC3 or DTS audio stream.
    static bool IsAudio(uint type)
    {
        return ((StreamID::MPEG1Audio     == type) ||
                (StreamID::MPEG2Audio     == type) ||
                (StreamID::MPEG2AudioAmd1 == type) ||
                (StreamID::MPEG2AACAudio  == type) ||
                (StreamID::AC3Audio       == type) ||
                (StreamID::EAC3Audio      == type) ||
                (StreamID::DTSAudio       == type));
    }
    /// Returns true iff stream contains DSMCC Object Carousel
    static bool IsObjectCarousel(uint type)
    {
        return ((StreamID::DSMCC_A == type) ||
                (StreamID::DSMCC_B == type) ||
                (StreamID::DSMCC_C == type) ||
                (StreamID::DSMCC_D == type));
    }
    static uint Normalize(uint stream_id, const desc_list_t &desc,
                          const QString &sistandard);
    static const char* toString(uint streamID);
    static QString GetDescription(uint stream_id);
};

/** \class PID
 *  \brief Contains Packet Identifier numeric values.
 */
class PID
{
  public:
    enum : std::uint16_t
    {
        MPEG_PAT_PID  = 0x0000,
        MPEG_CAT_PID  = 0x0001,
        MPEG_TSDT_PID = 0x0002,

        DVB_NIT_PID   = 0x0010,
        DVB_SDT_PID   = 0x0011,
        DVB_EIT_PID   = 0x0012,
        DVB_RST_PID   = 0x0013,
        DVB_TDT_PID   = 0x0014,

        // Dishnet longterm EIT is on pid 0x300
        DVB_DNLONG_EIT_PID = 0x0300,

        // Bell longterm EIT is on pid 0x441
        DVB_BVLONG_EIT_PID = 0x0441,

        // MCA
        MCA_EIT_PID        = 0x03fa,    // 1018

        // Premiere EIT for Direkt/Sport PPV
        PREMIERE_EIT_DIREKT_PID = 0x0b11,
        PREMIERE_EIT_SPORT_PID  = 0x0b12,


        ATSC_PSIP_PID = 0x1ffb,

        SCTE_PSIP_PID = 0x1ffc,

        // UK Freesat PIDs: SDTo/BAT, longterm EIT, shortterm EIT
        FREESAT_SI_PID     = 0x0f01,
        FREESAT_EIT_PID    = 0x0f02,    // 3842
        FREESAT_ST_EIT_PID = 0x0f03,

        /// The all-ones PID value 0x1FFF indicates a Null TS Packet
        /// introduced to maintain a constant bit rate of a TS Multiplex.
        MPEG_NULL_PID      = 0x1fff,

        // OpenTV EIT PIDs
        OTV_EIT_TIT_PID_START  = 0x30,
        OTV_EIT_TIT_PID_END    = 0x37,
        OTV_EIT_SUP_PID_START  = 0x40,
        OTV_EIT_SUP_PID_END    = 0x47,
    };
};

/** \class TableID
 *  \brief Contains listing of Table ID's for various tables (PAT=0,PMT=2,etc).
 */
class MTV_PUBLIC TableID
{
  public:
    enum : std::uint8_t
    {
        PAT      = 0x00, // always on pid 0x00
        CAT      = 0x01, // always on pid 0x01
        PMT      = 0x02,
        TSDT     = 0x03, // always on pid 0x02

        // DVB mandatory
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

        // DVB Conditional Access
        DVBCAbeg = 0x80,
        DVBCA_81 = 0x81,
        DVBCA_82 = 0x82,
        DVBCA_83 = 0x83,
        DVBCA_84 = 0x84,
        DVBCA_85 = 0x85,
        DVBCA_86 = 0x86,
        DVBCA_87 = 0x87,
        DVBCA_88 = 0x88,
        DVBCA_89 = 0x89,
        DVBCA_8a = 0x8a,
        DVBCA_8b = 0x8b,
        DVBCA_8c = 0x8c,
        DVBCA_8d = 0x8d,
        DVBCA_8e = 0x8e,
        DVBCAend = 0x8f,

        // Dishnet Longterm EIT data
        DN_EITbego = 0x80, // always on pid 0x300
        DN_EITendo = 0xfe, // always on pid 0x300

        // ARIB
        ARIBbeg  = 0x80,
        ARIBend  = 0x8f,

        // SCTE 57 -- old depreciated standard
        PIM      = 0xC0, // Program Information Message (57 2003) PMT PID
        PNM      = 0xC1, // Program Name Message (57 2003) PMT PID
        // The PIM and PNM do not have SCTE 65 equivalents.

        // Under SCTE 57 -- NIM, NTM, VCM, and STM were carried on the
        // network PID (Table ID 0x0) as defined in the PAT, but under
        // SCTE 65 the equivalent tables are carried on 0x1FFC

        NIM      = 0xC2, // Network Information Message (57 2003)
        NTM      = 0xC3, // Network Text Message (57 2003)
        // The NTM is like the SCTE 65 NTT table, except that it
        // carries more subtable types and the id for the source name
        // subtable they both share is type 5 rather than 6 which can
        // cause calimity if not dealt with properly.
        VCM      = 0xC4, // Virtual Channel Message (57 2003)
        STM      = 0xC5, // System Time Message (57 2003)

        // SCTE 65 -- Current US Cable TV standard
        NITscte  = 0xC2, // Network Information Table (NIT) (65 2002) on 0x1FFC
        NTT      = 0xC3, // Network Text Table (NTT) (65 2002) on 0x1FFC
        SVCTscte = 0xC4, // Short Virtual Channel Table (65 2002) on 0x1FFC
        STTscte  = 0xC5, // System Time Table (STT) (65 2002) on 0x1FFC

        // Other SCTE
        SM       = 0xC6, // subtitle_message (27 2003)
        CEA      = 0xD8, // Cable Emergency Alert (18 2002)
        ADET     = 0xD9, // Aggregate Data Event Table (80 2002)

        // SCTE 35
        SITscte  = 0xFC, // SCTE 35 Splice Info Table (Cueing messages)

        // ATSC Conditional Access (A/70)
        // DVB Conditional Access (TS 100 289)
        ECM0     = 0x80,
        ECM1     = 0x81,
        ECMbeg   = 0x82, // ECM begin private data
        ECMend   = 0x8f, // ECM end private data

        // ATSC main
        MGT      = 0xC7, // Master Guide Table A/65 on 0x1ffb
        TVCT     = 0xC8, // Terrestrial Virtual Channel Table A/65 on 0x1ffb
        CVCT     = 0xC9, // Cable Virtual Channel Table A/65 on 0x1ffb
        RRT      = 0xCA, // Region Rating Table A/65 on 0x1ffb
        EIT      = 0xCB, // Event Information Table A/65 (per MGT)
        ETT      = 0xCC, // Extended Text Table A/65 (per MGT)
        STT      = 0xCD, // System Time Table A/65
        DET      = 0xCE, // Data Event Table A/90 (per MGT)
        DST      = 0xCF, // Data Service Table A/90

        PIT      = 0xD0, // Program ID Table ???
        NRT      = 0xD1, // Network Resources Table A/90
        LTST     = 0xD2, // Long Term Service Table A/90
        DCCT     = 0xD3, // Directed Channel Change Table A/57 on 0x1ffb
        DCCSCT   = 0xD4, // DCC Selection Code Table A/57 on 0x1ffb
        SITatsc  = 0xD5, // Selection Information Table (EIA-775.2 2000)
        AEIT     = 0xD6, // Aggregate Event Information Table A/81, SCTE 65
        AETT     = 0xD7, // Aggregate Extended Text Table A/81, SCTE 65
        SVCT     = 0xDA, // Satellite VCT A/81

        SRM      = 0xE0, // System Renewability Message (ATSC TSG-717r0)

        // Unknown
        STUFFING = 0x80,
        CAPTION  = 0x86,
        CENSOR   = 0x87,

        // Private premiere.de
        PREMIERE_CIT = 0xA0,
        PREMIERE_CPT = 0xA1,

        ECN      = 0xA0,
        SRVLOC   = 0xA1,
        TSS      = 0xA2,
        CMPNAME  = 0xA3,

        // OpenTV
        OTV_EIT_TITbeg  = 0xA0, // always on pids config (0x30-0x37)
        OTV_EIT_TITend  = 0xA3, // always on pids config (0x30-0x37)
        OTV_EIT_SUMbeg  = 0xA8, // always on pids config (0x40-0x47)
        OTV_EIT_SUMend  = 0xAB, // always on pids config (0x40-0x47)
    };
};

/** \class PSIPTable
 *  \brief A PSIP table is a variant of a PES packet containing an
 *         MPEG, ATSC or DVB table.
 */
class MTV_PUBLIC PSIPTable : public PESPacket
{
    /// Only handles single TS packet PES packets, for PMT/PAT tables basically
    void InitPESPacket(TSPacket& tspacket)
    {
        if (tspacket.PayloadStart())
            m_psiOffset = tspacket.AFCOffset() + tspacket.StartOfFieldPointer();
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Started PESPacket, but !payloadStart()");
            m_psiOffset = tspacket.AFCOffset();
        }
        m_pesData = tspacket.data() + m_psiOffset + 1;

        m_badPacket = true;
        // first check if Length() will return something useful and
        // then check if the packet ends in the first TSPacket
        if ((m_pesData - tspacket.data()) <= (188-3) &&
            (m_pesData + Length() - tspacket.data()) <= (188-3))
        {
            m_badPacket = !VerifyCRC();
        }
    }

  protected:
    // does not create it's own data
    PSIPTable(const TSPacket& tspacket, bool /*unused*/)
    {
        m_pesData = nullptr;
        m_fullBuffer = nullptr;
        m_ccLast = tspacket.ContinuityCounter();
        m_allocSize = 0;
        InitPESPacket(const_cast<TSPacket&>(tspacket));
        m_fullBuffer = const_cast<unsigned char*>(tspacket.data());
        m_pesDataSize = TSPacket::kSize - (m_pesData - m_fullBuffer);
    }

  public:
    // may be modified
    PSIPTable(const TSPacket &tspacket,
              const unsigned char *pesdata, uint pes_size)
    { // clone
        m_ccLast = tspacket.ContinuityCounter();
        m_pesDataSize = pes_size;
        InitPESPacket(const_cast<TSPacket&>(tspacket)); // sets m_psiOffset
        int len     = pes_size+4;
        /* make alloc size multiple of 188 */
        m_allocSize  = ((len+m_psiOffset+187)/188)*188;
        m_fullBuffer = pes_alloc(m_allocSize);
        m_pesData    = m_fullBuffer + m_psiOffset + 1;
        memcpy(m_fullBuffer, tspacket.data(), 188);
        memcpy(m_pesData, pesdata, pes_size-1);
    }

  public:
    /// Constructor for viewing a section, does not create it's own data
    explicit PSIPTable(const unsigned char *pesdata)
        : PESPacket(pesdata)
    {
        // fixup wrong assumption about length for sections without CRC
        m_pesDataSize = SectionLength();
    }
    explicit PSIPTable(const std::vector<uint8_t> &pesdata)
        : PESPacket(pesdata)
    {
        // fixup wrong assumption about length for sections without CRC
        m_pesDataSize = SectionLength();
    }
  public:
    PSIPTable(const PSIPTable&) = default;
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    explicit PSIPTable(const PESPacket& table) : PESPacket(table)
    {
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1
    }
    // may be modified
    explicit PSIPTable(const TSPacket& table)
    {
        // section_syntax_ind   1       1.0       8   should always be 1
        // private_indicator    1       1.1       9   should always be 1

        m_ccLast = table.ContinuityCounter();
        m_pesDataSize = 188;

        // clone
        InitPESPacket(const_cast<TSPacket&>(table)); // sets m_psiOffset

        int len     = (4*1024) - 256; /* ~4KB */
        m_allocSize  = len + m_psiOffset;
        m_fullBuffer = pes_alloc(m_allocSize);
        m_pesData    = m_fullBuffer + m_psiOffset + 1;
        memcpy(m_fullBuffer, table.data(), TSPacket::kSize);
    }


    static PSIPTable View(const TSPacket& tspacket)
        { return {tspacket, false}; }

    static PSIPTable ViewData(const unsigned char* pesdata)
        { return PSIPTable(pesdata); }

    // Section            Bits   Start Byte sbit
    // -----------------------------------------
    // table_id             8       0.0       0
    uint TableID(void) const { return StreamID(); }

    // section_syntax_ind   1       1.0       8   ATSC -- should always be 1
    bool SectionSyntaxIndicator(void) const { return ( pesdata()[1] & 0x80 ) != 0; }
    // private_indicator    1       1.1       9
    bool PrivateIndicator(void) const { return ( pesdata()[1] & 0x40 ) != 0; }
    // reserved             2       1.2      10

    // section_length      12       1.4      12   always less than 0x3fd
    // adds 3 to the total section length to account for 3 bytes
    // before the end of the section length field.
    uint SectionLength(void) const { return Length() + 3; }
    uint SectionLengthRaw(void) const
        { return ((pesdata()[1]<<8) | pesdata()[2]) & 0x0fff; }

    ////////////////////////////////////////////////////////////
    // Things below this line may not apply to SCTE/DVB tables.

    // table_id_extension  16       3.0      24   table dependent
    uint TableIDExtension(void) const
        { return (pesdata()[3]<<8) | pesdata()[4]; }

    // reserved             2       5.0      40

    // version_number       5       5.2      42
    // incremented modulo 32 when table info changes
    uint Version(void) const { return (pesdata()[5]>>1) & 0x1f; }

    // current_next_ind     1       5.7      47
    // if 0 this table is not yet valid, but will be the next psip
    // table with the same sectionNumber(), tableIDExtension() and
    // tableID() to become valid.
    bool IsCurrent(void) const { return bool(pesdata()[5]&1); }

    // section_number       8       6.0      48
    uint Section(void) const { return pesdata()[6]; }

    // last_section_number  8       7.0      56
    uint LastSection(void) const { return pesdata()[7]; }

    // Protocol Version for ATSC PSIP tables
    // protocol_version     8       8.0      64   should always be 0 for now
    uint ATSCProtocolVersion(void) const { return pesdata()[8]; }

    // PSIP_table_data      x       8.0      72 (incl. protocolVersion)
    const unsigned char* psipdata(void) const
        { return pesdata() + kPsipOffset; }
    unsigned char* psipdata(void)
        { return pesdata() + kPsipOffset; }

    // sets
    void SetTableID(uint id) { SetStreamID(id); }
    // subtracts 3 from total section length to account for 3 bytes
    // before the end of the section length field.
    void SetSectionLength(uint length) { SetLength(length-3); }
    void SetTableIDExtension(uint len)
    {
        pesdata()[3] = (len>>8) & 0xff;
        pesdata()[4] = len & 0xff;
    }
    void SetVersionNumber(uint ver)
        { pesdata()[5] = (pesdata()[5] & 0xc1) | ((ver & 0x1f)<<1); }
    void SetCurrent(bool cur)
        { pesdata()[5] = (pesdata()[5] & 0xfe) | (cur ? 1 : 0); }
    void SetSection(uint num) { pesdata()[6] = num; }
    void SetLastSection(uint num) { pesdata()[7] = num; }

    // Only for real ATSC PSIP tables.
    void SetATSCProtocolVersion(int ver) { pesdata()[8] = ver; }

    bool HasCRC(void) const override; // PESPacket
    bool HasSectionNumber(void) const;

    bool VerifyPSIP(bool verify_crc) const;

    virtual QString toString(void) const;
    virtual QString toStringXML(uint indent_level) const;

    static const uint kPsipOffset = 8; // general PSIP header offset

  protected:
    QString XMLValues(uint indent_level) const;
};

/** \class ProgramAssociationTable
 *  \brief The Program Association Table lists all the programs
 *         in a stream, and is always found on PID 0.
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

class MTV_PUBLIC ProgramAssociationTable : public PSIPTable
{
  public:
    ProgramAssociationTable(const ProgramAssociationTable& table)
        : PSIPTable(table)
    {
        assert(TableID::PAT == TableID());
    }

    explicit ProgramAssociationTable(const PSIPTable &table) : PSIPTable(table)
    {
        assert(TableID::PAT == TableID());
    }

    // transport stream ID, program ID, count
    static ProgramAssociationTable* Create(uint tsid, uint version,
                                           const std::vector<uint>& pnum,
                                           const std::vector<uint>& pid);

    uint TransportStreamID(void) const { return TableIDExtension(); }

    uint ProgramCount(void) const
    {
        if (SectionLength() > (kPsipOffset+2))
            return (SectionLength()-kPsipOffset-2)>>2;
        return 0;
    }

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

    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable

  private:
    static ProgramAssociationTable* CreateBlank(bool smallPacket = true);

    // for testing if CreateBlank create a table with invalid CRC
    friend class TestMPEGTables;
};

/** \class ProgramMapTable
 *  \brief A PMT table maps a program described in the ProgramAssociationTable
 *         to various PID's which describe the multimedia contents of the
 *         program.
 */
class MTV_PUBLIC ProgramMapTable : public PSIPTable
{
  public:

    ProgramMapTable(const ProgramMapTable& table) : PSIPTable(table)
    {
        assert(TableID::PMT == TableID());
        Parse();
    }

    explicit ProgramMapTable(const PSIPTable& table) : PSIPTable(table)
    {
        assert(TableID::PMT == TableID());
        Parse();
    }

    static ProgramMapTable ViewData(const unsigned char* pesdata)
    {
        /* needed to call Parse() */
        return ProgramMapTable(PSIPTable::ViewData(pesdata));
    }

    static ProgramMapTable* Create(uint programNumber, uint basepid,
                                   uint pcrpid, uint version,
                                   std::vector<uint> pids, std::vector<uint> types);

    static ProgramMapTable* Create(uint programNumber, uint basepid,
                                   uint pcrpid, uint version,
                                   const desc_list_t         &global_desc,
                                   const std::vector<uint>        &pids,
                                   const std::vector<uint>        &types,
                                   const std::vector<desc_list_t> &prog_desc);

    /// stream that contains program clock reference.
    uint PCRPID(void) const
        { return ((psipdata()[0] << 8) | psipdata()[1]) & 0x1fff; }

    uint ProgramNumber(void) const
        { return TableIDExtension(); }

    uint ProgramInfoLength(void) const
        { return ((psipdata()[2]<<8) | psipdata()[3]) & 0x0fff; }

    const unsigned char* ProgramInfo(void) const
        { return psipdata() + 4; }

    uint StreamType(uint i) const
        { return m_ptrs[i][0]; }

    uint StreamPID(uint i) const
        { return ((m_ptrs[i][1] << 8) | m_ptrs[i][2]) & 0x1fff; }

    uint StreamInfoLength(uint i) const
        { return ((m_ptrs[i][3] << 8) | m_ptrs[i][4]) & 0x0fff; }

    const unsigned char* StreamInfo(uint i) const
        { return m_ptrs[i] + 5; }

    uint StreamCount(void) const
        { return (!m_ptrs.empty()) ? m_ptrs.size()-1 : 0; }

    // sets
    void SetPCRPID(uint pid)
    {
        psipdata()[0] = ((pid >> 8) & 0x1F) | (psipdata()[0] & 0xE0);
        psipdata()[1] = (pid & 0xFF);
    }

    void SetProgramNumber(uint num) { SetTableIDExtension(num); }

    void SetStreamPID(uint i, uint pid)
    {
        m_ptrs[i][1] = ((pid>>8) & 0x1f) | (m_ptrs[i][1] & 0xe0);
        m_ptrs[i][2] = pid & 0xff;
    }

    void SetStreamType(uint i, uint type)
        { m_ptrs[i][0] = type; }

    // helper methods
    bool IsVideo(uint i, const QString& sistandard) const;
    bool IsAudio(uint i, const QString& sistandard) const;
    bool IsEncrypted(const QString& sistandard) const;
    bool IsProgramEncrypted(void) const;
    bool IsStreamEncrypted(uint pid) const;
    /// Returns true iff PMT contains a still-picture video stream
    bool IsStillPicture(const QString& sistandard) const;
    /// Returns a string representation of type at stream index i
    QString StreamTypeString(uint i) const
        { return StreamID::toString(StreamType(i)); }
    /// Returns a better (and more expensive) string representation
    /// of type at stream index i than StreamTypeString(uint)
    QString StreamDescription(uint i, const QString& sistandard) const;
    /// Returns the canonical language if we find the iso639 descriptor
    QString GetLanguage(uint i) const;
    /// Returns the audio type from the iso 639 descriptor
    uint GetAudioType(uint i) const;

    uint FindPIDs(uint type, std::vector<uint> &pids,
                  const QString &sistandard) const;
    uint FindPIDs(uint type, std::vector<uint> &pids, std::vector<uint> &types,
                  const QString &sistandard, bool normalize) const;

    /// \brief Locates stream index of pid.
    /// \return stream index if successful, -1 otherwise
    int FindPID(uint pid) const
    {
        for (uint i = 0; i < StreamCount(); i++)
            if (pid == StreamPID(i))
                return i;
        return -1;
    }
    uint FindUnusedPID(uint desired_pid = 0x20) const;

    void RemoveAllStreams(void)
    {
        memset(psipdata(), 0xff, kPmtHeaderMinOffset);
        SetProgramInfoLength(0);
        m_ptrs.clear();
    }
    void AppendStream(uint pid, uint type, unsigned char* streamInfo = nullptr, uint infoLength = 0);

    void Parse(void) const;
    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable
    // unsafe sets
  private:
    void SetStreamInfoLength(uint i, uint length)
    {
        m_ptrs[i][3] = ((length>>8) & 0x0f) | (m_ptrs[i][3] & 0xf0);
        m_ptrs[i][4] = length & 0xff;
    }

    void SetStreamProgramInfo(uint i, unsigned char* streamInfo,
                              uint infoLength)
    {
        SetStreamInfoLength(i, infoLength);
        if ((streamInfo != nullptr) && (infoLength > 0))
            memcpy(m_ptrs[i] + 5, streamInfo, infoLength);
    }

    void SetProgramInfoLength(uint length)
    {
        psipdata()[2] = ((length>>8) & 0x0f) | (psipdata()[2] & 0xf0);
        psipdata()[3] = length & 0xff;
    }

    void SetProgramInfo(unsigned char *streamInfo, uint infoLength)
    {
        SetProgramInfoLength(infoLength);
        memcpy(psipdata() + 4, streamInfo, infoLength);
    }

    static ProgramMapTable* CreateBlank(bool smallPacket = true);

    static const uint kPmtHeaderMinOffset = 4; // minimum PMT header offset
    mutable std::vector<unsigned char*> m_ptrs; // used to parse
};

/** \class ConditionalAccessTable
 *  \brief The CAT is used to transmit additional ConditionalAccessDescriptor
 *         instances, in addition to the ones in the PMTs.
 */
class MTV_PUBLIC ConditionalAccessTable : public PSIPTable
{
  public:
    explicit ConditionalAccessTable(const PSIPTable &table) : PSIPTable(table)
    {
    // Section            Bits   Start Byte sbit
    // -----------------------------------------
    // table_id             8       0.0       0
        assert(TableID::CAT == TableID());
    // section_syntax_ind   1       1.0       8   should always be 1
    // private_indicator    1       1.1       9   should always be 0
    // reserved             2       1.2      10
    // section_length      12       1.4      12   always less than 0x3fd
    // table_id_extension  16       3.0      24   unused
    // reserved             2       5.0      40
    // version_number       5       5.2      42
    // current_next_ind     1       5.3      47
    // section_numben       8       6.0      48
    // last_section_number  8       7.0      56
    }

    // for (i = 0; i < N; i++)      8.0      64
    //   { descriptor() }
    uint DescriptorsLength(void) const
        { return SectionLength() - kPsipOffset; }
    const unsigned char *Descriptors(void) const { return psipdata(); }

    QString toString(void) const override; // PSIPTable
    QString toStringXML(uint indent_level) const override; // PSIPTable

    // CRC_32 32 rpchof
};

class MTV_PUBLIC SpliceTimeView
{
  public:
    explicit SpliceTimeView(const unsigned char *data) : m_data(data) { }
    //   time_specified_flag    1  0.0
    bool IsTimeSpecified(void) const { return ( m_data[0] & 0x80 ) != 0; }
    //   if (time_specified_flag == 1)
    //     reserved             6  0.1
    //     pts_time            33  0.6
    uint64_t PTSTime(void) const
    {
        return ((uint64_t(m_data[0] & 0x1) << 32) |
                (uint64_t(m_data[1])       << 24) |
                (uint64_t(m_data[2])       << 16) |
                (uint64_t(m_data[3])       <<  8) |
                (uint64_t(m_data[4])));
    }
    //   else
    //     reserved             7  0.1
    // }
    virtual QString toString(int64_t first, int64_t last) const;
    virtual QString toStringXML(
        uint indent_level, int64_t first, int64_t last) const;

    uint size(void) const { return IsTimeSpecified() ? 1 : 5; }
  private:
    const unsigned char *m_data;
};

class MTV_PUBLIC SpliceScheduleView
{
  public:
    SpliceScheduleView(std::vector<const unsigned char*> ptrs0,
                       std::vector<const unsigned char*> ptrs1) :
        m_ptrs0(std::move(ptrs0)), m_ptrs1(std::move(ptrs1))
    {
    }
    //   splice_count           8  14.0
    uint SpliceCount(void) const { return std::min(m_ptrs0.size(), m_ptrs1.size()); }

    //   splice_event_id       32  0.0 + m_ptrs0[i]
    uint SpliceEventID(uint i) const
    {
        return ((m_ptrs0[i][0] << 24) | (m_ptrs0[i][1] << 16) |
                (m_ptrs0[i][2] <<  8) | (m_ptrs0[i][3]));
    }
    //   splice_event_cancel_indicator 1 4.0 + m_ptrs0[i]
    //   reserved               7   4.1 + m_ptrs0[i]
    //   if (splice_event_cancel_indicator == ‘0’) {
    //     out_of_network_indicator 1 5.0 + m_ptrs0[i]
    //     program_splice_flag  1 5.1 + m_ptrs0[i]
    //     duration_flag        1 5.2 + m_ptrs0[i]
    //     reserved             5 5.3 + m_ptrs0[i]
    //     if (program_splice_flag == ‘1’)
    //       utc_splice_time   32 6.0 + m_ptrs0[i]
    //     else {
    //       component_count    8 6.0 + m_ptrs0[i]
    //       for(j = 0; j < component_count; j++) {
    //         component_tag    8 7.0 + m_ptrs0[i]+j*5
    //         utc_splice_time 32 8.0 + m_ptrs0[i]+j*5
    //       }
    //     }
    //     if (duration_flag) {
    //       auto_return        1 0.0 + m_ptrs1[i]
    //       reserved           6 0.1 + m_ptrs1[i]
    //       duration          33 0.7 + m_ptrs1[i]
    //     }
    //     unique_program_id   16 0.0 + m_ptrs1[i] + (duration_flag)?5:0
    //     avail_num            8 2.0 + m_ptrs1[i] + (duration_flag)?5:0
    //     avails_expected      8 3.0 + m_ptrs1[i] + (duration_flag)?5:0
    //   }

  private:
    std::vector<const unsigned char*> m_ptrs0;
    std::vector<const unsigned char*> m_ptrs1;
};

class MTV_PUBLIC SpliceInsertView
{
  public:
    SpliceInsertView(std::vector<const unsigned char*> ptrs0,
                     std::vector<const unsigned char*> ptrs1) :
        m_ptrs0(std::move(ptrs0)), m_ptrs1(std::move(ptrs1))
    {
    }

    //   splice_event_id       32   0.0 + m_ptrs1[0]
    uint SpliceEventID(void) const
    {
        return ((m_ptrs1[0][0] << 24) | (m_ptrs1[0][1] << 16) |
                (m_ptrs1[0][2] <<  8) | (m_ptrs1[0][3]));
    }
    //   splice_event_cancel    1    4.0 + m_ptrs1[0]
    bool IsSpliceEventCancel(void) const { return ( m_ptrs1[0][4] & 0x80 ) != 0; }
    //   reserved               7    4.1 + m_ptrs1[0]
    //   if (splice_event_cancel_indicator == 0) {
    //     out_of_network_flag  1    5.0 + m_ptrs1[0]
    bool IsOutOfNetwork(void) const { return ( m_ptrs1[0][5] & 0x80 ) != 0; }
    //     program_splice_flag  1    5.1 + m_ptrs1[0]
    bool IsProgramSplice(void) const { return ( m_ptrs1[0][5] & 0x40 ) != 0; }
    //     duration_flag        1    5.2 + m_ptrs1[0]
    bool IsDuration(void) const { return ( m_ptrs1[0][5] & 0x20 ) != 0; }
    //     splice_immediate_flag 1   5.3 + m_ptrs1[0]
    bool IsSpliceImmediate(void) const { return ( m_ptrs1[0][5] & 0x20 ) != 0; }
    //     reserved             4    5.4 + m_ptrs1[0]
    //     if ((program_splice_flag == 1) && (splice_immediate_flag == ‘0’))
    //       splice_time()   8-38    6.0 + m_ptrs1[0]
    SpliceTimeView SpliceTime(void) const
        { return SpliceTimeView(m_ptrs1[0]+6); }
    //     if (program_splice_flag == 0) {
    //       component_count    8    6.0 + m_ptrs1[0]
    //       for (i = 0; i < component_count; i++) {
    //         component_tag    8    0.0 + m_ptrs0[i]
    //         if (splice_immediate_flag == ‘0’)
    //           splice_time() 8-38  1.0 + m_ptrs0[i]
    //       }
    //     }
    //     if (duration_flag == ‘1’)
    //       auto_return        1    0.0 + m_ptrs1[1]
    //       reserved           6    0.1 + m_ptrs1[1]
    //       duration          33    0.7 + m_ptrs1[1]
    //     unique_program_id   16    0.0 + m_ptrs1[2]
    uint UniqueProgramID(void) const
        { return (m_ptrs1[2][0]<<8) | m_ptrs1[2][1]; }
    //     avail_num            8    2.0 + m_ptrs1[2]
    uint AvailNum(void) const { return m_ptrs1[2][2]; }
    //     avails_expected      8    3.0 + m_ptrs1[2]
    uint AvailsExpected(void) const { return m_ptrs1[2][3]; }
    //   }

    virtual QString toString(int64_t first, int64_t last) const;
    virtual QString toStringXML(
        uint indent_level, int64_t first, int64_t last) const;

  private:
    std::vector<const unsigned char*> m_ptrs0;
    std::vector<const unsigned char*> m_ptrs1;
};

class MTV_PUBLIC SpliceInformationTable : public PSIPTable
{
  public:
    SpliceInformationTable(const SpliceInformationTable &table) :
        PSIPTable(table)
    {
        assert(TableID::SITscte == TableID());
        Parse();
    }
    explicit SpliceInformationTable(const PSIPTable &table) :
        PSIPTable(table)
    {
        assert(TableID::SITscte == TableID());
        Parse();
    }
    ~SpliceInformationTable() override { ; }

    void setSCTEPID(int ts_pid){m_sctePid = ts_pid;}
    int getSCTEPID(void) const {return m_sctePid;}

    // ANCE/SCTE 35 2007
    //       Name             bits  loc  expected value
    // table_id                 8   0.0       0xFC
    // section_syntax_indicator 1   1.0          1
    // private_indicator        1   1.1          0
    // reserved                 2   1.2          3
    // section_length          12   1.4
    // ^^^ All above this line provided by PSIPTable
    // protocol_version         8   3.0          0
    uint SpliceProtocolVersion(void) const { return pesdata()[3]; }
    void SetSpliceProtocolVersion(uint ver) { pesdata()[3] = ver; }
    // encrypted_packet         1   4.0
    bool IsEncryptedPacket(void) const { return ( pesdata()[4] & 0x80 ) != 0; }
    void SetEncryptedPacket(bool val)
    {
        pesdata()[4] = (pesdata()[4] & ~0x80) | ((val) ? 0x80 : 0);
    }
    // encryption_algorithm     6   4.1
    enum : std::uint8_t
    {
        kNoEncryption = 0,
        kECB          = 1, // DES - ECB mode, FIPS PUB 81 (8 byte blocks)
        kCBC          = 2, // DES - CBC mode, FIPS PUB 81 (8 byte blocks)
        k3DES         = 3, // 3 DES - TDEA, FIPS PUB 46-3 (8 byte blocks)
        // values 4-31 are reserved for future extension
        // values 32-63 are user private
    };
    uint EncryptionAlgorithm(void) const { return (pesdata()[4] >> 1) & 0x3f; }
    QString EncryptionAlgorithmString(void) const;
    void SetEncryptionAlgorithm(uint val)
    {
        pesdata()[4] &= 0x81;
        pesdata()[4] |= ((val&0x3f) << 1);
    }
    // pts_adjustment          33   4.7
    uint64_t PTSAdjustment(void) const
    {
        return ((uint64_t(pesdata()[4] & 0x1) << 32) |
                (uint64_t(pesdata()[5])       << 24) |
                (uint64_t(pesdata()[6])       << 16) |
                (uint64_t(pesdata()[7])       <<  8) |
                (uint64_t(pesdata()[8])));
    }
    void SetPTSAdjustment(uint64_t val)
    {
        pesdata()[4] &= ~0x1;
        pesdata()[4] |= (val>>32) & 0x1;
        pesdata()[5] = ((val>>24) & 0xff);
        pesdata()[6] = ((val>>16) & 0xff);
        pesdata()[7] = ((val>>8 ) & 0xff);
        pesdata()[8] = ((val    ) & 0xff);
    }
    // cw_index (enc key)       8   9.0
    uint CodeWordIndex(void) const { return pesdata()[9]; }
    void SetCodeWordIndex(uint val) { pesdata()[9] = val; }
    // reserved                12  10.0
    // splice_command_length   12  11.4
    uint SpliceCommandLength(void) const
    {
        return ((pesdata()[11] & 0xf) << 8) | pesdata()[12];
    }
    void SetSpliceCommandLength(uint len)
    {
        pesdata()[11] &= ~0xf;
        pesdata()[11] |= (len >> 8) & 0xf;
        pesdata()[12] = len & 0xff;
    }
    // splice_command_type      8  13.0
    enum : std::uint8_t {
        kSCTNull                 = 0x00,
        kSCTReserved0            = 0x01,
        kSCTReserved1            = 0x02,
        kSCTReserved2            = 0x03,
        kSCTSpliceSchedule       = 0x04,
        kSCTSpliceInsert         = 0x05,
        kSCTTimeSignal           = 0x06,
        kSCTBandwidthReservation = 0x07,
        // 0x08-0xfe reserved
        kSCTPrivateCommand       = 0xff,
    };
    uint SpliceCommandType(void) const { return pesdata()[13]; }
    QString SpliceCommandTypeString(void) const;
    void SetSpliceCommandType(uint type) { pesdata()[13] = type & 0xff; }

    // ALL BELOW THIS LINE OTHER THAN CRC_32 ARE ENCRYPTED IF FLAG SET

    //////////// SPLICE NULL ////////////

    // nothing here, info in descriptors

    //////////// SPLICE SCHEDULE ////////////

    // if (splice_command_type == 0x04) splice_schedule()
    SpliceScheduleView SpliceSchedule(void) const
        { return {m_ptrs0, m_ptrs1}; }

    //////////// SPLICE INSERT ////////////

    // if (splice_command_type == 0x05) splice_insert()
    SpliceInsertView SpliceInsert(void) const
        { return {m_ptrs0, m_ptrs1}; }

    //////////// TIME SIGNAL ////////////

    // if (splice_command_type == 0x06) splice_time()
    SpliceTimeView TimeSignal(void) const
        { return SpliceTimeView(pesdata()+14); }

    //////////// BANDWIDTH RESERVATION ////////////

    // nothing here, info in descriptors

    //////////// PRIVATE COMMAND ////////////

    // if (splice_command_type == 0xff) private_command()
    //   identifier            32  14.0
    //   for (i = 0; i < N; i++)
    //     private_byte         8  ??.0
    //

    //////////////////////////////////////////////////////////////
    // NOTE: Aside from CRC's we can not interpret things below
    // this comment with private or reserved commands.

    // descriptor_loop_length  16   0.0 + m_epilog
    uint SpliceDescriptorsLength(uint /*i*/) const
    {
        return (m_epilog[0] << 8) | m_epilog[1];
    }

    // for (i = 0; i < ? ; i++)
    //   splice_descriptor()   ??   ??.?
    const unsigned char *SpliceDescriptors(void) const
    {
        return (m_epilog) ? m_epilog + 2 : nullptr;
    }
    // for (i = 0; i < ?; i++)
    //   alignment_stuffing     8   ??.0
    // if (encrypted_packet())
    //    E_CRC_32             32   ??.0
    // CRC_32                  32   ??.0

    static SpliceInformationTable *GetDecrypted(const QString &codeWord);
    bool Parse(void);

    QString toString(void) const override // PSIPTable
        { return toString(-1LL, -1LL); }
    QString toStringXML(uint indent_level) const override // PSIPTable
        { return toStringXML(indent_level, -1LL, -1LL); }

    QString toString(int64_t first, int64_t last) const;
    QString toStringXML(uint indent_level, int64_t first, int64_t last) const;

  private:
    std::vector<const unsigned char*> m_ptrs0;
    std::vector<const unsigned char*> m_ptrs1;
    const unsigned char *m_epilog {nullptr};
    int m_sctePid {0};
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
class MTV_PUBLIC AdaptationFieldControl
{
  public:
    explicit AdaptationFieldControl(const unsigned char* packet) : m_data(packet) { ; }

    /** adaptation header length
     * (after which is payload data)         8   0.0
     */
    uint Length(void) const             { return m_data[0]; }

    /** discontinuity_indicator
     *  (time base may change)               1   1.0
     */
    bool Discontinuity(void) const      { return ( m_data[1] & 0x80 ) != 0; }
    // random_access_indicator (?)           1   1.1
    bool RandomAccess(void) const       { return ( m_data[1] & 0x40 ) != 0; }
    // elementary_stream_priority_indicator  1   1.2
    bool Priority(void) const           { return ( m_data[1] & 0x20 ) != 0; }

// Each of the following extends the adaptation header.  In order:

    /** PCR flag (we have PCR data)          1   1.3
     *  (adds 6 bytes after adaptation header)
     */
    bool PCR(void) const                { return ( m_data[1] & 0x10 ) != 0; }
    /** OPCR flag (we have OPCR data)        1   1.4
     *  (adds 6 bytes) ((Original) Program Clock Reference; used to time output)
     */
    bool OPCR(void) const               { return ( m_data[1] & 0x08 ) != 0; }
    /** splicing_point_flag                  1   1.5
     *  (adds 1 byte) (we have splice point data)
     *  Splice data is packets until a good splice point for
     *  e.g. commercial insertion -- if these are still set,
     *  might be a good way to recognize potential commercials
     *  for flagging.
     */
    bool SplicingPoint(void) const      { return ( m_data[1] & 0x04 ) != 0; }
    //  transport_private_data_flag          1   1.6
    // (adds 1 byte)
    bool PrivateTransportData(void) const { return ( m_data[1] & 0x02 ) != 0; }
    // adaptation_field_extension_flag       1   1.7
    bool FieldExtension(void) const     { return ( m_data[1] & 0x01 ) != 0; }
    // extension length                      8   2.0
    uint ExtensionLength(void) const    { return m_data[2]; }
    // ltw flag                              1   3.0
    // (adds 2 bytes)
    bool LTW(void) const                { return ( m_data[3] & 0x80 ) != 0; }
    // piecewise_rate_flag (adds 3 bytes)    1   3.1
    bool PiecewiseRate(void) const      { return ( m_data[3] & 0x40 ) != 0; }
    // seamless_splice_flag (adds 5 bytes)   1   3.2
    bool SeamlessSplice(void) const     { return ( m_data[3] & 0x20 ) != 0; }
    // unused flags                          5   3.3

  private:
    const unsigned char* m_data;
};

#endif // MPEG_TABLES_H
