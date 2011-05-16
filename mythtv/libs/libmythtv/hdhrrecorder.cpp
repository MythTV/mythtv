/** -*- Mode: c++ -*-
 *  HDHRRecorder
 *  Copyright (c) 2006-2009 by Silicondust Engineering Ltd, and
 *    Daniel Thor Kristjansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

// MythTV includes
#include "hdhrstreamhandler.h"
#include "atscstreamdata.h"
#include "hdhrrecorder.h"
#include "hdhrchannel.h"
#include "tv_rec.h"
#include "mythverbose.h"

#define LOC QString("HDHRRec(%1): ").arg(tvrec->GetCaptureCardNum())
#define LOC_WARN QString("HDHRRec(%1), Warning: ") \
                     .arg(tvrec->GetCaptureCardNum())
#define LOC_ERR QString("HDHRRec(%1), Error: ") \
                    .arg(tvrec->GetCaptureCardNum())

HDHRRecorder::HDHRRecorder(TVRec *rec, HDHRChannel *channel)
    : DTVRecorder(rec), _channel(channel), _stream_handler(NULL)
{
}

bool HDHRRecorder::Open(void)
{
    if (IsOpen())
    {
        VERBOSE(VB_GENERAL, LOC_WARN + "Card already open");
        return true;
    }

    memset(_stream_id,  0, sizeof(_stream_id));
    memset(_pid_status, 0, sizeof(_pid_status));
    memset(_continuity_counter, 0xff, sizeof(_continuity_counter));

    _stream_handler = HDHRStreamHandler::Get(_channel->GetDevice());

    VERBOSE(VB_RECORD, LOC + "HDHR opened successfully");

    return true;
}

void HDHRRecorder::Close(void)
{
    VERBOSE(VB_RECORD, LOC + "Close() -- begin");

    if (IsOpen())
        HDHRStreamHandler::Return(_stream_handler);

    VERBOSE(VB_RECORD, LOC + "Close() -- end");
}

void HDHRRecorder::StartRecording(void)
{
    VERBOSE(VB_RECORD, LOC + "StartRecording -- begin");

    /* Create video socket. */
    if (!Open())
    {
        _error = "Failed to open HDHRRecorder device";
        VERBOSE(VB_IMPORTANT, LOC_ERR + _error);
        return;
    }

    _continuity_error_count = 0;

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
    }

    // Make sure the first things in the file are a PAT & PMT
    bool tmp = _wait_for_keyframe_option;
    _wait_for_keyframe_option = false;
    HandleSingleProgramPAT(_stream_data->PATSingleProgram());
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
    _wait_for_keyframe_option = tmp;

    _stream_data->AddAVListener(this);
    _stream_data->AddWritingListener(this);
    _stream_handler->AddListener(_stream_data);

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&pauseLock);
            if (!request_recording || request_pause)
                continue;
            unpauseWait.wait(&pauseLock, 100);
        }

        if (!_input_pmt)
        {
            VERBOSE(VB_GENERAL, LOC_WARN +
                    "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!_stream_handler->IsRunning())
        {
            _error = "Stream handler died unexpectedly."; 
            VERBOSE(VB_IMPORTANT, LOC_ERR + _error);
        }
    }

    VERBOSE(VB_RECORD, LOC + "StartRecording -- ending...");

    _stream_handler->RemoveListener(_stream_data);
    _stream_data->RemoveWritingListener(this);
    _stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();

    VERBOSE(VB_RECORD, LOC + "StartRecording -- end");
}

bool HDHRRecorder::PauseAndWait(int timeout)
{
    QMutexLocker locker(&pauseLock);
    if (request_pause)
    {
        if (!IsPaused(true))
        {
            _stream_handler->RemoveListener(_stream_data);

            paused = true;
            pauseWait.wakeAll();
            if (tvrec)
                tvrec->RecorderPaused();
        }

        unpauseWait.wait(&pauseLock, timeout);
    }

    if (!request_pause && IsPaused(true))
    {
        paused = false;
        _stream_handler->AddListener(_stream_data);
        unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

QString HDHRRecorder::GetSIStandard(void) const
{
    return _channel->GetSIStandard();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
