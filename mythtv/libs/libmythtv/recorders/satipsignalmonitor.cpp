// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"

#include "satipsignalmonitor.h"
#include "satipstreamhandler.h"
#include "satiprecorder.h"
#include "satipchannel.h"

#define LOC QString("SatIPSigMon[%1]: ").arg(m_inputid)

SatIPSignalMonitor::SatIPSignalMonitor(int db_cardnum,
                                       SatIPChannel* channel,
                                       bool release_stream,
                                       uint64_t flags) :
    DTVSignalMonitor(db_cardnum, channel, release_stream, flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    // Tuning timeout time for channel lock from database.
    std::chrono::milliseconds wait = 3s;             // Minimum timeout time 3 seconds
    std::chrono::milliseconds signal_timeout = 0ms;  // Not used here
    std::chrono::milliseconds tuning_timeout = 0ms;  // Maximum time for channel lock
    CardUtil::GetTimeouts(db_cardnum, signal_timeout, tuning_timeout);
    if (tuning_timeout < wait)
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Tuning timeout from database: %1 ms is too small, using %2 ms")
                .arg(tuning_timeout.count()).arg(wait.count()));
    }
    else
    {
        wait = tuning_timeout;              // Use value from database
        LOG(VB_CHANNEL, LOG_INFO, LOC +
            QString("Tuning timeout: %1 ms").arg(wait.count()));
    }

    m_signalStrength.SetThreshold(0);
    m_signalStrength.SetRange(0, 255);
    m_signalStrength.SetTimeout(wait);

    AddFlags(kSigMon_WaitForSig);

    m_streamHandler = SatIPStreamHandler::Get(channel->GetDevice(), m_inputid);
}

SatIPSignalMonitor::~SatIPSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    SatIPSignalMonitor::Stop();
    SatIPStreamHandler::Return(m_streamHandler, m_inputid);
}

void SatIPSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
    {
        m_streamHandler->RemoveListener(GetStreamData());
    }
    m_streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

SatIPChannel *SatIPSignalMonitor::GetSatIPChannel(void)
{
    return dynamic_cast<SatIPChannel*>(m_channel);
}

void SatIPSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (m_streamHandlerStarted)
    {
        if (!m_streamHandler->IsRunning())
        {
            m_error = tr("Error: stream handler died");
            m_updateDone = true;
            return;
        }

        EmitStatus();
        if (IsAllGood())
        {
            SendMessageAllGood();
        }

        // Update signal status
        int signalStrength = m_streamHandler->GetSignalStrength();
        m_signalStrength.SetValue(signalStrength);

        m_updateDone = true;
        return;
    }

    // Update signal status
    bool wasLocked = false;
    bool isLocked = false;
    {
        QMutexLocker locker(&m_statusLock);
        int signalStrength = m_streamHandler->GetSignalStrength();
        bool hasLock = m_streamHandler->HasLock();
        m_signalStrength.SetValue(signalStrength);
        wasLocked = m_signalLock.IsGood();
        m_signalLock.SetValue(hasLock);
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
    {
        SendMessageAllGood();
    }

    // Start table monitoring if we are waiting on any table and we have a lock.
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
