/*  -*- Mode: c++ -*-
 *   Class ASIRecorder
 *
 *   Copyright (C) Daniel Kristjansson 2010
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

// Qt includes
#include <QString>

// MythTV includes
#include "asistreamhandler.h"
#include "asirecorder.h"
#include "asichannel.h"
#include "ringbuffer.h"
#include "tv_rec.h"

#define LOC QString("ASIRec(%1): ").arg(tvrec->GetCaptureCardNum())

ASIRecorder::ASIRecorder(TVRec *rec, ASIChannel *channel) :
    DTVRecorder(rec), m_channel(channel), m_stream_handler(NULL),
    m_record_mpts(false)
{
    SetStreamData(new MPEGStreamData(-1,false));
    if (channel->GetProgramNumber() < 0 || !channel->GetMinorChannel())
        _stream_data->SetListeningDisabled(true);
}

void ASIRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                        const QString &videodev,
                                        const QString &audiodev,
                                        const QString &vbidev)
{
    // We don't want to call DTVRecorder::SetOptionsFromProfile() since
    // we do not have a "recordingtype" in our profile.
    DTVRecorder::SetOption("videodevice", videodev);
    DTVRecorder::SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetIntOption(profile, "recordmpts");
}

/** \fn ASIRecorder::SetOption(const QString&,int)
 *  \brief handles the "recordmpts" option.
 */
void ASIRecorder::SetOption(const QString &name, int value)
{
    if (name == "recordmpts")
        m_record_mpts = (value == 1);
    else
        DTVRecorder::SetOption(name, value);
}

void ASIRecorder::run(void)
{
    if (!Open())
    {
        _error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        return;
    }

    if (!_stream_data)
    {
        _error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        Close();
        return;        
    }

    {
        QMutexLocker locker(&pauseLock);
        request_recording = true;
        recording = true;
        recordingWait.wakeAll();
    }

    if (m_channel->HasGeneratedPAT())
    {
        const ProgramAssociationTable *pat = m_channel->GetGeneratedPAT();
        const ProgramMapTable         *pmt = m_channel->GetGeneratedPMT();
        _stream_data->ResetMPEG(pat->ProgramNumber(0));
        _stream_data->HandleTables(MPEG_PAT_PID, *pat);
        _stream_data->HandleTables(pat->ProgramPID(0), *pmt);
    }

    // Listen for time table on DVB standard streams
    if (m_channel && (m_channel->GetSIStandard() == "dvb"))
        _stream_data->AddListeningPID(DVB_TDT_PID);

    // Make sure the first things in the file are a PAT & PMT
    bool tmp = _wait_for_keyframe_option;
    _wait_for_keyframe_option = false;
    HandleSingleProgramPAT(_stream_data->PATSingleProgram());
    HandleSingleProgramPMT(_stream_data->PMTSingleProgram());
    _wait_for_keyframe_option = tmp;

    _stream_data->AddAVListener(this);
    _stream_data->AddWritingListener(this);
    m_stream_handler->AddListener(
        _stream_data, false, true,
        (m_record_mpts) ? ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&pauseLock);
            if (!request_recording || request_pause)
                continue;
            unpauseWait.wait(&pauseLock, 100);
        }

        if (!_input_pmt)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_stream_handler->IsRunning())
        {
            _error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + _error);
        }
    }

    m_stream_handler->RemoveListener(_stream_data);
    _stream_data->RemoveWritingListener(this);
    _stream_data->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&pauseLock);
    recording = false;
    recordingWait.wakeAll();
}

bool ASIRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    m_stream_handler = ASIStreamHandler::Get(m_channel->GetDevice());

    LOG(VB_RECORD, LOG_INFO, LOC + "Opened successfully");

    return true;
}

bool ASIRecorder::IsOpen(void) const
{
    return m_stream_handler;
}

void ASIRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        ASIStreamHandler::Return(m_stream_handler);

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}
