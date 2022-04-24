// C++ includes
#include <chrono>
#include <thread>

// MythTV includes
#include "libmythbase/mythlogging.h"

#include "io/mythmediabuffer.h"
#include "mpeg/tsstreamdata.h"
#include "satipchannel.h"
#include "satiprecorder.h"
#include "satipstreamhandler.h"
#include "tv_rec.h"

#define LOC QString("SatIPRec[%1]: ").arg(m_inputId)

SatIPRecorder::SatIPRecorder(TVRec *rec, SatIPChannel *channel)
    : DTVRecorder(rec)
    , m_channel(channel)
    , m_inputId(rec->GetInputId())
{
    LOG(VB_RECORD, LOG_INFO, LOC + QString("ctor %1").arg(m_channel->GetDevice()));
}

bool SatIPRecorder::Open(void)
{
    if (IsOpen())
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Card already open");
        return true;
    }

    ResetForNewFile();

    if (m_channel->GetFormat().compare("MPTS") == 0)
    {
        // MPTS only. Use TSStreamData to write out unfiltered data.
        LOG(VB_RECORD, LOG_INFO, LOC + "Using TSStreamData");
        SetStreamData(new TSStreamData(m_inputId));
        m_recordMptsOnly = true;
        m_recordMpts = false;
    }

    m_streamHandler = SatIPStreamHandler::Get(m_channel->GetDevice(), m_inputId);

    LOG(VB_RECORD, LOG_INFO, LOC + "SatIP opened successfully");

    return true;
}

void SatIPRecorder::Close(void)
{
    if (IsOpen())
    {
        SatIPStreamHandler::Return(m_streamHandler, m_inputId);
    }
}

void SatIPRecorder::StartNewFile(void)
{
    if (!m_recordMptsOnly)
    {
        if (m_recordMpts)
        {
            m_streamHandler->AddNamedOutputFile(m_ringBuffer->GetFilename());
        }

        // Make sure the first things in the file are a PAT & PMT
        HandleSingleProgramPAT(m_streamData->PATSingleProgram(), true);
        HandleSingleProgramPMT(m_streamData->PMTSingleProgram(), true);
    }
}

void SatIPRecorder::run(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "run -- begin");

    if (!Open())
    {
        m_error = "Failed to open SatIPRecorder device";
        LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        return;
    }

    {
        QMutexLocker locker(&m_pauseLock);
        m_requestRecording = true;
        m_recording = true;
        m_recordingWait.wakeAll();
    }

    // Listen for time table on DVB standard streams
    if (m_channel && (m_channel->GetSIStandard() == "dvb"))
        m_streamData->AddListeningPID(PID::DVB_TDT_PID);
    if (m_recordMptsOnly)
        m_streamData->AddListeningPID(0x2000);

    StartNewFile();

    m_streamData->AddAVListener(this);
    m_streamData->AddWritingListener(this);
    m_streamHandler->AddListener(m_streamData, false, false,
                         (m_recordMpts) ? m_ringBuffer->GetFilename() : QString());

    while (IsRecordingRequested() && !IsErrored())
    {
        if (PauseAndWait())
            continue;

        if (!IsRecordingRequested())
            break;

        // sleep 100 milliseconds unless StopRecording() or Unpause()
        // is called, just to avoid running this too often.
        {
            QMutexLocker locker(&m_pauseLock);
            if (!m_requestRecording || m_requestPause)
                continue;
            m_unpauseWait.wait(&m_pauseLock, 100);
        }

        if (!m_inputPmt && !m_recordMptsOnly)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Recording will not commence until a PMT is set.");
            std::this_thread::sleep_for(5ms);
            continue;
        }

        if (!m_streamHandler->IsRunning())
        {
            m_error = "Stream handler died unexpectedly.";
            LOG(VB_GENERAL, LOG_ERR, LOC + m_error);
        }
    }

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- ending...");

    m_streamHandler->RemoveListener(m_streamData);
    m_streamData->RemoveWritingListener(this);
    m_streamData->RemoveAVListener(this);

    Close();

    FinishRecording();

    QMutexLocker locker(&m_pauseLock);
    m_recording = false;
    m_recordingWait.wakeAll();

    LOG(VB_RECORD, LOG_INFO, LOC + "run -- end");
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("< %1").arg(__func__));
}

bool SatIPRecorder::PauseAndWait(std::chrono::milliseconds timeout)
{
    QMutexLocker locker(&m_pauseLock);
    if (m_requestPause)
    {
        if (!IsPaused(true))
        {
            m_streamHandler->RemoveListener(m_streamData);

            m_paused = true;
            m_pauseWait.wakeAll();
            if (m_tvrec)
                m_tvrec->RecorderPaused();
        }

        m_unpauseWait.wait(&m_pauseLock, timeout.count());
    }

    if (!m_requestPause && IsPaused(true))
    {
        m_paused = false;
        m_streamHandler->AddListener(m_streamData);
        m_unpauseWait.wakeAll();
    }

    return IsPaused(true);
}

QString SatIPRecorder::GetSIStandard(void) const
{
    return m_channel->GetSIStandard();
}
