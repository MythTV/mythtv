/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006 by Silicondust Engineering Ltd, and
 *    Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// C includes
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <fcntl.h>

// C++ includes
#include <iostream>
#include <algorithm>
using namespace std;

// MythTV includes
#include "RingBuffer.h"
#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "atsctables.h"
#include "atscstreamdata.h"
#include "eithelper.h"
#include "tv_rec.h"

#define LOC QString("HDHRRec(%1): ").arg(tvrec->GetCaptureCardNum())
#define LOC_ERR QString("HDHRRec(%1), Error: ") \
                    .arg(tvrec->GetCaptureCardNum())

HDHRRecorder::HDHRRecorder(TVRec *rec, HDHRChannel *channel)
    : DTVRecorder(rec),
      _channel(channel),        _video_socket(NULL),
      _stream_data(NULL),
      _input_pat(NULL),         _input_pmt(NULL),
      _reset_pid_filters(false),_pid_lock(true)
{
}

HDHRRecorder::~HDHRRecorder()
{
    TeardownAll();
}

void HDHRRecorder::TeardownAll(void)
{
    StopRecording();
    Close();
    if (_stream_data)
    {
        delete _stream_data;
        _stream_data = NULL;
    }

    if (_input_pat)
    {
        delete _input_pat;
        _input_pat = NULL;
    }

    if (_input_pmt)
    {
        delete _input_pmt;
        _input_pmt = NULL;
    }
}

void HDHRRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                         const QString &videodev,
                                         const QString &audiodev,
                                         const QString &vbidev)
{
    (void)audiodev;
    (void)vbidev;
    (void)profile;

    SetOption("videodevice", videodev);
    SetOption("tvformat", gContext->GetSetting("TVFormat"));
    SetOption("vbiformat", gContext->GetSetting("VbiFormat"));

    // HACK -- begin
    // This is to make debugging easier.
    SetOption("videodevice", QString::number(tvrec->GetCaptureCardNum()));
    // HACK -- end
}

bool HDHRRecorder::Open(void)
{
    VERBOSE(VB_RECORD, LOC + "Open()");
    if (_video_socket)
    {
        VERBOSE(VB_RECORD, LOC + "Card already open (recorder)");
        return true;
    }

    /* Create TS socket. */
    _video_socket = hdhomerun_video_create(VIDEO_DATA_BUFFER_SIZE_1S, 50);
    if (!_video_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC + "Open() failed to open socket");
        return false;
    }

    /* Success. */
    return true;
}

/** \fn HDHRRecorder::StartData(void)
 *  \brief Configure device to send video.
 */
bool HDHRRecorder::StartData(void)
{
    VERBOSE(VB_RECORD, LOC + "StartData()");
    uint localPort = hdhomerun_video_get_local_port(_video_socket);
    return _channel->DeviceSetTarget(localPort);
}

void HDHRRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close()");
    if (_video_socket)
    {
        hdhomerun_video_destroy(_video_socket);
        _video_socket = NULL;
    }
}

void HDHRRecorder::ProcessTSData(const uint8_t *buffer, int len)
{
    QMutexLocker locker(&_pid_lock);
    const uint8_t *data = buffer;
    const uint8_t *end = buffer + len;

    while (data + 188 <= end)
    {
        if (data[0] != 0x47)
        {
            return;
        }

        const TSPacket *tspacket = reinterpret_cast<const TSPacket*>(data);
        ProcessTSPacket(*tspacket);

        data += 188;
    }
}

void HDHRRecorder::SetStreamData(MPEGStreamData *data)
{
    if (data == _stream_data)
        return;

    MPEGStreamData *old_data = _stream_data;
    _stream_data = data;
    if (old_data)
        delete old_data;

    if (data)
    {
        data->AddMPEGSPListener(this);
        data->AddMPEGListener(this);

        ATSCStreamData *atsc = dynamic_cast<ATSCStreamData*>(data);

        if (atsc && atsc->DesiredMinorChannel())
            atsc->SetDesiredChannel(atsc->DesiredMajorChannel(),
                                    atsc->DesiredMinorChannel());
        else if (data->DesiredProgram() >= 0)
            data->SetDesiredProgram(data->DesiredProgram());
    }
}

ATSCStreamData *HDHRRecorder::GetATSCStreamData(void)
{
    return dynamic_cast<ATSCStreamData*>(_stream_data);
}

void HDHRRecorder::HandlePAT(const ProgramAssociationTable *_pat)
{
    if (!_pat)
    {
        VERBOSE(VB_RECORD, LOC + "SetPAT(NULL)");
        return;
    }

    QMutexLocker change_lock(&_pid_lock);

    int progNum = _stream_data->DesiredProgram();
    uint pmtpid = _pat->FindPID(progNum);

    if (!pmtpid)
    {
        VERBOSE(VB_RECORD, LOC + "SetPAT(): "
                "Ignoring PAT not containing our desired program...");
        return;
    }

    VERBOSE(VB_RECORD, LOC + QString("SetPAT(%1 on 0x%2)")
            .arg(progNum).arg(pmtpid,0,16));

    ProgramAssociationTable *oldpat = _input_pat;
    _input_pat = new ProgramAssociationTable(*_pat);
    delete oldpat;

    _reset_pid_filters = true;
}

void HDHRRecorder::HandlePMT(uint progNum, const ProgramMapTable *_pmt)
{
    QMutexLocker change_lock(&_pid_lock);

    if ((int)progNum == _stream_data->DesiredProgram())
    {
        VERBOSE(VB_RECORD, LOC + "SetPMT("<<progNum<<")");
        ProgramMapTable *oldpmt = _input_pmt;
        _input_pmt = new ProgramMapTable(*_pmt);
        delete oldpmt;

        _reset_pid_filters = true;
    }
}

void HDHRRecorder::HandleSingleProgramPAT(ProgramAssociationTable *pat)
{
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<TSPacket*>(pat->tsheader())));
}

void HDHRRecorder::HandleSingleProgramPMT(ProgramMapTable *pmt)
{
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<TSPacket*>(pmt->tsheader())));
}

/** \fn HDHRRecorder::HandleMGT(const MasterGuideTable*)
 *  \brief Processes Master Guide Table, by enabling the 
 *         scanning of all PIDs listed.
 */
/*
void HDHRRecorder::HandleMGT(const MasterGuideTable *mgt)
{
    VERBOSE(VB_IMPORTANT, LOC + "HandleMGT()");
    for (unsigned int i = 0; i < mgt->TableCount(); i++)
    {
        GetStreamData()->AddListeningPID(mgt->TablePID(i));
        _channel->AddPID(mgt->TablePID(i), false);
    }
    _channel->UpdateFilters();
}
*/

bool HDHRRecorder::ProcessTSPacket(const TSPacket& tspacket)
{
    bool ok = !tspacket.TransportError();
    if (ok && !tspacket.ScramplingControl())
    {
        if (tspacket.HasAdaptationField())
            GetStreamData()->HandleAdaptationFieldControl(&tspacket);
        if (tspacket.HasPayload())
        {
            const unsigned int lpid = tspacket.PID();
            // Pass or reject frames based on PID, and parse info from them
            if (lpid == GetStreamData()->VideoPIDSingleProgram())
            {
                //cerr<<"v";
                _buffer_packets = !FindMPEG2Keyframes(&tspacket);
                BufferedWrite(tspacket);
            }
            else if (GetStreamData()->IsAudioPID(lpid))
            {
                //cerr<<"a";
                BufferedWrite(tspacket);
            }
            else if (GetStreamData()->IsListeningPID(lpid))
            {
                //cerr<<"t";
                GetStreamData()->HandleTSTables(&tspacket);
            }
            else if (GetStreamData()->IsWritingPID(lpid))
                BufferedWrite(tspacket);
        }
    }
    return ok;
}

void HDHRRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StartRecording -- begin");

    /* Create video socket. */
    if (!Open())
    {
        _error = true;
        VERBOSE(VB_RECORD, LOC + "StartRecording -- end 1");
        return;
    }

    _request_recording = true;
    _recording = true;

    if (!StartData())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Starting recording "
                "(set target failed). Aborting.");
        Close();
        _error = true;
        VERBOSE(VB_RECORD, LOC + "StartRecording -- end 2");
        return;
    }

    hdhomerun_video_flush(_video_socket);
    while (_request_recording && !_error)
    {
        if (PauseAndWait())
            continue;

        if (_stream_data)
        {
            QMutexLocker read_lock(&_pid_lock);
            _reset_pid_filters |= _stream_data->HasEITPIDChanges(_eit_pids);
        }

        if (_reset_pid_filters)
        {
            _reset_pid_filters = false;
            VERBOSE(VB_RECORD, LOC + "Resetting Demux Filters");
            AdjustFilters();
        }

        unsigned long data_length;
        unsigned char *data_buffer =
            hdhomerun_video_recv_inplace(_video_socket,
                                         VIDEO_DATA_BUFFER_SIZE_1S / 5,
                                         &data_length);
        if (!data_buffer)
        {
            if (hdhomerun_video_get_state(_video_socket) == 0)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Recv error" + ENO);
                break;
            }
            usleep(5000);
	    continue;
	}

        ProcessTSData(data_buffer, data_length);
    }

    VERBOSE(VB_RECORD, LOC + "StartRecording -- ending...");

    _channel->DeviceClearTarget();
    Close();

    FinishRecording();
    _recording = false;

    VERBOSE(VB_RECORD, LOC + "StartRecording -- end");
}

bool HDHRRecorder::AdjustFilters(void)
{
    QMutexLocker change_lock(&_pid_lock);

    if (!_channel)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "AdjustFilters() no channel");
        return false;
    }

    if (!_input_pat || !_input_pmt)
    {
        VERBOSE(VB_IMPORTANT, LOC + "AdjustFilters() no pmt or no pat");
        return false;
    }

    uint_vec_t add_pid;

    add_pid.push_back(MPEG_PAT_PID);
    _stream_data->AddListeningPID(MPEG_PAT_PID);

    for (uint i = 0; i < _input_pat->ProgramCount(); i++)
    {
        add_pid.push_back(_input_pat->ProgramPID(i));
        _stream_data->AddListeningPID(_input_pat->ProgramPID(i));
    }

    // Record the streams in the PMT...
    bool need_pcr_pid = true;
    for (uint i = 0; i < _input_pmt->StreamCount(); i++)
    {
        add_pid.push_back(_input_pmt->StreamPID(i));
        need_pcr_pid &= (_input_pmt->StreamPID(i) != _input_pmt->PCRPID());
        _stream_data->AddWritingPID(_input_pmt->StreamPID(i));
    }

    if (need_pcr_pid && (_input_pmt->PCRPID()))
    {
        add_pid.push_back(_input_pmt->PCRPID());
        _stream_data->AddWritingPID(_input_pmt->PCRPID());
    }

    // Adjust for EIT
    AdjustEITPIDs();
    for (uint i = 0; i < _eit_pids.size(); i++)
    {
        add_pid.push_back(_eit_pids[i]);
        _stream_data->AddListeningPID(_eit_pids[i]);
    }

    // Delete filters for pids we no longer wish to monitor
    vector<uint>::const_iterator it;
    vector<uint> pids = _channel->GetPIDs();
    for (it = pids.begin(); it != pids.end(); ++it)
    {
        if (find(add_pid.begin(), add_pid.end(), *it) == add_pid.end())
        {
            _stream_data->RemoveListeningPID(*it);
            _stream_data->RemoveWritingPID(*it);
            _channel->DelPID(*it, false);
        }
    }

    for (it = add_pid.begin(); it != add_pid.end(); ++it)
        _channel->AddPID(*it, false);

    _channel->UpdateFilters();

    return add_pid.size();
}

/** \fn HDHRRecorder::AdjustEITPIDs(void)
 *  \brief Adjusts EIT PID monitoring to monitor the right number of EIT PIDs.
 */
bool HDHRRecorder::AdjustEITPIDs(void)
{
    bool changes = false;
    uint_vec_t add, del;

    QMutexLocker change_lock(&_pid_lock);

    if (GetStreamData()->HasEITPIDChanges(_eit_pids))
        changes = GetStreamData()->GetEITPIDChanges(_eit_pids, add, del);

    if (!changes)
        return false;

    for (uint i = 0; i < del.size(); i++)
    {
        uint_vec_t::iterator it;
        it = find(_eit_pids.begin(), _eit_pids.end(), del[i]);
        if (it != _eit_pids.end())
            _eit_pids.erase(it);
    }

    for (uint i = 0; i < add.size(); i++)
        _eit_pids.push_back(add[i]);

    return true;
}

