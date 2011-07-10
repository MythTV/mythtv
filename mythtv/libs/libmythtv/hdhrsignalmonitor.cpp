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
#include "hdhrsignalmonitor.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include "hdhrchannel.h"
#include "hdhrrecorder.h"
#include "hdhrstreamhandler.h"

#define LOC QString("HDHRSM(%1): ").arg(channel->GetDevice())

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
HDHRSignalMonitor::HDHRSignalMonitor(
    int db_cardnum, HDHRChannel* _channel, uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    streamHandlerStarted(false), streamHandler(NULL)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    signalStrength.SetThreshold(45);

    AddFlags(kSigMon_WaitForSig);

    streamHandler = HDHRStreamHandler::Get(_channel->GetDevice());
}

/** \fn HDHRSignalMonitor::~HDHRSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
HDHRSignalMonitor::~HDHRSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    HDHRStreamHandler::Return(streamHandler);
}

/** \fn HDHRSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void HDHRSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        streamHandler->RemoveListener(GetStreamData());
    streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

HDHRChannel *HDHRSignalMonitor::GetHDHRChannel(void)
{
    return dynamic_cast<HDHRChannel*>(channel);
}

/** \fn HDHRSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void HDHRSignalMonitor::UpdateValues(void)
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

    struct hdhomerun_tuner_status_t status;
    streamHandler->GetTunerStatus(&status);

    uint sig = status.signal_strength;
    uint snq = status.signal_to_noise_quality;
    uint seq = status.symbol_error_quality;

    (void) snq; // TODO should convert to S/N
    (void) seq; // TODO should report this...

    LOG(VB_RECORD, LOG_DEBUG, LOC + "Tuner status: " + QString("'%1:%2:%3'")
            .arg(sig).arg(snq).arg(seq));

    // Set SignalMonitorValues from info from card.
    bool isLocked = false;
    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(sig);
        signalLock.SetValue(status.lock_supported);
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
