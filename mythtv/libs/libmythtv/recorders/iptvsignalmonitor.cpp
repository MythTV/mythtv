// -*- Mode: c++ -*-

// MythTV headers
#include "iptvsignalmonitor.h"
#include "mpegstreamdata.h"
#include "iptvchannel.h"
#include "mythlogging.h"

#define LOC QString("IPTVSM[%1](%2): ") \
            .arg(capturecardnum).arg(channel->GetDevice())

/** \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param db_cardnum Recorder number to monitor,
 *                    if this is less than 0, SIGNAL events will not be
 *                    sent to the frontend even if SetNotifyFrontend(true)
 *                    is called.
 *  \param _channel IPTVChannel for card
 *  \param _flags   Flags to start with
 */
IPTVSignalMonitor::IPTVSignalMonitor(int db_cardnum,
                                     IPTVChannel *_channel,
                                     uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    m_lock_timeout(1000 * 60 /* 1 minute */)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");

    // TODO init isLocked
    bool isLocked = true;

    QMutexLocker locker(&statusLock);
    signalLock.SetValue((isLocked) ? 1 : 0);
    signalStrength.SetValue((isLocked) ? 100 : 0);
}

/** \fn IPTVSignalMonitor::~IPTVSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
IPTVSignalMonitor::~IPTVSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    Stop();
}

IPTVChannel *IPTVSignalMonitor::GetChannel(void)
{
    return dynamic_cast<IPTVChannel*>(channel);
}

/** \fn IPTVSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void IPTVSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    GetChannel()->SetStreamData(NULL);
    m_streamHandlerStarted = false;
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

void IPTVSignalMonitor::SetStreamData(MPEGStreamData *data)
{
    DTVSignalMonitor::SetStreamData(data);
    GetChannel()->SetStreamData(GetStreamData());
}

void IPTVSignalMonitor::HandlePAT(const ProgramAssociationTable *pat)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("HandlePAT pn: %1")
        .arg(programNumber));
    DTVSignalMonitor::HandlePAT(pat);
}

/** \fn IPTVSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by run(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void IPTVSignalMonitor::UpdateValues(void)
{
    if (lock_timer.elapsed() > m_lock_timeout)
        error = "Timed out.";

    if (!running || exit)
        return;

    if (m_streamHandlerStarted)
    {
        EmitStatus();
        if (IsAllGood())
            SendMessageAllGood();
        // TODO dtv signals...

        update_done = true;
        return;
    }

    bool isLocked = false;
    {
        QMutexLocker locker(&statusLock);
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
        GetChannel()->SetStreamData(GetStreamData());
        m_streamHandlerStarted = true;
    }

    update_done = true;
}
