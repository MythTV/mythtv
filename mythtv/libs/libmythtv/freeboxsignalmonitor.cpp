// -*- Mode: c++ -*-

// MythTV headers
#include "mpegstreamdata.h"
#include "freeboxsignalmonitor.h"
#include "rtspcomms.h"

#undef DBG_SM
#define DBG_SM(FUNC, MSG) VERBOSE(VB_CHANNEL, \
    "FreeboxSM("<<channel->GetDevice()<<")::"<<FUNC<<": "<<MSG);

#define LOC QString("FreeboxSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("FreeboxSM(%1), Error: ").arg(channel->GetDevice())

/** \fn FreeboxSignalMonitor::FreeboxSignalMonitor(int,FreeboxChannel*,uint,const char*)
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
 *  \param _channel FreeboxChannel for card
 *  \param _flags   Flags to start with
 *  \param _name    Instance name for Qt signal/slot debugging
 */
FreeboxSignalMonitor::FreeboxSignalMonitor(
    int db_cardnum, FreeboxChannel *_channel,
    uint _flags, const char *_name)
    : DTVSignalMonitor(db_cardnum, _channel, _flags, _name),
      dtvMonitorRunning(false)
{
    bool isLocked = false;
    if (GetChannel()->GetRTSP()->Init())
    {
        FreeboxChannelInfo chaninfo = GetChannel()->GetCurrentChanInfo();
        isLocked = (chaninfo.isValid() && 
                    GetChannel()->GetRTSP()->Open(chaninfo.m_url));
    }

    QMutexLocker locker(&statusLock);
    signalLock.SetValue((isLocked) ? 1 : 0);
    signalStrength.SetValue((isLocked) ? 100 : 0);
}

/** \fn FreeboxSignalMonitor::~FreeboxSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
FreeboxSignalMonitor::~FreeboxSignalMonitor()
{
    GetChannel()->GetRTSP()->RemoveListener(this);
    Stop();
}

FreeboxChannel *FreeboxSignalMonitor::GetChannel(void)
{
    return dynamic_cast<FreeboxChannel*>(channel);
}

void FreeboxSignalMonitor::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    GetChannel()->GetRTSP()->RemoveListener(this);
    Stop();
    DTVSignalMonitor::deleteLater();
}

/** \fn FreeboxSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void FreeboxSignalMonitor::Stop(void)
{
    DBG_SM("Stop", "begin");
    GetChannel()->GetRTSP()->RemoveListener(this);
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        GetChannel()->GetRTSP()->Stop();
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    DBG_SM("Stop", "end");
}

void *FreeboxSignalMonitor::TableMonitorThread(void *param)
{
    FreeboxSignalMonitor *mon = (FreeboxSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

/** \fn FreeboxSignalMonitor::RunTableMonitor(void)
 */
void FreeboxSignalMonitor::RunTableMonitor(void)
{
    DBG_SM("Run", "begin");
    dtvMonitorRunning = true;

    GetStreamData()->AddListeningPID(0);

    GetChannel()->GetRTSP()->AddListener(this);
    GetChannel()->GetRTSP()->Run();
    GetChannel()->GetRTSP()->RemoveListener(this);

    dtvMonitorRunning = false;
    DBG_SM("Run", "end");
}

void FreeboxSignalMonitor::AddData(
    unsigned char *data, unsigned dataSize, struct timeval)
{
    GetStreamData()->ProcessData(data, dataSize);
}

/** \fn FreeboxSignalMonitor::UpdateValues(void)
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void FreeboxSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (dtvMonitorRunning)
    {
        EmitFreeboxSignals();
        if (IsAllGood())
            emit AllGood();
        // TODO dtv signals...

        update_done = true;
        return;
    }

    bool isLocked = false;
    {
        QMutexLocker locker(&statusLock);
        isLocked = signalLock.IsGood();
    }

    EmitFreeboxSignals();
    if (IsAllGood())
        emit AllGood();

    // Start table monitoring if we are waiting on any table
    // and we have a lock.
    if (isLocked && GetStreamData() &&
        HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                   kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                   kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
    {
        pthread_create(&table_monitor_thread, NULL,
                       TableMonitorThread, this);
        DBG_SM("UpdateValues", "Waiting for table monitor to start");
        while (!dtvMonitorRunning)
            usleep(50);
        DBG_SM("UpdateValues", "Table monitor started");
    }

    update_done = true;
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

/** \fn FreeboxSignalMonitor::EmitFreeboxSignals(void)
 *  \brief Emits signals for lock, signal strength, etc.
 */
void FreeboxSignalMonitor::EmitFreeboxSignals(void)
{
    // Emit signals..
    EMIT(StatusSignalLock, signalLock); 
    if (HasFlags(kDTVSigMon_WaitForSig))
        EMIT(StatusSignalStrength, signalStrength);
}

#undef EMIT
