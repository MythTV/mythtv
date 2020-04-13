// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>
#if !defined(USING_MINGW) && !defined(_WIN32)
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

#define LOC QString("ExternSigMon[%1](%2): ").arg(m_inputid).arg(m_loc)

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
ExternalSignalMonitor::ExternalSignalMonitor(int db_cardnum,
                                             ExternalChannel *_channel,
                                             bool _release_stream,
                                             uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    QString result;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
    m_stream_handler = ExternalStreamHandler::Get(m_channel->GetDevice(),
                                                  m_channel->GetInputID(),
                                                  m_channel->GetMajorID());
    if (!m_stream_handler || m_stream_handler->HasError())
        LOG(VB_GENERAL, LOG_ERR, LOC + "Open failed");
    else
        m_lock_timeout = GetLockTimeout() * 1000;

    if (GetExternalChannel()->IsBackgroundTuning())
        m_scriptStatus.SetValue(1);
}

/** \fn ExternalSignalMonitor::~ExternalSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
ExternalSignalMonitor::~ExternalSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    ExternalSignalMonitor::Stop();
    ExternalStreamHandler::Return(m_stream_handler, m_inputid);
}

/** \fn ExternalSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void ExternalSignalMonitor::Stop(void)
{
    QString result;

    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");

    SignalMonitor::Stop();
    if (GetStreamData())
    {
        m_stream_handler->StopStreaming();
        m_stream_handler->RemoveListener(GetStreamData());
    }
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
    if (!m_running || m_exit)
        return;

    if (GetExternalChannel()->IsExternalChannelChangeInUse())
    {
        SignalMonitor::UpdateValues();

        QMutexLocker locker(&m_statusLock);
        if (!m_scriptStatus.IsGood())
            return;
    }

    if (GetExternalChannel()->IsBackgroundTuning())
    {
        QMutexLocker locker(&m_statusLock);
        if (m_scriptStatus.GetValue() < 2)
            m_scriptStatus.SetValue(GetExternalChannel()->GetTuneStatus());

        if (!m_scriptStatus.IsGood())
            return;
    }

    if (m_stream_handler_started)
    {
        if (!m_stream_handler->IsRunning())
        {
            m_error = QObject::tr("Error: stream handler died");
            LOG(VB_CHANNEL, LOG_ERR, LOC + m_error);
            m_update_done = true;
            return;
        }

        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();

        m_update_done = true;
        return;
    }

    AddFlags(kSigMon_WaitForSig);

    int strength   = GetSignalStrengthPercent();
    bool is_locked = HasLock();

    // Set SignalMonitorValues
    {
        QMutexLocker locker(&m_statusLock);
        m_signalStrength.SetValue(strength);
        m_signalLock.SetValue(static_cast<int>(is_locked));
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
            m_stream_handler->StartStreaming();
            m_stream_handler_started = true;
        }
    }

    m_update_done = true;
}

bool ExternalSignalMonitor::HasLock(void)
{
    QString result;

    m_stream_handler->ProcessCommand("HasLock?", result);
    if (result.startsWith("OK:"))
    {
        return result.mid(3, 3) == "Yes";
    }
    LOG(VB_CHANNEL, LOG_ERR, LOC + QString
        ("HasLock: invalid response '%1'").arg(result));
    if (!result.startsWith("WARN"))
        m_error = QString("HasLock: invalid response '%1'").arg(result);
    return false;
}

int ExternalSignalMonitor::GetSignalStrengthPercent(void)
{
    QString result;

    m_stream_handler->ProcessCommand("SignalStrengthPercent?", result);
    if (result.startsWith("OK:"))
    {
        bool ok = false;
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
        m_error = QString("GetSignalStrengthPercent: invalid response '%1'")
                .arg(result);
    return -1;
}

int ExternalSignalMonitor::GetLockTimeout(void)
{
    QString result;

    m_stream_handler->ProcessCommand("LockTimeout?", result, 10000);
    if (result.startsWith("OK:"))
    {
        bool ok = false;
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
        m_error = QString("GetLockTimeout: invalid response '%1'").arg(result);
    return -1;
}
