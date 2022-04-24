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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

// Qt includes
#include <QString>

// MythTV includes
#include "asichannel.h"
#include "asirecorder.h"
#include "asistreamhandler.h"
#include "io/mythmediabuffer.h"
#include "mpeg/tsstreamdata.h"
#include "tv_rec.h"

#define LOC QString("ASIRec[%1](%2): ") \
            .arg(m_tvrec ? m_tvrec->GetInputId() : -1) \
            .arg(m_channel->GetDevice())

ASIRecorder::ASIRecorder(TVRec *rec, ASIChannel *channel) :
    DTVRecorder(rec), m_channel(channel)
{
    if (channel->GetFormat().compare("MPTS") == 0)
    {
        // MPTS only.  Use TSStreamData to write out unfiltered data
        LOG(VB_RECORD, LOG_INFO, LOC + "Using TSStreamData");
        SetStreamData(new TSStreamData(m_tvrec ? m_tvrec->GetInputId() : -1));
        m_recordMptsOnly = true;
        m_recordMpts = false;
    }
    else
    {
        LOG(VB_RECORD, LOG_INFO, LOC + "Using MPEGStreamData");
        SetStreamData(new MPEGStreamData(-1, rec ? rec->GetInputId() : -1,
                                         false));
        if (channel->GetProgramNumber() < 0 || !channel->GetMinorChannel())
            m_streamData->SetListeningDisabled(true);
    }
}

void ASIRecorder::SetOptionsFromProfile(RecordingProfile *profile,
                                        const QString &videodev,
                                        const QString &/*audiodev*/,
                                        const QString &/*vbidev*/)
{
    // We don't want to call DTVRecorder::SetOptionsFromProfile() since
    // we do not have a "recordingtype" in our profile.
    DTVRecorder::SetOption("videodevice", videodev);
    DTVRecorder::SetOption("tvformat", gCoreContext->GetSetting("TVFormat"));
    SetIntOption(profile, "recordmpts");
}

void ASIRecorder::StartNewFile(void)
{
    if (!m_recordMptsOnly)
    {
        if (m_recordMpts)
            m_streamHandler->AddNamedOutputFile(m_ringBuffer->GetFilename());

        // Make sure the first things in the file are a PAT & PMT
        HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
    }
}


void ASIRecorder::run(void)
{
    if (!Open())
    {
        m_error = "Failed to open device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    if (!m_streamData)
    {
        m_error = "MPEGStreamData pointer has not been set";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        Close();
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    if (m_channel->HasGeneratedPAT())
    {
        const ProgramAssociationTable *pat = m_channel->GetGeneratedPAT();
        const ProgramMapTable         *pmt = m_channel->GetGeneratedPMT();
        m_streamData->Reset(pat->ProgramNumber(0));
        m_streamData->HandleTables(PID::MPEG_PAT_PID, *pat);
        m_streamData->HandleTables(pat->ProgramPID(0), *pmt);
    }

    // Listen for time table on DVB standard streams
    if (m_channel && (m_channel->GetSIStandard() == "dvb"))
        m_streamData->AddListeningPID(PID::DVB_TDT_PID);

    StartNewFile();

    m_streamData->AddAVListener(this);
    m_streamData->AddWritingListener(this);
    m_streamHandler->AddListener(
        m_streamData, false, true,
        (m_recordMpts) ? m_ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        {   // sleep 100 milliseconds unless StopRecording() or Unpause()
            // is called, just to avoid running this too often.
            QMutexLocker locker(&m_pauseLock);
            if (!m_requestRecording || m_requestPause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 100);
        }

        if (!m_inputPmt && !m_recordMptsOnly)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Recording will not commence until a PMT is set.");
            usleep(5000);
            continue;
        }

        if (!m_streamHandler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }
    }

    m_streamHandler->RemoveListener(m_streamData);
    m_streamData->RemoveWritingListener(this);
    m_streamData->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();
}

bool ASIRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    m_streamHandler = ASIStreamHandler::Get(m_channel->GetDevice(),
                                             m_tvrec ? m_tvrec->GetInputId() : -1);


    LOG(VB_RECORD, LOG_INFO, LOC + "Opened successfully");

    return true;
}

bool ASIRecorder::IsOpen(void) const
{
    return m_streamHandler;
}

void ASIRecorder::Close(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- begin");

    if (IsOpen())
        ASIStreamHandler::Return(m_streamHandler,
                                 m_tvrec ? m_tvrec->GetInputId() : -1);

    LOG(VB_RECORD, LOG_INFO, LOC + "Close() -- end");
}
