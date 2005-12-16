/*
 *  Class DVBRecorder
 *
 *  Copyright (C) Kenneth Aafloy 2003
 *
 *  Description:
 *      Has the responsibility of opening the Demux device and reading
 *      data from it. Code for controlling which of the mpeg2 streams
 *      from the DVB device gets through. In ProcessData there is
 *      a 'map builder' which saves information about the stream
 *      to the database.
 *
 *  Author(s):
 *      Isaac Richards
 *          - Wrote the original class this work derived from.
 *      Kenneth Aafloy (ke-aa at frisurf.no)
 *          - Rewritten Recording Functions.
 *          - Moved PID handling here and rewritten.
 *      Ben Bucksch
 *          - Developed the original implementation.
 *      Dave Chapman (dave at dchapman.com)
 *          - The dvbstream library, which some code,
 *            in ::StartRecording, is based upon.
 *      Martin Smith (martin at spamcop.net)
 *          - The signal quality monitor
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

// C includes
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

// C++ includes
#include <iostream>
#include <algorithm>
using namespace std;

// MythTV includes
#include "RingBuffer.h"
#include "programinfo.h"
#include "mpegtables.h"
#include "iso639.h"

// MythTV DVB includes
#include "transform.h"
#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

// AVLib/FFMPEG includes
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"

const int DVBRecorder::PMT_PID = 0x20; ///< PID for rewritten PMT
const int DVBRecorder::TSPACKETS_BETWEEN_PSIP_SYNC = 2000;
const int DVBRecorder::POLL_INTERVAL        =  50; // msec
const int DVBRecorder::POLL_WARNING_TIMEOUT = 500; // msec

#define USE_DRB
#ifndef USE_DRB
static ssize_t safe_read(int fd, unsigned char *buf, size_t bufsize);
#endif // USE_DRB

#define LOC      QString("DVBRec(%1): ").arg(_card_number_option)
#define LOC_WARN QString("DVBRec(%1) Warning: ").arg(_card_number_option)
#define LOC_ERR  QString("DVBRec(%1) Error: ").arg(_card_number_option)

DVBRecorder::DVBRecorder(TVRec *rec, DVBChannel* advbchannel)
    : DTVRecorder(rec, "DVBRecorder"),
      _drb(NULL),
      // Options set in SetOption()
      _card_number_option(0), _record_transport_stream_option(false),
      _hw_decoder_option(false),
      // DVB stuff
      dvbchannel(advbchannel),
      _reset_pid_filters(true),
      _pid_lock(true),
      // PS recorder stuff
      _ps_rec_audio_id(0xC0), _ps_rec_video_id(0xE0),
      // Output stream info
      _pat(NULL), _pmt(NULL), _next_pmt_version(0),
      _ts_packets_until_psip_sync(0),
      // Statistics
      _continuity_error_count(0), _stream_overflow_count(0),
      _bad_packet_count(0)
{
    bzero(_ps_rec_buf, sizeof(unsigned char) * 3);

#ifdef USE_DRB
    _drb = new DeviceReadBuffer(this);
    _buffer_size = (1024*1024 / TSPacket::SIZE) * TSPacket::SIZE;
#else
    _buffer_size = (4*1024*1024 / TSPacket::SIZE) * TSPacket::SIZE;
#endif

    _buffer = new unsigned char[_buffer_size];
    bzero(_buffer, _buffer_size);
}

DVBRecorder::~DVBRecorder()
{
    TeardownAll();
}

void DVBRecorder::TeardownAll(void)
{
    // Make SURE that the device read thread is cleaned up -- John Poet
    StopRecording();

    if (IsOpen())
        Close();

    if (_buffer)
    {
        delete[] _buffer;
        _buffer = NULL;
    }

#ifdef USE_DRB
    if (_drb)
    {
        delete _drb;
        _drb = NULL;
    }
#endif

    SetPAT(NULL);
    SetPMT(NULL);
}

void DVBRecorder::deleteLater(void)
{
    TeardownAll();
    DTVRecorder::deleteLater();
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
    {
        _card_number_option = value;
        videodevice = QString::number(value);
    }
    else if (name == "hw_decoder")
        _hw_decoder_option = (value == 1);
    else if (name == "recordts")
        _record_transport_stream_option = (value == 1);
    else
        DTVRecorder::SetOption(name, value);
}

void DVBRecorder::SetOptionsFromProfile(RecordingProfile*, 
                                        const QString &videodev,
                                        const QString&, const QString&, int)
{
    SetOption("cardnum", videodev.toInt());
    DTVRecorder::SetOption("tvformat", gContext->GetSetting("TVFormat"));
}

void DVBRecorder::SetPMTObject(const PMTObject *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);
    VERBOSE(VB_RECORD, LOC + "SetPMTObject()");
    _input_pmt = *_pmt;

    AutoPID();

    /* Rev the PMT version since PIDs are changing */
    _next_pmt_version = (_next_pmt_version + 1) & 0x1f;
    _ts_packets_until_psip_sync = 0;

    dvbchannel->SetCAPMT(&_input_pmt);

    _reset_pid_filters = true;

    bzero(_ps_rec_buf, sizeof(unsigned char) * 3);
}

bool DVBRecorder::Open(void)
{
    if (IsOpen())
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Card already open");
        return true;
    }

    _stream_fd = open(dvbdevice(DVB_DEV_DVR,_card_number_option),
                      O_RDONLY | O_NONBLOCK);
    if (!IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open DVB device" + ENO);
        return false;
    }

#ifdef USE_DRB
    if (_drb)
        _drb->Reset(videodevice, _stream_fd);
#endif

    connect(dvbchannel, SIGNAL(UpdatePMTObject(const PMTObject *)),
            this, SLOT(SetPMTObject(const PMTObject *)));

    VERBOSE(VB_RECORD, LOC + QString("Card opened successfully fd(%1)")
            .arg(_stream_fd) + QString(" (using %2 mode).")
            .arg(_record_transport_stream_option ? "TS" : "PS"));

    dvbchannel->RecorderStarted();

    return true;
}

void DVBRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- begin");

    if (IsOpen())
    {

#ifdef USE_DRB
    if (_drb)
        _drb->Reset(videodevice, -1);
#endif

        CloseFilters();
        close(_stream_fd);
        _stream_fd = -1;
    }

    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- end");
}

void DVBRecorder::CloseFilters(void)
{
    QMutexLocker change_lock(&_pid_lock);

    PIDInfoMap::iterator it;
    for (it = _pid_infos.begin(); it != _pid_infos.end(); ++it)
    {
        (*it)->Close();
        delete *it;
    }
    _pid_infos.clear();
}

void DVBRecorder::OpenFilter(uint           pid,
                             ES_Type        type,
                             dmx_pes_type_t pes_type,
                             uint           stream_type)
{
    // bits per millisecond
    uint bpms = (StreamID::IsVideo(stream_type)) ? 19200 : 500;
    // msec of buffering we want
    uint msec_of_buffering = max(POLL_WARNING_TIMEOUT + 50, 1500);
    // actual size of buffer we need
    uint pid_buffer_size = ((bpms*msec_of_buffering + 7) / 8);
    // rounded up to the nearest page
    pid_buffer_size = ((pid_buffer_size + 4095) / 4096) * 4096;

    VERBOSE(VB_RECORD, LOC + QString("Adding pid 0x%2 size(%3)")
            .arg((int)pid,0,16).arg(pid_buffer_size));

    // Open the demux device
    int fd_tmp = open(dvbdevice(DVB_DEV_DEMUX,_card_number_option), O_RDWR);
    if (fd_tmp < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open demux device." + ENO);
        return;
    }

    // Try to make the demux buffer large enough to
    // allow for longish disk writes.
    uint sz    = pid_buffer_size;
    uint usecs = msec_of_buffering * 1000;
    while (ioctl(fd_tmp, DMX_SET_BUFFER_SIZE, sz) < 0 && sz > 1024*8)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set demux buffer size for "+
                QString("pid 0x%1 to %2").arg(pid,0,16).arg(sz) + ENO);

        sz    /= 2;
        sz     = ((sz+4095)/4096)*4096;
        usecs /= 2;
    }
    VERBOSE(VB_RECORD, LOC + "Set demux buffer size for " +
            QString("pid 0x%1 to %2,\n\t\t\twhich gives us a %3 msec buffer.")
            .arg(pid,0,16).arg(sz).arg(usecs/1000));

    // Set the filter type
    struct dmx_pes_filter_params params;
    bzero(&params, sizeof(params));
    params.input    = DMX_IN_FRONTEND;
    params.output   = DMX_OUT_TS_TAP;
    params.flags    = DMX_IMMEDIATE_START;
    params.pid      = pid;
    params.pes_type = pes_type;
    if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
    {
        close(fd_tmp);

        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set demux filter." + ENO);
        return;
    }

    PIDInfo *info = new PIDInfo();
    // Set isVideo based on stream type
    info->isVideo = StreamID::IsVideo(stream_type);
    // Add the file descriptor to the filter list
    info->filter_fd = fd_tmp;

    // If we are in legacy PES mode, initialize TS->PES converter
    if (!_record_transport_stream_option)
        info->ip = CreateIPack(type);

    // Add the new info to the map
    QMutexLocker change_lock(&_pid_lock);    
    _pid_infos[pid] = info;
}

bool DVBRecorder::OpenFilters(void)
{
    CloseFilters();

    QMutexLocker change_lock(&_pid_lock);

    _ps_rec_audio_id = 0xC0;
    _ps_rec_video_id = 0xE0;

    // Record all streams flagged for recording
    bool need_pcr_pid = true;
    QValueList<ElementaryPIDObject>::const_iterator es;
    for (es = _input_pmt.Components.begin();
         es != _input_pmt.Components.end(); ++es)
    {
        if (!(*es).Record)
            continue;

        int pid = (*es).PID;
        dmx_pes_type_t pes_type;
            
        if (_hw_decoder_option)
        {
            switch ((*es).Type)
            {
                case ES_TYPE_AUDIO_MPEG1:
                case ES_TYPE_AUDIO_MPEG2:
                    pes_type = DMX_PES_AUDIO;
                    break;
                case ES_TYPE_VIDEO_MPEG1:
                case ES_TYPE_VIDEO_MPEG2:
                    pes_type = DMX_PES_VIDEO;
                    break;
                case ES_TYPE_TELETEXT:
                    pes_type = DMX_PES_TELETEXT;
                    break;
                case ES_TYPE_SUBTITLE:
                    pes_type = DMX_PES_SUBTITLE;
                    break;
                default:
                    pes_type = DMX_PES_OTHER;
                    break;
            }
        }
        else
            pes_type = DMX_PES_OTHER;

        OpenFilter(pid, (*es).Type, pes_type, (*es).Orig_Type);

        if (_hw_decoder_option)
        {
            // Set PCRPID if it's not the same as video
            if ((pes_type == DMX_PES_VIDEO) &&
                (pid != _input_pmt.PCRPID) &&
                (_input_pmt.PCRPID != 0))
            {
                OpenFilter(_input_pmt.PCRPID, ES_TYPE_UNKNOWN, DMX_PES_PCR,
                            (*es).Orig_Type);
            }
        }
        else if (pid == _input_pmt.PCRPID)
            need_pcr_pid = false;
    }

    if (!_hw_decoder_option && need_pcr_pid && (_input_pmt.PCRPID != 0))
        OpenFilter(_input_pmt.PCRPID, ES_TYPE_UNKNOWN, DMX_PES_OTHER,
                    (*es).Orig_Type);


    if (_pid_infos.empty())
    {        
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Recording will not commence until a PID is set.");
        return false;
    }
    return true;
}

void DVBRecorder::StartRecording(void)
{
    if (!Open())
    {
        _error = true;
        return;
    }

    _continuity_error_count = 0;
    _stream_overflow_count = 0;
    _bad_packet_count = 0;

    _request_recording = true;
    _recording = true;

    SetPAT(NULL);
    SetPMT(NULL);
    _ts_packets_until_psip_sync = 0;

#ifdef USE_DRB
    bool ok = _drb->Setup(videodevice, _stream_fd);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
        Close();
        _error = true;
        return;
    }
    _drb->Start();
#else
    _poll_timer.start();
#endif // USE_DRB

    while (_request_recording && !_error)
    {
        if (PauseAndWait())
            continue;

        if (!IsOpen())
        {
            usleep(5000);
            continue;
        }

        if (_reset_pid_filters)
        {
            _reset_pid_filters = false;
            VERBOSE(VB_RECORD, LOC + "Resetting Demux Filters");
            if (OpenFilters())
            {
                CreatePAT();
                CreatePMT();
            }
        }

        if (Poll())
        {
#ifdef USE_DRB
            ssize_t len = _drb->Read(_buffer, _buffer_size);
#else // if !USE_DRB
            ssize_t len = safe_read(_stream_fd, _buffer, _buffer_size);
#endif // !USE_DRB
            if (len > 0)
                ProcessDataTS(_buffer, len);
        }

#ifdef USE_DRB
        // Check for DRB errors
        if (_drb->IsErrored())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Device error detected");
            _error = true;
        }

        if (_drb->IsEOF())
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Device EOF detected");
            _error = true;
        }
#endif // !USE_DRB
    }

#ifdef USE_DRB
    if (_drb && _drb->IsRunning())
        _drb->Stop();
#endif        

    Close();

    FinishRecording();

    _recording = false;
}

void DVBRecorder::WritePATPMT(void)
{
    if (_ts_packets_until_psip_sync <= 0)
    {
        QMutexLocker read_lock(&_pid_lock);
        uint next_cc;
        if (_pat && _pmt && ringBuffer)
        {
            next_cc = (_pat->tsheader()->ContinuityCounter()+1)&0xf;
            _pat->tsheader()->SetContinuityCounter(next_cc);
            BufferedWrite(*(reinterpret_cast<TSPacket*>(_pat->tsheader())));

            next_cc = (_pmt->tsheader()->ContinuityCounter()+1)&0xf;
            _pmt->tsheader()->SetContinuityCounter(next_cc);
            BufferedWrite(*(reinterpret_cast<TSPacket*>(_pmt->tsheader())));

            _ts_packets_until_psip_sync = TSPACKETS_BETWEEN_PSIP_SYNC;
        }
    }
    else
        _ts_packets_until_psip_sync--;
}

void DVBRecorder::Reset(void)
{
    DTVRecorder::Reset();

    if (curRecording)
    {
        curRecording->ClearPositionMap(MARK_GOP_BYFRAME);
    }
}

void DVBRecorder::SetPAT(ProgramAssociationTable *new_pat)
{
    QMutexLocker change_lock(&_pid_lock);

    ProgramAssociationTable *old_pat = _pat;
    _pat = new_pat;
    if (old_pat)
        delete old_pat;

    if (_pat)
        VERBOSE(VB_SIPARSER, "DVBRecorder::SetPAT()\n" + _pat->toString());
    else
        VERBOSE(VB_SIPARSER, "DVBRecorder::SetPAT(NULL)");
}

void DVBRecorder::SetPMT(ProgramMapTable *new_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    ProgramMapTable *old_pmt = _pmt;
    _pmt = new_pmt;
    if (old_pmt)
        delete old_pmt;

    if (_pmt)
        VERBOSE(VB_SIPARSER, "DVBRecorder::SetPMT()\n" + _pmt->toString());
    else
        VERBOSE(VB_SIPARSER, "DVBRecorder::SetPMT(NULL)");
}

void DVBRecorder::CreatePAT(void)
{
    QMutexLocker read_lock(&_pid_lock);
    uint cc = 0;
    if (_pat)
        cc = (_pat->tsheader()->ContinuityCounter()+1) & 0xf;

    uint tsid = 1; // transport stream id
    vector<uint> pnum, pid;
    pnum.push_back(1); // program number / service id
    pid.push_back(PMT_PID);

    SetPAT(ProgramAssociationTable::Create(tsid, cc, pnum, pid));
}

static void DescList_to_desc_list(DescriptorList &list, desc_list_t &vec)
{
    vec.clear();
    for (DescriptorList::iterator it = list.begin(); it != list.end(); ++it)
        vec.push_back((*it).Data);
}

void DVBRecorder::CreatePMT(void)
{
    QMutexLocker read_lock(&_pid_lock);

    // Figure out what goes into the PMT
    uint programNumber = 1; // MPEG Program Number
    desc_list_t gdesc;
    vector<uint> pids;
    vector<uint> types;
    vector<desc_list_t> pdesc;
    QValueList<ElementaryPIDObject>::iterator it;

    DescList_to_desc_list(_input_pmt.Descriptors, gdesc);

    it = _input_pmt.Components.begin();
    for (; it != _input_pmt.Components.end(); ++it)
    {
        if ((*it).Record)
        {
            pdesc.resize(pdesc.size()+1);
            DescList_to_desc_list((*it).Descriptors, pdesc.back());
            pids.push_back((*it).PID);
            uint type = StreamID::Normalize((*it).Orig_Type, pdesc.back());
            types.push_back(type);
        }
    }

    // Create the PMT
    ProgramMapTable *pmt = ProgramMapTable::Create(
        programNumber, PMT_PID, _input_pmt.PCRPID,
        _next_pmt_version, gdesc,
        pids, types, pdesc);

    // Set the continuity counter...
    if (_pmt)
    {
        uint cc = (_pmt->tsheader()->ContinuityCounter()+1) & 0xf;
        pmt->tsheader()->SetContinuityCounter(cc);
    }

    SetPMT(pmt);
}

void DVBRecorder::StopRecording(void)
{
    _request_recording = false;
#ifdef USE_DRB
    if (_drb && _drb->IsRunning())
        _drb->Stop();

    while (_recording)
        usleep(2000);
#endif
}

void DVBRecorder::ReaderPaused(int /*fd*/)
{
#ifdef USE_DRB
    pauseWait.wakeAll();
    if (tvrec)
        tvrec->RecorderPaused();
#endif // USE_DRB
}

bool DVBRecorder::PauseAndWait(int timeout)
{
#ifdef USE_DRB
    if (request_pause)
    {
        paused = true;
        if (!_drb->IsPaused())
            _drb->SetRequestPause(true);

        unpauseWait.wait(timeout);
    }
    else if (_drb->IsPaused())
    {
        _drb->SetRequestPause(false);
        _drb->WaitForUnpause(timeout);
        paused = _drb->IsPaused();
    }
    else
    {
        paused = false;
    }
    return paused;
#else // if !USE_DRB
    return RecorderBase::PauseAndWait(timeout);
#endif // !USE_DRB
}

uint DVBRecorder::ProcessDataTS(unsigned char *buffer, uint len)
{
    if (len % TSPacket::SIZE)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Incomplete packet received.");
        len = len - (len % TSPacket::SIZE);
    }
    if (len < TSPacket::SIZE)
        return len;

    uint pos = 0;
    uint end = len - TSPacket::SIZE;
    while (pos <= end)
    {
        const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[pos]);
        ProcessTSPacket(*pkt);
        pos += TSPacket::SIZE;
    }
    return len - pos;
}

bool DVBRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    if (tspacket.TransportError())
    {
        VERBOSE(VB_RECORD, LOC + "Packet dropped due to uncorrectable error.");
        ++_bad_packet_count;
        return false; // Drop bad TS packets...
    }

    const uint pid = tspacket.PID();

    QMutexLocker locker(&_pid_lock);
    PIDInfo *info = _pid_infos[pid];
    if (!info)
       info = _pid_infos[pid] = new PIDInfo();

    // track scrambled pids
    if (tspacket.ScramplingControl())
    {
        if (!info->isEncrypted)
        {
            VERBOSE(VB_RECORD, LOC +
                    QString("PID 0x%1 is encrypted, ignoring").arg(pid,0,16));
            info->isEncrypted = true;
        }
        return true; // Drop encrypted TS packets...
    }
    else if (info->isEncrypted)
    {
        VERBOSE(VB_RECORD, LOC +
                QString("PID 0x%1 is no longer encrypted").arg(pid,0,16));
        info->isEncrypted = false;
    }

    // Check continuity counter
    if (tspacket.HasPayload())
    {
        if (!info->CheckCC(tspacket.ContinuityCounter()))
        {
            VERBOSE(VB_RECORD, LOC +
                    QString("PID 0x%1 discontinuity detected").arg(pid,0,16));
            _continuity_error_count++;
        }
    }

    // handle legacy PES recording mode
    if (!_record_transport_stream_option)
    {
        // handle PS recording

        ipack *ip = info->ip;
        if (ip == NULL)
            return true;

        ip->ps = 1;

        if (tspacket.PayloadStart() && (ip->plength == MMAX_PLENGTH-6))
        {
            ip->plength = ip->found-6;
            ip->found = 0;
            send_ipack(ip);
            reset_ipack(ip);
        }

        uint afc_offset = tspacket.AFCOffset();
        if (afc_offset > 187)
            return true;

        instant_repack((uint8_t*)tspacket.data() + afc_offset,
                       TSPacket::SIZE - afc_offset, ip);
        return true;
    }

    // handle TS recording

    // Check for keyframes and count frames
    if (info->isVideo)
        _buffer_packets = !FindKeyframes(&tspacket);

    // Sync recording start to first keyframe
    if (_wait_for_keyframe_option && _first_keyframe<0)
        return true;

    // Sync streams to the first Payload Unit Start Indicator
    // _after_ first keyframe iff _wait_for_keyframe_option is true
    if (!info->payloadStartSeen)
    {
        if (!tspacket.PayloadStart())
            return true; // not payload start - drop packet

        VERBOSE(VB_RECORD,
                QString("PID 0x%1 Found Payload Start").arg(pid,0,16));
        info->payloadStartSeen = true;
    }

    // Write PAT & PMT tables occasionally
    WritePATPMT();

    // Write Data
    BufferedWrite(tspacket);

    return true;
}

////////////////////////////////////////////////////////////
// Stuff below this comment will be phased out after 0.20 //
////////////////////////////////////////////////////////////

static void print_pmt_info(
    QString pre, const QValueList<ElementaryPIDObject> &pmt_list,
    bool fta, uint program_number, uint pcr_pid)
{
    if (!(print_verbose_messages|VB_RECORD))
        return;

    VERBOSE(VB_RECORD, pre +
            QString("AutoPID for MPEG Program Number(%1), PCR PID(0x%2)")
            .arg(program_number).arg((pcr_pid),0,16));

    QValueList<ElementaryPIDObject>::const_iterator it;
    for (it = pmt_list.begin(); it != pmt_list.end(); ++it)
    {
        VERBOSE(VB_RECORD, pre +
                QString("AutoPID %1 PID 0x%2, %3")
                .arg(((*it).Record) ? "recording" : "skipping")
                .arg((*it).PID,0,16).arg((*it).Description));
    }

    VERBOSE(VB_RECORD, pre + "AutoPID Complete - PAT/PMT Loaded for service\n"
            "\t\t\tA/V Streams are " + ((fta ? "unencrypted" : "ENCRYPTED")));
}

/// processes pmt program list for things to record
/// return true if audio is found
bool select_streams_for_hw_decode(QValueList<ElementaryPIDObject> &pmt_list,
                                  bool use_ts,
                                  bool use_language_pref = true)
{
    // Wanted stream types:
    QValueList<ES_Type> StreamTypes;
    StreamTypes += ES_TYPE_VIDEO_MPEG1;
    StreamTypes += ES_TYPE_VIDEO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_MPEG1;
    StreamTypes += ES_TYPE_AUDIO_MPEG2;
    StreamTypes += ES_TYPE_AUDIO_AC3;
    if (use_ts)
    {
        // The following types are only supported with TS recording
        StreamTypes += ES_TYPE_AUDIO_DTS;
        StreamTypes += ES_TYPE_AUDIO_AAC;
        StreamTypes += ES_TYPE_TELETEXT;
        StreamTypes += ES_TYPE_SUBTITLE;
    }

    QMap<ES_Type, bool> flagged;
    // Wanted languages:
    QStringList preferred_languages;
    if (use_language_pref)
        preferred_languages = iso639_get_language_list();

    QValueList<ElementaryPIDObject>::iterator it;
    for (it = pmt_list.begin(); it != pmt_list.end(); ++it)
    {
        (*it).Record = false;
        if (!StreamTypes.contains((*it).Type))
        {
            // Type not wanted
            continue;
        }

        if (((*it).Type == ES_TYPE_AUDIO_MPEG1) ||
            ((*it).Type == ES_TYPE_AUDIO_MPEG2) ||
            ((*it).Type == ES_TYPE_AUDIO_AC3))
        {
            bool ignore = false;
            // Check for audio descriptors
            DescriptorList::Iterator dit;
            for (dit = (*it).Descriptors.begin();
                 dit != (*it).Descriptors.end(); ++dit)
            {
                // Check for "Hearing impaired" or 
                // "Visual impaired commentary" stream
                if (((*dit).Data[0] == 0x0A) &&
                    ((*dit).Data[5] & 0xFE == 0x02))
                {
                    ignore = true;
                    break;
                }
            }

            if (ignore)
                continue; // Ignore this stream
        }

        // Limit hardware decoders to one A/V stream
        switch ((*it).Type)
        {
            case ES_TYPE_VIDEO_MPEG1:
            case ES_TYPE_VIDEO_MPEG2:
                if (flagged.contains(ES_TYPE_VIDEO_MPEG1) ||
                    flagged.contains(ES_TYPE_VIDEO_MPEG2))
                {
                    continue; // Ignore this stream
                }
                break;

            case ES_TYPE_AUDIO_MPEG1: 
            case ES_TYPE_AUDIO_MPEG2:
            case ES_TYPE_AUDIO_AC3:
            case ES_TYPE_AUDIO_DTS:
            case ES_TYPE_AUDIO_AAC:
                if (flagged.contains(ES_TYPE_AUDIO_MPEG1) ||
                    flagged.contains(ES_TYPE_AUDIO_MPEG2) ||
                    flagged.contains(ES_TYPE_AUDIO_AC3)   ||
                    flagged.contains(ES_TYPE_AUDIO_DTS)   ||
                    flagged.contains(ES_TYPE_AUDIO_AAC))
                {
                    continue; // Ignore this stream
                }
                break;

            default:
                break;
        }

        // Record if no specific language wanted or component
        // has no language or this is a desirable language.
        if (preferred_languages.isEmpty() ||
            (*it).Language.isEmpty() ||
            preferred_languages.contains((*it).Language))
        {
            (*it).Record = true;
            flagged[(*it).Type] = true;
        }
    }

    return (flagged.contains(ES_TYPE_AUDIO_MPEG1) ||
            flagged.contains(ES_TYPE_AUDIO_MPEG2) ||
            flagged.contains(ES_TYPE_AUDIO_AC3)   ||
            flagged.contains(ES_TYPE_AUDIO_DTS)   ||
            flagged.contains(ES_TYPE_AUDIO_AAC));
}

/** \fn DVBRecorder::AutoPID(void)
 *  \brief Process PMT and decide which components should be recorded
 *
 *   This is particularly for hardware decoders which don't
 *   like to see more than one audio or one video stream.
 *
 *   When the hardware decoder is not specified all components
 *   are recorded.
 */
void DVBRecorder::AutoPID(void)
{
    QMutexLocker change_lock(&_pid_lock);
    QValueList<ElementaryPIDObject> &pmt_list = _input_pmt.Components;
    QValueList<ElementaryPIDObject>::iterator it;

    if (_hw_decoder_option)
    {
        bool ts = _record_transport_stream_option;
        if (!select_streams_for_hw_decode(pmt_list, ts))
            select_streams_for_hw_decode(pmt_list, ts, false);
    }
    else
    {
        // If this is not for a hardware decoder, record everything
        // we know about (ffmpeg doesn't like some DVB streams).
        for (it = pmt_list.begin(); it != pmt_list.end(); ++it)
        {
            desc_list_t list;
            DescList_to_desc_list((*it).Descriptors, list);
            uint type = StreamID::Normalize((*it).Orig_Type, list);
            if (StreamID::IsAudio(type) || StreamID::IsVideo(type) ||
                (ES_TYPE_TELETEXT == (*it).Type) ||
                (ES_TYPE_SUBTITLE == (*it).Type))
            {                
                (*it).Record = true;
            }
        }
    }

    print_pmt_info(LOC, pmt_list,
                   _input_pmt.FTA(), _input_pmt.ServiceID, _input_pmt.PCRPID);
}

bool DVBRecorder::Poll(void) const
{
#ifdef USE_DRB
    return true;
#else // if !USE_DRB
    struct pollfd polls;
    polls.fd      = _stream_fd;
    polls.events  = POLLIN;
    polls.revents = 0;

    int ret;
    do
        ret = poll(&polls, 1, POLL_INTERVAL);
    while (!request_pause && IsOpen() &&
           ((-1 == ret) && ((EAGAIN == errno) || (EINTR == errno))));

    if (request_pause || !IsOpen())
        return false;

    if (ret > 0 && polls.revents & POLLIN)
    {
        if (_poll_timer.elapsed() >= POLL_WARNING_TIMEOUT)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("Got data from card after %1 ms. (>%2)")
                    .arg(_poll_timer.elapsed()).arg(POLL_WARNING_TIMEOUT));
        }
        _poll_timer.start();
        return true;
    }

    if (ret == 0 && _poll_timer.elapsed() > POLL_WARNING_TIMEOUT)
    {
        VERBOSE(VB_GENERAL, LOC_WARN +
                QString("No data from card in %1 ms.")
                .arg(_poll_timer.elapsed()));
    }

    if (ret < 0 && (EOVERFLOW == errno))
    {
        _stream_overflow_count++;
        VERBOSE(VB_RECORD, LOC_ERR + "Driver buffer overflow detected.");
    }

    if ((ret < 0) || (ret > 0 && polls.revents & POLLERR))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Poll failed while waiting for data." + ENO);
    }

    return false;
#endif // USE_DRB
}

void DVBRecorder::ProcessDataPS(unsigned char *buffer, uint len)
{
    if (len < 4)
        return;

    if (buffer[0] != 0x00 || buffer[1] != 0x00 || buffer[2] != 0x01)
    {
        if (!_wait_for_keyframe_option || _first_keyframe>=0)
            ringBuffer->Write(buffer, len);
        return;
    }

    uint stream_id = buffer[3];
    if ((stream_id >= PESStreamID::MPEGVideoStreamBegin) &&
        (stream_id <= PESStreamID::MPEGVideoStreamEnd))
    {
        uint pos = 8 + buffer[8];
        uint datalen = len - pos;

        unsigned char *bufptr = &buffer[pos];
        uint state = 0xFFFFFFFF;
        uint state_byte = 0;
        int prvcount = -1;

        uint framesWritten = _frames_written_count;
        while (bufptr < &buffer[pos] + datalen)
        {
            state_byte  = (++prvcount < 3) ? _ps_rec_buf[prvcount] : *bufptr++;
            uint last   = state;
            state       = ((state << 8) | state_byte) & 0xFFFFFF;
            stream_id   = state_byte;

            // Skip non-prefixed stream id's and skip slice PES stream id's
            if ((last != 0x000001) ||
                ((stream_id >= PESStreamID::SliceStartCodeBegin) &&
                 (stream_id <= PESStreamID::SliceStartCodeEnd)))
            {
                continue;
            }

            // Now process the stream id's we care about
            if (PESStreamID::PictureStartCode == stream_id)
                _frames_written_count++;
            else if (PESStreamID::SequenceStartCode == stream_id)
                _first_keyframe = framesWritten;
            else if (PESStreamID::GOPStartCode == stream_id)
            {
                _position_map_lock.lock();
                bool save_map = false;
                if (!_position_map.contains(_frames_written_count))
                {
                    long long startpos = ringBuffer->GetWritePosition();
                    _position_map_delta[_frames_written_count] = startpos;
                    _position_map[_frames_written_count] = startpos;
                    save_map = true;
                }
                _position_map_lock.unlock();
                if (save_map)
                    SavePositionMap(false);
            }
        }
    }
    memcpy(_ps_rec_buf, &buffer[len - 3], 3);

    if (!_wait_for_keyframe_option || _first_keyframe>=0)
        ringBuffer->Write(buffer, len);
}

void DVBRecorder::process_data_ps_cb(unsigned char *buffer,
                                     int len, void *priv)
{
    if (len>=0)
        ((DVBRecorder*)priv)->ProcessDataPS(buffer, (uint)len);
}

ipack *DVBRecorder::CreateIPack(ES_Type type)
{
    ipack* ip = (ipack*)malloc(sizeof(ipack));
    assert(ip);
    switch (type)
    {
        case ES_TYPE_VIDEO_MPEG1:
        case ES_TYPE_VIDEO_MPEG2:
            init_ipack(ip, 2048, process_data_ps_cb);
            ip->replaceid = _ps_rec_video_id++;
            break;

        case ES_TYPE_AUDIO_MPEG1:
        case ES_TYPE_AUDIO_MPEG2:
            init_ipack(ip, 65535, process_data_ps_cb); /* don't repack PES */
            ip->replaceid = _ps_rec_audio_id++;
            break;

        case ES_TYPE_AUDIO_AC3:
            init_ipack(ip, 65535, process_data_ps_cb); /* don't repack PES */
            ip->priv_type = PRIV_TS_AC3;
            break;

        case ES_TYPE_SUBTITLE:
        case ES_TYPE_TELETEXT:
            init_ipack(ip, 65535, process_data_ps_cb); /* don't repack PES */
            ip->priv_type = PRIV_DVB_SUB;
            break;

        default:
            init_ipack(ip, 2048, process_data_ps_cb);
            break;
    }
    ip->data = (void*)this;
    return ip;
}

#ifndef USE_DRB
static ssize_t safe_read(int fd, unsigned char *buf, size_t bufsize)
{
    ssize_t size = read(fd, buf, bufsize);

    if ((size < 0) &&
        (errno != EAGAIN) && (errno != EOVERFLOW) && (EINTR != errno))
    {
        VERBOSE(VB_IMPORTANT, "DVB:safe_read(): "
                "Error reading from DVB device." + ENO);
    }

    return size;
}
#endif // USE_DRB

void DVBRecorder::DebugTSHeader(unsigned char* buffer, int len)
{
    (void) len;

    uint8_t sync = buffer[0];
    uint8_t transport_error = (buffer[1] & 0x80) >> 7;
    uint8_t payload_start = (buffer[1] & 0x40) >> 6;
    uint16_t pid = (buffer[1] & 0x1F) << 8 | buffer[2];
    uint8_t transport_scrambled = (buffer[3] & 0xB0) >> 6;
    uint8_t adaptation_control = (buffer[3] & 0x30) >> 4;
    uint8_t counter = buffer[3] & 0x0F;

    int pos=4;
    if (adaptation_control == 2 || adaptation_control == 3)
    {
        unsigned char adaptation_length;
        adaptation_length = buffer[pos++];
        pos += adaptation_length;
    }

    QString debugmsg =
        QString("sync: %1 err: %2 paystart: %3 "
                "pid: %4 enc: %5 adaptation: %6 counter: %7")
        .arg(sync, 2, 16)
        .arg(transport_error)
        .arg(payload_start)
        .arg(pid)
        .arg(transport_scrambled)
        .arg(adaptation_control)
        .arg(counter);

    const TSPacket *pkt = reinterpret_cast<const TSPacket*>(&buffer[0]);
    FindKeyframes(pkt);

    int cardnum = _card_number_option;
    GENERAL(debugmsg);
}
