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
 *      David Matthews (dm at prolingua.co.uk)
 *          - Added video stream for radio channels
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

const int DVBRecorder::PMT_PID = 0x1700; ///< PID for rewritten PMT
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
      // DVB stuff
      dvbchannel(advbchannel),
      _reset_pid_filters(true),
      _pid_lock(true),
      // PS recorder stuff
      _ps_rec_audio_id(0xC0), _ps_rec_video_id(0xE0),
      // Output stream info
      _pat(NULL), _pmt(NULL), _next_pmt_version(0),
      _ts_packets_until_psip_sync(0),
      // Fake video
      _video_stream_fd(-1),
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
    else if (name == "recordts")
        _record_transport_stream_option = (value == 1);
    else
        DTVRecorder::SetOption(name, value);
}

void DVBRecorder::SetOptionsFromProfile(RecordingProfile*, 
                                        const QString &videodev,
                                        const QString&, const QString&)
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

        if (_video_stream_fd >= 0)
        {
            close(_video_stream_fd);
            _video_stream_fd = -1;
        }

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

    VERBOSE(VB_RECORD, LOC + QString("Adding pid 0x%2 size(%3) type(%4)")
            .arg((int)pid,0,16).arg(pid_buffer_size).arg(type));

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

#define TS_TICKS_PER_SEC    90000
#define DUMMY_VIDEO_PID     VIDEO_PID(0x20)

bool DVBRecorder::OpenFilters(void)
{
    bool videoMissing = true;
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

        if ((*es).Type == ES_TYPE_VIDEO_MPEG1 ||
            (*es).Type == ES_TYPE_VIDEO_MPEG2)
            videoMissing = false;
            
        pes_type = DMX_PES_OTHER;

        OpenFilter(pid, (*es).Type, pes_type, (*es).Orig_Type);

        if (pid == _input_pmt.PCRPID)
            need_pcr_pid = false;
    }

    if (need_pcr_pid && (_input_pmt.PCRPID != 0))
        OpenFilter(_input_pmt.PCRPID, ES_TYPE_UNKNOWN, DMX_PES_OTHER,
                    (*es).Orig_Type);

    if (_video_stream_fd >= 0)
    {
        close(_video_stream_fd);
        _video_stream_fd = -1;
    }

    if (videoMissing)
    {
        VERBOSE(VB_RECORD, LOC + "Creating dummy video");
        // Create a dummy video stream.

        QString p = gContext->GetThemesParentDir();
        QString path[] =
        {
            p+gContext->GetSetting("Theme", "G.A.N.T.")+"/",
            p+"default/",
        };
        uint width  = 768;
        uint height = 576;
        
        _frames_per_sec = 50;

        QString filename = QString("dummy%1x%2p%3.ts")
            .arg(width).arg(height)
            .arg(_frames_per_sec, 0, 'f', 2);

        _video_stream_fd = open(path[0]+filename.ascii(), O_RDONLY);
        if (_video_stream_fd < 0)
            _video_stream_fd = open(path[1]+filename.ascii(), O_RDONLY);
        _video_header_pos = 0;
        _audio_header_pos = 0;
        _audio_pid = 0;
        _time_stamp = 0;
        _next_time_stamp = 0;
        _video_cc = 0;
        _ts_change_count = 0;

        PIDInfo *info = new PIDInfo();
        info->isVideo = true;

        QMutexLocker change_lock(&_pid_lock);    
        _pid_infos[DUMMY_VIDEO_PID] = info;
    }

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

    if (_video_stream_fd >= 0)
    {
        pdesc.resize(pdesc.size()+1);
        pdesc.back().clear();
        pids.push_back(DUMMY_VIDEO_PID);
        types.push_back(StreamID::MPEG2Video);
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

    // Create dummy video stream if needed
    if (_video_stream_fd >= 0 && pid != DUMMY_VIDEO_PID)
    {
        GetTimeStamp(tspacket);
        CreateFakeVideo();
    }

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

void DVBRecorder::GetTimeStamp(const TSPacket& tspacket)
{
    const uint pid = tspacket.PID();
    if (pid != _audio_pid)
        _audio_header_pos = 0;

    // Find the current audio time stamp.  This code is based on
    // DTVRecorder::FindKeyframes.
    if (tspacket.PayloadStart())
        _audio_header_pos = 0;
    for (uint i = tspacket.AFCOffset(); i < TSPacket::SIZE; i++)
    {
        const unsigned char k = tspacket.data()[i];
        switch (_audio_header_pos)
        {
            case 0:
                _audio_header_pos = (k == 0x00) ? 1 : 0;
                break;
            case 1:
                _audio_header_pos = (k == 0x00) ? 2 : 0;
                break;
            case 2:
                _audio_header_pos = (k == 0x00) ? 2 : ((k == 0x01) ? 3 : 0);
                break;
            case 3:
                if ((k >= PESStreamID::MPEGAudioStreamBegin) &&
                    (k <= PESStreamID::MPEGAudioStreamEnd))
                {
                    _audio_header_pos++;
                }
                else
                {
                    _audio_header_pos = 0;
                }
                break;
            case 4: case 5: case 6: case 8:
                _audio_header_pos++;
                break;
            case 7:
                _audio_header_pos = (k & 0x80) ? 8 : 0;
                break;
            case 9:
                _audio_header_pos++;
                _next_time_stamp = (k >> 1) & 0x7;
                break;
            case 10:
                _audio_header_pos++;
                _next_time_stamp = (_next_time_stamp << 8) | k;
                break;
            case 11:
                _audio_header_pos++;
                _next_time_stamp = (_next_time_stamp << 7) | ((k >> 1) & 0x7f);
                break;
            case 12:
                _audio_header_pos++;
                _next_time_stamp = (_next_time_stamp << 8) | k;
                break;
            case 13:
                _next_time_stamp = (_next_time_stamp << 7) | ((k >> 1) & 0x7f);
                _audio_header_pos = 0;

                // We've found a time-stamp.
                // Generally the time stamps will increase at a steady rate
                // but they may jump or wrap round.  Because of errors in
                // the stream we may get odd values out of sequence.
                if (_next_time_stamp > _time_stamp+TS_TICKS_PER_SEC*5 ||
                    _next_time_stamp+TS_TICKS_PER_SEC < _time_stamp)
                {
                    if (_ts_change_count != 0 &&
                        _next_time_stamp > _new_time_stamp &&
                        _next_time_stamp <=
                        _new_time_stamp+TS_TICKS_PER_SEC*10)
                    {
                        _ts_change_count++;
                        if (_ts_change_count == 4)
                        {
                            // We've seen 4 stamps in the new sequence.
                            VERBOSE(VB_RECORD, LOC +
                                    QString("Time stamp change: was %1 now %2")
                                    .arg(_time_stamp).arg(_new_time_stamp));
                            _time_stamp = _new_time_stamp;
                            _ts_change_count = 0;
                        }
                        else
                        {
                            _next_time_stamp = _time_stamp;
                        }
                    }
                    else
                    {
                        _new_time_stamp = _next_time_stamp;
                        _next_time_stamp = _time_stamp;
                        _ts_change_count = 1;
                    }
                }
                else
                {
                    _ts_change_count = 0;
                }
        }
    }

    // If we're part way through a packet only continue if it's the same pid
    if (_audio_header_pos != 0)
        _audio_pid = pid;
}

void DVBRecorder::CreateFakeVideo(void)
{
    if (_video_stream_fd < 0 ||
        _next_time_stamp > _time_stamp + TS_TICKS_PER_SEC*5)
    {
        return;
    }

    while (_next_time_stamp > _time_stamp)
    {
        unsigned char buffer[TSPacket::SIZE];
        int len = read(_video_stream_fd, buffer, TSPacket::SIZE);
        if (len == 0)
        {
            // Reached the end - rewind
            lseek(_video_stream_fd, 0, SEEK_SET);
            _video_header_pos = 0;
            continue;
        }
        else if (len != (int)TSPacket::SIZE)
            break; // Something wrong

        TSPacket *pkt = reinterpret_cast<TSPacket*>(buffer);
        if (pkt->PID() != DUMMY_VIDEO_PID)
            continue; // Skip the tables
        // Find the time-stamp field and overwrite it.
        if (pkt->PayloadStart())
            _video_header_pos = 0;
        for (uint i = pkt->AFCOffset(); i < TSPacket::SIZE; i++)
        {
            const unsigned char k = buffer[i];
            switch (_video_header_pos) {
            case 0:
                _video_header_pos = (k == 0x00) ? 1 : 0;
                break;
            case 1:
                _video_header_pos = (k == 0x00) ? 2 : 0;
                break;
            case 2:
                _video_header_pos = (k == 0x00) ? 2 : ((k == 0x01) ? 3 : 0);
                break;
            case 3:
                if (k >= PESStreamID::MPEGVideoStreamBegin &&
                    k <= PESStreamID::MPEGVideoStreamEnd)
                {
                    _video_header_pos++;
                }
                else if (k == PESStreamID::PictureStartCode)
                {
                    _video_header_pos = 0;
                    _time_stamp += (int)
                        ((double)TS_TICKS_PER_SEC/_frames_per_sec);
                }
                else _video_header_pos = 0;
                break;
            case 4: case 5: case 6: case 8:
                _video_header_pos++;
                break;
            case 7:
                // Is there a time-stamp?
                _video_header_pos = (k & 0x80) ? 8 : 0;
                break;
            case 9:
                buffer[i] = (k & 0xf0) | ((_time_stamp >> 29) & 0x0e) | 1;
                _video_header_pos++;
                break;
            case 10:
                buffer[i] = (_time_stamp >> 22) & 0xff;
                _video_header_pos++;
                break;
            case 11:
                buffer[i] = ((_time_stamp >> 14) & 0xfe) | 1;
                _video_header_pos++;
                break;
            case 12:
                buffer[i] = (_time_stamp >> 7) & 0xff; // 7..14
                _video_header_pos++;
                break;
            case 13:
                buffer[i] = ((_time_stamp << 1) & 0xfe) | 1; // 0..6
                _video_header_pos = 0;
            }
        }
        pkt->SetContinuityCounter(_video_cc);
        _video_cc = (_video_cc + 1) & 0xf;
        // Recursive call to pass the packet to the ring-buffer
        ProcessTSPacket(*pkt);
    }
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

    // If this is not for a hardware decoder, record everything
    // we know about (ffmpeg doesn't like some DVB streams).
    for (it = pmt_list.begin(); it != pmt_list.end(); ++it)
    {
        desc_list_t list;
        DescList_to_desc_list((*it).Descriptors, list);
        uint type = StreamID::Normalize((*it).Orig_Type, list);
        (*it).Record |= (StreamID::IsAudio(type) || StreamID::IsVideo(type) ||
                         (ES_TYPE_TELETEXT == (*it).Type) ||
                         (ES_TYPE_SUBTITLE == (*it).Type));
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
