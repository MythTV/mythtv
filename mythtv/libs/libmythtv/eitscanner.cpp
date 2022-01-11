// -*- Mode: c++ -*-

// POSIX headers
#include <sys/time.h>
#include "compat.h"

// C++
#include <cstdlib>

// MythTV
#include "scheduledrecording.h"
#include "channelbase.h"
#include "channelutil.h"
#include "mythlogging.h"
#include "eitscanner.h"
#include "eithelper.h"
#include "mythtimer.h"
#include "mythdate.h"
#include "mythrandom.h"
#include "mthread.h"
#include "iso639.h"
#include "mythdb.h"
#include "tv_rec.h"

#define LOC QString("EITScanner: ")
#define LOC_ID QString("EITScanner[%1]: ").arg(m_cardnum)

/** \class EITScanner
 *  \brief Acts as glue between ChannelBase, EITSource and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 *
 */
EITScanner::EITScanner(uint cardnum)
    : m_eitHelper(new EITHelper(cardnum)),
      m_eventThread(new MThread("EIT", this)),
      m_activeScanNextChanIndex(MythRandom()),
      m_cardnum(cardnum)
{
    QStringList langPref = iso639_get_language_list();
    m_eitHelper->SetLanguagePreferences(langPref);

    LOG(VB_EIT, LOG_INFO, LOC_ID + "Start EIT scanner thread");
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
    m_lock.lock();

    MythTimer t;
    uint eitCount = 0;

    while (!m_exitThread)
    {
        m_lock.unlock();
        uint list_size = m_eitHelper->GetListSize();

        m_lock.lock();
        if (m_eitSource)
            m_eitSource->SetEITRate(1.0F);
        m_lock.unlock();

        if (list_size)
        {
            eitCount += m_eitHelper->ProcessEvents();
            t.start();
        }

        // Tell the scheduler to run if we are in passive scan
        // and there have been updated events since the last scheduler run
        // but not in the last 60 seconds
        if (!m_activeScan && eitCount && (t.elapsed() > 60s))
        {
            LOG(VB_EIT, LOG_INFO,
                LOC_ID + QString("Added %1 EIT events in passive scan").arg(eitCount));
            eitCount = 0;
            RescheduleRecordings();
        }

        // Is it time to move to the next transport in active scan?
        if (m_activeScan && (MythDate::current() > m_activeScanNextTrig))
        {
            // If there have been any new events, tell scheduler to run.
            if (eitCount)
            {
                LOG(VB_EIT, LOG_INFO,
                    LOC_ID + QString("Added %1 EIT events in active scan").arg(eitCount));
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
                EITHelper::WriteEITCache();
                if (m_rec->QueueEITChannelChange(*m_activeScanNextChan))
                {
                    m_eitHelper->SetChannelID(ChannelUtil::GetChanID(
                        m_rec->GetSourceID(), *m_activeScanNextChan));
                    LOG(VB_EIT, LOG_INFO, LOC_ID +
                        QString("Next looking for EIT data on multiplex of channel %1 of source %2")
                        .arg(*m_activeScanNextChan).arg(m_rec->GetSourceID()));
                }
            }

            m_activeScanNextTrig = MythDate::current()
                .addSecs(m_activeScanTrigTime.count());
            if (!m_activeScanChannels.empty())
            {
                ++m_activeScanNextChan;
                m_activeScanNextChanIndex =
                    (m_activeScanNextChanIndex+1) % m_activeScanChannels.size();
            }

            // 24 hours ago
            EITHelper::PruneEITCache(m_activeScanNextTrig.toSecsSinceEpoch() - 86400);
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
 */
void EITScanner::RescheduleRecordings(void)
{
    m_eitHelper->RescheduleRecordings();
}

/** \fn EITScanner::StartEITEventProcessing(ChannelBase*, EITSource*, bool)
 *  \brief Start inserting Event Information Tables from the multiplex
 *         we happen to be tuned to into the database.
 */
void EITScanner::StartEITEventProcessing(ChannelBase *channel,
                                         EITSource *eitSource)
{
    QMutexLocker locker(&m_lock);

    m_eitSource   = eitSource;
    m_channel     = channel;

    m_eitSource->SetEITHelper(m_eitHelper);
    m_eitSource->SetEITRate(1.0F);
    int chanid = m_channel->GetChanID();
    if (chanid > 0)
    {
        m_eitHelper->SetChannelID(chanid);
        m_eitHelper->SetSourceID(ChannelUtil::GetSourceIDForChannel(chanid));
        LOG(VB_EIT, LOG_INFO, LOC_ID +
            QString("Start processing EIT events in %1 scan for channel %2 chanid %3")
                .arg(m_activeScan ? "active" : "passive",
                     m_channel->GetChannelName(),
                     QString::number(chanid)));
    }
    else
    {
        LOG(VB_EIT, LOG_INFO, LOC_ID +
            QString("Failed to start processing EIT events, invalid chanid:%1")
                .arg(chanid));
    }
}

/** \fn EITScanner::StopEITEventProcessing(void)
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopEITEventProcessing(void)
{
    QMutexLocker locker(&m_lock);

    if (m_eitSource)
    {
        m_eitSource->SetEITHelper(nullptr);
        m_eitSource  = nullptr;
    }
    m_channel = nullptr;

    EITHelper::WriteEITCache();
    m_eitHelper->SetChannelID(0);
    m_eitHelper->SetSourceID(0);
    LOG(VB_EIT, LOG_INFO, LOC_ID +
        QString("Stop processing EIT events in %1 scan")
            .arg(m_activeScanStopped ? "passive" : "active"));
}

void EITScanner::StartActiveScan(TVRec *rec, std::chrono::seconds max_seconds_per_source)
{
    m_rec = rec;

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
            "      visible              > 0                AND "
            "      useonairguide        = 1                AND "
            "      useeit               = 1                AND "
            "      channum             != ''               AND "
            "      capturecard.cardid   = :CARDID "
            "GROUP BY mplexid "
            "ORDER BY capturecard.sourceid, mplexid, "
            "         atsc_major_chan, atsc_minor_chan ");
        query.bindValue(":CARDID", m_rec->GetInputId());

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
        m_activeScanTrigTime += std::chrono::seconds(MythRandom(0, 28));

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
    StopEITEventProcessing();
    locker.relock();

    while (!m_activeScan && !m_activeScanStopped)
        m_activeScanCond.wait(&m_lock, 100);

    m_rec = nullptr;
}
