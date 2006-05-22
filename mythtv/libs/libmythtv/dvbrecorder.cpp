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
#include "atscstreamdata.h"
#include "cardutil.h"

// MythTV DVB includes
#include "dvbtypes.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"

// AVLib/FFMPEG includes
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"

const int DVBRecorder::TSPACKETS_BETWEEN_PSIP_SYNC = 2000;
const int DVBRecorder::POLL_INTERVAL        =  50; // msec
const int DVBRecorder::POLL_WARNING_TIMEOUT = 500; // msec

#define TS_TICKS_PER_SEC    90000
#define DUMMY_VIDEO_PID     VIDEO_PID(0x20)

#define LOC      QString("DVBRec(%1): ").arg(_card_number_option)
#define LOC_WARN QString("DVBRec(%1) Warning: ").arg(_card_number_option)
#define LOC_ERR  QString("DVBRec(%1) Error: ").arg(_card_number_option)

DVBRecorder::DVBRecorder(TVRec *rec, DVBChannel* advbchannel)
    : DTVRecorder(rec),
      _drb(NULL),
      // Options set in SetOption()
      _card_number_option(0),
      // DVB stuff
      dvbchannel(advbchannel),
      _stream_data(NULL),
      _reset_pid_filters(true),
      _pid_lock(true),
      _input_pmt(NULL),
      // Output stream info
      _pat(NULL), _pmt(NULL),
      _pmt_pid(0x1700),
      _next_pat_version(0),
      _next_pmt_version(0),
      _ts_packets_until_psip_sync(0),
      // Fake video
      _audio_header_pos(0),       _video_header_pos(0),
      _audio_pid(0),             
      _video_time_stamp(0),       _synch_time_stamp(0),
      _audio_time_stamp(0),       _new_time_stamp(0),
      _ts_change_count(0),        _video_stream_fd(-1),
      _frames_per_sec(0.0f),      _dummy_output_video_pid(0),
      _stop_dummy(true),          _dummy_stopped(true),
      _video_cc(0xff),
      // Statistics
      _continuity_error_count(0), _stream_overflow_count(0),
      _bad_packet_count(0)
{
    _drb = new DeviceReadBuffer(this);
    _buffer_size = (1024*1024 / TSPacket::SIZE) * TSPacket::SIZE;

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

    if (_drb)
    {
        delete _drb;
        _drb = NULL;
    }

    SetOutputPAT(NULL);
    SetOutputPMT(NULL);

    if (_input_pmt)
    {
        delete _input_pmt;
        _input_pmt = NULL;
    }
}

void DVBRecorder::SetOption(const QString &name, int value)
{
    if (name == "cardnum")
    {
        _card_number_option = value;
        videodevice = QString::number(value);
    }
    else
        DTVRecorder::SetOption(name, value);
}

void DVBRecorder::SetOptionsFromProfile(RecordingProfile *profile, 
                                        const QString &videodev,
                                        const QString&, const QString&)
{
    SetOption("cardnum", videodev.toInt());
    DTVRecorder::SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetStrOption(profile,  "recordingtype");
}

void DVBRecorder::HandlePAT(const ProgramAssociationTable *_pat)
{
    if (!_pat)
    {
        VERBOSE(VB_RECORD, LOC + "SetPAT(NULL)");
        return;
    }

    QMutexLocker change_lock(&_pid_lock);

    int progNum = _stream_data->DesiredProgram();
    _pmt_pid = _pat->FindPID(progNum);

    VERBOSE(VB_RECORD, LOC + QString("SetPAT(%1 on 0x%2)")
            .arg(progNum).arg(_pmt_pid,0,16));

    _input_pat = new ProgramAssociationTable(*_pat);

    /* Rev the PAT version since PMT PID is changing */
    _next_pat_version = (_next_pat_version + 1) & 0x1f;
    _ts_packets_until_psip_sync = 0;

    _reset_pid_filters = true;
}

void DVBRecorder::HandlePMT(uint progNum, const ProgramMapTable *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    if ((int)progNum == _stream_data->DesiredProgram())
    {
        VERBOSE(VB_RECORD, LOC + "SetPMT("<<progNum<<")");
        _input_pmt = new ProgramMapTable(*_pmt);
        dvbchannel->SetPMT(_input_pmt);

        /* Rev the PMT version since PIDs are changing */
        _next_pmt_version = (_next_pmt_version + 1) & 0x1f;
        _ts_packets_until_psip_sync = 0;

        _reset_pid_filters = true;
    }
}

bool DVBRecorder::Open(void)
{
    if (IsOpen())
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Card already open");
        return true;
    }

    QString dvbdev = CardUtil::GetDeviceName(DVB_DEV_DVR, _card_number_option);
    _stream_fd = open(dvbdev.ascii(), O_RDONLY | O_NONBLOCK);
    if (!IsOpen())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to open DVB device" + ENO);
        return false;
    }

    if (_drb)
        _drb->Reset(videodevice, _stream_fd);

    VERBOSE(VB_RECORD, LOC + QString("Card opened successfully fd(%1)")
            .arg(_stream_fd));

    return true;
}

void DVBRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- begin");

    if (IsOpen())
    {

    if (_drb)
        _drb->Reset(videodevice, -1);

        StopDummyVideo();
        CloseFilters();
        close(_stream_fd);
        _stream_fd = -1;
    }

    VERBOSE(VB_RECORD, LOC + "Close() fd("<<_stream_fd<<") -- end");
}

void DVBRecorder::SetStreamData(MPEGStreamData *data)
{
    if (data == _stream_data)
        return;

    MPEGStreamData *old_data = _stream_data;
    _stream_data = data;
    if (old_data)
        delete old_data;

    if (data)
    {
        data->AddMPEGListener(this);

        ATSCStreamData *atsc = dynamic_cast<ATSCStreamData*>(data);

        if (atsc && atsc->DesiredMinorChannel())
            atsc->SetDesiredChannel(atsc->DesiredMajorChannel(),
                                    atsc->DesiredMinorChannel());
        else if (data->DesiredProgram() >= 0)
            data->SetDesiredProgram(data->DesiredProgram());
    }
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
    _eit_pids.clear();
}

int DVBRecorder::OpenFilterFd(uint pid, int pes_type, uint stream_type)
{
    // bits per millisecond
    uint bpms = (StreamID::IsVideo(stream_type)) ? 19200 : 500;
    // msec of buffering we want
    uint msec_of_buffering = max(POLL_WARNING_TIMEOUT + 50, 1500);
    // actual size of buffer we need
    uint pid_buffer_size = ((bpms*msec_of_buffering + 7) / 8);
    // rounded up to the nearest page
    pid_buffer_size = ((pid_buffer_size + 4095) / 4096) * 4096;

    VERBOSE(VB_RECORD, LOC + QString("Adding pid 0x%1 size(%2)")
            .arg((int)pid,0,16).arg(pid_buffer_size));

    // Open the demux device
    QString dvbdev = CardUtil::GetDeviceName(
        DVB_DEV_DEMUX, _card_number_option);

    int fd_tmp = open(dvbdev.ascii(), O_RDWR);
    if (fd_tmp < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not open demux device." + ENO);
        return -1;
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
    /*
    VERBOSE(VB_RECORD, LOC + "Set demux buffer size for " +
            QString("pid 0x%1 to %2,\n\t\t\twhich gives us a %3 msec buffer.")
            .arg(pid,0,16).arg(sz).arg(usecs/1000));
    */

    // Set the filter type
    struct dmx_pes_filter_params params;
    bzero(&params, sizeof(params));
    params.input    = DMX_IN_FRONTEND;
    params.output   = DMX_OUT_TS_TAP;
    params.flags    = DMX_IMMEDIATE_START;
    params.pid      = pid;
    params.pes_type = (dmx_pes_type_t) pes_type;
    if (ioctl(fd_tmp, DMX_SET_PES_FILTER, &params) < 0)
    {
        close(fd_tmp);

        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set demux filter." + ENO);
        return -1;
    }

    return fd_tmp;
}

void DVBRecorder::OpenFilter(uint pid, int pes_type, uint stream_type)
{
    PIDInfo *info   = NULL;
    int      fd_tmp = -1;

    QMutexLocker change_lock(&_pid_lock);
    PIDInfoMap::iterator it = _pid_infos.find(pid);
    if (it != _pid_infos.end())
    {
        info = *it;
        if (info->pesType == pes_type)
            fd_tmp = info->filter_fd;
        else
            info->Close();
    }

    if (fd_tmp < 0)
        fd_tmp = OpenFilterFd(pid, pes_type, stream_type);

    if (fd_tmp < 0)
    {
        if (info)
        {
            delete *it;
            _pid_infos.erase(it);
        }
        return;
    }

    if (!info)
        info = new PIDInfo();

    // Add the file descriptor to the filter list
    info->filter_fd  = fd_tmp;
    info->streamType = stream_type;
    info->pesType    = pes_type;

    // Add the new info to the map
    _pid_infos[pid] = info;
}

bool DVBRecorder::AdjustFilters(void)
{
    QMutexLocker change_lock(&_pid_lock);

    if (!_input_pat || !_input_pmt)
        return false;

    uint_vec_t add_pid, add_stream_type;

    add_pid.push_back(PAT_PID);
    add_stream_type.push_back(StreamID::PrivSec);
    _stream_data->AddListeningPID(PAT_PID);

    for (uint i = 0; i < _input_pat->ProgramCount(); i++)
    {
        add_pid.push_back(_input_pat->ProgramPID(i));
        add_stream_type.push_back(StreamID::PrivSec);
        _stream_data->AddListeningPID(_input_pat->ProgramPID(i));
    }

    // Record the streams in the PMT...
    bool need_pcr_pid = true;
    for (uint i = 0; i < _input_pmt->StreamCount(); i++)
    {
        add_pid.push_back(_input_pmt->StreamPID(i));
        add_stream_type.push_back(_input_pmt->StreamType(i));
        need_pcr_pid &= (_input_pmt->StreamPID(i) != _input_pmt->PCRPID());
        _stream_data->AddWritingPID(_input_pmt->StreamPID(i));
    }

    if (need_pcr_pid && (_input_pmt->PCRPID()))
    {
        add_pid.push_back(_input_pmt->PCRPID());
        add_stream_type.push_back(StreamID::PrivData);
        _stream_data->AddWritingPID(_input_pmt->PCRPID());
    }

    // Adjust for EIT
    AdjustEITPIDs();
    for (uint i = 0; i < _eit_pids.size(); i++)
    {
        add_pid.push_back(_eit_pids[i]);
        add_stream_type.push_back(StreamID::PrivSec);
        _stream_data->AddListeningPID(_eit_pids[i]);
    }

    // Delete filters for pids we no longer wish to monitor
    PIDInfoMap::iterator it   = _pid_infos.begin();
    PIDInfoMap::iterator next = it;
    for (; it != _pid_infos.end(); it = next)
    {
        next = it; next++;

        if (find(add_pid.begin(), add_pid.end(), it.key()) == add_pid.end())
        {
            _stream_data->RemoveListeningPID(it.key());
            _stream_data->RemoveWritingPID(it.key());
            (*it)->Close();
            delete *it;
            _pid_infos.erase(it);
        }
    }

    // Add or adjust filters for pids we wish to monitor
    for (uint i = 0; i < add_pid.size(); i++)
        OpenFilter(add_pid[i], DMX_PES_OTHER, add_stream_type[i]);


    // [Re]start dummy video
    StopDummyVideo();
    StartDummyVideo();

    // Report if there are no PIDs..
    if (_pid_infos.empty())
    {        
        VERBOSE(VB_GENERAL, LOC_WARN +
                "Recording will not commence until a PID is set.");
        return false;
    }

    return true;
}

/** \fn DVBRecorder::AdjustEITPIDs(void)
 *  \brief Adjusts EIT PID monitoring to monitor the right number of EIT PIDs.
 */
void DVBRecorder::AdjustEITPIDs(void)
{
    bool changes = false;
    uint_vec_t add, del;

    QMutexLocker change_lock(&_pid_lock);

    if (GetStreamData()->HasEITPIDChanges(_eit_pids))
        changes = GetStreamData()->GetEITPIDChanges(_eit_pids, add, del);

    if (changes)
    {
        for (uint i = 0; i < del.size(); i++)
        {
            uint_vec_t::iterator it;
            it = find(_eit_pids.begin(), _eit_pids.end(), del[i]);
            if (it != _eit_pids.end())
                _eit_pids.erase(it);
        }

        for (uint i = 0; i < add.size(); i++)
        {
            _eit_pids.push_back(add[i]);
        }
    }
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

    //SetOutputPAT(NULL);
    //SetOutputPMT(NULL);
    _ts_packets_until_psip_sync = 0;

    bool ok = _drb->Setup(videodevice, _stream_fd);
    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to allocate DRB buffer");
        Close();
        _error = true;
        return;
    }
    _drb->Start();

    while (_request_recording && !_error)
    {
        if (PauseAndWait())
            continue;

        if (!IsOpen())
        {
            usleep(5000);
            continue;
        }

        if (!_input_pmt)
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (_stream_data)
        {
            QMutexLocker read_lock(&_pid_lock);
            _reset_pid_filters |= _stream_data->HasEITPIDChanges(_eit_pids);
        }

        if (_reset_pid_filters)
        {
            _reset_pid_filters = false;
            VERBOSE(VB_RECORD, LOC + "Resetting Demux Filters");
            if (AdjustFilters())
            {
                CreatePAT();
                CreatePMT();
            }
        }

        ssize_t len = _drb->Read(_buffer, _buffer_size);
        if (len > 0)
            ProcessDataTS(_buffer, len);

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
    }

    if (_drb && _drb->IsRunning())
        _drb->Stop();

    Close();

    FinishRecording();

    _recording = false;
}

void DVBRecorder::WritePATPMT(void)
{
    if (_ts_packets_until_psip_sync <= 0)
    {
        QMutexLocker read_lock(&_pid_lock);

        VERBOSE(VB_IMPORTANT, "Writing PAT & PMT @"
                <<ringBuffer->GetWritePosition()<<" + "
                <<(_payload_buffer.size()*188));

        uint next_cc;
        if (_pat && _pmt && ringBuffer)
        {
            next_cc = (_pat->tsheader()->ContinuityCounter()+1)&0xf;
            _pat->tsheader()->SetContinuityCounter(next_cc);
            BufferedWrite(*(reinterpret_cast<TSPacket*>(_pat->tsheader())));

            unsigned char buf[8 * 1024];
            uint cc = _pmt->tsheader()->ContinuityCounter();
            uint size = _pmt->WriteAsTSPackets(buf, cc);
            _pmt->tsheader()->SetContinuityCounter(cc);

            for (uint i = 0; i < size ; i += TSPacket::SIZE)
                BufferedWrite(*(reinterpret_cast<TSPacket*>(&buf[i])));

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

void DVBRecorder::SetOutputPAT(ProgramAssociationTable *new_pat)
{
    QMutexLocker change_lock(&_pid_lock);

    ProgramAssociationTable *old_pat = _pat;
    _pat = new_pat;
    if (old_pat)
        delete old_pat;

    if (_pat)
        VERBOSE(VB_RECORD, LOC + "SetOutputPAT()\n" + _pat->toString());
    else
        VERBOSE(VB_RECORD, LOC + "SetOutputPAT(NULL)");
}

void DVBRecorder::SetOutputPMT(ProgramMapTable *new_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    ProgramMapTable *old_pmt = _pmt;
    _pmt = new_pmt;
    if (old_pmt)
        delete old_pmt;

    if (_pmt)
        VERBOSE(VB_RECORD, LOC + "SetOutputPMT()\n" + _pmt->toString());
    else
        VERBOSE(VB_RECORD, LOC + "SetOutputPMT(NULL)");
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
    pid.push_back(_pmt_pid);

    ProgramAssociationTable *pat =
        ProgramAssociationTable::Create(tsid, cc, pnum, pid);

    pat->SetVersionNumber(_next_pat_version);
    pat->SetCRC(pat->CalcCRC());

    SetOutputPAT(pat);
}

void DVBRecorder::CreatePMT(void)
{
    QMutexLocker read_lock(&_pid_lock);

    VERBOSE(VB_RECORD, LOC + "CreatePMT(void) INPUT\n\n" +
            _input_pmt->toString());

    // Figure out what goes into the PMT
    uint programNumber = 1; // MPEG Program Number
    
    desc_list_t gdesc = MPEGDescriptor::ParseAndExclude(
        _input_pmt->ProgramInfo(), _input_pmt->ProgramInfoLength(),
        DescriptorID::conditional_access);

    vector<uint> pids;
    vector<uint> types;
    vector<desc_list_t> pdesc;
    bool addVideoPid = true;

    for (uint i = 0; i < _input_pmt->StreamCount(); i++)
    {
        desc_list_t desc = MPEGDescriptor::ParseAndExclude(
            _input_pmt->StreamInfo(i), _input_pmt->StreamInfoLength(i),
            DescriptorID::conditional_access);
        uint type = StreamID::Normalize(_input_pmt->StreamType(i), desc);

        // Filter out streams not used for basic television
        if (_recording_type == "tv" &&
            !StreamID::IsAudio(type) &&
            !StreamID::IsVideo(type) &&
            !MPEGDescriptor::Find(desc, DescriptorID::teletext) &&
            !MPEGDescriptor::Find(desc, DescriptorID::subtitling))
        {
            continue;
        }

        pdesc.push_back(desc);
        uint pid = _input_pmt->StreamPID(i);
        pids.push_back(pid);
        types.push_back(type);
        if (pid == _dummy_output_video_pid)
            addVideoPid = false;
    }

    if (addVideoPid)
    {
        desc_list_t dummy;
        pdesc.push_back(dummy);
        pids.push_back(_dummy_output_video_pid);
        types.push_back(StreamID::MPEG2Video);
    }

    // Create the PMT
    ProgramMapTable *pmt = ProgramMapTable::Create(
        programNumber, _pmt_pid, _input_pmt->PCRPID(),
        _next_pmt_version, gdesc,
        pids, types, pdesc);

    // Set the continuity counter...
    if (_pmt)
    {
        uint cc = (_pmt->tsheader()->ContinuityCounter()+1) & 0xf;
        pmt->tsheader()->SetContinuityCounter(cc);
    }

    VERBOSE(VB_RECORD, LOC + "CreatePMT(void) OUTPUT\n\n" +
            pmt->toString());

    SetOutputPMT(pmt);
}

void DVBRecorder::StopRecording(void)
{
    _request_recording = false;
    if (_drb && _drb->IsRunning())
        _drb->Stop();

    while (_recording)
        usleep(2000);
}

void DVBRecorder::ReaderPaused(int /*fd*/)
{
    pauseWait.wakeAll();
    if (tvrec)
        tvrec->RecorderPaused();
}

bool DVBRecorder::PauseAndWait(int timeout)
{
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

    if (!_pat || !_pmt)
        return true;

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

    if (StreamID::IsVideo(info->streamType))
        _stop_dummy = true;

    if (StreamID::IsAudio(info->streamType))
        GetTimeStamp(tspacket);

    ProcessTSPacket2(tspacket);
    return true;
}

void DVBRecorder::ProcessTSPacket2(const TSPacket& tspacket)
{
    const uint pid = tspacket.PID();

    QMutexLocker locker(&_pid_lock);

    PIDInfo *info = _pid_infos[pid];
    if (!info)
        info = _pid_infos[pid] = new PIDInfo();

    // Check for keyframes and count frames
    if (StreamID::IsVideo(info->streamType))
        _buffer_packets = !FindKeyframes(&tspacket);

    // Sync recording start to first keyframe
    if (_wait_for_keyframe_option && _first_keyframe<0)
        return;

    // Write PAT & PMT tables occasionally
    WritePATPMT();

    if (GetStreamData()->IsListeningPID(pid))
        GetStreamData()->HandleTSTables(&tspacket);
    else if (GetStreamData()->IsWritingPID(pid))
    {
        // Sync streams to the first Payload Unit Start Indicator
        // _after_ first keyframe iff _wait_for_keyframe_option is true
        if (!info->payloadStartSeen)
        {
            if (!tspacket.PayloadStart())
                return; // not payload start - drop packet

            VERBOSE(VB_RECORD,
                    QString("PID 0x%1 Found Payload Start").arg(pid,0,16));
            info->payloadStartSeen = true;
        }

        BufferedWrite(tspacket);
    }
    else
        VERBOSE(VB_IMPORTANT, LOC + QString("Unknown PID 0x%1")
                .arg(pid,0,16));
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
                _audio_time_stamp = (k >> 1) & 0x7;
                break;
            case 10:
                _audio_header_pos++;
                _audio_time_stamp = (_audio_time_stamp << 8) | k;
                break;
            case 11:
                _audio_header_pos++;
                _audio_time_stamp = (_audio_time_stamp << 7) |
                    ((k >> 1) & 0x7f);
                break;
            case 12:
                _audio_header_pos++;
                _audio_time_stamp = (_audio_time_stamp << 8) | k;
                break;
            case 13:
                _audio_time_stamp = (_audio_time_stamp << 7) |
                    ((k >> 1) & 0x7f);
                _audio_header_pos = 0;
                // We've found a time-stamp.
                // Generally the time stamps will increase at a steady rate
                // but they may jump or wrap round.  Because of errors in
                // the stream we may get odd values out of sequence.
                {
                    if (_synch_time_stamp != 0 &&
                        (_audio_time_stamp >
                         _synch_time_stamp + TS_TICKS_PER_SEC * 5 ||
                         _audio_time_stamp + TS_TICKS_PER_SEC <
                         _synch_time_stamp))
                    {
                        if (_ts_change_count != 0 &&
                            _audio_time_stamp > _new_time_stamp &&
                            _audio_time_stamp <=
                            _new_time_stamp + TS_TICKS_PER_SEC * 10)
                        {
                            _ts_change_count++;
                            if (_ts_change_count == 4)
                            {
                                QMutexLocker lock(&_ts_lock);
                                // We've seen 4 stamps in the new sequence.
                                VERBOSE(VB_RECORD, LOC +
                                        QString("Time stamp change: "
                                                "was %1 now %2")
                                        .arg(_synch_time_stamp)
                                        .arg(_new_time_stamp));
                                _synch_time_stamp = _new_time_stamp;
                                _ts_change_count = 0;
                            }
                        }
                        else
                        {
                            _new_time_stamp = _audio_time_stamp;
                            _ts_change_count = 1;
                        }
                    }
                    else
                    {
                        QMutexLocker lock(&_ts_lock);
                        // Within the sequence or this is the first time stamp.
                        _synch_time_stamp = _audio_time_stamp;
                        _ts_change_count = 0;
                    }
                }
        }
    }

    // If we're part way through a packet only continue if it's the same pid
    if (_audio_header_pos != 0)
        _audio_pid = pid;
}

void *DVBRecorder::run_dummy_video(void *param)
{
    DVBRecorder *dvbrec = (DVBRecorder*) param;
    dvbrec->RunDummyVideo();
    dvbrec->_dummy_stopped = true;
    dvbrec->_wait_stop.wakeAll();
    return NULL;
}

void DVBRecorder::StartDummyVideo(void)
{
    QMutexLocker change_lock(&_pid_lock);

    _dummy_output_video_pid = 0;

    if (_video_stream_fd >= 0)
    {
        close(_video_stream_fd);
        _video_stream_fd = -1;
    }

    vector<uint> video_pids;
    uint video_cnt = _input_pmt->FindPIDs(StreamID::AnyVideo, video_pids);

    if (!video_cnt)
    {
        VERBOSE(VB_RECORD, LOC + "Missing video - adding dummy to PMT");
        // If there is no video in the PMT we need to add it here.
        PIDInfo *info = new PIDInfo();
        info->streamType = StreamID::MPEG2Video;

        _dummy_output_video_pid = _input_pmt->FindUnusedPID(DUMMY_VIDEO_PID);
        _pid_infos[_dummy_output_video_pid] = info;
    }
    else
        _dummy_output_video_pid = video_pids[0];

    _video_header_pos = 0;
    _audio_header_pos = 0;
    _audio_pid = 0;
    _video_time_stamp = 0;
    _synch_time_stamp = 0;
    _audio_time_stamp = 0;
    _video_cc = 0;
    _ts_change_count = 0;

    // Start the dummy video thread.
    _stop_dummy = false;
    int ret = pthread_create(&_video_thread, NULL, run_dummy_video, this);
    _dummy_stopped = (0 != ret);
}

void DVBRecorder::RunDummyVideo(void)
{
    QString p = gContext->GetThemesParentDir();
    QString path[] =
    {
        p + gContext->GetSetting("Theme", "G.A.N.T.") + "/",
        p + "default/",
    };

    uint width = 720;
    uint height = 576;
    _frames_per_sec = 25;

    QString filename = QString("dummy%1x%2p%3.ts")
        .arg(width).arg(height).arg(_frames_per_sec, 0, 'f', 2);

    _video_stream_fd = open(path[0] + filename.ascii(), O_RDONLY);

    if (_video_stream_fd < 0)
        _video_stream_fd = open(path[1] + filename.ascii(), O_RDONLY);

    if (_video_stream_fd < 0)
        return;

    unsigned long frameTime = (unsigned long)(1000 / _frames_per_sec);
    int64_t last_synch = 0;
    _wait_time.wait(frameTime * 4); // Initial wait

    while (! _stop_dummy)
    {
        _wait_time.wait(frameTime);
        _ts_lock.lock();
        int64_t synch_ts = _synch_time_stamp;
        _ts_lock.unlock();
        if (synch_ts != last_synch) // The audio time stamp has changed.
        {
            // If the time stamp has jumped just catch up.
            if (synch_ts > last_synch + TS_TICKS_PER_SEC*5 ||
                synch_ts + TS_TICKS_PER_SEC < last_synch)
                _video_time_stamp = synch_ts;
            else
            {
                // Catch up if behind, otherwise skip a time.
                while (_video_time_stamp < synch_ts)
                    CreateVideoFrame();
            }
            last_synch = synch_ts;

        }
        else
        {
            CreateVideoFrame(); // Just generate one frame
        }
    }

    close(_video_stream_fd);
    _video_stream_fd = -1;
}

// Stop the dummy video thread
void DVBRecorder::StopDummyVideo(void)
{
    while (!_dummy_stopped)
    {
        _stop_dummy = true;
        _wait_time.wakeAll();
        _wait_stop.wait(1000);
        pthread_join(_video_thread, NULL);
    }
}

void DVBRecorder::CreateVideoFrame(void)
{
    bool foundStart = false;

    while (!foundStart)
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

        pkt->SetPID(_dummy_output_video_pid);

        // Find the time-stamp field and overwrite it.
        if (pkt->PayloadStart())
        {
            _video_header_pos = 0;
        }

        for (uint i = pkt->AFCOffset(); i < TSPacket::SIZE; i++)
        {
            const unsigned char k = buffer[i];
            switch (_video_header_pos)
            {
                case 0:
                    _video_header_pos = (k == 0x00) ? 1 : 0;
                    break;
                case 1:
                    _video_header_pos = (k == 0x00) ? 2 : 0;
                    break;
                case 2:
                    _video_header_pos =
                        (k == 0x00) ? 2 : ((k == 0x01) ? 3 : 0);
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
                        _video_time_stamp += (int)((double) TS_TICKS_PER_SEC /
                                                   _frames_per_sec);
                        foundStart = true;
                    }
                    else
                    {
                        _video_header_pos = 0;
                    }
                    break;
                case 4: case 5: case 6: case 8:
                    _video_header_pos++;
                    break;
                case 7:
                    // Is there a time-stamp?
                    _video_header_pos = (k & 0x80) ? 8 : 0;
                    break;
                case 9:
                    buffer[i]  = (k & 0xf0);
                    buffer[i] |= ((_video_time_stamp >> 29) & 0x0e);
                    buffer[i] |= 1;
                    _video_header_pos++;
                    break;
                case 10:
                    buffer[i] = (_video_time_stamp >> 22) & 0xff;
                    _video_header_pos++;
                    break;
                case 11:
                    buffer[i] = ((_video_time_stamp >> 14) & 0xfe) | 1;
                    _video_header_pos++;
                    break;
                case 12:
                    buffer[i] = (_video_time_stamp >> 7) & 0xff; // 7..14
                    _video_header_pos++;
                    break;
                case 13:
                    buffer[i] = ((_video_time_stamp << 1) & 0xfe) | 1; // 0..6
                    _video_header_pos = 0;
            }
        }
        pkt->SetContinuityCounter(_video_cc);
        _video_cc = (_video_cc + 1) & 0xf;

        // Pass the packet to the ring-buffer
        ProcessTSPacket2(*pkt);
    }
}
