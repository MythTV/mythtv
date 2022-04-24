// -*- Mode: c++ -*-

// MythTV headers
#include "libmythbase/mythlogging.h"

#include "iptvchannel.h"
#include "iptvsignalmonitor.h"
#include "mpeg/mpegstreamdata.h"

#define LOC QString("IPTVSigMon[%1](%2): ") \
            .arg(m_inputid).arg(m_channel->GetDevice())

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
                                     bool _release_stream,
                                     uint64_t _flags)
    : DTVSignalMonitor(db_cardnum, _channel, _release_stream, _flags)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "ctor");
    m_signalLock.SetValue(0);
    m_signalStrength.SetValue(0);
}

/** \fn IPTVSignalMonitor::~IPTVSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
IPTVSignalMonitor::~IPTVSignalMonitor()
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "dtor");
    IPTVSignalMonitor::Stop();
}

IPTVChannel *IPTVSignalMonitor::GetIPTVChannel(void)
{
    return dynamic_cast<IPTVChannel*>(m_channel);
}

/** \fn IPTVSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void IPTVSignalMonitor::Stop(void)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    GetIPTVChannel()->SetStreamData(nullptr);
    m_streamHandlerStarted = false;
    LOG(VB_CHANNEL, LOG_INFO, LOC + "Stop() -- end");
}

void IPTVSignalMonitor::SetStreamData(MPEGStreamData *data)
{
    DTVSignalMonitor::SetStreamData(data);
    GetIPTVChannel()->SetStreamData(GetStreamData());
}

void IPTVSignalMonitor::HandlePAT(const ProgramAssociationTable *pat)
{
    LOG(VB_CHANNEL, LOG_INFO, LOC + QString("HandlePAT pn: %1")
        .arg(m_programNumber));
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
    if (!m_running || m_exit)
        return;

    IPTVChannel *channel = GetIPTVChannel();
    if (channel == nullptr)
        return;

    if (!m_locked && channel->IsOpen())
    {
        QMutexLocker locker(&m_statusLock);
        m_signalLock.SetValue(1);
        m_signalStrength.SetValue(100);
        m_locked = true;
    }

    EmitStatus();
    if (IsAllGood())
        SendMessageAllGood();

    m_updateDone = true;

    if (m_streamHandlerStarted)
        return;

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        LOG(VB_CHANNEL, LOG_INFO, LOC + "UpdateValues: start sigmon");
        channel->SetStreamData(GetStreamData());
        m_streamHandlerStarted = true;
    }
}
