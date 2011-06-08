// -*- Mode: c++ -*-

#include <unistd.h>

// MythTV headers
#include "mpegstreamdata.h"
#include "iptvchannel.h"
#include "iptvfeederwrapper.h"
#include "iptvsignalmonitor.h"
#include "mythlogging.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    QString("IPTVSM(%1)::%2: %3").arg(channel->GetDevice()).arg(FUNC).arg(MSG))

#define LOC QString("IPTVSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("IPTVSM(%1), Error: ").arg(channel->GetDevice())

void IPTVTableMonitorThread::run(void)
{
    threadRegister("IPTVTableMonitor");
    m_parent->RunTableMonitor();
    threadDeregister();
}

/** \fn IPTVSignalMonitor::IPTVSignalMonitor(int,IPTVChannel*,uint64_t)
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
 *  \param _channel IPTVChannel for card
 *  \param _flags   Flags to start with
 */
IPTVSignalMonitor::IPTVSignalMonitor(int db_cardnum,
                                     IPTVChannel *_channel,
                                     uint64_t _flags) :
    DTVSignalMonitor(db_cardnum, _channel, _flags),
    dtvMonitorRunning(false), tableMonitorThread(NULL)
{
    bool isLocked = false;
    IPTVChannelInfo chaninfo = GetChannel()->GetCurrentChanInfo();
    if (chaninfo.isValid())
    {
        isLocked = GetChannel()->GetFeeder()->Open(chaninfo.m_url);
    }

    QMutexLocker locker(&statusLock);
    signalLock.SetValue((isLocked) ? 1 : 0);
    signalStrength.SetValue((isLocked) ? 100 : 0);
}

/** \fn IPTVSignalMonitor::~IPTVSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
IPTVSignalMonitor::~IPTVSignalMonitor()
{
    GetChannel()->GetFeeder()->RemoveListener(this);
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
    DBG_SM("Stop", "begin");
    GetChannel()->GetFeeder()->RemoveListener(this);
    SignalMonitor::Stop();
    if (tableMonitorThread)
    {
        GetChannel()->GetFeeder()->Stop();
        dtvMonitorRunning = false;
        tableMonitorThread->wait();
        delete tableMonitorThread;
        tableMonitorThread = NULL;
    }
    DBG_SM("Stop", "end");
}

/** \fn IPTVSignalMonitor::RunTableMonitor(void)
 */
void IPTVSignalMonitor::RunTableMonitor(void)
{
    DBG_SM("Run", "begin");
    dtvMonitorRunning = true;

    GetStreamData()->AddListeningPID(0);

    GetChannel()->GetFeeder()->AddListener(this);
    GetChannel()->GetFeeder()->Run();
    GetChannel()->GetFeeder()->RemoveListener(this);

    while (dtvMonitorRunning)
        usleep(10000);

    DBG_SM("Run", "end");
}

void IPTVSignalMonitor::AddData(
    const unsigned char *data, unsigned int dataSize)
{
    GetStreamData()->ProcessData((unsigned char*)data, dataSize);
}

/** \fn IPTVSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void IPTVSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (dtvMonitorRunning)
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
        tableMonitorThread = new IPTVTableMonitorThread(this);
        DBG_SM("UpdateValues", "Waiting for table monitor to start");
        while (!dtvMonitorRunning)
            usleep(5000);
        DBG_SM("UpdateValues", "Table monitor started");
    }

    update_done = true;
}
