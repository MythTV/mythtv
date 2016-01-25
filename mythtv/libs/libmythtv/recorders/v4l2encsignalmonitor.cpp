// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <sys/select.h>
#endif

#include "mythlogging.h"
#include "mythdbcon.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include "v4lchannel.h"
#include "v4l2encrecorder.h"
#include "v4l2encstreamhandler.h"
#include "v4l2encsignalmonitor.h"

#define LOC QString("V4L2SigMon(%1): ").arg(channel->GetDevice())

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
V4L2encSignalMonitor::V4L2encSignalMonitor(int db_cardnum, V4LChannel *_channel,
                                     uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    m_stream_handler(nullptr), m_isTS(false),
    m_strength(0), m_stable_time(1500), m_width(0), m_height(0), m_lock_cnt(0)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_v4l2.Open(channel->GetDevice());
    if (!m_v4l2.IsOpen())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "ctor -- Open failed");
        return;
    }

    m_isTS = (m_v4l2.GetStreamType() == V4L2_MPEG_STREAM_TYPE_MPEG2_TS);


    signalStrength.SetRange(0, 100);
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("%1 stream.")
        .arg(m_isTS ? "Transport" : "Program"));
}

/** \fn V4L2encSignalMonitor::~V4L2encSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
V4L2encSignalMonitor::~V4L2encSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    if (m_stream_handler)
        V4L2encStreamHandler::Return(m_stream_handler);
}

/** \fn V4L2encSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void V4L2encSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");

    SignalMonitor::Stop();
    if (m_stream_handler && GetStreamData())
            m_stream_handler->RemoveListener(GetStreamData());

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
    if (!running || exit)
        return;

    if (!m_isTS)
    {
        RemoveFlags(SignalMonitor::kDTVSigMon_WaitForPAT |
                    SignalMonitor::kDTVSigMon_WaitForPMT |
                    SignalMonitor::kDVBSigMon_WaitForPos);

        SignalMonitor::UpdateValues();

        {
            QMutexLocker locker(&statusLock);
            if (!scriptStatus.IsGood())
                return;
        }
    }

    if (m_stream_handler)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        update_done = true;
        return;
    }

    bool isLocked = HasLock();

    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("isLocked: %1").arg(isLocked));

    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(m_strength);
        signalLock.SetValue(isLocked ? 1 : 0);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && !m_stream_handler && GetStreamData() &&
            HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                       kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                       kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        V4LChannel* chn = reinterpret_cast<V4LChannel*>(channel);
        m_stream_handler =
            V4L2encStreamHandler::Get(chn->GetDevice(),
                                      chn->GetAudioDevice().toInt());
        if (!m_stream_handler || m_stream_handler->HasError())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "V4L2encSignalMonitor -- failed to start StreamHandler.");
        }
        else
        {
            m_stream_handler->AddListener(GetStreamData());
            m_stream_handler->StartEncoding();
        }
    }

    update_done = true;
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
        return (true || StableResolution() == 100);

    return m_strength > 50;
}

/** \fn V4L2encSignalMonitor::HandleHDPVR(void)
 *  \brief Wait for a stable signal
 *
 *  The HD-PVR will produce garbage if the resolution or audio type
 *  changes after it is told to start encoding.  m_stable_time is used
 *  to designate how long we need to see a stable resolution reported
 *  from the HD-PVR driver, before we consider it a good lock.  In my
 *  testing 2 seconds is safe, while 1 second worked most of the time. --jp
 */
int V4L2encSignalMonitor::StableResolution(void)
{
    int width, height;

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
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Device wedged?");
            return 0;
        }
    }

    if (m_width == width)
    {
        if (!m_timer.isRunning())
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("Resolution %1 x %2")
                .arg(width).arg(height));
            if (++m_lock_cnt > 9)
                m_lock_cnt = 1;
            m_timer.start();
        }
        else if (m_timer.elapsed() > m_stable_time)
        {
            LOG(VB_CHANNEL, LOG_INFO, QString("Resolution stable at %1 x %2")
                .arg(width).arg(height));
            m_timer.stop();
            return 100;
        }
        else
            return 40 + m_lock_cnt;
    }
    else
    {
        // Indicate that we are still waiting for a valid res, every 3 seconds.
        if (!m_status_time.isValid() || MythDate::current() > m_status_time)
        {
            LOG(VB_CHANNEL, LOG_WARNING, "Waiting for valid resolution");
            m_status_time = MythDate::current().addSecs(3);
        }
        m_timer.stop();
    }

    m_width = width;
    return 20 + m_lock_cnt;
}
