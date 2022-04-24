// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#ifndef _WIN32
#include <sys/select.h>
#endif

#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"

#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "hdhrsignalmonitor.h"
#include "hdhrstreamhandler.h"
#include "mpeg/atscstreamdata.h"
#include "mpeg/atsctables.h"
#include "mpeg/mpegtables.h"

#define LOC QString("HDHRSigMon[%1](%2): ") \
            .arg(m_inputid).arg(m_channel->GetDevice())

/**
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel HDHRChannel for card
 *  \param _flags   Flags to start with
 */
HDHRSignalMonitor::HDHRSignalMonitor(int db_cardnum,
                                     HDHRChannel* _channel,
                                     bool _release_stream,
                                     uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags),
      m_signalToNoise    (QCoreApplication::translate("(Common)",
                          "Signal To Noise"),  "snr",
                          0,      true,      0, 100, 0ms)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_signalStrength.SetThreshold(45);

    AddFlags(kSigMon_WaitForSig);

    m_streamHandler = HDHRStreamHandler::Get(m_channel->GetDevice(),
                                           m_channel->GetInputID(),
                                           m_channel->GetMajorID());
}

/** \fn HDHRSignalMonitor::~HDHRSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
HDHRSignalMonitor::~HDHRSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    HDHRSignalMonitor::Stop();
    HDHRStreamHandler::Return(m_streamHandler, m_inputid);
}

/** \fn HDHRSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void HDHRSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        m_streamHandler->RemoveListener(GetStreamData());
    m_streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

HDHRChannel *HDHRSignalMonitor::GetHDHRChannel(void)
{
    return dynamic_cast<HDHRChannel*>(m_channel);
}

/** \fn HDHRSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void HDHRSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (m_streamHandlerStarted)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        // Update signal status for display
        struct hdhomerun_tuner_status_t status {};
        m_streamHandler->GetTunerStatus(&status);

        // Get info from card
        uint sig = status.signal_strength;
        uint snq = status.signal_to_noise_quality;
        uint seq = status.symbol_error_quality;

        LOG(VB_RECORD, LOG_DEBUG, LOC + "Tuner status: " + QString("'sig:%1 snq:%2 seq:%3'")
            .arg(sig).arg(snq).arg(seq));

        // Set SignalMonitorValues from info from card.
        {
            QMutexLocker locker(&m_statusLock);
            m_signalStrength.SetValue(sig);
            m_signalToNoise.SetValue(snq);
        }

        m_updateDone = true;
        return;
    }

    struct hdhomerun_tuner_status_t status {};
    m_streamHandler->GetTunerStatus(&status);

    uint sig = status.signal_strength;
    uint snq = status.signal_to_noise_quality;
    uint seq = status.symbol_error_quality;

    LOG(VB_RECORD, LOG_DEBUG, LOC + "Tuner status: " + QString("'sig:%1 snq:%2 seq:%3'")
            .arg(sig).arg(snq).arg(seq));

    // Set SignalMonitorValues from info from card.
    bool wasLocked = false;
    bool isLocked = false;
    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(sig);
        m_signalToNoise.SetValue(snq);
        wasLocked = m_signalLock.IsGood();
        m_signalLock.SetValue(static_cast<int>(status.lock_supported));
        isLocked = m_signalLock.IsGood();
    }

    // Signal lock change
    if (wasLocked != isLocked)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues -- Signal " +
                (isLocked ? "Locked" : "Lost"));
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        m_streamHandler->AddListener(GetStreamData());
        m_streamHandlerStarted = true;
    }

    m_updateDone = true;
}

void HDHRSignalMonitor::EmitStatus(void)
{
    // Emit signals..
    DTVSignalMonitor::EmitStatus();
    SendMessage(kStatusSignalToNoise, m_signalToNoise);
}
