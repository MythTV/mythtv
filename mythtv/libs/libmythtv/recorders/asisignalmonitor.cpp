// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#ifndef USING_MINGW
#include <sys/select.h>
#endif

#include "mythlogging.h"
#include "mythdbcon.h"
#include "asisignalmonitor.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include "asichannel.h"
#include "asirecorder.h"
#include "asistreamhandler.h"

#define LOC QString("ASISM[%1](%2): ") \
            .arg(capturecardnum).arg(channel->GetDevice())

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
 *  \param _channel ASIChannel for card
 *  \param _flags   Flags to start with
 */
ASISignalMonitor::ASISignalMonitor(
    int db_cardnum, ASIChannel *_channel, uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    streamHandlerStarted(false), streamHandler(NULL),
    lock_timeout(1000 * 30 /* 30 seconds */ )
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
    streamHandler = ASIStreamHandler::Get(_channel->GetDevice());
}

/** \fn ASISignalMonitor::~ASISignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
ASISignalMonitor::~ASISignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    ASIStreamHandler::Return(streamHandler);
}

/** \fn ASISignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void ASISignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        streamHandler->RemoveListener(GetStreamData());
    streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

ASIChannel *ASISignalMonitor::GetASIChannel(void)
{
    return dynamic_cast<ASIChannel*>(channel);
}

/** \fn ASISignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void ASISignalMonitor::UpdateValues(void)
{
    if (lock_timer.elapsed() > lock_timeout)
        error = "Timed out.";

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

    // Set SignalMonitorValues from info from card.
    bool isLocked = true;
    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(100);
        signalLock.SetValue(1);
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
