// -*- Mode: c++ -*-
// Copyright (c) 2006, Daniel Thor Kristjansson

#include <cerrno>
#include <cstring>

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "mythcontext.h"
#include "mythdbcon.h"
#include "hdhrsignalmonitor.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include "hdhrchannel.h"
#include "hdhrrecorder.h"

#define LOC QString("HDHRSM(%1): ").arg(channel->GetDevice())
#define LOC_ERR QString("HDHRSM(%1), Error: ").arg(channel->GetDevice())

/** \fn HDHRSignalMonitor::HDHRSignalMonitor(int,HDHRChannel*,uint,const char*)
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
 *  \param _name    Name for Qt signal debugging
 */
HDHRSignalMonitor::HDHRSignalMonitor(int db_cardnum,
                                     HDHRChannel* _channel,
                                     uint _flags, const char *_name)
    : DTVSignalMonitor(db_cardnum, _channel, _flags, _name),
      dtvMonitorRunning(false)
{
    VERBOSE(VB_CHANNEL, LOC + "ctor");

    _channel->DelAllPIDs();

    signalStrength.SetThreshold(65);

    AddFlags(kDTVSigMon_WaitForSig);
}

/** \fn HDHRSignalMonitor::~HDHRSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
HDHRSignalMonitor::~HDHRSignalMonitor()
{
    VERBOSE(VB_CHANNEL, LOC + "dtor");
    Stop();
}

void HDHRSignalMonitor::deleteLater(void)
{
    disconnect(); // disconnect signals we may be sending...
    Stop();
    DTVSignalMonitor::deleteLater();
}

/** \fn HDHRSignalMonitor::Stop(void)
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void HDHRSignalMonitor::Stop(void)
{
    VERBOSE(VB_CHANNEL, LOC + "Stop() -- begin");
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    VERBOSE(VB_CHANNEL, LOC + "Stop() -- end");
}

void *HDHRSignalMonitor::TableMonitorThread(void *param)
{
    HDHRSignalMonitor *mon = (HDHRSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

bool HDHRSignalMonitor::UpdateFiltersFromStreamData(void)
{
    vector<int> add_pids;
    vector<int> del_pids;

    if (!GetStreamData())
        return false;

    const QMap<uint, bool> &listening = GetStreamData()->ListeningPIDs();

    // PIDs that need to be added..
    QMap<uint, bool>::const_iterator lit = listening.constBegin();
    for (; lit != listening.constEnd(); ++lit)
        if (lit.data() && (filters.find(lit.key()) == filters.end()))
            add_pids.push_back(lit.key());

    // PIDs that need to be removed..
    FilterMap::const_iterator fit = filters.constBegin();
    for (; fit != filters.constEnd(); ++fit)
        if (listening.find(fit.key()) == listening.end())
            del_pids.push_back(fit.key());

    HDHRChannel *hdhr = dynamic_cast<HDHRChannel*>(channel);
    // Remove PIDs
    bool ok = true;
    vector<int>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
    {
        ok &= hdhr->DelPID(*dit);
        filters.erase(filters.find(*dit));
    }

    // Add PIDs
    vector<int>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
    {
        ok &= hdhr->AddPID(*ait);
        filters[*ait] = 1;
    }

    return ok;
}

void HDHRSignalMonitor::RunTableMonitor(void)
{
    dtvMonitorRunning = true;

    struct hdhomerun_video_sock_t *_video_socket;
    _video_socket = hdhomerun_video_create();
    if (!_video_socket)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to get video socket");
        return;
    }

    HDHRChannel *hdrc = dynamic_cast<HDHRChannel*>(channel);
    uint localPort = hdhomerun_video_get_local_port(_video_socket);
    if (!hdrc->DeviceSetTarget(localPort))
    {
        hdhomerun_video_destroy(_video_socket);
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to set target");
        return;
    }

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): " +
            QString("begin (# of pids %1)")
            .arg(GetStreamData()->ListeningPIDs().size()));

    while (dtvMonitorRunning && GetStreamData())
    {
        UpdateFiltersFromStreamData();

        struct hdhomerun_video_data_t data;

        int ret = hdhomerun_video_recv(_video_socket, &data, 100);
        if (ret > 0)
            GetStreamData()->ProcessData(data.buffer, data.length);
        else if (ret < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Recv error" + ENO);
            break;
        }
        else
            usleep(2500);
    }

    hdrc->DeviceClearTarget();
    hdhomerun_video_destroy(_video_socket);

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- shutdown");

    // TODO teardown PID filters here

    VERBOSE(VB_CHANNEL, LOC + "RunTableMonitor(): -- end");
}

/** \fn HDHRSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This function uses five ioctl's FE_READ_SNR, FE_READ_SIGNAL_STRENGTH
 *   FE_READ_BER, FE_READ_UNCORRECTED_BLOCKS, and FE_READ_STATUS to obtain
 *   statistics from the frontend.
 *
 *   This is automatically called by MonitorLoop(), after Start()
 *   has been used to start the signal monitoring thread.
 */
void HDHRSignalMonitor::UpdateValues(void)
{
    if (!running || exit)
        return;

    if (dtvMonitorRunning)
    {
        EmitHDHRSignals();
        if (IsAllGood())
            emit AllGood();
        // TODO dtv signals...

        update_done = true;
        return;
    }

    QString msg = ((HDHRChannel*)channel)->TunerGet("status");
    //ss  = signal strength,        [0,100]
    //snq = signal to noise quality [0,100]
    //seq = signal error quality    [0,100]
    int loc_sig = msg.find("ss="),  loc_snq = msg.find("snq=");
    int loc_seq = msg.find("seq="), loc_end = msg.length();
    bool ok0, ok1, ok2;
    uint sig = msg.mid(loc_sig + 3, loc_snq - loc_sig - 4).toUInt(&ok0);
    uint snq = msg.mid(loc_snq + 4, loc_seq - loc_snq - 5).toUInt(&ok1);
    uint seq = msg.mid(loc_seq + 4, loc_end - loc_seq - 4).toUInt(&ok2);
    (void) snq; // TODO should convert to S/N
    (void) seq; // TODO should report this...

    //VERBOSE(VB_RECORD, LOC + "Tuner status: " + msg);
    //VERBOSE(VB_RECORD, LOC + QString("'%1:%2:%3'")
    //        .arg(sig).arg(snq).arg(seq));

    // Set SignalMonitorValues from info from card.
    bool isLocked = false;
    {
        QMutexLocker locker(&statusLock);
        if (loc_sig > 0 && loc_snq > 0 && ok0)
            signalStrength.SetValue(sig);
        signalLock.SetValue(signalStrength.IsGood() ? 1 : 0);
        isLocked = signalLock.IsGood();
    }

    EmitHDHRSignals();
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

        VERBOSE(VB_CHANNEL, LOC + "UpdateValues() -- "
                "Waiting for table monitor to start");

        while (!dtvMonitorRunning)
            usleep(50);

        VERBOSE(VB_CHANNEL, LOC + "UpdateValues() -- "
                "Table monitor started");
    }

    update_done = true;
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

/** \fn HDHRSignalMonitor::EmitHDHRSignals(void)
 *  \brief Emits signals for lock, signal strength, etc.
 */
void HDHRSignalMonitor::EmitHDHRSignals(void)
{
    // Emit signals..
    EMIT(StatusSignalLock, signalLock); 
    if (HasFlags(kDTVSigMon_WaitForSig))
        EMIT(StatusSignalStrength, signalStrength);
}

#undef EMIT
