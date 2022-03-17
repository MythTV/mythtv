/** -*- Mode: c++ -*-
 *  CetonSignalMonitor
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006 Daniel Kristansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"

#include "cetonsignalmonitor.h"
#include "cetonstreamhandler.h"
#include "cetonrecorder.h"
#include "cetonchannel.h"

#define LOC QString("CetonSigMon[%1](%2): ") \
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
 *  \param _channel CetonChannel for card
 *  \param _flags   Flags to start with
 */
CetonSignalMonitor::CetonSignalMonitor(int db_cardnum,
                                       CetonChannel* _channel,
                                       bool _release_stream,
                                       uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    m_signalStrength.SetThreshold(45);

    AddFlags(kSigMon_WaitForSig);

    m_streamHandler = CetonStreamHandler::Get(m_channel->GetDevice(), m_inputid);
}

/** \fn CetonSignalMonitor::~CetonSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
CetonSignalMonitor::~CetonSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    CetonSignalMonitor::Stop();
    CetonStreamHandler::Return(m_streamHandler, m_inputid);
}

/** \fn CetonSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void CetonSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        m_streamHandler->RemoveListener(GetStreamData());
    m_streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

CetonChannel *CetonSignalMonitor::GetCetonChannel(void)
{
    return dynamic_cast<CetonChannel*>(m_channel);
}

/** \fn CetonSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void CetonSignalMonitor::UpdateValues(void)
{
    if (!m_running || m_exit)
        return;

    if (m_streamHandlerStarted)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        // TODO dtv signals...

        m_updateDone = true;
        return;
    }

    uint sig = 100;  // TODO find some way to actually monitor signal level

    // Set SignalMonitorValues from info from card.
    bool isLocked = false;
    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(sig);
        m_signalLock.SetValue(static_cast<int>(true));
        // TODO add some way to indicate if there is actually a lock
        isLocked = m_signalLock.IsGood();
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
