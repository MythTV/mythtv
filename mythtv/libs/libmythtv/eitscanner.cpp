// -*- Mode: c++ -*-

#include "tv_rec.h"

#include "channelbase.h"
#include "iso639.h"
#include "eitscanner.h"
#include "eithelper.h"
#include "scheduledrecording.h"

#define LOC QString("EITScanner: ")

/** \class EITScanner
 *  \brief Acts as glue between ChannelBase, EITSource, and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 *
 */

QMutex     EITScanner::resched_lock;
QDateTime  EITScanner::resched_next_time      = QDateTime::currentDateTime();
const uint EITScanner::kMinRescheduleInterval = 150;

EITScanner::EITScanner()
    : channel(NULL), eitSource(NULL), eitHelper(new EITHelper()),
      exitThread(false), rec(NULL), activeScan(false)
{
    QStringList langPref = iso639_get_language_list();
    eitHelper->SetLanguagePreferences(langPref);

    pthread_create(&eventThread, NULL, SpawnEventLoop, this);

    // Lower scheduling priority, to avoid problems with recordings.
    struct sched_param sp = {19 /* very low priority */};
    pthread_setschedparam(eventThread, SCHED_OTHER, &sp);
}

void EITScanner::TeardownAll(void)
{
    StopActiveScan();
    if (!exitThread)
    {
        exitThread = true;
        exitThreadCond.wakeAll();
        pthread_join(eventThread, NULL);
    }

    if (eitHelper)
    {
        delete eitHelper;
        eitHelper = NULL;
    }
}

/** \fn EITScanner::SpawnEventLoop(void*)
 *  \brief Thunk that allows scanner_thread pthread to
 *         call EITScanner::RunScanner().
 */
void *EITScanner::SpawnEventLoop(void *param)
{
    EITScanner *scanner = (EITScanner*) param;
    scanner->RunEventLoop();
    return NULL;
}

/** \fn EITScanner::RunEventLoop()
 *  \brief This runs the event loop for EITScanner until 'exitThread' is true.
 */
void EITScanner::RunEventLoop(void)
{
    static const uint  sz[] = { 2000, 1800, 1600, 1400, 1200, };
    static const float rt[] = { 0.0f, 0.2f, 0.4f, 0.6f, 0.8f, };

    exitThread = false;

    MythTimer t;
    uint eitCount = 0;
    
    while (!exitThread)
    {
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

        // If there have been any new events and we haven't
        // seen any in a while, tell scheduler to run.
        if (eitCount && (t.elapsed() > 60 * 1000))
        {
            VERBOSE(VB_GENERAL, LOC + "Added "<<eitCount<<" EIT Events");
            eitCount = 0;
            RescheduleRecordings();
        }

        if (activeScan && (QDateTime::currentDateTime() > activeScanNextTrig))
        {
            // if there have been any new events, tell scheduler to run.
            if (eitCount)
            {
                VERBOSE(VB_GENERAL, LOC + "Added "<<eitCount<<" EIT Events");
                eitCount = 0;
                RescheduleRecordings();
            }

            if (activeScanNextChan == activeScanChannels.end())
                activeScanNextChan = activeScanChannels.begin();
 
            if (!(*activeScanNextChan).isEmpty())
            {
                rec->SetChannel(*activeScanNextChan, TVRec::kFlagEITScan);
                VERBOSE(VB_GENERAL, LOC + 
                        QString("Now looking for EIT data on "
                                "multiplex of channel %1")
                        .arg(*activeScanNextChan));
            }

            activeScanNextTrig = QDateTime::currentDateTime()
                .addSecs(activeScanTrigTime);
            activeScanNextChan++;
        }

        exitThreadCond.wait(200); // sleep up to 200 ms.
    }
}

/** \fn EITScanner::RescheduleRecordings(void)
 *  \brief Tells scheduler about programming changes.
 *
 *  This implements some very basic rate limiting. If a call is made
 *  to this within kMinRescheduleInterval of the last call it is ignored.
 */
void EITScanner::RescheduleRecordings(void)
{
    if (!resched_lock.tryLock())
        return;

    if (resched_next_time > QDateTime::currentDateTime())
    {
        VERBOSE(VB_EIT, LOC + "Rate limiting reschedules..");
        resched_lock.unlock();
        return;
    }

    resched_next_time =
        QDateTime::currentDateTime().addSecs(kMinRescheduleInterval);
    resched_lock.unlock();

    ScheduledRecording::signalChange(-1);
}

/** \fn EITScanner::StartPassiveScan(ChannelBase*, EITSource*, bool)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(ChannelBase *_channel,
                                  EITSource *_eitSource,
                                  bool _ignore_source)
{
    QMutexLocker locker(&lock);
    
    uint sourceid = (_ignore_source) ? 0 : _channel->GetCurrentSourceID();
    eitSource     = _eitSource;
    channel       = _channel;
    ignore_source = _ignore_source;

    if (ignore_source)
        VERBOSE(VB_EIT, LOC + "EIT scan ignoring sourceid..");

    eitHelper->SetSourceID(sourceid);
    eitSource->SetEITHelper(eitHelper);
    eitSource->SetEITRate(1.0f);
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

    eitHelper->SetSourceID(0);
}

void EITScanner::StartActiveScan(TVRec *_rec, uint max_seconds_per_source,
                                 bool _ignore_source)
{
    rec           = _rec;
    ignore_source = _ignore_source;

    if (!activeScanChannels.size())
    {
        // TODO get input name and use it in crawl.
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT min(channum) "
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
            "ORDER BY cardinput.sourceid, atscsrcid, mplexid");
        query.bindValue(":CARDID", rec->GetCaptureCardNum());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("EITScanner::StartActiveScan", query);
            return;
        }

        while (query.next())
            activeScanChannels.push_back(query.value(0).toString());

        activeScanNextChan = activeScanChannels.begin();
    }

    VERBOSE(VB_EIT, LOC +
            QString("StartActiveScan called with %1 multiplexes")
            .arg(activeScanChannels.size()));

    // Start at a random channel. This is so that multiple cards with
    // the same source don't all scan the same channels in the same 
    // order when the backend is first started up.
    if (activeScanChannels.size())
    {
        uint randomStart = random() % activeScanChannels.size();
        activeScanNextChan = activeScanChannels.at(randomStart);

        activeScanNextTrig = QDateTime::currentDateTime();
        activeScanTrigTime = max_seconds_per_source;
        // Add a little randomness to trigger time so multiple 
        // cards will have a staggered channel changing time.
        activeScanTrigTime += random() % 29;
        activeScan = true;
    }
}

void EITScanner::StopActiveScan()
{
    activeScan = false;
    rec = NULL;
    StopPassiveScan();
}
