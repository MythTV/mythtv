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
#include "ExternalSignalMonitor.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include "ExternalChannel.h"
#include "ExternalRecorder.h"
#include "ExternalStreamHandler.h"

#define LOC QString("ExternSigMon(%1): ").arg(channel->GetDevice())

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
 *  \param _channel ExternalChannel for card
 *  \param _flags   Flags to start with
 */
ExternalSignalMonitor::ExternalSignalMonitor(
    int db_cardnum, ExternalChannel *_channel, uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    m_stream_handler(NULL), m_stream_handler_started(false)
{
    QString result;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
    m_stream_handler = ExternalStreamHandler::Get(_channel->GetDevice());
    if (!m_stream_handler || m_stream_handler->HasError())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
    else
        m_lock_timeout = GetLockTimeout() * 1000;
}

/** \fn ExternalSignalMonitor::~ExternalSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
ExternalSignalMonitor::~ExternalSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
    ExternalStreamHandler::Return(m_stream_handler);
}

/** \fn ExternalSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void ExternalSignalMonitor::Stop(void)
{
    QString result;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    m_stream_handler->StopStreaming();

    SignalMonitor::Stop();
    if (GetStreamData())
        m_stream_handler->RemoveListener(GetStreamData());
    m_stream_handler_started = false;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

/** \fn ExternalSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void ExternalSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (m_stream_handler_started)
    {
        if (!m_stream_handler->IsRunning())
        {
            error = QObject::tr("Error: stream handler died");
            LOG(VB_CHANNEL, LOG_ERR, LOC + error);
            update_done = true;
            return;
        }

        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        update_done = true;
        return;
    }

    AddFlags(kSigMon_WaitForSig);

    int strength   = GetSignalStrengthPercent();
    bool is_locked = HasLock();

    // Set SignalMonitorValues
    {
        QMutexLocker locker(&statusLock);
        signalStrength.SetValue(strength);
        signalLock.SetValue(is_locked);
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (is_locked && GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        if (!m_stream_handler_started)
        {
            m_stream_handler->AddListener(GetStreamData());
            m_stream_handler->StartStreaming(false);
            m_stream_handler_started = true;
        }
    }

    update_done = true;
}

bool ExternalSignalMonitor::HasLock(void)
{
    QString result;

    m_stream_handler->ProcessCommand("HasLock?", 2500, result);
    if (result.startsWith("OK:"))
    {
        return result.mid(3, 3) == "Yes";
    }
    LOG(VB_CHANNEL, LOG_ERR, LOC + QString
        ("HasLock: invalid response '%1'").arg(result));
    if (!result.startsWith("WARN"))
        error = QString("HasLock: invalid response '%1'").arg(result);
    return false;
}

int ExternalSignalMonitor::GetSignalStrengthPercent(void)
{
    QString result;

    m_stream_handler->ProcessCommand("SignalStrengthPercent?", 2500, result);
    if (result.startsWith("OK:"))
    {
        bool ok;
        int percent = result.mid(3).toInt(&ok);
        if (!ok)
        {
            LOG(VB_CHANNEL, LOG_ERR, LOC + QString
                ("GetSignalStrengthPercent: invalid response '%1'")
                .arg(result));
            return -1;
        }
        return percent;
    }
    LOG(VB_CHANNEL, LOG_ERR, LOC + QString
        ("GetSignalStrengthPercent: invalid response '%1'").arg(result));
    if (!result.startsWith("WARN"))
        error = QString("GetSignalStrengthPercent: invalid response '%1'")
                .arg(result);
    return -1;
}

int ExternalSignalMonitor::GetLockTimeout(void)
{
    QString result;

    m_stream_handler->ProcessCommand("LockTimeout?", 2500, result);
    if (result.startsWith("OK:"))
    {
        bool ok;
        int timeout = result.mid(3).toInt(&ok);
        if (!ok)
        {
            LOG(VB_CHANNEL, LOG_ERR, LOC + QString
                ("GetLockTimeout: invalid response '%1'")
                .arg(result));
            return -1;
        }
        return timeout;
    }
    LOG(VB_CHANNEL, LOG_ERR, LOC + QString
        ("GetLockTimeout: invalid response '%1'").arg(result));
    if (!result.startsWith("WARN"))
        error = QString("GetLockTimeout: invalid response '%1'").arg(result);
    return -1;
}
