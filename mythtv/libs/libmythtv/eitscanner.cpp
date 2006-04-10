// -*- Mode: c++ -*-

#ifndef USING_DVB
#error USING_DVB must be defined to compile eitscanner.cpp
#endif

#include "tv_rec.h"
#include "dvbchannel.h"
#include "dvbsiparser.h"
#include "dvbtypes.h"

#include "eitscanner.h"
#include "eithelper.h"
#include "scheduledrecording.h"

#define LOC QString("EITScanner: ")

/** \class EITScanner
 *  \brief Acts as glue between DVBChannel, DVBSIParser, and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 *
 */

QMutex     EITScanner::resched_lock;
QDateTime  EITScanner::resched_next_time      = QDateTime::currentDateTime();
const uint EITScanner::kMinRescheduleInterval = 150;

EITScanner::EITScanner()
    : channel(NULL), parser(NULL), eitHelper(new EITHelper()),
      exitThread(false), rec(NULL), activeScan(false)
{
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
    exitThread = false;

    MythTimer t;
    uint eitCount = 0;
    
    while (!exitThread)
    {
        if (channel)
        {
            uint sourceid = channel->GetCurrentSourceID();
            if (sourceid && parser && eitHelper->GetListSize())
            {
                eitCount += eitHelper->ProcessEvents(sourceid, ignore_source);
                t.start();
            }
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

/** \fn EITScanner::StartPassiveScan(DVBChannel*, DVBSIParser*)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(DVBChannel *_channel, DVBSIParser *_parser,
                                  bool _ignore_source)
{
    eitHelper->ClearList();
    parser        = _parser;
    channel       = _channel;
    ignore_source = _ignore_source;

    if (ignore_source)
        VERBOSE(VB_EIT, LOC + "EIT scan ignoring sourceid..");

    parser->SetEITHelper(eitHelper);
}

/** \fn EITScanner::StopPassiveScan(void)
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopPassiveScan(void)
{
    parser->SetEITHelper(NULL);
    eitHelper->ClearList();

    channel = NULL;
    parser  = NULL;
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
