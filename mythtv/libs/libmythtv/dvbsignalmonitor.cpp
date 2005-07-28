// -*- Mode: c++ -*-

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"
#include "dvbchannel.h"
#include "atscstreamdata.h"
#include "mpegtables.h"
#include "atsctables.h"

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

#include "transform.h"
#include "dvbtypes.h"
#include "dvbdev.h"
#include "dvbchannel.h"
#include "dvbrecorder.h"
#include "dvbtypes.h"

static bool fill_frontend_stats(int fd_frontend, dvb_stats_t &stats);

/** \fn DVBSignalMonitor::DVBSignalMonitor(int,int,int)
 *  \brief Initializes signal lock and signal values.
 *
 *   Start() must be called to actually begin continuous
 *   signal monitoring. The timeout is set to 3 seconds,
 *   and the signal threshold is initialized to 0%.
 *
 *  \param capturecardnum Recorder number to monitor,
 *                        if this is less than 0, SIGNAL events will not be
 *                        sent to the frontend even if SetNotifyFrontend(true)
 *                        is called.
 *  \param cardnum DVB card number.
 *  \param channel DVBChannel for card.
 */
DVBSignalMonitor::DVBSignalMonitor(int db_cardnum, DVBChannel* _channel,
                                   uint _flags, const char *_name)
    : DTVSignalMonitor(db_cardnum, _flags, _name),
      cardnum(_channel->GetCardNum()),
      signalToNoise(
          QObject::tr("Signal To Noise"), "snr", -32768,  true, -32768, 32767, 0),
      bitErrorRate(
          QObject::tr("Bit Error Rate"), "ber", 65535, false, 0, 65535, 0),
      uncorrectedBlocks(
          QObject::tr("Uncorrected Blocks"), "ucb", 65535,  false, 0, 65535, 0),
      dtvMonitorRunning(false), channel(_channel)
{
    // These two values should probably come from the database...
    int wait = 3000; // timeout when waiting on signal
    int threshold = 0; // signal strength threshold in %

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);
    signalStrength.SetMax(65535);

    table_monitor_thread = PTHREAD_CREATE_JOINABLE;

    dvb_stats_t stats;
    bzero(&stats, sizeof(stats));
    uint newflags = 0;
    QString msg = QString("DVBSignalMonitor(%1,%2): %3 (%4)")
        .arg(capturecardnum).arg(cardnum);

#define DVB_IO(WHAT,WHERE,ERRMSG,FLAG) \
    if (ioctl(_channel->GetFd(), WHAT, WHERE)) \
        VERBOSE(VB_IMPORTANT, msg.arg(ERRMSG).arg(strerror(errno))); \
    else newflags |= FLAG;

    DVB_IO(FE_READ_SIGNAL_STRENGTH, &stats.ss,
           "Warning, can not measure Signal Strength", kDTVSigMon_WaitForSig);
    DVB_IO(FE_READ_SNR, &stats.snr,
           "Warning, can not measure S/N", kDVBSigMon_WaitForSNR);
    DVB_IO(FE_READ_BER, &stats.ber,
           "Warning, can not measure Bit Error Rate", kDVBSigMon_WaitForBER);
    DVB_IO(FE_READ_UNCORRECTED_BLOCKS, &stats.ub,
           "Warning, can not count Uncorrected Blocks", kDVBSigMon_WaitForUB);
    DVB_IO(FE_READ_STATUS, &stats.status, "Error, can not read status!", 0);
    AddFlags(newflags);
    cerr<<"newflags: "<<newflags<<endl;
#undef DVB_IO
}

/** \fn DVBSignalMonitor::~DVBSignalMonitor()
 *  \brief Stops signal monitoring and table monitoring threads.
 */
DVBSignalMonitor::~DVBSignalMonitor()
{
    Stop();
}

/** \fn DVBSignalMonitor::Stop()
 *  \brief Stop signal monitoring and table monitoring threads.
 */
void DVBSignalMonitor::Stop(void)
{
    VERBOSE(VB_CHANNEL, "DVBSignalMonitor::Stop() -- begin");
    SignalMonitor::Stop();
    if (dtvMonitorRunning)
    {
        dtvMonitorRunning = false;
        pthread_join(table_monitor_thread, NULL);
    }
    VERBOSE(VB_CHANNEL, "DVBSignalMonitor::Stop() -- end");
}

QStringList DVBSignalMonitor::GetStatusList(bool kick)
{
    QStringList list = DTVSignalMonitor::GetStatusList(kick);
    statusLock.lock();
    if (HasFlags(kDVBSigMon_WaitForSNR))
        list<<signalToNoise.GetName()<<signalToNoise.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForBER))
        list<<bitErrorRate.GetName()<<bitErrorRate.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForUB))
        list<<uncorrectedBlocks.GetName()<<uncorrectedBlocks.GetStatus();
    statusLock.unlock();
    return list;
}

void *DVBSignalMonitor::TableMonitorThread(void *param)
{
    DVBSignalMonitor *mon = (DVBSignalMonitor*) param;
    mon->RunTableMonitor();
    return NULL;
}

const int buffer_size = TSPacket::SIZE * 1500;

bool DVBSignalMonitor::AddPIDFilter(uint pid)
{ 
    VERBOSE(VB_CHANNEL, QString("AddPIDFilter(0x%1)").arg(pid, 0, 16));
    int mux_fd = open(dvbdevice(DVB_DEV_DEMUX, cardnum), O_RDWR | O_NONBLOCK);
    if (mux_fd == -1)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to open demux device %1 for filter on pid %2")
                .arg(dvbdevice(DVB_DEV_DEMUX, cardnum)).arg(pid));
        return false;
    }

    struct dmx_pes_filter_params pesFilterParams;
    bzero(&pesFilterParams, sizeof(struct dmx_pes_filter_params));
    pesFilterParams.pid      = (__u16) pid;
    pesFilterParams.input    = DMX_IN_FRONTEND;
    pesFilterParams.output   = DMX_OUT_TS_TAP;
    pesFilterParams.flags    = DMX_IMMEDIATE_START;
    pesFilterParams.pes_type = DMX_PES_OTHER;

    if (ioctl(mux_fd, DMX_SET_PES_FILTER, &pesFilterParams) < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to set pes filter (pid %1)").arg(pid));
        return false;
    }

    unsigned char *buffer = new unsigned char[buffer_size];
    if (buffer)
    {
        bzero(buffer, buffer_size);
        filters[pid]    = mux_fd;        
        buffers[pid]    = buffer;
        remainders[pid] = 0;
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to allocate buffer for pes filter (pid %1)")
                .arg(pid));
        if (close(mux_fd)<0)
            VERBOSE(VB_IMPORTANT, QString("Failed to close mux (pid %1)")
                    .arg(pid));
 
        return false;
    }

    return true;
}

bool DVBSignalMonitor::RemovePIDFilter(uint pid)
{
    VERBOSE(VB_CHANNEL, QString("RemovePIDFilter(0x%1)").arg(pid, 0, 16));
    if (filters.find(pid)==filters.end())
        return false;

    int mux_fd = filters[pid];
    filters.erase(filters.find(pid));
    int err = close(mux_fd);
    if (err < 0)
        VERBOSE(VB_IMPORTANT, QString("Failed to close mux (pid %1)").arg(pid));

    unsigned char *buffer = buffers[pid];
    buffers.erase(buffers.find(pid));
    if (buffer)
        delete[] buffer;

    remainders.erase(remainders.find(pid));

    return err >= 0;
}

bool DVBSignalMonitor::UpdateFiltersFromStreamData(void)
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

    // Remove PIDs
    bool ok = true;
    vector<int>::iterator dit = del_pids.begin();
    for (; dit != del_pids.end(); ++dit)
        ok &= RemovePIDFilter(*dit);

    // Add PIDs
    vector<int>::iterator ait = add_pids.begin();
    for (; ait != add_pids.end(); ++ait)
        ok &= AddPIDFilter(*ait);

    return ok;
}

void DVBSignalMonitor::RunTableMonitor(void)
{
    dtvMonitorRunning = true;
    int remainder = 0;
    int buffer_size = TSPacket::SIZE * 15000;
    unsigned char *buffer = new unsigned char[buffer_size];
    if (!buffer)
        return;
    bzero(buffer, buffer_size);

    int dvr_fd = open(dvbdevice(DVB_DEV_DVR, cardnum), O_RDONLY | O_NONBLOCK);
    if (dvr_fd < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Failed to open DVR device %1 : %2")
                .arg(dvbdevice(DVB_DEV_DVR, cardnum))
                .arg(strerror(errno)));
        return;
    }

    VERBOSE(VB_CHANNEL, "RunTableMonitor() -- begin ("
            <<GetStreamData()->ListeningPIDs().size()<<")");
    while (dtvMonitorRunning && GetStreamData())
    {
        UpdateFiltersFromStreamData();

        long long len = read(dvr_fd, &(buffer[remainder]), buffer_size - remainder);

        if ((0 == len) || (-1 == len))
        {
            usleep(100);
            continue;
        }

        len += remainder;
        remainder = GetStreamData()->ProcessData(buffer, len);
        if (remainder > 0) // leftover bytes
            memmove(buffer, &(buffer[buffer_size - remainder]), remainder);
    }
    VERBOSE(VB_CHANNEL, "RunTableMonitor() -- shutdown");

    if (GetStreamData())
    {
        vector<int> del_pids;
        FilterMap::iterator it = filters.begin();
        for (; it != filters.end(); ++it)
            del_pids.push_back(it.key());

        vector<int>::iterator dit = del_pids.begin();
        for (; dit != del_pids.end(); ++dit)
            RemovePIDFilter(*dit);
    }
    close(dvr_fd);
    VERBOSE(VB_CHANNEL, "RunTableMonitor() -- end");
}

#define EMIT(SIGNAL_FUNC, SIGNAL_VAL) \
    do { statusLock.lock(); \
         SignalMonitorValue val = SIGNAL_VAL; \
         statusLock.unlock(); \
         emit SIGNAL_FUNC(val); } while (false)

/** \fn DVBSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This uses fill_frontend_stats(int,dvb_stats_t&)
 *   to actually collect the signal values. It is automatically
 *   called by MonitorLoop(), after Start() has been used to start
 *   the signal monitoring thread.
 */
void DVBSignalMonitor::UpdateValues(void)
{
    dvb_stats_t stats;

    if (!dtvMonitorRunning && fill_frontend_stats(channel->GetFd(), stats) &&
        !(stats.status & FE_TIMEDOUT))
    {
        statusLock.lock();
        int wasLocked = signalLock.GetValue();
        int locked = (stats.status & FE_HAS_LOCK) ? 1 : 0;
        signalLock.SetValue(locked);
        signalStrength.SetValue((int) stats.ss);
        signalToNoise.SetValue((int) stats.snr);
        bitErrorRate.SetValue(stats.ber);
        uncorrectedBlocks.SetValue(stats.ub);
        statusLock.unlock();

        //cerr<<"locked("<<locked<<") slock("<<signalLock.IsGood()<<")"<<endl;
        
        EMIT(StatusSignalLock, signalLock);

        if (HasFlags(kDTVSigMon_WaitForSig))
            EMIT(StatusSignalStrength, signalStrength);
        if (HasFlags(kDVBSigMon_WaitForSNR))
            EMIT(StatusSignalToNoise, signalToNoise);
        if (HasFlags(kDVBSigMon_WaitForBER))
            EMIT(StatusBitErrorRate, bitErrorRate);
        if (HasFlags(kDVBSigMon_WaitForUB))
            EMIT(StatusUncorrectedBlocks, uncorrectedBlocks);

        if (wasLocked != signalLock.GetValue())
            GENERAL((wasLocked ? "Signal Lost" : "Signal Lock"));

        statusLock.lock();
        bool ok = signalLock.IsGood();
        statusLock.unlock();
        if (ok && GetStreamData() &&
            HasAnyFlag(kDTVSigMon_WaitForPAT | kDTVSigMon_WaitForPMT |
                       kDTVSigMon_WaitForMGT | kDTVSigMon_WaitForVCT |
                       kDTVSigMon_WaitForNIT | kDTVSigMon_WaitForSDT))
        {
            pthread_create(&table_monitor_thread, NULL,
                           TableMonitorThread, this);
            while (!dtvMonitorRunning)
                usleep(50);
        }
    }
    else
    {
        // TODO dtv signals...
    }

    update_done = true;
}
#undef EMIT

/** \fn fill_frontend_stats(int,dvb_stats_t&)
 *  \brief Returns signal statistics for the frontend file descriptor.
 *
 *   This function uses five ioctl's FE_READ_SNR, FE_READ_SIGNAL_STRENGTH
 *   FE_READ_BER, FE_READ_UNCORRECTED_BLOCKS, and FE_READ_STATUS to obtain
 *   statistics from the frontend.
 *
 *  \param fd_frontend File descriptor for DVB frontend device.
 *  \param stats Used to return the statistics collected.
 *  \return true if successful, false if there is an error.
 */
static bool fill_frontend_stats(int fd_frontend, dvb_stats_t& stats)
{
    if (fd_frontend < 0)
        return false;

    stats.snr=0;
    ioctl(fd_frontend, FE_READ_SNR, &stats.snr);
    stats.ss=0;
    ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &stats.ss);
    stats.ber=0;
    ioctl(fd_frontend, FE_READ_BER, &stats.ber);
    stats.ub=0;
    ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &stats.ub);
    ioctl(fd_frontend, FE_READ_STATUS, &stats.status);

    return true;
}
