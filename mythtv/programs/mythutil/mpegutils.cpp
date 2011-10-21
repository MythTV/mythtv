// -*- Mode: c++ -*-
/**
 *   MPEG-TS processing utilities (for debugging.)
 *   Copyright (c) 2003-2004, Daniel Thor Kristjansson
 *   Copyright (c) 2011, Digital Nirvana, Inc.
 *   Author: Daniel Kristjansson
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// MythTV headers
#include "streamlisteners.h"
#include "scanstreamdata.h"
#include "premieretables.h"
#include "mythlogging.h"
#include "atsctables.h"
#include "ringbuffer.h"
#include "dvbtables.h"
#include "exitcodes.h"

// Application local headers
#include "mpegutils.h"

static QHash<uint,bool> extract_pids(const QString &pidsStr, bool required)
{
    QHash<uint,bool> use_pid;
    if (pidsStr.isEmpty())
    {
        if (required)
            LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --pids option\n");
    }
    else
    {
        QStringList pidsList = pidsStr.split(",");
        for (uint i = 0; i < (uint) pidsList.size(); i++)
        {
            bool ok;
            uint tmp = pidsList[i].toUInt(&ok, 0);
            if (ok && (tmp < 0x2000))
                use_pid[tmp] = true;
        }
        if (required && use_pid.empty())
        {
            LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
                "At least one pid must be specified\n");
        }
    }
    return use_pid;
}

static int resync_stream(
    const char *buffer, int curr_pos, int len, int packet_size)
{
    // Search for two sync bytes 188 bytes apart,
    int pos = curr_pos;
    int nextpos = pos + packet_size;
    if (nextpos >= len)
        return -1; // not enough bytes; caller should try again

    while (buffer[pos] != SYNC_BYTE || buffer[nextpos] != SYNC_BYTE)
    {
        pos++;
        nextpos++;
        if (nextpos == len)
            return -2; // not found
    }

    return pos;
}

static int pid_counter(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --infile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open input URL\n");
        return GENERIC_EXIT_NOT_OK;
    }

    uint packet_size = cmdline.toUInt("packetsize");
    if (packet_size == 0)
    {
        packet_size = 188;
    }
    else if (packet_size != 188 &&
             packet_size != (188+16) &&
             packet_size != (188+20))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Invalid packet size %1, must be 188, 204, or 208\n")
            .arg(packet_size));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    const int kBufSize = 2 * 1024 * 1024;
    long long pid_count[0x2000];
    memset(pid_count, 0, sizeof(pid_count));
    char *buffer = new char[kBufSize];
    int offset = 0;
    long long total_count = 0;

    while (true)
    {
        int r = srcRB->Read(&buffer[offset], kBufSize - offset);
        if (r <= 0)
            break;
        int pos = 0;
        int len = offset + r;
        while (pos + 187 < len) // while we have a whole packet left
        {
            if (buffer[pos] != SYNC_BYTE)
            {
                pos = resync_stream(buffer, pos+1, len, packet_size);
                if (pos < 0)
                {
                    offset = 0;
                    break;
                }
            }
            int pid = ((buffer[pos+1]<<8) | buffer[pos+2]) & 0x1fff;
            pid_count[pid]++;
            pos += packet_size;
            total_count++;
        }

        if (len - pos > 0)
        {
            memcpy(buffer, buffer + pos, len - pos);
            offset = len - pos;
        }
        else
        {
            offset = 0;
        }
        LOG(VB_STDIO, logLevel,
            QString("\r                                            \r"
                    "Processed %1 packets")
            .arg(total_count));
    }
    LOG(VB_STDIO|VB_FLUSH, logLevel, "\n");

    delete[] buffer;
    delete srcRB;

    for (uint i = 0; i < 0x2000; i++)
    {
        if (pid_count[i])
        {
            LOG(VB_STDIO|VB_FLUSH, LOG_CRIT,
                QString("PID 0x%1 -- %2\n")
                .arg(i,4,16,QChar('0'))
                .arg(pid_count[i],11));
        }
    }

    return GENERIC_EXIT_OK;
}

static int pid_filter(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --infile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    if (cmdline.toString("outfile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --outfile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString dest = cmdline.toString("outfile");

    uint packet_size = cmdline.toUInt("packetsize");
    if (packet_size == 0)
    {
        packet_size = 188;
    }
    else if (packet_size != 188 &&
             packet_size != (188+16) &&
             packet_size != (188+20))
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR,
            QString("Invalid packet size %1, must be 188, 204, or 208\n")
            .arg(packet_size));
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QHash<uint,bool> use_pid = extract_pids(cmdline.toString("pids"), true);
    if (use_pid.empty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open input URL\n");
        return GENERIC_EXIT_NOT_OK;
    }

    RingBuffer *destRB = RingBuffer::Create(dest, true);
    if (!destRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open output URL\n");
        delete srcRB;
        return GENERIC_EXIT_NOT_OK;
    }

    const int kBufSize = 2 * 1024 * 1024;
    char *buffer = new char[kBufSize];
    int offset = 0;
    long long total_count = 0;
    long long write_count = 0;

    while (true)
    {
        int r = srcRB->Read(&buffer[offset], kBufSize - offset);
        if (r <= 0)
            break;
        int pos = 0;
        int len = offset + r;
        while (pos + 187 < len) // while we have a whole packet left
        {
            if (buffer[pos] != SYNC_BYTE)
            {
                pos = resync_stream(buffer, pos+1, len, packet_size);
                if (pos < 0)
                {
                    offset = 0;
                    break;
                }
            }
            int pid = ((buffer[pos+1]<<8) | buffer[pos+2]) & 0x1fff;
            if (use_pid[pid])
            {
                destRB->Write(buffer+pos, packet_size);
                write_count++;
            }
            pos += packet_size;
            total_count++;
        }

        if (len - pos > 0)
        {
            memcpy(buffer, buffer + pos, len - pos);
            offset = len - pos;
        }
        else
        {
            offset = 0;
        }
        LOG(VB_STDIO|VB_FLUSH, logLevel,
            QString("\r                                            \r"
                    "Processed %1 packets")
            .arg(total_count));
    }
    LOG(VB_STDIO|VB_FLUSH, logLevel, "\n");

    delete[] buffer;
    delete srcRB;
    delete destRB;

    LOG(VB_STDIO|VB_FLUSH, logLevel, QString("Wrote %1 of %2 packets\n")
        .arg(write_count).arg(total_count));

    return GENERIC_EXIT_OK;
}

class PTSListener :
    public TSPacketListener,
    public TSPacketListenerAV
{
  public:
    PTSListener() : m_start_code(0xFFFFFFFF)
    {
        for (int i = 0; i < 256; i++)
            m_pts_count[i] = 0;
        for (int i = 0; i < 256; i++)
            m_pts_first[i] = -1LL;
        for (int i = 0; i < 256; i++)
            m_pts_last[i] = -1LL;
            
    }
    bool ProcessTSPacket(const TSPacket &tspacket);
    bool ProcessVideoTSPacket(const TSPacket &tspacket)
        { return ProcessTSPacket(tspacket); }
    bool ProcessAudioTSPacket(const TSPacket &tspacket)
        { return ProcessTSPacket(tspacket); }
    int64_t GetFirstPTS(void) const
    {
        QMap<uint,uint>::const_iterator it = m_pts_streams.begin();
        int64_t pts = -1LL;
        for (; it != m_pts_streams.end(); ++it)
        {
            int64_t v = m_pts_first[*it];
            if (v >= 0)
                pts = min((pts < 0) ? v : pts, v);
        }
        return pts;
    }
    int64_t GetLastPTS(void) const
    {
        QMap<uint,uint>::const_iterator it = m_pts_streams.begin();
        int64_t pts = -1LL;
        for (; it != m_pts_streams.end(); ++it)
        {
            int64_t v = m_pts_last[*it];
            if (v >= 0)
                pts = max(pts, v);
        }
        return pts;
    }
    int64_t GetElapsedPTS(void) const
    {
        int64_t elapsed = GetLastPTS() - GetFirstPTS();
        return (elapsed < 0) ? elapsed + 0x1000000000LL : elapsed;
    }

  public:
    uint32_t m_start_code;
    QMap<uint,uint> m_pts_streams;
    uint32_t m_pts_count[256];
    int64_t  m_pts_first[256];
    int64_t  m_pts_last[256];
};


extern "C" {
extern const uint8_t *ff_find_start_code(
    const uint8_t *p, const uint8_t *end, uint32_t *state);
}

bool PTSListener::ProcessTSPacket(const TSPacket &tspacket)
{
    // if packet contains start of PES packet, start
    // looking for first byte of MPEG start code (3 bytes 0 0 1)
    // otherwise, pick up search where we left off.
    const bool payloadStart = tspacket.PayloadStart();
    m_start_code = (payloadStart) ? 0xffffffff : m_start_code;

    // Scan for PES header codes; specifically picture_start
    // sequence_start (SEQ) and group_start (GOP).
    //   00 00 01 C0-DF: audio stream
    //   00 00 01 E0-EF: video stream
    //   (there are others that we don't care about)
    const uint8_t *bufptr = tspacket.data() + tspacket.AFCOffset();
    const uint8_t *bufend = tspacket.data() + TSPacket::kSize;

    while (bufptr < bufend)
    {
        bufptr = ff_find_start_code(bufptr, bufend, &m_start_code);
        int bytes_left = bufend - bufptr;
        if ((m_start_code & 0xffffff00) == 0x00000100)
        {
            // At this point we have seen the start code 0 0 1
            // the next byte will be the PES packet stream id.
            const int stream_id = m_start_code & 0x000000ff;
            if ((stream_id < 0xc0) || (stream_id > 0xef) ||
                (bytes_left < 10))
            {
                continue;
            }
            bool has_pts = bufptr[3] & 0x80;
            if (has_pts && (bytes_left > 5+5))
            {
                int i = 5;
                int64_t pts =
                    (uint64_t(bufptr[i+0] & 0x0e) << 29) |
                    (uint64_t(bufptr[i+1]       ) << 22) |
                    (uint64_t(bufptr[i+2] & 0xfe) << 14) |
                    (uint64_t(bufptr[i+3]       ) <<  7) |
                    (uint64_t(bufptr[i+4] & 0xfe) >> 1);
                m_pts_streams[stream_id] = stream_id;
                m_pts_last[stream_id] = pts;
                if (m_pts_count[stream_id] < 30)
                {
                    if (!m_pts_count[stream_id])
                        m_pts_first[stream_id] = pts;
                    else if (pts < m_pts_first[stream_id])
                        m_pts_first[stream_id] = pts;
                }
                m_pts_count[stream_id]++;
            }
        }
    }

    return true;
}

class PrintMPEGStreamListener : public MPEGStreamListener
{
  public:
    PrintMPEGStreamListener(PTSListener &ptsl) : m_ptsl(ptsl) { }

    void HandlePAT(const ProgramAssociationTable *pat)
    {
        if (pat)
            LOG(VB_STDIO|VB_FLUSH, logLevel, pat->toString() + "\n");
    }

    void HandleCAT(const ConditionalAccessTable *cat)
    {
        if (cat)
            LOG(VB_STDIO|VB_FLUSH, logLevel, cat->toString() + "\n");
    }

    void HandlePMT(uint program_num, const ProgramMapTable *pmt)
    {
        if (pmt)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("Program Number %1\n").arg(program_num) +
                pmt->toString());
        }
    }

    void HandleEncryptionStatus(uint program_number, bool)
    {
    }

    void HandleSplice(const SpliceInformationTable *sit)
    {
        if (sit)
        {
            QTime ot = QTime(0,0,0,0).addMSecs(m_ptsl.GetElapsedPTS()/90);
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                ot.toString("hh:mm:ss.zzz") + " " +
                sit->toString(m_ptsl.GetFirstPTS(),
                              m_ptsl.GetLastPTS()) + "\n");
        }
    }
  private:
    const PTSListener &m_ptsl;
};

class PrintATSCMainStreamListener : public ATSCMainStreamListener
{
    void HandleSTT(const SystemTimeTable *stt)
    {
        if (stt)
            LOG(VB_STDIO|VB_FLUSH, logLevel, stt->toString() + "\n");
    }

    void HandleMGT(const MasterGuideTable *mgt)
    {
        if (mgt)
            LOG(VB_STDIO|VB_FLUSH, logLevel, mgt->toString() + "\n");
    }

    void HandleVCT(uint pid, const VirtualChannelTable *vct)
    {
        if (vct)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("VCT PID 0x%1\n").arg(pid,0,16) + vct->toString());
        }
    }
};

class PrintATSCAuxStreamListener : public ATSCAuxStreamListener
{
    virtual void HandleTVCT(
        uint pid, const TerrestrialVirtualChannelTable *tvct)
    {
        if (tvct)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("TVCT PID 0x%1\n").arg(pid,0,16) + tvct->toString());
        }
    }

    virtual void HandleCVCT(uint pid, const CableVirtualChannelTable *cvct)
    {
        if (cvct)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("CVCT PID 0x%1\n").arg(pid,0,16) + cvct->toString());
        }
    }

    virtual void HandleRRT(const RatingRegionTable *rrt)
    {
        if (rrt)
            LOG(VB_STDIO|VB_FLUSH, logLevel, rrt->toString() + "\n");
    }

    virtual void HandleDCCT(const DirectedChannelChangeTable *dcct)
    {
        if (dcct)
            LOG(VB_STDIO|VB_FLUSH, logLevel, dcct->toString() + "\n");
    }

    virtual void HandleDCCSCT(
        const DirectedChannelChangeSelectionCodeTable *dccsct)
    {
        if (dccsct)
            LOG(VB_STDIO|VB_FLUSH, logLevel, dccsct->toString() + "\n");
    }
};

class PrintATSCEITStreamListener : public ATSCEITStreamListener
{
    virtual void HandleEIT(uint pid, const EventInformationTable *eit)
    {
        if (eit)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("EIT PID 0x%1\n").arg(pid,0,16) + eit->toString());
        }
    }

    virtual void HandleETT(uint pid, const ExtendedTextTable *ett)
    {
        if (ett)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("ETT PID 0x%1\n").arg(pid,0,16) + ett->toString());
        }
    }
};

class PrintDVBMainStreamListener : public DVBMainStreamListener
{
    virtual void HandleTDT(const TimeDateTable *tdt)
    {
        if (tdt)
            LOG(VB_STDIO|VB_FLUSH, logLevel, tdt->toString() + "\n");
    }

    virtual void HandleNIT(const NetworkInformationTable *nit)
    {
        if (nit)
            LOG(VB_STDIO|VB_FLUSH, logLevel, nit->toString() + "\n");
    }

    virtual void HandleSDT(uint tsid, const ServiceDescriptionTable *sdt)
    {
        if (sdt)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("TSID 0x%1\n").arg(tsid,0,16) + sdt->toString());
        }
    }

};

class PrintDVBOtherStreamListener : public DVBOtherStreamListener
{
    virtual void HandleNITo(const NetworkInformationTable *nit)
    {
        if (nit)
            LOG(VB_STDIO|VB_FLUSH, logLevel, nit->toString() + "\n");
    }

    virtual void HandleSDTo(uint tsid, const ServiceDescriptionTable *sdt)
    {
        if (sdt)
        {
            LOG(VB_STDIO|VB_FLUSH, logLevel,
                QString("TSID 0x%1\n").arg(tsid,0,16) + sdt->toString());
        }
    }

    virtual void HandleBAT(const BouquetAssociationTable *bat)
    {
        if (bat)
            LOG(VB_STDIO|VB_FLUSH, logLevel, bat->toString() + "\n");
    }

};

class PrintDVBEITStreamListener : public DVBEITStreamListener
{
    virtual void HandleEIT(const DVBEventInformationTable *eit)
    {
        if (eit)
            LOG(VB_STDIO|VB_FLUSH, logLevel, eit->toString() + "\n");
    }

    virtual void HandleEIT(const PremiereContentInformationTable *pcit)
    {
        if (pcit)
            LOG(VB_STDIO|VB_FLUSH, logLevel, pcit->toString() + "\n");
    }
};

static int pid_printer(const MythUtilCommandLineParser &cmdline)
{
    if (cmdline.toString("infile").isEmpty())
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Missing --infile option\n");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }
    QString src = cmdline.toString("infile");

    RingBuffer *srcRB = RingBuffer::Create(src, false);
    if (!srcRB)
    {
        LOG(VB_STDIO|VB_FLUSH, LOG_ERR, "Couldn't open input URL\n");
        return GENERIC_EXIT_NOT_OK;
    }

    QHash<uint,bool> use_pid = extract_pids(cmdline.toString("pids"), true);
    if (use_pid.empty())
        return GENERIC_EXIT_INVALID_CMDLINE;

    QHash<uint,bool> use_pid_for_pts =
        extract_pids(cmdline.toString("ptspids"), false);

    ScanStreamData *sd = new ScanStreamData();
    for (QHash<uint,bool>::iterator it = use_pid.begin();
         it != use_pid.end(); ++it)
    {
        sd->AddListeningPID(it.key());
    }

    for (QHash<uint,bool>::iterator it = use_pid_for_pts.begin();
         it != use_pid_for_pts.end(); ++it)
    {
        sd->AddWritingPID(it.key());
    }

    PTSListener                 *ptsl  = new PTSListener();
    PrintMPEGStreamListener     *pmsl  = new PrintMPEGStreamListener(*ptsl);
    PrintATSCMainStreamListener *pasl  = new PrintATSCMainStreamListener();
    PrintATSCAuxStreamListener  *paasl = new PrintATSCAuxStreamListener();
    PrintATSCEITStreamListener  *paesl = new PrintATSCEITStreamListener();
    PrintDVBMainStreamListener  *pdmsl = new PrintDVBMainStreamListener();
    PrintDVBOtherStreamListener *pdosl = new PrintDVBOtherStreamListener();
    PrintDVBEITStreamListener   *pdesl = new PrintDVBEITStreamListener();

    sd->AddWritingListener(ptsl);
    sd->AddMPEGListener(pmsl);
    sd->AddATSCMainListener(pasl);
    sd->AddATSCAuxListener(paasl);
    sd->AddATSCEITListener(paesl);
    sd->AddDVBMainListener(pdmsl);
    sd->AddDVBOtherListener(pdosl);
    sd->AddDVBEITListener(pdesl);

    const int kBufSize = 2 * 1024 * 1024;
    char *buffer = new char[kBufSize];
    int offset = 0;

    while (true)
    {
        int r = srcRB->Read(&buffer[offset], kBufSize - offset);
        if (r <= 0)
            break;

        int len = offset + r;

        offset = sd->ProcessData((const unsigned char*)buffer, len);

        LOG(VB_STDIO|VB_FLUSH, logLevel,
            QString("\r                                            \r"
                    "Processed %1 bytes")
            .arg(len - offset));
    }
    LOG(VB_STDIO|VB_FLUSH, logLevel, "\n");

    if (!cmdline.toString("ptspids").isEmpty())
    {
        QTime ot = QTime(0,0,0,0).addMSecs(ptsl->GetElapsedPTS()/90);

        LOG(VB_STDIO|VB_FLUSH, logLevel,
            QString("First PTS %1, Last PTS %2, elapsed %3 %4\n")
            .arg(ptsl->GetFirstPTS()).arg(ptsl->GetLastPTS())
            .arg(ptsl->GetElapsedPTS())
            .arg(ot.toString("hh:mm:ss.zzz")));
    }

    delete sd;
    delete pmsl;
    delete pasl;
    delete paasl;
    delete paesl;
    delete pdmsl;
    delete pdosl;
    delete pdesl;
    delete srcRB;
    delete ptsl;

    return GENERIC_EXIT_OK;
}

void registerMPEGUtils(UtilMap &utilMap)
{
    utilMap["pidcounter"] = &pid_counter;
    utilMap["pidfilter"]  = &pid_filter;
    utilMap["pidprinter"] = &pid_printer;
}
