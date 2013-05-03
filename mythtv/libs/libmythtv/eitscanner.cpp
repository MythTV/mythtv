// -*- Mode: c++ -*-

// POSIX headers
#include <sys/time.h>
#include "compat.h"

#include <cstdlib>

#include "scheduledrecording.h"
#include "channelbase.h"
#include "channelutil.h"
#include "mythlogging.h"
#include "eitscanner.h"
#include "eithelper.h"
#include "mythtimer.h"
#include "mythdate.h"
#include "mthread.h"
#include "iso639.h"
#include "mythdb.h"
#include "tv_rec.h"

#define LOC QString("EITScanner: ")
#define LOC_ID QString("EITScanner (%1): ").arg(cardnum)

/** \class EITScanner
 *  \brief Acts as glue between ChannelBase, EITSource, and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 *
 */

EITScanner::EITScanner(uint _cardnum)
    : channel(NULL),              eitSource(NULL),
      eitHelper(new EITHelper()), eventThread(new MThread("EIT", this)),
      exitThread(false),
      rec(NULL),                  activeScan(false),
      activeScanStopped(true),    activeScanTrigTime(0),
      cardnum(_cardnum)
{
    QStringList langPref = iso639_get_language_list();
    eitHelper->SetLanguagePreferences(langPref);

    eventThread->start(QThread::IdlePriority);
}

void EITScanner::TeardownAll(void)
{
    StopActiveScan();
    if (!exitThread)
    {
        lock.lock();
        exitThread = true;
        exitThreadCond.wakeAll();
        lock.unlock();
    }
    eventThread->wait();
    delete eventThread;
    eventThread = NULL;

    if (eitHelper)
    {
        delete eitHelper;
        eitHelper = NULL;
    }
}

/**
 *  \brief This runs the event loop for EITScanner until 'exitThread' is true.
 */
void EITScanner::run(void)
{
    static const uint  sz[] = { 2000, 1800, 1600, 1400, 1200, };
    static const float rt[] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, };

    lock.lock();

    MythTimer t;
    uint eitCount = 0;

    while (!exitThread)
    {
        lock.unlock();
        uint list_size = eitHelper->GetListSize();

        float rate = 1.0f;
        for (uint i = 0; i < 5; i++)
        {
            if (list_size >= sz[i])
            {
                rate = rt[i];
                break;
            }
        }

        lock.lock();
        if (eitSource)
            eitSource->SetEITRate(rate);
        lock.unlock();

        if (list_size)
        {
            eitCount += eitHelper->ProcessEvents();
            t.start();
        }

        // Tell the scheduler to run if
        // we are in passive scan
        // and there have been updated events since the last scheduler run
        // but not in the last 60 seconds
        if (!activeScan && eitCount && (t.elapsed() > 60 * 1000))
        {
            LOG(VB_EIT, LOG_INFO,
                LOC_ID + QString("Added %1 EIT Events").arg(eitCount));
            eitCount = 0;
            RescheduleRecordings();
        }

        // Is it time to move to the next transport in active scan?
        if (activeScan && (MythDate::current() > activeScanNextTrig))
        {
            // if there have been any new events, tell scheduler to run.
            if (eitCount)
            {
                LOG(VB_EIT, LOG_INFO,
                    LOC_ID + QString("Added %1 EIT Events").arg(eitCount));
                eitCount = 0;
                RescheduleRecordings();
            }

            if (activeScanNextChan == activeScanChannels.end())
                activeScanNextChan = activeScanChannels.begin();

            if (!(*activeScanNextChan).isEmpty())
            {
                eitHelper->WriteEITCache();
                if (rec->QueueEITChannelChange(*activeScanNextChan))
                {
                    eitHelper->SetChannelID(ChannelUtil::GetChanID(
                        rec->GetSourceID(), *activeScanNextChan));
                    LOG(VB_EIT, LOG_INFO,
                        LOC_ID + QString("Now looking for EIT data on "
                                         "multiplex of channel %1")
                        .arg(*activeScanNextChan));
                }
            }

            activeScanNextTrig = MythDate::current()
                .addSecs(activeScanTrigTime);
            ++activeScanNextChan;

            // 24 hours ago
            eitHelper->PruneEITCache(activeScanNextTrig.toTime_t() - 86400);
        }

        lock.lock();
        if ((activeScan || activeScanStopped) && !exitThread)
            exitThreadCond.wait(&lock, 400); // sleep up to 400 ms.

        if (!activeScan && !activeScanStopped)
        {
            activeScanStopped = true;
            activeScanCond.wakeAll();
        }
    }

    if (eitCount) /* some events have been handled since the last schedule request */
    {
        eitCount = 0;
        RescheduleRecordings();
    }

    activeScanStopped = true;
    activeScanCond.wakeAll();
    lock.unlock();
}

/** \fn EITScanner::RescheduleRecordings(void)
 *  \brief Tells scheduler about programming changes.
 *
 *  This implements some very basic rate limiting. If a call is made
 *  to this within kMinRescheduleInterval of the last call it is ignored.
 */
void EITScanner::RescheduleRecordings(void)
{
    eitHelper->RescheduleRecordings();
}

/** \fn EITScanner::StartPassiveScan(ChannelBase*, EITSource*, bool)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(ChannelBase *_channel,
                                  EITSource *_eitSource)
{
    QMutexLocker locker(&lock);

    eitSource     = _eitSource;
    channel       = _channel;

    eitSource->SetEITHelper(eitHelper);
    eitSource->SetEITRate(1.0f);
    eitHelper->SetChannelID(channel->GetChanID());
    eitHelper->SetSourceID(ChannelUtil::GetSourceIDForChannel(channel->GetChanID()));

    LOG(VB_EIT, LOG_INFO, LOC_ID + "Started passive scan.");
}

/** \fn EITScanner::StopPassiveScan(void)
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopPassiveScan(void)
{
    QMutexLocker locker(&lock);

    if (eitSource)
    {
        eitSource->SetEITHelper(NULL);
        eitSource  = NULL;
    }
    channel = NULL;

    eitHelper->WriteEITCache();
    eitHelper->SetChannelID(0);
    eitHelper->SetSourceID(0);
}

void EITScanner::StartActiveScan(TVRec *_rec, uint max_seconds_per_source)
{
    rec = _rec;

    if (activeScanChannels.isEmpty())
    {
        // TODO get input name and use it in crawl.
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT channum, MIN(chanid) "
            "FROM channel, cardinput, capturecard, videosource "
            "WHERE cardinput.sourceid   = channel.sourceid AND "
            "      videosource.sourceid = channel.sourceid AND "
            "      capturecard.cardid   = cardinput.cardid AND "
            "      channel.mplexid        IS NOT NULL      AND "
            "      useonairguide        = 1                AND "
            "      useeit               = 1                AND "
            "      channum             != ''               AND "
            "      cardinput.cardid     = :CARDID "
            "GROUP BY mplexid "
            "ORDER BY cardinput.sourceid, mplexid, "
            "         atsc_major_chan, atsc_minor_chan ");
        query.bindValue(":CARDID", rec->GetCaptureCardNum());

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("EITScanner::StartActiveScan", query);
            return;
        }

        while (query.next())
            activeScanChannels.push_back(query.value(0).toString());

        activeScanNextChan = activeScanChannels.begin();
    }

    LOG(VB_EIT, LOG_INFO, LOC_ID +
        QString("StartActiveScan called with %1 multiplexes")
            .arg(activeScanChannels.size()));

    // Start at a random channel. This is so that multiple cards with
    // the same source don't all scan the same channels in the same
    // order when the backend is first started up.
    if (activeScanChannels.size())
    {
        uint randomStart = random() % activeScanChannels.size();
        activeScanNextChan = activeScanChannels.begin()+randomStart;

        activeScanNextTrig = MythDate::current();
        activeScanTrigTime = max_seconds_per_source;
        // Add a little randomness to trigger time so multiple
        // cards will have a staggered channel changing time.
        activeScanTrigTime += random() % 29;
        activeScanStopped = false;
        activeScan = true;
    }
}

void EITScanner::StopActiveScan(void)
{
    QMutexLocker locker(&lock);

    activeScanStopped = false;
    activeScan = false;
    exitThreadCond.wakeAll();

    locker.unlock();
    StopPassiveScan();
    locker.relock();

    while (!activeScan && !activeScanStopped)
        activeScanCond.wait(&lock, 100);

    rec = NULL;
}
