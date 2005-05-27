#include <cerrno>
#include <cstring>
#include <unistd.h>
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"
#include "dvbchannel.h"

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
DVBSignalMonitor::DVBSignalMonitor(int _capturecardnum, int _cardnum,
                                   DVBChannel* _channel):
    DTVSignalMonitor(_capturecardnum, _channel->GetFd(), 0), cardnum(_cardnum),
    signalToNoise(QObject::tr("Signal To Noise"), "snr", 0,  true, 0, 65535, 0),
    bitErrorRate(
        QObject::tr("Bit Error Rate"), "ber", 65535, false, 0, 65535, 0),
    uncorrectedBlocks(
        QObject::tr("Uncorrected Blocks"), "ucb", 65535,  false, 0, 65535, 0),
    channel(_channel)
{
    // These two values should probably come from the database...
    int wait = 3000; // timeout when waiting on signal
    int threshold = 0; // signal strength threshold in %

    signalLock.SetTimeout(wait);
    signalStrength.SetTimeout(wait);
    signalStrength.SetThreshold(threshold);
    signalStrength.SetMax(65535);

    dvb_stats_t stats;
    bzero(&stats, sizeof(stats));
    uint newflags = 0;
    QString msg = QString("DVBSignalMonitor(%1,%2): %3 (%4)")
        .arg(_capturecardnum).arg(_cardnum);

#define DVB_IO(WHAT,WHERE,ERRMSG,FLAG) \
    if (ioctl(fd, WHAT, WHERE)) \
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
#undef DVB_IO
}

/** \fn DVBSignalMonitor::~DVBSignalMonitor()
 *  \brief Stops monitoring thread.
 */
DVBSignalMonitor::~DVBSignalMonitor()
{
}

QStringList DVBSignalMonitor::GetStatusList(bool kick)
{
    QStringList list = DTVSignalMonitor::GetStatusList(kick);
    if (HasFlags(kDVBSigMon_WaitForSNR))
        list<<signalToNoise.GetName()<<signalToNoise.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForBER))
        list<<bitErrorRate.GetName()<<bitErrorRate.GetStatus();
    if (HasFlags(kDVBSigMon_WaitForUB))
        list<<uncorrectedBlocks.GetName()<<uncorrectedBlocks.GetStatus();
    return list;
}

/** \fn DVBSignalMonitor::UpdateValues()
 *  \brief Fills in frontend stats and emits status Qt signals.
 *
 *   This uses FillFrontendStats(int,dvb_stats_t&)
 *   to actually collect the signal values. It is automatically
 *   called by MonitorLoop(), after Start() has been used to start
 *   the signal monitoring thread.
 */
void DVBSignalMonitor::UpdateValues()
{
    dvb_stats_t stats;

/*
    dvb_channel_t *cur_chan;
    channel->GetCurrentChannel(cur_chan);
*/

    if (FillFrontendStats(fd, stats) && !(stats.status & FE_TIMEDOUT))
    {
        int wasLocked = signalLock.GetValue();
        int locked = (stats.status & FE_HAS_LOCK) ? 1 : 0;
        signalLock.SetValue(locked);
        signalStrength.SetValue((int) stats.ss);
        signalToNoise.SetValue((int) stats.snr);
        bitErrorRate.SetValue(stats.ber);
        uncorrectedBlocks.SetValue(stats.ub);
        
        emit StatusSignalLock(locked);
        if (HasFlags(kDTVSigMon_WaitForSig))
            emit StatusSignalStrength((int) stats.ss);
        if (HasFlags(kDVBSigMon_WaitForSNR))
            emit StatusSignalToNoise((int) stats.snr);
        if (HasFlags(kDVBSigMon_WaitForBER))
            emit StatusBitErrorRate(stats.ber);
        if (HasFlags(kDVBSigMon_WaitForUB))
            emit StatusUncorrectedBlocks(stats.ub);

        if (wasLocked != signalLock.GetValue())
            GENERAL((wasLocked ? "Signal Lost" : "Signal Lock"));
    }
 
    update_done = true;
}

/** \fn DVBSignalMonitor::FillFrontendStats(int,dvb_stats_t&)
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
bool DVBSignalMonitor::FillFrontendStats(int fd_frontend, dvb_stats_t& stats)
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
