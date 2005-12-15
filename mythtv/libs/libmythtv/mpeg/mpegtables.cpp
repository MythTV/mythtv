// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mpegtables.h"
#include "atscdescriptors.h"

const unsigned char init_patheader[9] =
{
    0x00,

    0x00, // TableID::PAT
    0xb0, // Syntax indicator
    0x00, // Length (set seperately)
    0x00, // Transport stream ID top bits

    0x00, // Transport stream ID bottom bits
    0xc1, // current | reserved
    0x00, // Current Section
    0x00, // Last Section
};

const unsigned char DEFAULT_PMT_HEADER[9] =
{
    0x00,

    0x02, // TableID::PMT
    0xb0, // Syntax indicator
    0x00, // Length (set seperately)
    0x00, // MPEG Program number top bits (set seperately)

    0x00, // MPEG Program number bottom bits (set seperately)
    0xc1, // Version + Current/Next
    0x00, // Current Section
    0x00, // Last Section
};

uint StreamID::Normalize(uint stream_id, const desc_list_t &desc)
{
    if (OpenCableVideo == stream_id)
        return MPEG2Video;

    if (MPEGDescriptor::Find(desc, DescriptorID::AC3))
        return AC3Audio;

    const unsigned char* d = NULL;
    QString reg("");
    if ((d = MPEGDescriptor::Find(desc, DescriptorID::registration)))
        reg = RegistrationDescriptor(d).FormatIdentifierString();

    if (reg == "DTS1")
        return DTSAudio;

#if 0
    // not needed while there is no specific stream id for these
    if (MPEGDescriptor::Find(desc, DescriptorID::teletext) ||
        MPEGDescriptor::Find(desc, DescriptorID::subtitling))
        return stream_id;
#endif

    return stream_id;
}

ProgramAssociationTable* ProgramAssociationTable::Create(
    uint tsid, uint version,
    const vector<uint>& pnum, const vector<uint>& pid)
{
    const uint count = min(pnum.size(), pid.size());
    ProgramAssociationTable* pat = CreateBlank();
    pat->SetVersionNumber(version);
    pat->SetTranportStreamID(tsid);
    pat->SetLength(PSIP_OFFSET+(count*4));

    // create PAT data
    if ((count * 4) >= (184 - PSIP_OFFSET))
    { // old PAT must be in single TS for this create function
        VERBOSE(VB_IMPORTANT, "PAT::Create: Error, old "
                "PAT size exceeds maximum PAT size.");
        return 0;
    }

    uint offset = PSIP_OFFSET;
    for (uint i=0; i<count; i++)
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

ProgramMapTable* ProgramMapTable::Create(
    uint programNumber, uint basepid, uint pcrpid, uint version,
    vector<uint> pids, vector<uint> types)
{
    const uint count = min(pids.size(), types.size());
    ProgramMapTable* pmt = CreateBlank();
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
    ProgramMapTable* pmt = CreateBlank();
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

    for (uint i=0; i<count; i++)
    {
        vector<unsigned char> pdesc;
        for (uint j=0; j<prog_desc[i].size(); j++)
        {
            uint len = prog_desc[i][j][1] + 2;
            pdesc.insert(pdesc.end(),
                         prog_desc[i][j], prog_desc[i][j] + len);
        }
        
        pmt->AppendStream(pids[i], types[i], &pdesc[0], pdesc.size());
    }
    pmt->Finalize();

    return pmt;
}

void  ProgramMapTable::Parse() const
{
    _ptrs.clear();
    unsigned char *pos =
      const_cast<unsigned char*>
        (pesdata() + PSIP_OFFSET + pmt_header + ProgramInfoLength());
    for (unsigned int i=0; pos < pesdata()+Length(); i++) {
        _ptrs.push_back(pos);
        pos += 5 + StreamInfoLength(i);
    }
    _ptrs.push_back(pos);
    VERBOSE(VB_SIPARSER, "Parsed PMT(0x"<<this<<") "<<this->toString());
}

void ProgramMapTable::AppendStream(
    unsigned int pid, unsigned int type,
    unsigned char* streamInfo, unsigned int infoLength)
{
    if (!StreamCount())
        _ptrs.push_back(pesdata() + PSIP_OFFSET +
                        pmt_header + ProgramInfoLength());
    memset(_ptrs[StreamCount()], 0xff, 5);
    SetStreamPID(StreamCount(), pid);
    SetStreamType(StreamCount(), type);
    SetStreamProgramInfo(StreamCount(), streamInfo, infoLength);
    _ptrs.push_back(_ptrs[StreamCount()]+5+StreamInfoLength(StreamCount()));
    SetLength(_ptrs[StreamCount()] - pesdata());
}

/** \fn ProgramMapTable::IsAudio(uint) const
 *  \brief Returns true iff the stream at index i is an audio stream.
 *
 *   This of course returns true if StreamID::IsAudio() is true.
 *   And, it also returns true if IsAudio returns true after
 *   StreamID::Normalize() is used on the stream type.
 *
 *  \param i index of stream
 */
bool ProgramMapTable::IsAudio(uint i) const
{
    if (StreamID::IsAudio(StreamType(i)))
        return true;

    desc_list_t list = MPEGDescriptor::
        Parse(StreamInfo(i), StreamInfoLength(i));
    uint stream_id = StreamID::Normalize(StreamType(i), list);

    return StreamID::IsAudio(stream_id);
}

bool ProgramMapTable::IsEncrypted(void) const
{
    desc_list_t descs = MPEGDescriptor::Parse(
        ProgramInfo(), ProgramInfoLength());
    const unsigned char* data = MPEGDescriptor::Find(
        descs, DescriptorID::conditional_access);

    if (data)
    {
        ConditionalAccessDescriptor ca(data);
        return 0x0 != ca.SystemID(); // System ID of 0 == no encrytion
    }

    return false;
#if 0
    QMap<uint,uint> encryption_system;
    if (data)
    {
        for (uint i = 0; i < descs.size(); ++i)
        {
            MPEGDescriptor mpegdesc(descs[i]);
            VERBOSE(VB_IMPORTANT, "DTVsm: "<<mpegdesc.toString());
            if (DescriptorID::conditional_access == mpegdesc.DescriptorTag())
            {
                ConditionalAccessDescriptor cad(descs[i]);
                encryption_system[cad.PID()] = cad.SystemID();
            }
        }
    }
#endif
}

bool ProgramMapTable::IsStillPicture(void) const
{
    static const unsigned char STILL_PICTURE_FLAG = 0x01;
    
    for (uint i = 0; i < StreamCount(); i++)
    {
        if (IsVideo(i))
        {
            return StreamInfoLength(i) > 2 &&
                   (StreamInfo(i)[2] & STILL_PICTURE_FLAG);
        }
    }
    return false;
}


/** \fn ProgramMapTable::FindPIDs(uint type, vector<uint>& pids) const
 *  \brief Finds all pids matching type.
 *  \param pids vector pids will be added to
 *  \return number of pids in list
 */
uint ProgramMapTable::FindPIDs(uint type, vector<uint>& pids) const
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
            if (IsVideo(i))
                pids.push_back(StreamPID(i));
    }
    else if (StreamID::AnyAudio == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsAudio(i))
                pids.push_back(StreamPID(i));
    }

    return pids.size();
}

/** \fn ProgramMapTable::FindPIDs(uint, vector<uint>&, vector<uint>&) const
 *  \brief Finds all pids w/types, matching type (useful for AnyVideo/AnyAudio).
 *  \param pids vector pids will be added to
 *  \param type vector types will be added to
 *  \return number of items in pids and types lists.
 */
uint ProgramMapTable::FindPIDs(uint type, vector<uint>& pids,
                               vector<uint>& types) const
{
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
            if (IsVideo(i))
            {
                pids.push_back(StreamPID(i));
                types.push_back(StreamType(i));
            }
    }
    else if (StreamID::AnyAudio == type)
    {
        for (uint i=0; i < StreamCount(); i++)
            if (IsAudio(i))
            {
                pids.push_back(StreamPID(i));
                types.push_back(StreamType(i));
            }
    }

    return pids.size();
}

const QString PSIPTable::toString() const
{
    QString str;
    //for (unsigned int i=0; i<9; i++)
    //str.append(QString(" 0x%1").arg(int(pesdata()[i]), 0, 16));
    //str.append("\n");
    str.append(QString(" PSIP prefix(0x%1) tableID(0x%1) "
                       "length(%2) extension(0x%3)\n")
               .arg(StartCodePrefix(), 0, 16).arg(TableID(), 0, 16)
               .arg(Length()).arg(TableIDExtension(), 0, 16));
    str.append(QString("      version(%1) current(%2) "
                       "section(%3) last_section(%4)\n")
               .arg(Version()).arg(IsCurrent())
               .arg(Section()).arg(LastSection()));
    //str.append(QString("   protocol ver: "<<protocolVersion()<<endl;
    return str;
}

const QString ProgramAssociationTable::toString() const
{
    QString str;
    str.append(QString("Program Association Table\n"));
    str.append(static_cast<const PSIPTable*>(this)->toString());
    str.append(QString("         tsid: %1\n").arg(TransportStreamID()));
    str.append(QString(" programCount: %1\n").arg(ProgramCount()));
    for (unsigned int i=0; i<ProgramCount(); i++) {
        const unsigned char* p=pesdata()+PSIP_OFFSET+(i<<2);
        str.append(QString("  program number %1").arg(int(ProgramNumber(i)))).
            append(QString(" has PID 0x%1   data ").arg(ProgramPID(i),4,16)).
            append(QString(" 0x%1 0x%2").arg(int(p[0])).arg(int(p[1]))).
            append(QString(" 0x%1 0x%2\n").arg(int(p[2])).arg(int(p[3])));
    }
    return str;
}

const QString ProgramMapTable::toString() const
{
    QString str = 
        QString("Program Map Table ver(%1) pid(0x%2) pnum(%3)\n")
        .arg(Version()).arg(tsheader()->PID(), 0, 16)
        .arg(ProgramNumber());

    if (0 != StreamCount())
    {
        vector<const unsigned char*> desc = 
            MPEGDescriptor::Parse(ProgramInfo(), ProgramInfoLength());
        for (uint i=0; i<desc.size(); i++)
            str.append(QString("  %1\n")
                       .arg(MPEGDescriptor(desc[i]).toString()));
    }
    str.append("\n");
    for (unsigned int i=0; i<StreamCount(); i++)
    {
        str.append(QString(" Stream #%1 pid(0x%2) type(%3  0x%4)\n")
                   .arg(i).arg(StreamPID(i), 0, 16)
                   .arg(StreamTypeString(i)).arg(int(StreamType(i))));
        if (0 != StreamInfoLength(i))
        {
            vector<const unsigned char*> desc = 
                MPEGDescriptor::Parse(StreamInfo(i), StreamInfoLength(i));
            for (uint i=0; i<desc.size(); i++)
                str.append(QString("  %1\n")
                           .arg(MPEGDescriptor(desc[i]).toString()));
        }
    }
    return str;
}

const char *StreamID::toString(unsigned int streamID)
{
    // valid for some ATSC/DVB stuff too
    char* retval = "unknown";

    // video
    if (StreamID::MPEG2Video==streamID)
        return "video-mpeg2";
    else if (StreamID::MPEG1Video==streamID)
        return "video-mpeg1";
    else if (StreamID::MPEG4Video==streamID)
        return "video-mpeg4";
    else if (StreamID::H264Video==streamID)
        return "video-h264";
    else if (StreamID::OpenCableVideo==streamID)
        return "video-opencable";

    // audio
    else if (StreamID::AC3Audio==streamID)
        return "audio-ac3";  // EIT, PMT
    else if (StreamID::MPEG2Audio==streamID)
        return "audio-mp2-layer[1,2,3]"; // EIT, PMT
    else if (StreamID::MPEG1Audio==streamID)
        return "audio-mp1-layer[1,2,3]"; // EIT, PMT
    else if (StreamID::AACAudio==streamID)
        return "audio-aac"; // EIT, PMT
    else if (StreamID::DTSAudio==streamID)
        return "audio-dts"; // EIT, PMT

    // other
    else if (StreamID::PrivSec==streamID)
        return "private-sec";
    else if (StreamID::PrivData==streamID)
        return "private-data";

    // DSMCC Object Carousel
    else if (StreamID::DSMCC_A==streamID)
	return "dsmcc-a encap";
    else if (StreamID::DSMCC_B==streamID)
	return "dsmcc-b std data";
    else if (StreamID::DSMCC_C==streamID)
	return "dsmcc-c NPD data";
    else if (StreamID::DSMCC_D==streamID)
	return "dsmcc-d data";

    else switch (streamID)
    {
        case (TableID::STUFFING):
            retval="stuffing"; break; // optionally in any
        case (TableID::CAPTION):
            retval="caption service"; break; // EIT, optionally in PMT
        case (TableID::CENSOR):
            retval="censor"; break; // EIT, optionally in PMT
        case (TableID::ECN):
            retval="extended channel name"; break;
        case (TableID::SRVLOC):
            retval="service location"; break; // required in VCT
        case (TableID::TSS): // other channels with same stuff
            retval="time-shifted service"; break;
        case (TableID::CMPNAME): retval="component name"; break; //??? PMT
    }
    return retval;
}

const QString ProgramMapTable::StreamTypeString(unsigned int i) const
{
    return QString( StreamID::toString(StreamType(i)) );
}
