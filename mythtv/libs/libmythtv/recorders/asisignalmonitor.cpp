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

#include "asichannel.h"
#include "asirecorder.h"
#include "asisignalmonitor.h"
#include "asistreamhandler.h"
#include "mpeg/atscstreamdata.h"
#include "mpeg/atsctables.h"
#include "mpeg/mpegtables.h"

#define LOC QString("ASISigMon[%1](%2): ") \
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
 *  \param _channel ASIChannel for card
 *  \param _flags   Flags to start with
 */
ASISignalMonitor::ASISignalMonitor(int db_cardnum,
                                   ASIChannel *_channel,
                                   bool _release_stream,
                                   uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
    m_streamHandler = ASIStreamHandler::Get(_channel->GetDevice(), m_inputid);
}

/** \fn ASISignalMonitor::~ASISignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
ASISignalMonitor::~ASISignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    ASISignalMonitor::Stop();
    ASIStreamHandler::Return(m_streamHandler, m_inputid);
}

/** \fn ASISignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void ASISignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (GetStreamData())
        m_streamHandler->RemoveListener(GetStreamData());
    m_streamHandlerStarted = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

ASIChannel *ASISignalMonitor::GetASIChannel(void)
{
    return dynamic_cast<ASIChannel*>(m_channel);
}

/** \fn ASISignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void ASISignalMonitor::UpdateValues(void)
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

    // Set SignalMonitorValues from info from card.
    bool isLocked = true;
    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(100);
        m_signalLock.SetValue(1);
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
