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
#define LOC_ID QString("EITScanner[%1]: ").arg(m_cardnum)

/** \class EITScanner
 *  \brief Acts as glue between ChannelBase, EITSource, and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 *
 */

EITScanner::EITScanner(uint cardnum)
    : m_eitHelper(new EITHelper()), m_eventThread(new MThread("EIT", this)),
      m_cardnum(cardnum)
{
    QStringList langPref = iso639_get_language_list();
    m_eitHelper->SetLanguagePreferences(langPref);

    m_eventThread->start(QThread::IdlePriority);
}

void EITScanner::TeardownAll(void)
{
    StopActiveScan();
    if (!m_exitThread)
    {
        m_lock.lock();
        m_exitThread = true;
        m_exitThreadCond.wakeAll();
        m_lock.unlock();
    }
    m_eventThread->wait();
    delete m_eventThread;
    m_eventThread = nullptr;

    if (m_eitHelper)
    {
        delete m_eitHelper;
        m_eitHelper = nullptr;
    }
}

/**
 *  \brief This runs the event loop for EITScanner until 'exitThread' is true.
 */
void EITScanner::run(void)
{
    static const uint  sz[] = { 2000, 1800, 1600, 1400, 1200, };
    static const float rt[] = { 0.0F, 0.2F, 0.4F, 0.6F, 0.8F, };

    m_lock.lock();

    MythTimer t;
    uint eitCount = 0;

    while (!m_exitThread)
    {
        m_lock.unlock();
        uint list_size = m_eitHelper->GetListSize();

        float rate = 1.0F;
        for (uint i = 0; i < 5; i++)
        {
            if (list_size >= sz[i])
            {
                rate = rt[i];
                break;
            }
        }

        m_lock.lock();
        if (m_eitSource)
            m_eitSource->SetEITRate(rate);
        m_lock.unlock();

        if (list_size)
        {
            eitCount += m_eitHelper->ProcessEvents();
            t.start();
        }

        // Tell the scheduler to run if
        // we are in passive scan
        // and there have been updated events since the last scheduler run
        // but not in the last 60 seconds
        if (!m_activeScan && eitCount && (t.elapsed() > 60 * 1000))
        {
            LOG(VB_EIT, LOG_INFO,
                LOC_ID + QString("Added %1 EIT Events").arg(eitCount));
            eitCount = 0;
            RescheduleRecordings();
        }

        // Is it time to move to the next transport in active scan?
        if (m_activeScan && (MythDate::current() > m_activeScanNextTrig))
        {
            // if there have been any new events, tell scheduler to run.
            if (eitCount)
            {
                LOG(VB_EIT, LOG_INFO,
                    LOC_ID + QString("Added %1 EIT Events").arg(eitCount));
                eitCount = 0;
                RescheduleRecordings();
            }

            if (m_activeScanNextChan == m_activeScanChannels.end())
            {
                m_activeScanNextChan = m_activeScanChannels.begin();
                m_activeScanNextChanIndex = 0;
            }

            if (!(*m_activeScanNextChan).isEmpty())
            {
                m_eitHelper->WriteEITCache();
                if (rec->QueueEITChannelChange(*m_activeScanNextChan))
                {
                    m_eitHelper->SetChannelID(ChannelUtil::GetChanID(
                        rec->GetSourceID(), *m_activeScanNextChan));
                    LOG(VB_EIT, LOG_INFO,
                        LOC_ID + QString("Now looking for EIT data on "
                                         "multiplex of channel %1")
                        .arg(*m_activeScanNextChan));
                }
            }

            m_activeScanNextTrig = MythDate::current()
                .addSecs(m_activeScanTrigTime);
            if (!m_activeScanChannels.empty())
            {
                ++m_activeScanNextChan;
                m_activeScanNextChanIndex =
                    (m_activeScanNextChanIndex+1) % m_activeScanChannels.size();
            }

            // 24 hours ago
#if QT_VERSION < QT_VERSION_CHECK(5,8,0)
            m_eitHelper->PruneEITCache(m_activeScanNextTrig.toTime_t() - 86400);
#else
            m_eitHelper->PruneEITCache(m_activeScanNextTrig.toSecsSinceEpoch() - 86400);
#endif
        }

        m_lock.lock();
        if ((m_activeScan || m_activeScanStopped) && !m_exitThread)
            m_exitThreadCond.wait(&m_lock, 400); // sleep up to 400 ms.

        if (!m_activeScan && !m_activeScanStopped)
        {
            m_activeScanStopped = true;
            m_activeScanCond.wakeAll();
        }
    }

    if (eitCount) /* some events have been handled since the last schedule request */
    {
        RescheduleRecordings();
    }

    m_activeScanStopped = true;
    m_activeScanCond.wakeAll();
    m_lock.unlock();
}

/** \fn EITScanner::RescheduleRecordings(void)
 *  \brief Tells scheduler about programming changes.
 *
 *  This implements some very basic rate limiting. If a call is made
 *  to this within kMinRescheduleInterval of the last call it is ignored.
 */
void EITScanner::RescheduleRecordings(void)
{
    m_eitHelper->RescheduleRecordings();
}

/** \fn EITScanner::StartPassiveScan(ChannelBase*, EITSource*, bool)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartPassiveScan(ChannelBase *channel,
                                  EITSource *eitSource)
{
    QMutexLocker locker(&m_lock);

    m_eitSource   = eitSource;
    m_channel     = channel;

    m_eitSource->SetEITHelper(m_eitHelper);
    m_eitSource->SetEITRate(1.0F);
    m_eitHelper->SetChannelID(m_channel->GetChanID());
    m_eitHelper->SetSourceID(ChannelUtil::GetSourceIDForChannel(m_channel->GetChanID()));

    LOG(VB_EIT, LOG_INFO, LOC_ID + "Started passive scan.");
}

/** \fn EITScanner::StopPassiveScan(void)
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopPassiveScan(void)
{
    QMutexLocker locker(&m_lock);

    if (m_eitSource)
    {
        m_eitSource->SetEITHelper(nullptr);
        m_eitSource  = nullptr;
    }
    m_channel = nullptr;

    m_eitHelper->WriteEITCache();
    m_eitHelper->SetChannelID(0);
    m_eitHelper->SetSourceID(0);
}

void EITScanner::StartActiveScan(TVRec *_rec, uint max_seconds_per_source)
{
    rec = _rec;

    if (m_activeScanChannels.isEmpty())
    {
        // TODO get input name and use it in crawl.
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT channum, MIN(chanid) "
            "FROM channel, capturecard, videosource "
            "WHERE deleted              IS NULL            AND "
            "      capturecard.sourceid = channel.sourceid AND "
            "      videosource.sourceid = channel.sourceid AND "
            "      channel.mplexid        IS NOT NULL      AND "
            "      visible              = 1                AND "
            "      useonairguide        = 1                AND "
            "      useeit               = 1                AND "
            "      channum             != ''               AND "
            "      capturecard.cardid   = :CARDID "
            "GROUP BY mplexid "
            "ORDER BY capturecard.sourceid, mplexid, "
            "         atsc_major_chan, atsc_minor_chan ");
        query.bindValue(":CARDID", rec->GetInputId());

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("EITScanner::StartActiveScan", query);
            return;
        }

        while (query.next())
            m_activeScanChannels.push_back(query.value(0).toString());

        m_activeScanNextChan = m_activeScanChannels.begin();
    }

    LOG(VB_EIT, LOG_INFO, LOC_ID +
        QString("StartActiveScan called with %1 multiplexes")
            .arg(m_activeScanChannels.size()));

    // Start at a random channel. This is so that multiple cards with
    // the same source don't all scan the same channels in the same
    // order when the backend is first started up.
    if (!m_activeScanChannels.empty())
    {
        // The start channel is random.  From now on, start on the
        // next channel.  This makes sure the immediately following
        // channels get scanned in a timely manner if we keep erroring
        // out on the previous channel.
        m_activeScanNextChanIndex =
            (m_activeScanNextChanIndex+1) % m_activeScanChannels.size();
        m_activeScanNextChan =
            m_activeScanChannels.begin() + m_activeScanNextChanIndex;

        m_activeScanNextTrig = MythDate::current();
        m_activeScanTrigTime = max_seconds_per_source;
        // Add a little randomness to trigger time so multiple
        // cards will have a staggered channel changing time.
        m_activeScanTrigTime += random() % 29;
        m_activeScanStopped = false;
        m_activeScan = true;
    }
}

void EITScanner::StopActiveScan(void)
{
    QMutexLocker locker(&m_lock);

    m_activeScanStopped = false;
    m_activeScan = false;
    m_exitThreadCond.wakeAll();

    locker.unlock();
    StopPassiveScan();
    locker.relock();

    while (!m_activeScan && !m_activeScanStopped)
        m_activeScanCond.wait(&m_lock, 100);

    rec = nullptr;
}
