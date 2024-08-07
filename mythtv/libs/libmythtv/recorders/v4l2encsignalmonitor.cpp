// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <sys/select.h>
#endif

#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"

#include "mpeg/atscstreamdata.h"
#include "mpeg/atsctables.h"
#include "mpeg/mpegtables.h"

#include "v4lchannel.h"
#include "v4l2encrecorder.h"
#include "v4l2encstreamhandler.h"
#include "v4l2encsignalmonitor.h"

#define LOC QString("V4L2SigMon[%1](%2): ").arg(m_inputid).arg(m_channel->GetDevice())

/**
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel V4lChannel for card
 *  \param _flags   Flags to start with
 */
V4L2encSignalMonitor::V4L2encSignalMonitor(int db_cardnum,
                                           V4LChannel *_channel,
                                           bool _release_stream,
                                           uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_v4l2.Open(m_channel->GetDevice());
    if (!m_v4l2.IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ctor -- Open failed");
        return;
    }

    m_isTS = (m_v4l2.GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS);


    m_signalStrength.SetRange(0, 100);
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("%1 stream.")
        .arg(m_isTS ? "Transport" : "Program"));
}

/** \fn V4L2encSignalMonitor::~V4L2encSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
V4L2encSignalMonitor::~V4L2encSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    V4L2encSignalMonitor::Stop();
    if (m_streamHandler)
        V4L2encStreamHandler::Return(m_streamHandler, m_inputid);
}

/** \fn V4L2encSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void V4L2encSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");

    SignalMonitor::Stop();
    if (m_streamHandler && GetStreamData())
            m_streamHandler->RemoveListener(GetStreamData());

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

/** \fn V4L2encSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void V4L2encSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (!m_isTS)
    {
        RemoveFlags(SignalMonitor::kDTVSigMon_WaitForPAT |
                    SignalMonitor::kDTVSigMon_WaitForPMT |
                    SignalMonitor::kDVBSigMon_WaitForPos);

        SignalMonitor::UpdateValues();

        {
            QMutexLocker locker(&m_statusLock);
            if (!m_scriptStatus.IsGood())
                return;
        }
    }

    if (m_streamHandler)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        m_updateDone = true;
        return;
    }

    bool isLocked = HasLock();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("isLocked: %1").arg(isLocked));

    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(m_strength);
        m_signalLock.SetValue(isLocked ? 1 : 0);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && !m_streamHandler && GetStreamData() &&
            HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                       kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                       kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        auto* chn = reinterpret_cast<V4LChannel*>(m_channel);
        m_streamHandler = V4L2encStreamHandler::Get(chn->GetDevice(),
                                      chn->GetAudioDevice().toInt(), m_inputid);
        if (!m_streamHandler || m_streamHandler->HasError())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "V4L2encSignalMonitor -- failed to start StreamHandler.");
        }
        else
        {
            m_streamHandler->AddListener(GetStreamData());
            m_streamHandler->StartEncoding();
        }
    }

    m_updateDone = true;
}

bool V4L2encSignalMonitor::HasLock(void)
{
    if (!m_v4l2.IsOpen())
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            "GetSignalStrengthPercent() -- v4l2 device not open");
        return false;
    }

    if (m_strength >= 0)
        m_strength = m_v4l2.GetSignalStrength();
    if (m_strength < 0)
        return (true /* || StableResolution() == 100 */);

    return m_strength > 50;
}

/**
 *  \brief Wait for a stable signal
 *
 *  The HD-PVR will produce garbage if the resolution or audio type
 *  changes after it is told to start encoding.  m_stableTime is used
 *  to designate how long we need to see a stable resolution reported
 *  from the HD-PVR driver, before we consider it a good lock.  In my
 *  testing 2 seconds is safe, while 1 second worked most of the time. --jp
 */
int V4L2encSignalMonitor::StableResolution(void)
{
    int width = 0;
    int height = 0;

    if (m_v4l2.GetResolution(width, height))
    {
        m_timer.stop();
        if (width > 0)
        {
            LOG(VB_CHANNEL, LOG_INFO, LOC +
                QString("Resolution already stable at %1 x %2")
                .arg(width).arg(height));
            return 100;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + "Device wedged?");
        return 0;
    }

    if (m_width == width)
    {
        if (!m_timer.isRunning())
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("Resolution %1 x %2")
                .arg(width).arg(height));
            if (++m_lockCnt > 9)
                m_lockCnt = 1;
            m_timer.start();
        }
        else if (m_timer.elapsed() > m_stableTime)
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("Resolution stable at %1 x %2")
                .arg(width).arg(height));
            m_timer.stop();
            return 100;
        }
        else
        {
            return 40 + m_lockCnt;
        }
    }
    else
    {
        // Indicate that we are still waiting for a valid res, every 3 seconds.
        if (!m_statusTime.isValid() || MythDate::current() > m_statusTime)
        {
            LOG(VB_CHANNEL, LOG_WARNING, "Waiting for valid resolution");
            m_statusTime = MythDate::current().addSecs(3);
        }
        m_timer.stop();
    }

    m_width = width;
    return 20 + m_lockCnt;
}
