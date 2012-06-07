/** -*- Mode: c++ -*-
 *  CetonSignalMonitor
 *  Copyright (c) 2011 Ronald Frazier
 *  Copyright (c) 2006 Daniel Kristansson
 *  Distributed as part of MythTV under GPL v2 and later.
 */

#include "mythlogging.h"
#include "mythdbcon.h"

#include "cetonsignalmonitor.h"
#include "cetonstreamhandler.h"
#include "cetonrecorder.h"
#include "cetonchannel.h"

#define LOC QString("CetonSM(%1): ").arg(channel->GetDevice())

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
CetonSignalMonitor::CetonSignalMonitor(
    int db_cardnum, CetonChannel* _channel, uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    streamHandlerStarted(false), streamHandler(NULL)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    signalStrength.SetThreshold(45);

    AddFlags(kSigMon_WaitForSig);

    streamHandler = CetonStreamHandler::Get(_channel->GetDevice());
}

/** \fn CetonSignalMonitor::~CetonSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
CetonSignalMonitor::~CetonSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    CetonStreamHandler::Return(streamHandler);
}

/** \fn CetonSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void CetonSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        streamHandler->RemoveListener(GetStreamData());
    streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

CetonChannel *CetonSignalMonitor::GetCetonChannel(void)
{
    return dynamic_cast<CetonChannel*>(channel);
}

/** \fn CetonSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void CetonSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (streamHandlerStarted)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        // TODO dtv signals...

        update_done = true;
        return;
    }

    uint sig = 100;  // TODO find some way to actually monitor signal level

    // Set SignalMonitorValues from info from card.
    bool isLocked = false;
    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(sig);
        signalLock.SetValue(true);
        // TODO add some way to indicate if there is actually a lock
        isLocked = signalLock.IsGood();
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
        streamHandler->AddListener(GetStreamData());
        streamHandlerStarted = true;
    }

    update_done = true;
}
