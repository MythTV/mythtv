// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson

#include "splicedescriptors.h"
#include "atscdescriptors.h"
#include "mythmiscutil.h" // for xml_indent
#include "mythlogging.h"
#include "mpegtables.h"

const unsigned char DEFAULT_PAT_HEADER[8] =
{
    0x00, // TableID::PAT
    0xb0, // Syntax indicator
    0x00, // Length (set seperately)
    0x00, // Transport stream ID top bits

    0x00, // Transport stream ID bottom bits
    0xc1, // current | reserved
    0x00, // Current Section
    0x00, // Last Section
};

const unsigned char DEFAULT_PMT_HEADER[12] =
{
    0x02, // TableID::PMT
    0xb0, // Syntax indicator
    0x00, // Length (set seperately)
    0x00, // MPEG Program number top bits (set seperately)

    0x00, // MPEG Program number bottom bits (set seperately)
    0xc1, // Version + Current/Next
    0x00, // Current Section
    0x00, // Last Section
    0xff, 0xff, // PCR pid
    0x00, 0x00, // Program Info Length
};

static const uint len_for_alloc[] =
{
    TSPacket::kPayloadSize
    - 1 /* for start of field pointer */
    - 3 /* for data before data last byte of pes length */,
    4000,
};

uint StreamID::Normalize(uint stream_id, const desc_list_t &desc,
                         const QString &sistandard)
{
    if ((sistandard != "dvb") && (OpenCableVideo == stream_id))
        return MPEG2Video;

    /* normalize DVB style signalling to ATSC style signalling to make
     * IsAudio work with either, see A/52:2010 A4 vs A5 */
    if (MPEGDescriptor::Find(desc, DescriptorID::ac3))
        return AC3Audio;

    /* normalize DVB style signalling to ATSC style signalling to make
     * IsAudio work with either */
    if (MPEGDescriptor::Find(desc, DescriptorID::eac3))
        return EAC3Audio;

    QString reg;
    const unsigned char *d = MPEGDescriptor::Find(
        desc, DescriptorID::registration);
    if (d)
    {
        RegistrationDescriptor rd(d);
        if (rd.IsValid())
            reg = rd.FormatIdentifierString();
    }

    /* normalize all three DTS frame sizes, via http://www.smpte-ra.org/mpegreg/mpegreg.html */
    if ((reg == "DTS1") || (reg == "DTS2") || (reg == "DTS3"))
        return DTSAudio;

    /* normalize AC-3 signalling according to A/52:2010 A4 */
    if (reg == "AC-3")
        return AC3Audio;

    /* normalize E-AC-3 signalling with guesswork via http://www.smpte-ra.org/mpegreg/mpegreg.html */
    if (reg == "EAC3")
        return EAC3Audio;

#if 0
    // not needed while there is no specific stream id for these
    if (MPEGDescriptor::Find(desc, DescriptorID::teletext) ||
        MPEGDescriptor::Find(desc, DescriptorID::subtitling))
        return stream_id;
#endif

    return stream_id;
}

bool PSIPTable::HasCRC(void) const
{
    // default is false, but gets set to true for 0x80-0xfe at the end!
    bool has_crc = false;

    switch (TableID())
    {
        // MPEG
        case TableID::PAT:
        case TableID::CAT:
        case TableID::PMT:
            has_crc = true;
            break;
//      case TableID::TSDT

        // DVB manditory
        case TableID::NIT:
        case TableID::SDT:
        case TableID::PF_EIT:
            has_crc = true;
            break;
        case TableID::TDT:
            has_crc = false;
            break;

        // DVB optional
        case TableID::NITo:
        case TableID::SDTo:
        case TableID::BAT:
        case TableID::PF_EITo:
            has_crc = true;
            break;
        case TableID::RST:
        case TableID::ST:
            has_crc = false;
            break;
        case TableID::TOT:
            has_crc = true;
            break;
//      case TableID::RNT:
//      case TableID::CT:
//      case TableID::RCT:
//      case TableID::CIT:
//      case TableID::MPEFEC:
        case TableID::DIT:
            has_crc = false;
            break;
        case TableID::SIT:
            has_crc = true;
            break;

        // SCTE
        case TableID::NITscte:
        case TableID::NTT:
        case TableID::SVCTscte:
        case TableID::STTscte:
        case TableID::SITscte:
            has_crc = true;
            break;
        case TableID::ADET:
            has_crc = false;
            break;

        // ATSC
        case TableID::MGT:
        case TableID::TVCT:
        case TableID::CVCT:
        case TableID::RRT:
        case TableID::EIT:
        case TableID::ETT:
        case TableID::STT:
        case TableID::DET:
        case TableID::DST:

        //case TableID::PIT:
        case TableID::NRT:
        case TableID::LTST:
        case TableID::DCCT:
        case TableID::DCCSCT:
        //case TableID::SITatsc:
        case TableID::AEIT:
        case TableID::AETT:
        case TableID::SVCT:
            has_crc = true;
            break;

        default:
        {
            // DVB Longterm EIT data
            if (TableID::SC_EITbeg <= TableID() &&
                TableID() <= TableID::SC_EITendo)
            {
                has_crc = true;
            }

            // FIXME Dishnet Longterm EIT data, only on PID 0x300! Forces
            // table_id 0x80-0xfe to true, unless handled before or after!
            if (TableID::DN_EITbego <= TableID() &&
                TableID() <= TableID::DN_EITendo)
            {
                has_crc = true;
            }

            // ATSC/DVB conditional access ECM/EMM, reset to false after Dishnet
            if (TableID::ECM0 <= TableID() &&
                TableID() <= TableID::ECMend)
            {
                has_crc = false;
            }
        }
        break;
    }

    return has_crc;
}

bool PSIPTable::HasSectionNumber(void) const
{
    bool has_sn = false;
    switch (TableID())
    {
        // MPEG
        case TableID::PAT:
        case TableID::CAT:
        case TableID::PMT:
        // ATSC
        case TableID::MGT:
        case TableID::TVCT:
        case TableID::CVCT:
        case TableID::RRT:
        case TableID::EIT:
        case TableID::ETT:
        case TableID::STT:
        case TableID::DET:
        case TableID::DST:
            has_sn = true;
            break;
    }

    return has_sn;
}

bool PSIPTable::VerifyPSIP(bool verify_crc) const
{
    if (verify_crc && (CalcCRC() != CRC()))
    {
        LOG(VB_SIPARSER, LOG_ERR,
            QString("PSIPTable: Failed CRC check 0x%1 != 0x%2 "
                    "for StreamID = 0x%3")
                .arg(CRC(),0,16).arg(CalcCRC(),0,16).arg(StreamID(),0,16));
        return false;
    }

    unsigned char *bufend = _fullbuffer + _allocSize;

    if ((_pesdata + 2) >= bufend)
        return false; // can't query length

    if (psipdata() >= bufend)
        return false; // data outside buffer

    if (TableID::PAT == TableID())
    {
        uint pcnt = (SectionLength() - PSIP_OFFSET - 2) >> 2;
        bool ok = (psipdata() + (pcnt << 2) + 3 < bufend);
        if (!ok)
        {
            LOG(VB_SIPARSER, LOG_ERR,
                "PSIPTable: PAT: program list extends past end of buffer");
            return false;
        }

        if ((Length() == 0xfff) && (TableIDExtension() == 0xffff) &&
            (Section() == 0xff) && (LastSection() == 0xff))
        {
            LOG(VB_SIPARSER, LOG_ERR, "PSIPTable: PAT: All values at maximums");
            return false;
        }

        return true;
    }

    if (TableID::PMT == TableID())
    {
        if (psipdata() + 3 >= bufend)
        {
            LOG(VB_SIPARSER, LOG_ERR,
                "PSIPTable: PMT: can't query program info length");
            return false;
        }

        if (psipdata() + Length() - 9 > bufend)
        {
            LOG(VB_SIPARSER, LOG_ERR,
                "PSIPTable: PMT: reported length too large");
            return false;
        }

        uint proginfolen = ((psipdata()[2]<<8) | psipdata()[3]) & 0x0fff;
        const unsigned char *proginfo = psipdata() + 4;
        const unsigned char *cpos = proginfo + proginfolen;
        if (cpos > bufend)
        {
            LOG(VB_SIPARSER, LOG_ERR,
                "PSIPTable: PMT: program info extends past end of buffer");
            return false;
        }

        vector<unsigned char*> _ptrs;
        const unsigned char *pos = cpos;
        uint i = 0;
        for (; pos < psipdata() + Length() - 9; i++)
        {
            const unsigned char *ptr = pos;
            if (pos + 4 > bufend)
            {
                LOG(VB_SIPARSER, LOG_ERR,
                    QString("PSIPTable: PMT: stream info %1 extends "
                            "past end of buffer").arg(i));
                return false;
            }
            pos += 5 + (((ptr[3] << 8) | ptr[4]) & 0x0fff);
        }
        if (pos > bufend)
        {
            LOG(VB_SIPARSER, LOG_ERR,
                QString("PSIPTable: PMT: last stream info %1 extends "
                        "past end of buffer").arg(i));
            return false;
        }

        return true;
    }

    return true;
}

ProgramAssociationTable* ProgramAssociationTable::CreateBlank(bool smallPacket)
{
    (void) smallPacket; // currently always a small packet..
    TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
    memcpy(tspacket->data() + sizeof(TSHeader) + 1/* start of field pointer */,
           DEFAULT_PAT_HEADER, sizeof(DEFAULT_PAT_HEADER));
    PSIPTable psip = PSIPTable::View(*tspacket);
    psip.SetLength(TSPacket::kPayloadSize
                   - 1 /* for start of field pointer */
                   - 3 /* for data before data last byte of pes length */);
    ProgramAssociationTable *pat = new ProgramAssociationTable(psip);
    pat->SetTotalLength(sizeof(DEFAULT_PAT_HEADER));
    delete tspacket;
    return pat;
}

ProgramAssociationTable* ProgramAssociationTable::Create(
    uint tsid, uint version,
    const vector<uint>& pnum, const vector<uint>& pid)
{
    const uint count = min(pnum.size(), pid.size());
    ProgramAssociationTable* pat = CreateBlank();
    pat->SetVersionNumber(version);
    pat->SetTranportStreamID(tsid);
    pat->SetTotalLength(PSIP_OFFSET + (count * 4));

    // create PAT data
    if ((count * 4) >= (184 - (PSIP_OFFSET+1)))
    { // old PAT must be in single TS for this create function
        LOG(VB_GENERAL, LOG_ERR,
            "PAT::Create: Error, old PAT size exceeds maximum PAT size.");
        delete pat;
        return 0;
    }

    uint offset = PSIP_OFFSET;
    for (uint i = 0; i < count; i++)
    {
        // pnum
        pat->pesdata()[offset++] = pnum[i]>>8;
        pat->pesdata()[offset++] = pnum[i] & 0xff;
        // pid
        pat->pesdata()[offset++] = ((pid[i]>>8) & 0x1f) | 0xe0;
        pat->pesdata()[offset++] = pid[i] & 0xff;
    }

    pat->Finalize();

    return pat;
}

ProgramMapTable* ProgramMapTable::CreateBlank(bool smallPacket)
{
    ProgramMapTable *pmt = NULL;
    TSPacket *tspacket = TSPacket::CreatePayloadOnlyPacket();
    memcpy(tspacket->data() + sizeof(TSHeader) + 1/* start of field pointer */,
           DEFAULT_PMT_HEADER, sizeof(DEFAULT_PMT_HEADER));

    if (smallPacket)
    {
        PSIPTable psip = PSIPTable::View(*tspacket);
        psip.SetLength(len_for_alloc[0]);
        pmt = new ProgramMapTable(psip);
    }
    else
    {
        PSIPTable psip(*tspacket);
        psip.SetLength(len_for_alloc[1]);
        pmt = new ProgramMapTable(psip);
    }

    pmt->SetTotalLength(sizeof(DEFAULT_PMT_HEADER));
    delete tspacket;
    return pmt;
}

ProgramMapTable* ProgramMapTable::Create(
    uint programNumber, uint basepid, uint pcrpid, uint version,
    vector<uint> pids, vector<uint> types)
{
    const uint count = min(pids.size(), types.size());
    ProgramMapTable* pmt = CreateBlank(false);
    pmt->tsheader()->SetPID(basepid);

    pmt->RemoveAllStreams();
    pmt->SetProgramNumber(programNumber);
    pmt->SetPCRPID(pcrpid);
    pmt->SetVersionNumber(version);

    for (uint i=0; i<count; i++)
        pmt->AppendStream(pids[i], types[i]);
    pmt->Finalize();

    return pmt;
}

ProgramMapTable* ProgramMapTable::Create(
    uint programNumber, uint basepid, uint pcrpid, uint version,
    const desc_list_t         &global_desc,
    const vector<uint>        &pids,
    const vector<uint>        &types,
    const vector<desc_list_t> &prog_desc)
{
    const uint count = min(pids.size(), types.size());
    ProgramMapTable* pmt = CreateBlank(false);
    pmt->tsheader()->SetPID(basepid);

    pmt->RemoveAllStreams();
    pmt->SetProgramNumber(programNumber);
    pmt->SetPCRPID(pcrpid);
    pmt->SetVersionNumber(version);

    vector<unsigned char> gdesc;
    for (uint i=0; i<global_desc.size(); i++)
    {
        uint len = global_desc[i][1] + 2;
        gdesc.insert(gdesc.end(), global_desc[i], global_desc[i] + len);
    }
    pmt->SetProgramInfo(&gdesc[0], gdesc.size());

    for (uint i = 0; i < count; i++)
    {
        vector<unsigned char> pdesc;
        for (uint j = 0; j < prog_desc[i].size(); j++)
        {
            uint len = prog_desc[i][j][1] + 2;
            pdesc.insert(pdesc.end(),
                         prog_desc[i][j], prog_desc[i][j] + len);
        }

        pmt->AppendStream(pids[i], types[i], &pdesc[0], pdesc.size());
    }
    pmt->Finalize();

    LOG(VB_SIPARSER, LOG_INFO, "Created PMT \n" + pmt->toString());

    return pmt;
}

void ProgramMapTable::Parse() const
{
    _ptrs.clear();
    const unsigned char *cpos = psipdata() + pmt_header + ProgramInfoLength();
    unsigned char *pos = const_cast<unsigned char*>(cpos);
    for (uint i = 0; pos < psipdata() + Length() - 9; i++)
    {
        _ptrs.push_back(pos);
        pos += 5 + StreamInfoLength(i);
#if 0
        LOG(VB_SIPARSER, LOG_DEBUG, QString("Parsing PMT(0x%1) i(%2) len(%3)")
                .arg((uint64_t)this, 0, 16) .arg(i) .arg(StreamInfoLength(i)));
#endif
    }
    _ptrs.push_back(pos);
#if 0
    LOG(VB_SIPARSER, LOG_DEBUG, QString("Parsed PMT(0x%1)\n%2")
            .arg((uint64_t)this, 0, 16) .arg(toString()));
#endif
}

void ProgramMapTable::AppendStream(
    uint pid, uint type,
    unsigned char* streamInfo, uint infoLength)
{
    if (!StreamCount())
        _ptrs.push_back(psipdata() + pmt_header + ProgramInfoLength());
    memset(_ptrs[StreamCount()], 0xff, 5);
    SetStreamPID(StreamCount(), pid);
    SetStreamType(StreamCount(), type);
    SetStreamProgramInfo(StreamCount(), streamInfo, infoLength);
    _ptrs.push_back(_ptrs[StreamCount()]+5+StreamInfoLength(StreamCount()));
    SetTotalLength(_ptrs[StreamCount()] - pesdata());
}

/** \fn ProgramMapTable::IsVideo(uint,QString) const
 *  \brief Returns true iff the stream at index i is a video stream.
 *
 *   This of course returns true if StreamID::IsVideo() is true.
 *   And, it also returns true if IsVideo returns true after
 *   StreamID::Normalize() is used on the stream type.
 *
 *  \param i index of stream
 */
bool ProgramMapTable::IsVideo(uint i, QString sistandard) const
{
    if (StreamID::IsVideo(StreamType(i)))
        return true;

    desc_list_t list = MPEGDescriptor::
        Parse(StreamInfo(i), StreamInfoLength(i));
    uint stream_id = StreamID::Normalize(StreamType(i), list, sistandard);

    return StreamID::IsVideo(stream_id);
}

/** \fn ProgramMapTable::IsAudio(uint,QString) const
 *  \brief Returns true iff the stream at index i is an audio stream.
 *
 *   This of course returns true if StreamID::IsAudio() is true.
 *   And, it also returns true if IsAudio returns true after
 *   StreamID::Normalize() is used on the stream type.
 *
 *  \param i index of stream
 */
bool ProgramMapTable::IsAudio(uint i, QString sistandard) const
{
    if (StreamID::IsAudio(StreamType(i)))
        return true;

    desc_list_t list = MPEGDescriptor::
        Parse(StreamInfo(i), StreamInfoLength(i));
    uint stream_id = StreamID::Normalize(StreamType(i), list, sistandard);

    return StreamID::IsAudio(stream_id);
}

/** \fn ProgramMapTable::IsEncrypted(QString sistandard) const
 *  \brief Returns true iff PMT contains CA descriptor for a vid/aud stream.
 */
bool ProgramMapTable::IsEncrypted(QString sistandard) const
{
    bool encrypted = IsProgramEncrypted();

    for (uint i = 0; !encrypted && i < StreamCount(); i++) {
    /* Only check audio/video streams */
        if (IsAudio(i,sistandard) || IsVideo(i,sistandard))
            encrypted |= IsStreamEncrypted(i);
    }

    return encrypted;
}

/** \fn ProgramMapTable::IsProgramEncrypted(void) const
 *  \brief Returns true iff PMT's ProgramInfo contains CA descriptor.
 */
bool ProgramMapTable::IsProgramEncrypted(void) const
{
    desc_list_t descs = MPEGDescriptor::ParseOnlyInclude(
        ProgramInfo(), ProgramInfoLength(), DescriptorID::conditional_access);

    uint encrypted = 0;
    QMap<uint,uint> encryption_system;
    for (uint i = 0; i < descs.size(); i++)
    {
        ConditionalAccessDescriptor cad(descs[i]);
        encryption_system[cad.PID()] = cad.SystemID();
        encrypted |= cad.SystemID();

#if 0
        LOG(VB_GENERAL, LOG_INFO, "DTVsm: " + cad.toString());
#endif
    }

    return encrypted != 0;
}

/** \fn ProgramMapTable::IsStreamEncrypted(uint i) const
 *  \brief Returns true iff PMT contains CA descriptor.
 *
 *  \param i index of stream
 */
bool ProgramMapTable::IsStreamEncrypted(uint i) const
{
    desc_list_t descs = MPEGDescriptor::ParseOnlyInclude(
        StreamInfo(i), StreamInfoLength(i), DescriptorID::conditional_access);

    uint encrypted = 0;
    QMap<uint,uint> encryption_system;
    for (uint j = 0; j < descs.size(); j++)
    {
        ConditionalAccessDescriptor cad(descs[j]);
        encryption_system[cad.PID()] = cad.SystemID();
        encrypted |= cad.SystemID();

#if 0
        LOG(VB_GENERAL, LOG_INFO, "DTVsm: " + cad.toString());
#endif
    }

    return encrypted != 0;
}

bool ProgramMapTable::IsStillPicture(QString sistandard) const
{
    static const unsigned char STILL_PICTURE_FLAG = 0x01;

    for (uint i = 0; i < StreamCount(); i++)
    {
        if (IsVideo(i, sistandard))
        {
            return StreamInfoLength(i) > 2 &&
                   (StreamInfo(i)[2] & STILL_PICTURE_FLAG);
        }
    }
    return false;
}


/** \fn ProgramMapTable::FindPIDs(uint type, vector<uint>& pids,
                                  const QString&) const
 *  \brief Finds all pids matching type.
 *  \param type StreamType to match
 *  \param pids vector pids will be added to
 *  \param sistandard standard to use in determining if this is a A/V stream
 *  \return number of pids in list
 */
uint ProgramMapTable::FindPIDs(uint           type,
                               vector<uint>  &pids,
                               const QString &sistandard) const
{
    if ((StreamID::AnyMask & type) != StreamID::AnyMask)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (type == StreamType(i))
                pids.push_back(StreamPID(i));
    }
    else if (StreamID::AnyVideo == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsVideo(i, sistandard))
                pids.push_back(StreamPID(i));
    }
    else if (StreamID::AnyAudio == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsAudio(i, sistandard))
                pids.push_back(StreamPID(i));
    }

    return pids.size();
}

/** \fn ProgramMapTable::FindPIDs(uint, vector<uint>&, vector<uint>&,
                                  const QString&, bool normalize) const
 *  \brief Finds all pids w/types, matching type (useful for AnyVideo/AnyAudio).
 *  \param type  StreamType to match
 *  \param pids  vector pids will be added to
 *  \param types vector types will be added to
 *  \param sistandard standard to use in determining if this is a A/V stream
 *  \param normalize  if set types will be normalized
 *  \return number of items in pids and types lists.
 */
uint ProgramMapTable::FindPIDs(uint           type,
                               vector<uint>  &pids,
                               vector<uint>  &types,
                               const QString &sistandard,
                               bool           normalize) const
{
    uint pids_start = pids.size();

    if ((StreamID::AnyMask & type) != StreamID::AnyMask)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (type == StreamType(i))
            {
                pids.push_back(StreamPID(i));
                types.push_back(StreamType(i));
            }
    }
    else if (StreamID::AnyVideo == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsVideo(i, sistandard))
            {
                pids.push_back(StreamPID(i));
                types.push_back(StreamType(i));
            }
    }
    else if (StreamID::AnyAudio == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsAudio(i, sistandard))
            {
                pids.push_back(StreamPID(i));
                types.push_back(StreamType(i));
            }
    }

    if (!normalize)
        return pids.size();

    for (uint i = pids_start; i < pids.size(); i++)
    {
        int index = FindPID(pids[i]);
        if (index >= 0)
        {
            desc_list_t desc = MPEGDescriptor::Parse(
                StreamInfo(i), StreamInfoLength(i));
            types[i] = StreamID::Normalize(types[i], desc, sistandard);
        }
    }

    return pids.size();
}

uint ProgramMapTable::FindUnusedPID(uint desired_pid)
{
    uint pid = desired_pid;
    if (pid >= MPEG_NULL_PID)
        pid = 0x20;

    while (FindPID(pid) != -1)
        pid += 0x10;

    if (pid < MPEG_NULL_PID)
        return pid;

    pid = desired_pid;
    while (FindPID(pid) != -1)
        pid += 1;

    if (pid < MPEG_NULL_PID)
        return pid;

    pid = 0x20;
    while (FindPID(pid) != -1)
        pid += 1;

    return pid & 0x1fff;
}

QString PSIPTable::toString(void) const
{
    QString str;
    str.append(QString(" PSIP tableID(0x%1) length(%2) extension(0x%3)\n")
               .arg(TableID(), 0, 16).arg(Length())
               .arg(TableIDExtension(), 0, 16));
    str.append(QString("      version(%1) current(%2) "
                       "section(%3) last_section(%4)\n")
               .arg(Version()).arg(IsCurrent())
               .arg(Section()).arg(LastSection()));
    if ((TableID() >= TableID::MGT) && (TableID() <= TableID::SRM))
    {
        str.append(QString("      atsc_protocol_version(%1)\n")
                   .arg(ATSCProtocolVersion()));
    }
    return str;
}

QString PSIPTable::toStringXML(uint indent_level) const
{
    QString indent = xml_indent(indent_level);
    return indent + "<PSIPSection " + XMLValues(indent_level + 1) + " />";
}

QString PSIPTable::XMLValues(uint indent_level) const
{
    QString indent = xml_indent(indent_level);

    QString str = QString(
        "table_id=\"0x%1\" length=\"%2\"")
        .arg(TableID(), 2, 16, QChar('0'))
        .arg(Length());

    if (HasSectionNumber())
    {
        str += QString(" section=\"%4\" last_section=\"%5\"")
            .arg(Section()).arg(LastSection());
    }

    if ((TableID() >= TableID::MGT) && (TableID() <= TableID::SRM))
    {
        str += QString("\n%1version=\"%2\" current=\"%3\" "
                       "protocol_version=\"%4\" extension=\"0x%5\"")
            .arg(indent)
            .arg(Version()).arg(xml_bool_to_string(IsCurrent()))
            .arg(ATSCProtocolVersion())
            .arg(TableIDExtension(), 0, 16);
    }

    return str;
}

QString ProgramAssociationTable::toString(void) const
{
    QString str;
    str.append(QString("Program Association Section\n"));
    str.append(PSIPTable::toString());
    str.append(QString("      tsid(%1) ").arg(TransportStreamID()));
    str.append(QString("programCount(%1)\n").arg(ProgramCount()));

    uint cnt0 = 0, cnt1fff = 0;
    for (uint i = 0; i < ProgramCount(); i++)
    {
        if (0x1fff == ProgramPID(i))
        {
            cnt1fff++;
            continue;
        }

        if (0x0 == ProgramPID(i))
        {
            cnt0++;
            continue;
        }

        str += QString("  program number %1 has PID 0x%2\n")
            .arg(ProgramNumber(i),5)
            .arg(ProgramPID(i),4,16,QChar('0'));
    }

    if (cnt0 || cnt1fff)
    {
        str.append(QString("  also contains %1 dummy programs\n")
                   .arg(cnt0 + cnt1fff));
    }

    return str;
}

QString ProgramAssociationTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);

    QString str = QString(
        "%1<ProgramAssociationSection tsid=\"0x%2\" program_count=\"%3\""
        "\n%4%5>\n")
        .arg(indent_0)
        .arg(TransportStreamID(),4,16,QChar('0'))
        .arg(ProgramCount())
        .arg(indent_1)
        .arg(PSIPTable::XMLValues(indent_level + 1));

    for (uint i = 0; i < ProgramCount(); i++)
    {
        bool dummy = (0x1fff == ProgramPID(i)) || (0x0 == ProgramPID(i));
        str += QString("%1<Program number=\"%2\" pid=\"0x%3\" %4/>\n")
            .arg(indent_1)
            .arg(ProgramNumber(i))
            .arg(ProgramPID(i),4,16,QChar('0'))
            .arg(dummy ? "comment=\"Dummy Program\" " : "");
    }

    return str + indent_0 + "</ProgramAssociationSection>";
}

QString ProgramMapTable::toString(void) const
{
    QString str =
        QString("Program Map Section"
                "\n%1"
                "      pnum(%2) pid(0x%3)\n")
        .arg(PSIPTable::toString())
        .arg(ProgramNumber())
        .arg(tsheader()->PID(),0,16);

    vector<const unsigned char*> desc =
        MPEGDescriptor::Parse(ProgramInfo(), ProgramInfoLength());
    for (uint i = 0; i < desc.size(); i++)
    {
        str.append(QString("  %1\n")
                   .arg(MPEGDescriptor(desc[i], 300).toString()));
    }

    for (uint i = 0; i < StreamCount(); i++)
    {
        str.append(QString("  Stream #%1 pid(0x%2) type(0x%3 %4)\n")
                   .arg(i).arg(StreamPID(i), 0, 16)
                   .arg(StreamType(i), 2, 16, QChar('0'))
                   .arg(StreamTypeString(i)));
        vector<const unsigned char*> desc =
            MPEGDescriptor::Parse(StreamInfo(i), StreamInfoLength(i));
        for (uint i = 0; i < desc.size(); i++)
        {
            str.append(QString("    %1\n")
                       .arg(MPEGDescriptor(desc[i], 300).toString()));
        }
    }
    return str;
}

QString ProgramMapTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);

    QString str = QString(
        "%1<ProgramMapSection pcr_pid=\"0x%2\" program_number=\"%3\"\n"
        "%4program_info_length=\"%5\" stream_count=\"%7\"%8>\n")
        .arg(indent_0)
        .arg(PCRPID(),0,16)
        .arg(ProgramNumber())
        .arg(indent_1)
        .arg(ProgramInfoLength())
        .arg(PSIPTable::XMLValues(indent_level + 1));

    vector<const unsigned char*> gdesc =
        MPEGDescriptor::Parse(ProgramInfo(), ProgramInfoLength());
    for (uint i = 0; i < gdesc.size(); i++)
    {
        str += MPEGDescriptor(gdesc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }

    for (uint i = 0; i < StreamCount(); i++)
    {
        str += QString("%1<Stream pid=\"0x%2\" type=\"0x%3\" "
                       "type_desc=\"%4\" stream_info_length=\"%5\"")
            .arg(indent_1)
            .arg(StreamPID(i),2,16,QChar('0'))
            .arg(StreamType(i),2,16,QChar('0'))
            .arg(StreamTypeString(i))
            .arg(StreamInfoLength(i));
        vector<const unsigned char*> ldesc =
            MPEGDescriptor::Parse(StreamInfo(i), StreamInfoLength(i));
        str += (ldesc.empty()) ? " />\n" : ">\n";
        for (uint i = 0; i < ldesc.size(); i++)
        {
            str += MPEGDescriptor(ldesc[i], 300)
                .toStringXML(indent_level + 2) + "\n";
        }
        if (!ldesc.empty())
            str += indent_1 + "</Stream>\n";
    }

    return str + indent_0 + "</ProgramMapSection>";
}

const char *StreamID::toString(uint streamID)
{
    // valid for some ATSC/DVB stuff too
    switch (streamID)
    {
    case StreamID::MPEG2Video:
        return "video-mpeg2";
    case StreamID::MPEG1Video:
        return "video-mpeg1";
    case StreamID::MPEG4Video:
        return "video-mpeg4";
    case StreamID::H264Video:
        return "video-h264";
    case StreamID::OpenCableVideo:
        return "video-opencable";

    // audio
    case StreamID::AC3Audio:
        return "audio-ac3";  // EIT, PMT
    case StreamID::EAC3Audio:
        return "audio-eac3";  // EIT, PMT
    case StreamID::MPEG2Audio:
        return "audio-mp2-layer[1,2,3]"; // EIT, PMT
    case StreamID::MPEG1Audio:
        return "audio-mp1-layer[1,2,3]"; // EIT, PMT
    case StreamID::MPEG2AudioAmd1:
         return "audio-aac-latm"; // EIT, PMT
    case StreamID::MPEG2AACAudio:
        return "audio-aac"; // EIT, PMT
    case StreamID::DTSAudio:
        return "audio-dts"; // EIT, PMT

    // other
    case StreamID::PrivSec:
        return "private-sec";
    case StreamID::PrivData:
        return "private-data";

    // DSMCC Object Carousel
    case StreamID::DSMCC_A:
        return "dsmcc-a encap";
    case StreamID::DSMCC_B:
        return "dsmcc-b std data";
    case StreamID::DSMCC_C:
        return "dsmcc-c NPD data";
    case StreamID::DSMCC_D:
        return "dsmcc-d data";

    // Can be in any MPEG stream ATSC, DVB, or ARIB ; but defined in SCTE 35
    case StreamID::Splice:
        return "splice"; // PMT

    //case TableID::STUFFING: XXX: Duplicate?
    //    return "stuffing"; // optionally in any
    //case TableID::CENSOR: FIXME collides with StreamID::EAC3Audio
    //    return "censor"; // EIT, optionally in PMT
    case TableID::ECN:
        return "extended channel name";
    case TableID::SRVLOC:
        return "service location"; // required in VCT
    case TableID::TSS: // other channels with same stuff
        return "time-shifted service";
    case TableID::CMPNAME:
        return "component name"; //??? PMT
    }
    return "unknown";
}

QString StreamID::GetDescription(uint stream_id)
{
    // valid for some ATSC/DVB stuff too
    switch (stream_id)
    {
        // video
        case StreamID::MPEG1Video:
            return "11172-2 MPEG-1 Video";
        case StreamID::MPEG2Video:
            return "13818-2 MPEG-2 Video";
        case StreamID::MPEG4Video:
            return "14492-2 MPEG-4 Video";
        case StreamID::H264Video:
            return "H.264 Video";
        case StreamID::OpenCableVideo:
            return "OpenCable Video";
        case StreamID::VC1Video:
            return "VC-1 Video";

        // audio
        case StreamID::MPEG1Audio:
            return "11172-2 MPEG-1 Audio";
        case StreamID::MPEG2Audio:
            return "13818-3 MPEG-2 Audio";
        case StreamID::MPEG2AACAudio:
            return "13818-7 AAC MPEG-2 Audio";
        case StreamID::MPEG2AudioAmd1:
            return "13818-3 AAC LATM MPEG-2 Audio";
        case StreamID::AC3Audio:
            return "AC3 Audio";
        case StreamID::EAC3Audio:
            return "E-AC3 Audio";
        case StreamID::DTSAudio:
            return "DTS Audio";

        // DSMCC Object Carousel
        case StreamID::DSMCC:
            return "13818-1 DSM-CC";
        case StreamID::DSMCC_A:
            return "13818-6 DSM-CC Type A";
        case StreamID::DSMCC_B:
            return "13818-6 DSM-CC Type B";
        case StreamID::DSMCC_C:
            return "13818-6 DSM-CC Type C";
        case StreamID::DSMCC_D:
            return "13818-6 DSM-CC Type D";
        case StreamID::DSMCC_DL:
            return "13818-6 Download";
        case StreamID::MetaDataPES:
            return "13818-6 Metadata in PES";
        case StreamID::MetaDataSec:
            return "13818-6 Metadata in Sections";
        case StreamID::MetaDataDC:
            return "13818-6 Metadata in Data Carousel";
        case StreamID::MetaDataOC:
            return "13818-6 Metadata in Obj Carousel";
        case StreamID::MetaDataDL:
            return "13818-6 Metadata in Download";

        // other
        case StreamID::PrivSec:
            return "13818-1 Private Sections";
        case StreamID::PrivData:
            return "13818-3 Private Data";
        case StreamID::MHEG:
            return "13522 MHEG";
        case StreamID::H222_1:
            return "ITU H.222.1";
        case StreamID::MPEG2Aux:
            return "13818-1 Aux & ITU H.222.0";
        case StreamID::FlexMuxPES:
            return "14496-1 SL/FlexMux in PES";
        case StreamID::FlexMuxSec:
            return "14496-1 SL/FlexMux in Sections";
        case StreamID::MPEG2IPMP:
            return "13818-10 IPMP";
        case StreamID::MPEG2IPMP2:
            return "13818-10 IPMP2";

        case AnyMask:  return QString();
        case AnyVideo: return "video";
        case AnyAudio: return "audio";
    }

    return QString();
}

QString ProgramMapTable::GetLanguage(uint i) const
{
    const desc_list_t list = MPEGDescriptor::Parse(
        StreamInfo(i), StreamInfoLength(i));
    const unsigned char *lang_desc = MPEGDescriptor::Find(
        list, DescriptorID::iso_639_language);

    if (!lang_desc)
        return QString::null;

    ISO639LanguageDescriptor iso_lang(lang_desc);
    return iso_lang.CanonicalLanguageString();
}

uint ProgramMapTable::GetAudioType(uint i) const
{
    const desc_list_t list = MPEGDescriptor::Parse(
        StreamInfo(i), StreamInfoLength(i));
    const unsigned char *lang_desc = MPEGDescriptor::Find(
        list, DescriptorID::iso_639_language);

    if (!lang_desc)
        return 0;

    ISO639LanguageDescriptor iso_lang(lang_desc);

    // Hack for non-standard AD labelling on UK Satellite and Irish DTTV
    // Language string of 'nar' for narrative indicates an AD track
    if (iso_lang.AudioType() == 0x0 &&
        iso_lang.LanguageString() == "nar")
        return 0x03;
    
    return iso_lang.AudioType();
}

QString ProgramMapTable::StreamDescription(uint i, QString sistandard) const
{
    desc_list_t list;

    list         = MPEGDescriptor::Parse(StreamInfo(i), StreamInfoLength(i));
    uint    type = StreamID::Normalize(StreamType(i), list, sistandard);
    QString desc = StreamID::toString(type);
    QString lang = GetLanguage(i);

    if (!lang.isEmpty())
        desc += QString(" (%1)").arg(lang);

    return desc;
}

QString ConditionalAccessTable::toString(void) const
{
    QString str =
        QString("Condiditional Access Section %1")
        .arg(PSIPTable::toString());
    
    vector<const unsigned char*> gdesc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    for (uint i = 0; i < gdesc.size(); i++)
        str += "  " + MPEGDescriptor(gdesc[i], 300).toString() + "\n";

    str += "\n";

    return str;
}

QString ConditionalAccessTable::toStringXML(uint indent_level) const
{
    QString indent_0 = xml_indent(indent_level);

    QString str =
        QString("%1<ConditionalAccessSection %3")
        .arg(indent_0)
        .arg(PSIPTable::XMLValues(indent_level + 1));

    vector<const unsigned char*> gdesc =
        MPEGDescriptor::Parse(Descriptors(), DescriptorsLength());
    str += (gdesc.empty()) ? " />\n" : ">\n";
    for (uint i = 0; i < gdesc.size(); i++)
    {
        str += MPEGDescriptor(gdesc[i], 300)
            .toStringXML(indent_level + 1) + "\n";
    }
    if (!gdesc.empty())
        str += indent_0 + "</ConditionalAccessSection>\n";

    return str;
}

QString SpliceTimeView::toString(int64_t first, int64_t last) const
{
    if (!IsTimeSpecified())
        return QString("splice_time(N/A)");

    int64_t abs_pts_time = PTSTime();
    if ((first > 0) && (last > 0))
    {
        int64_t elapsed = abs_pts_time - first;
        elapsed = (elapsed < 0) ? elapsed + 0x1000000000LL : elapsed;
        QTime abs = QTime(0,0,0,0).addMSecs(elapsed/90);

        elapsed = abs_pts_time - last; /* rel_pts_time */
        elapsed = (elapsed < 0) ? elapsed + 0x1000000000LL : elapsed;
        QTime rel = QTime(0,0,0,0).addMSecs(elapsed/90);

        return QString("splice_time(pts: %1 abs: %2, rel: +%3)")
            .arg(abs_pts_time)
            .arg(abs.toString("hh:mm:ss.zzz"))
            .arg(rel.toString("hh:mm:ss.zzz"));
    }

    return QString("splice_time(pts: %1)").arg(abs_pts_time);
}

QString SpliceTimeView::toStringXML(
    uint indent_level, int64_t first, int64_t last) const
{
    QString indent = xml_indent(indent_level);

    if (!IsTimeSpecified())
        return indent + "<SpliceTime />";

    int64_t abs_pts_time = PTSTime();

    QString abs_str;
    if (first > 0)
    {
        int64_t elapsed = abs_pts_time - first;
        elapsed = (elapsed < 0) ? elapsed + 0x1000000000LL : elapsed;
        QTime abs = QTime(0,0,0,0).addMSecs(elapsed/90);
        abs_str = QString("absolute=\"%1\" ")
            .arg(abs.toString("hh:mm:ss.zzz"));
    }

    QString rel_str;
    if (last > 0)
    {
        int64_t elapsed = abs_pts_time - last; /* rel_pts_time */
        elapsed = (elapsed < 0) ? elapsed + 0x1000000000LL : elapsed;
        QTime rel = QTime(0,0,0,0).addMSecs(elapsed/90);
        rel_str = QString("relative=\"+%1\" ")
            .arg(rel.toString("hh:mm:ss.zzz"));
    }

    return QString("%1<SpliceTime pts=\"%2\" %3%4/>")
        .arg(indent).arg(abs_pts_time).arg(abs_str).arg(rel_str);
}

/// \brief Returns decrypted version of this packet.
SpliceInformationTable *SpliceInformationTable::GetDecrypted(
    const QString &codeWord) const
{
    // TODO
    return NULL;
}

bool SpliceInformationTable::Parse(void)
{
    _epilog = NULL;
    _ptrs0.clear();
    _ptrs1.clear();

    if (TableID::SITscte != TableID())
        return false;

    if (SpliceProtocolVersion() != 0)
        return false;

    if (IsEncryptedPacket())
        return true; // it's "parsed" but you can't read encrypted portion

    uint type = SpliceCommandType();
    if (kSCTNull == type || kSCTBandwidthReservation == type)
    {
        _epilog = pesdata() + 14;
    }
    else if (kSCTTimeSignal == type)
    {
        _epilog = pesdata() + 14 + TimeSignal().size();
    }
    else if (kSCTSpliceSchedule == type)
    {
        uint splice_count = pesdata()[14];
        const unsigned char *cur = pesdata() + 15;
        for (uint i = 0; i < splice_count; i++)
        {
            _ptrs0.push_back(cur);
            bool event_cancel = cur[4] & 0x80;
            if (event_cancel)
            {
                _ptrs1.push_back(NULL);
                cur += 5;
                continue;
            }
            bool program_slice = cur[5] & 0x40;
            uint component_count = cur[6];
            _ptrs1.push_back(cur + (program_slice ? 10 : 7 * component_count));
        }
        if (splice_count)
        {
            bool duration = _ptrs0.back()[5] & 0x2;
            _epilog = _ptrs1.back() + ((duration) ? 9 : 4);
        }
        else
        {
            _epilog = cur;
        }
    }
    else if (kSCTSpliceInsert == type)
    {
        _ptrs1.push_back(pesdata() + 14);
        bool splice_cancel = pesdata()[18] & 0x80;
        if (splice_cancel)
        {
            _epilog = pesdata() + 19;
        }
        else
        {
            bool program_splice = pesdata()[19] & 0x40;
            bool duration = pesdata()[19] & 0x20;
            bool splice_immediate = pesdata()[19] & 0x10;
            const unsigned char *cur = pesdata() + 20;
            if (program_splice && !splice_immediate)
            {
                cur += SpliceTimeView(cur).size();
            }
            else if (!program_splice)
            {
                uint component_count = pesdata()[20];
                cur = pesdata() + 21;
                for (uint i = 0; i < component_count; i++)
                {
                    _ptrs0.push_back(cur);
                    cur += (splice_immediate) ?
                        1 : 1 + SpliceTimeView(cur).size();
                }
            }
            _ptrs1.push_back(cur);
            _ptrs1.push_back(cur + (duration ? 5 : 0));
        }
    }
    else
    {
        _epilog = NULL;
    }

    return _epilog != NULL;
}

QString SpliceInformationTable::EncryptionAlgorithmString(void) const
{
    uint alg = EncryptionAlgorithm();
    switch (alg)
    {
        case kNoEncryption: return "None";
        case kECB:          return "DES-ECB";
        case kCBC:          return "DES-CBC";
        case k3DES:         return "3DES";
        default:
            return QString((alg<32) ? "Reserved(%1)" : "Private(%1)").arg(alg);
    }
}

QString SpliceInformationTable::SpliceCommandTypeString(void) const
{
    uint type = SpliceCommandType();
    switch (type)
    {
        case kSCTNull:
            return "Null";
        case kSCTSpliceSchedule:
            return "SpliceSchedule";
        case kSCTSpliceInsert:
            return "SpliceInsert";
        case kSCTTimeSignal:
            return "TimeSignal";
        case kSCTBandwidthReservation:
            return "BandwidthReservation";
        case kSCTPrivateCommand:
            return "Private";
        default:
            return QString("Reserved(%1)").arg(type);
    };
}

QString SpliceInformationTable::toString(int64_t first, int64_t last) const
{
    QString str =
        QString("SpliceInformationSection enc_alg(%1) pts_adj(%2)")
        .arg(IsEncryptedPacket()?EncryptionAlgorithmString():"None")
        .arg(PTSAdjustment());
    str += IsEncryptedPacket() ? QString(" cw_index(%1)") : QString("");
    str += QString(" command_len(%1) command_type(%2)")
        .arg(SpliceCommandLength())
        .arg(SpliceCommandTypeString());

    if (IsEncryptedPacket())
        return str;

    switch (SpliceCommandType())
    {
        case kSCTSpliceSchedule:
            break;
        case kSCTSpliceInsert:
        {
            str += "\n  " + SpliceInsert().toString(first, last);
            break;
        }
        case kSCTTimeSignal:
            break;
    }

    return str;
}

QString SpliceInsertView::toString(int64_t first, int64_t last) const
{
    QString str = 
        QString("eventid(0x%1) cancel(%2) "
                "out_of_network(%3) program_splice(%4) "
                "duration(%5) immediate(%6)\n  ")
        .arg(SpliceEventID(),0,16)
        .arg(IsSpliceEventCancel()?"yes":"no")
        .arg(IsOutOfNetwork()?"yes":"no")
        .arg(IsProgramSplice()?"yes":"no")
        .arg(IsDuration()?"yes":"no")
        .arg(IsSpliceImmediate()?"yes":"no");

    if (IsProgramSplice() && !IsSpliceImmediate())
        str += SpliceTime().toString(first, last);

    str += QString(" unique_program_id(%1)")
        .arg(UniqueProgramID());

    str += QString(" avail(%1/%2)")
        .arg(AvailNum()).arg(AvailsExpected());

    return str;
}

QString SpliceInformationTable::toStringXML(
    uint indent_level, int64_t first, int64_t last) const
{
    QString indent = xml_indent(indent_level);

    QString cap_time = "";
    if (first >= 0)
    {
        cap_time = QString("pts=\"%1\" ").arg(first);
        if (last >= 0) 
        {
            QTime abs = QTime(0,0,0,0).addMSecs((last - first)/90);
            cap_time += QString("capture_time=\"%1\" ")
                .arg(abs.toString("hh:mm:ss.zzz"));
        }
    }

    QString str = QString(
        "%1<SpliceInformationSection %2 encryption_algorithm=\"%3\" "
        "pts_adjustment=\"%4\" code_word_index=\"%5\" command_type=\"%6\">\n")
        .arg(indent)
        .arg(cap_time)
        .arg(EncryptionAlgorithmString())
        .arg(PTSAdjustment())
        .arg(CodeWordIndex())
        .arg(SpliceCommandTypeString());

    if (IsEncryptedPacket())
        return str + indent + "</SpliceInformationSection>";

    switch (SpliceCommandType())
    {
        case kSCTSpliceSchedule:
            break;
        case kSCTSpliceInsert:
        {
            str += SpliceInsert().toStringXML(indent_level + 1, first, last);
            str += "\n";
            break;
        }
        case kSCTTimeSignal:
            break;
    }

    str += indent + "</SpliceInformationSection>";
    return str;
}

QString SpliceInsertView::toStringXML(
    uint indent_level, int64_t first, int64_t last) const
{
    QString indent_0 = xml_indent(indent_level);
    QString indent_1 = xml_indent(indent_level + 1);
    QString str = QString(
        "%1<SpliceInsert eventid=\"0x%2\" cancel=\"%3\"\n")
        .arg(indent_0)
        .arg(SpliceEventID(),0,16)
        .arg(xml_bool_to_string(IsSpliceEventCancel()));

    str += QString(
        "%1out_of_network=\"%2\" program_splice=\"%3\" duration=\"%4\"\n")
        .arg(indent_1)
        .arg(xml_bool_to_string(IsOutOfNetwork()))
        .arg(xml_bool_to_string(IsProgramSplice()))
        .arg(xml_bool_to_string(IsDuration()));

    str += QString(
        "%1immediate=\"%2\" unique_program_id=\"%3\"\n"
        "%4avail_num=\"%5\" avails_expected=\"%6\">\n")
        .arg(indent_1)
        .arg(xml_bool_to_string(IsSpliceImmediate()))
        .arg(UniqueProgramID())
        .arg(indent_1)
        .arg(AvailNum())
        .arg(AvailsExpected());
    
    if (IsProgramSplice() && !IsSpliceImmediate())
    {
        str += SpliceTime().toStringXML(indent_level + 1, first, last) + "\n";
    }

    str += indent_0 + "</SpliceInsert>";
    return str;
}
