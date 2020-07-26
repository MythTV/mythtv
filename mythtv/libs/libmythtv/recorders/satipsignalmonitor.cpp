// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include "mythlogging.h"
#include "mythdbcon.h"

#include "satipsignalmonitor.h"
#include "satipstreamhandler.h"
#include "satiprecorder.h"
#include "satipchannel.h"

#define LOC QString("SatIPSigMon[%1](%2): ") \
            .arg(m_inputid).arg(m_channel->GetDevice())

SatIPSignalMonitor::SatIPSignalMonitor(
    int db_cardnum,
    SatIPChannel* channel,
    uint64_t flags) :
    DTVSignalMonitor(db_cardnum, channel, true, flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_signalStrength.SetThreshold(0);
    m_signalStrength.SetRange(0, 255);
    m_signalStrength.SetTimeout(3000);  // TODO Use value from database

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
        int signalStrength = m_streamHandler->m_rtsp->GetSignalStrength();
        m_signalStrength.SetValue(signalStrength);

        m_updateDone = true;
        return;
    }

    // Update signal status
    bool isLocked = false;
    {
        int signalStrength = m_streamHandler->m_rtsp->GetSignalStrength();
        bool hasLock = m_streamHandler->m_rtsp->HasLock();
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(signalStrength);
        m_signalLock.SetValue(hasLock);
        isLocked = m_signalLock.IsGood();
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
