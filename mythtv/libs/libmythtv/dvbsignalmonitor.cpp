#include <unistd.h>
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dvbsignalmonitor.h"

const QString DVBSignalMonitor::TIMEOUT(QObject::tr("Timed out waiting for signal."));
const QString DVBSignalMonitor::LOCKED(QObject::tr("Signal Locked"));
const QString DVBSignalMonitor::NOTLOCKED(QObject::tr("No Lock"));

DVBSignalMonitor::DVBSignalMonitor(int _cardnum, int _fd_frontend):
    cardnum(_cardnum), fd_frontend(_fd_frontend)
{
    running = false;
    exit = false;

    if (fd_frontend >=0)
       fd_frontend = dup(fd_frontend);
    //signal_monitor_interval = 0;
    //expire_data_days = 3;
}

DVBSignalMonitor::~DVBSignalMonitor()
{
}

void DVBSignalMonitor::Start()
{
    if (!running)
        pthread_create(&monitor_thread, NULL, SpawnMonitorLoop, this);
}

void DVBSignalMonitor::Stop()
{
    exit = true;
    pthread_join(monitor_thread, NULL);
}

void DVBSignalMonitor::MonitorLoop()
{
    dvb_stats_t stats;

    running = true;
    exit = false;

    bool PrevLockedState = false;

    GENERAL("DVB Signal Monitor Starting");

    while (!exit && (fd_frontend >= 0))
    {
        if (FillFrontendStats(stats))
        {
//            emit Status(stats);
            emit StatusSignalToNoise((int) stats.snr);
            emit StatusSignalStrength((int) stats.ss);
            emit StatusBitErrorRate(stats.ber);
            emit StatusUncorrectedBlocks(stats.ub);

            QString str = "";
            if (stats.status & FE_TIMEDOUT)
            {
                str = TIMEOUT;
            }
            else
            {
                if (stats.status & FE_HAS_LOCK)
                {
                    str = LOCKED;
                    if (!PrevLockedState)
                    {
                        GENERAL("Signal Locked");
                        PrevLockedState = true;
                    }
                }
                else
                {
                    str = NOTLOCKED;
                    if (PrevLockedState)
                    {
                        GENERAL("Signal Lost");
                        PrevLockedState = false;
                    }
                }
            }
            emit Status(str);
        }

        usleep(250 * 1000);
    }
    if (fd_frontend>=0)
       close(fd_frontend);

    running = false;
    GENERAL("DVB Signal Monitor Stopped");
}

void* DVBSignalMonitor::SpawnMonitorLoop(void* self)
{
    ((DVBSignalMonitor*)self)->MonitorLoop();
    return NULL;
}

bool DVBSignalMonitor::FillFrontendStats(dvb_stats_t& stats)
{
    if (fd_frontend < 0)
        return false;

    memset (&stats,0,sizeof(dvb_stats_t));
    ioctl(fd_frontend, FE_READ_SNR, &stats.snr);
    ioctl(fd_frontend, FE_READ_SIGNAL_STRENGTH, &stats.ss);
    ioctl(fd_frontend, FE_READ_BER, &stats.ber);
    ioctl(fd_frontend, FE_READ_UNCORRECTED_BLOCKS, &stats.ub);
    ioctl(fd_frontend, FE_READ_STATUS, &stats.status);

    return true;
}

/*
void DVBRecorder::QualityMonitorSample(int cardid,
                                       dvb_stats_t& sample)
{
    QString sql = QString("INSERT INTO dvb_signal_quality("
                          "sampletime, cardid, fe_snr, fe_ss, fe_ber, "
                          "fe_unc, myth_cont, myth_over, myth_pkts) "
                          "VALUES(NOW(), "
                          "\"%1\",\"%2\",\"%3\",\"%4\","
                          "\"%5\",\"%6\",\"%7\",\"%8\");")
        .arg(cardid)
        .arg(sample.snr & 0xffff)
        .arg(sample.ss & 0xffff).arg(sample.ber).arg(sample.ub)
        .arg(cont_errors).arg(stream_overflows).arg(bad_packets);

    MSqlQuery result(MSqlQuery::InitCon());
    // = db_conn->exec(sql);

    if (!result.isActive())
        MythContext::DBError("DVB quality sample insert failed", result);
}

void *DVBRecorder::QualityMonitorThread()
{
    dvb_stats_t fe_stats;

    int cardid = GetIDForCardNumber(cardnum);

    // loop until cancelled, wake at intervals and log data

    while (!signal_monitor_quit)
    {
        sleep(signal_monitor_interval);

        if (signal_monitor_quit)
            break;

        if (cardid >= 0 &&
            && dvbchannel != NULL &&
            dvbchannel -> FillFrontendStats(fe_stats))
        {

            QualityMonitorSample(cardid, fe_stats);
        }
    }

    RECORD("DVB Quality monitor thread stopped for card " << cardnum);
    return NULL;
}

void DVBRecorder::ExpireQualityData()
{
    RECORD("Expiring DVB quality data older than " << expire_data_days <<
           " day(s)");


    QString sql = QString("DELETE FROM dvb_signal_quality "
                          "WHERE sampletime < "
                          "SUBDATE(NOW(), INTERVAL \"%1\" DAY);").
        arg(expire_data_days);

    MSqlQuery query(MSqlQuery::InitCon());
    // = db_conn->exec(sql);

    if (!query.isActive())
        MythContext::DBError("Could not expire DVB signal data",
                             query);

}

// we need the capture card id and I can't see an easy way to get it
// from this object
int DVBRecorder::GetIDForCardNumber(int cardnum)
{
    int cardid = -1;

    MSqlQuery result(MSqlQuery::InitCon());
    // = db_conn->exec(QString("SELECT cardid FROM capturecard "
                                        "WHERE videodevice=\"%1\" AND "
                                        " cardtype='DVB';").arg(cardnum));

    if (result.isActive() && result.numRowsAffected() > 0)
    {
        result.next();

        cardid = result.value(0).toInt();
    }
    else
        RECORD("Could not get cardid for card number " << cardnum);


    return cardid;
}

*/
