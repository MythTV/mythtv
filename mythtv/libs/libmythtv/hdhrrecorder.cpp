/**
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
using namespace std;

// MythTV includes
#include "RingBuffer.h"
#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "atsctables.h"
#include "atscstreamdata.h"

#define LOC QString("HDHRRec(%1): ").arg(tvrec->GetCaptureCardNum())
#define LOC_ERR QString("HDHRRec(%1), Error: ") \
                    .arg(tvrec->GetCaptureCardNum())

HDHRRecorder::HDHRRecorder(TVRec *rec, HDHRChannel *channel)
    : DTVRecorder(rec, "HDHRRecorder"),
      _channel(channel), _video_socket(NULL),
      _atsc_stream_data(new ATSCStreamData(-1, 1))
{
    connect(_atsc_stream_data,
            SIGNAL(UpdatePATSingleProgram(ProgramAssociationTable*)),
            this, SLOT(WritePAT(ProgramAssociationTable*)));
    connect(_atsc_stream_data,
            SIGNAL(UpdatePMTSingleProgram(ProgramMapTable*)),
            this, SLOT(WritePMT(ProgramMapTable*)));
    connect(_atsc_stream_data, SIGNAL(UpdateMGT(const MasterGuideTable*)),
            this, SLOT(ProcessMGT(const MasterGuideTable*)));
    connect(_atsc_stream_data,
            SIGNAL(UpdateVCT(uint, const VirtualChannelTable*)),
            this, SLOT(ProcessVCT(uint, const VirtualChannelTable*)));
}

HDHRRecorder::~HDHRRecorder()
{
    TeardownAll();
}

void HDHRRecorder::TeardownAll(void)
{
    StopRecording();
    Close();
    if (_atsc_stream_data)
    {
        _atsc_stream_data->deleteLater();
        _atsc_stream_data = NULL;
    }
}

void HDHRRecorder::deleteLater(void)
{
    TeardownAll();
    DTVRecorder::deleteLater();
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
    _video_socket = hdhomerun_video_create();
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

void HDHRRecorder::SetStreamData(ATSCStreamData *stream_data)
{
    if (stream_data == _atsc_stream_data)
        return;

    ATSCStreamData *old_data = _atsc_stream_data;
    _atsc_stream_data = stream_data;
    if (old_data)
        old_data->deleteLater();
}

void HDHRRecorder::WritePAT(ProgramAssociationTable *pat)
{
    //VERBOSE(VB_IMPORTANT, "ProcessPAT()");
    if (!pat)
        return;

    int next = (pat->tsheader()->ContinuityCounter()+1)&0xf;
    pat->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<TSPacket*>(pat->tsheader())));
}

void HDHRRecorder::WritePMT(ProgramMapTable* pmt)
{
    //VERBOSE(VB_IMPORTANT, "ProcessPMT()");
    if (!pmt)
        return;

    int next = (pmt->tsheader()->ContinuityCounter()+1)&0xf;
    pmt->tsheader()->SetContinuityCounter(next);
    BufferedWrite(*(reinterpret_cast<TSPacket*>(pmt->tsheader())));
}

/** \fn HDHRRecorder::ProcessMGT(const MasterGuideTable*)
 *  \brief Processes Master Guide Table, by enabling the 
 *         scanning of all PIDs listed.
 */
void HDHRRecorder::ProcessMGT(const MasterGuideTable *mgt)
{
    //VERBOSE(VB_IMPORTANT, "ProcessMGT()");
    for (unsigned int i=0; i<mgt->TableCount(); i++)
        GetStreamData()->AddListeningPID(mgt->TablePID(i));
}

/** \fn HDHRRecorder::ProcessVCT(uint, const VirtualChannelTable*)
 *  \brief Processes Virtual Channel Tables by finding the program
 *         number to use.
 *  \bug Assumes there is only one VCT, may break on Cable.
 */
void HDHRRecorder::ProcessVCT(uint /*tsid*/,
                              const VirtualChannelTable *vct)
{
    //VERBOSE(VB_IMPORTANT, "ProcessVCT()");
    if (vct->ChannelCount() < 1)
    {
        VERBOSE(VB_IMPORTANT,
                "HDHRRecorder::ProcessVCT: table has no channels");
        return;
    }

    bool found = false;    
    VERBOSE(VB_RECORD, QString("Desired channel %1_%2")
            .arg(GetStreamData()->DesiredMajorChannel())
            .arg(GetStreamData()->DesiredMinorChannel()));
    for (uint i=0; i<vct->ChannelCount(); i++)
    {
        if ((GetStreamData()->DesiredMajorChannel() == -1 ||
             vct->MajorChannel(i)==
             (uint)GetStreamData()->DesiredMajorChannel()) &&
            vct->MinorChannel(i)==
            (uint)GetStreamData()->DesiredMinorChannel())
        {
            if (vct->ProgramNumber(i) != 
                (uint)GetStreamData()->DesiredProgram())
            {
                VERBOSE(VB_RECORD, 
                        QString("Resetting desired program from %1"
                                " to %2")
                        .arg(GetStreamData()->DesiredProgram())
                        .arg(vct->ProgramNumber(i)));
                GetStreamData()->MPEGStreamData::Reset(vct->ProgramNumber(i));
            }
            found = true;
        }
    }
    if (!found)
    {
        VERBOSE(VB_IMPORTANT, 
                QString("Desired channel %1_%2 not found;"
                        " using %3_%4 instead.")
                .arg(GetStreamData()->DesiredMajorChannel())
                .arg(GetStreamData()->DesiredMinorChannel())
                .arg(vct->MajorChannel(0))
                .arg(vct->MinorChannel(0)));
        VERBOSE(VB_IMPORTANT, vct->toString());
        GetStreamData()->MPEGStreamData::Reset(vct->ProgramNumber(0));
    }
}

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
                _buffer_packets = !FindKeyframes(&tspacket);
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

    while (_request_recording && !_error)
    {
        if (PauseAndWait())
            continue;

        struct hdhomerun_video_data_t data;
        int ret = hdhomerun_video_recv(_video_socket, &data, 100);

        if (ret > 0)
            ProcessTSData(data.buffer, data.length);
        else if (ret < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Recv error" + ENO);
            if (_video_socket)
                break;
        }
        else
            usleep(2500);
    }

    VERBOSE(VB_RECORD, LOC + "StartRecording -- ending...");

    _channel->DeviceClearTarget();
    Close();

    FinishRecording();
    _recording = false;

    VERBOSE(VB_RECORD, LOC + "StartRecording -- end");
}
