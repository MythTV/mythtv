// -*- Mode: c++ -*-

// POSIX headers
#include <sys/time.h>
#include "libmythbase/compat.h"

// C++
#include <cstdlib>

// MythTV
#include "libmythbase/iso639.h"
#include "libmythbase/mthread.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythrandom.h"
#include "libmythbase/mythtimer.h"

#include "cardutil.h"
#include "channelutil.h"
#include "eithelper.h"
#include "eitscanner.h"
#include "recorders/channelbase.h"
#include "scheduledrecording.h"
#include "sourceutil.h"
#include "tv_rec.h"

#define LOC QString("EITScanner[%1]: ").arg(m_cardnum)

/** \class EITScanner
 *  \brief Acts as glue between ChannelBase, EITSource and EITHelper.
 *
 *   This is the class where the "EIT Crawl" is implemented.
 */
EITScanner::EITScanner(uint cardnum)
    : m_eitHelper(new EITHelper(cardnum)),
      m_eventThread(new MThread("EIT", this)),
      m_cardnum(cardnum),
      m_sourceid(CardUtil::GetSourceID(cardnum))
{
    QStringList langPref = iso639_get_language_list();
    m_eitHelper->SetLanguagePreferences(langPref);

    m_sourceName = SourceUtil::GetSourceName(m_sourceid);
    LOG(VB_EIT, LOG_INFO, LOC +
        QString("Start EIT scanner thread for source %1 '%2'")
            .arg(m_sourceid).arg(m_sourceName));

    m_eventThread->start(QThread::IdlePriority);
}

/** \fn EITScanner::TeardownAll(void)
 *  \brief Stop active scan, delete thread and delete eithelper.
 */
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

/** \fn EITScanner::run(void)
 *  \brief This runs the event loop for EITScanner until 'm_exitThread' is true.
 */
void EITScanner::run(void)
{
    m_lock.lock();

    MythTimer tsle;             // Time since last event or since start of active scan
    MythTimer tsrr;             // Time since RescheduleRecordings in passive scan
    tsrr.start();
    m_eitCount = 0;             // Number of new events processed

    while (!m_exitThread)
    {
        m_lock.unlock();

        uint list_size = m_eitHelper->GetListSize();
        if (list_size)
        {
            m_eitCount += m_eitHelper->ProcessEvents();
            tsle.start();
        }

        // Run the scheduler every 5 minutes if we are in passive scan
        // and there have been new events.
        if (!m_activeScan && m_eitCount && (tsrr.elapsed() > 5min))
        {
            LOG(VB_EIT, LOG_INFO, LOC +
                QString("Added %1 EIT events in passive scan ").arg(m_eitCount) +
                QString("for source %1 '%2'").arg(m_sourceid).arg(m_sourceName));
            m_eitCount = 0;
            RescheduleRecordings();
            tsrr.start();
        }

        // Move to the next transport in active scan when no events
        // have been received for 60 seconds or when the scan time is up.
        const std::chrono::seconds eventTimeout = 60s;
        bool noEvents = tsle.isRunning() && (tsle.elapsed() > eventTimeout);
        bool scanTimeUp = MythDate::current() > m_activeScanNextTrig;
        if (m_activeScan && (noEvents || scanTimeUp))
        {
            if (noEvents && !scanTimeUp)
            {
                LOG(VB_EIT, LOG_INFO, LOC +
                    QString("No new EIT events received in last %1 seconds ").arg(eventTimeout.count()) +
                    QString("for source %2 '%3' ").arg(m_sourceid).arg(m_sourceName) +
                    QString("on multiplex %4, move to next multiplex").arg(m_mplexid));
            }

            if (m_activeScanNextChan == m_activeScanChannels.end())
            {
                m_activeScanNextChan = m_activeScanChannels.begin();
            }

            if (!(*m_activeScanNextChan).isEmpty())
            {
                EITHelper::WriteEITCache();
                if (m_rec->QueueEITChannelChange(*m_activeScanNextChan))
                {
                    uint chanid = ChannelUtil::GetChanID(m_sourceid, *m_activeScanNextChan);
                    m_eitHelper->SetChannelID(chanid);
                    m_mplexid = ChannelUtil::GetMplexID(chanid);
                    LOG(VB_EIT, LOG_DEBUG, LOC +
                        QString("Next EIT active scan source %1 '%2' multiplex %3 chanid %4 channel %5")
                            .arg(m_sourceid).arg(m_sourceName).arg(m_mplexid).arg(chanid).arg(*m_activeScanNextChan));
                }
            }

            m_activeScanNextTrig = MythDate::current().addSecs(m_activeScanTrigTime.count());
            m_activeScanNextChan++;

            tsle.start();

            // Remove all EIT cache entries that are more than 24 hours old
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

    // Some events have been handled since the last schedule request
    if (m_eitCount)
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

/** \fn EITScanner::StartEITEventProcessing(ChannelBase*, EITSource*)
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
    QString channum = m_channel->GetChannelName();
    if (chanid > 0)
    {
        m_mplexid = ChannelUtil::GetMplexID(chanid);
        m_eitHelper->SetChannelID(chanid);
        m_eitHelper->SetSourceID(m_sourceid);
        LOG(VB_EIT, LOG_INFO, LOC +
            QString("Start EIT %1 scan source %2 '%3' multiplex %4 chanid %5 channel %6")
                .arg(m_activeScan ? "active" : "passive").arg(m_sourceid).arg(m_sourceName)
                .arg(m_mplexid).arg(chanid).arg(channum));
    }
    else
    {
        LOG(VB_EIT, LOG_INFO, LOC +
            QString("Failed to start EIT scan, invalid chanid %1 channum %2")
                .arg(chanid).arg(channum));
    }
}

/** \fn EITScanner::StopEITEventProcessing(void)
 *  \brief Stops inserting Event Information Tables into DB.
 */
void EITScanner::StopEITEventProcessing(void)
{
    QMutexLocker locker(&m_lock);

    if (!m_eitSource)
        return;

    LOG(VB_EIT, LOG_INFO, LOC +
        QString("Stop EIT scan source %1 '%2', added %3 EIT events")
            .arg(m_sourceid).arg(m_sourceName).arg(m_eitCount));

    if (m_eitCount)
    {
        m_eitCount = 0;
        RescheduleRecordings();
    }

    if (m_eitSource)
    {
        m_eitSource->SetEITHelper(nullptr);
        m_eitSource  = nullptr;
    }
    m_channel = nullptr;

    EITHelper::WriteEITCache();
    m_eitHelper->SetChannelID(0);
    m_eitHelper->SetSourceID(0);
}

/** \fn EITScanner::StartActiveScan(TVRec *rec, std::chrono::seconds)
 *  \brief Start active EIT scan.
 */
void EITScanner::StartActiveScan(TVRec *rec, std::chrono::seconds max_seconds_per_multiplex)
{
    QMutexLocker locker(&m_lock);

    m_rec = rec;

    if (m_activeScanChannels.isEmpty())
    {
        // Create a channel list with one channel from each multiplex
        // from the video source connected to this input.
        // This list is then used in the EIT crawl so that all
        // available multiplexes are visited in turn.
        MSqlQuery query(MSqlQuery::InitCon());
        query.prepare(
            "SELECT channum, MIN(chanid) "
            "FROM channel, capturecard, videosource "
            "WHERE capturecard.sourceid  = channel.sourceid AND "
            "      videosource.sourceid  = channel.sourceid AND "
            "      channel.deleted       IS NULL            AND "
            "      channel.mplexid       IS NOT NULL        AND "
            "      channel.visible       > 0                AND "
            "      channel.useonairguide = 1                AND "
            "      channel.channum      != ''               AND "
            "      videosource.useeit    = 1                AND "
            "      capturecard.cardid    = :CARDID "
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

        // Start at a random channel. This is so that multiple cards with
        // the same source don't all scan the same channels in the same
        // order when the backend is first started up.
        // Also, from now on, always start on the next channel.
        // This makes sure the immediately following channels get scanned
        // in a timely manner if we keep erroring out on the previous channel.

        if (!m_activeScanChannels.empty())
        {
            auto nextChanIndex = MythRandom() % m_activeScanChannels.size();
            m_activeScanNextChan = m_activeScanChannels.begin() + nextChanIndex;
        }
        uint sourceid = m_rec->GetSourceID();
        QString sourcename = SourceUtil::GetSourceName(sourceid);
        LOG(VB_EIT, LOG_INFO, LOC +
            QString("EIT scan channel table for source %1 '%2' with %3 multiplexes")
                .arg(sourceid).arg(sourcename).arg(m_activeScanChannels.size()));
    }

    if (!m_activeScanChannels.empty())
    {
        // Start scan now
        m_activeScanNextTrig = MythDate::current();

        // Add a little randomness to trigger time so multiple
        // cards will have a staggered channel changing time.
        m_activeScanTrigTime = max_seconds_per_multiplex;
        m_activeScanTrigTime += std::chrono::seconds(MythRandom(0, 28));

        m_activeScanStopped = false;
        m_activeScan = true;

        LOG(VB_EIT, LOG_INFO, LOC +
            QString("Start EIT active scan on source %1 '%2'")
                .arg(m_sourceid).arg(m_sourceName));
    }
}

/** \fn EITScanner::StopActiveScan(void)
 *  \brief Stop active EIT scan.
 */
void EITScanner::StopActiveScan(void)
{
    QMutexLocker locker(&m_lock);

    m_activeScanStopped = false;
    m_activeScan = false;
    m_exitThreadCond.wakeAll();

    locker.unlock();
    StopEITEventProcessing();
    locker.relock();

    LOG(VB_EIT, LOG_INFO, LOC +
        QString("Stop EIT active scan on source %1 '%2'")
            .arg(m_sourceid).arg(m_sourceName));

    while (!m_activeScan && !m_activeScanStopped)
        m_activeScanCond.wait(&m_lock, 100);

    m_rec = nullptr;
}
