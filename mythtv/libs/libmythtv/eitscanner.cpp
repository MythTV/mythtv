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
      m_cardnum(cardnum)
{
    QStringList langPref = iso639_get_language_list();
    m_eitHelper->SetLanguagePreferences(langPref);

    LOG(VB_EIT, LOG_INFO, LOC + "Start EIT scanner thread");
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

    MythTimer t;
    uint eitCount = 0;

    while (!m_exitThread)
    {
        m_lock.unlock();

        uint list_size = m_eitHelper->GetListSize();
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
                LOC + QString("Added %1 EIT events in passive scan").arg(eitCount));
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
                    LOC + QString("Added %1 EIT events in active scan").arg(eitCount));
                eitCount = 0;
                RescheduleRecordings();
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
                    uint sourceid = m_rec->GetSourceID();
                    uint chanid = ChannelUtil::GetChanID(sourceid, *m_activeScanNextChan);
                    m_eitHelper->SetChannelID(chanid);

                    uint mplexid = ChannelUtil::GetMplexID(chanid);
                    QString sourcename = SourceUtil::GetSourceName(sourceid);
                    LOG(VB_EIT, LOG_INFO, LOC +
                        QString("Next EIT active scan source %1 '%2' multiplex %3 chanid %4 channel %5")
                            .arg(sourceid).arg(sourcename).arg(mplexid).arg(chanid).arg(*m_activeScanNextChan));
                }
            }

            m_activeScanNextTrig = MythDate::current().addSecs(m_activeScanTrigTime.count());
            m_activeScanNextChan++;

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
        uint sourceid = m_channel->GetSourceID();
        uint mplexid = ChannelUtil::GetMplexID(chanid);
        QString sourcename = SourceUtil::GetSourceName(sourceid);
        m_eitHelper->SetChannelID(chanid);
        m_eitHelper->SetSourceID(sourceid);
        LOG(VB_EIT, LOG_INFO, LOC +
            QString("Start EIT %1 scan source %2 '%3' multiplex %4 chanid %5 channel %6")
                .arg(m_activeScan ? "active" : "passive").arg(sourceid).arg(sourcename)
                .arg(mplexid).arg(chanid).arg(channum));
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

    if (m_eitSource)
    {
        m_eitSource->SetEITHelper(nullptr);
        m_eitSource  = nullptr;
    }
    m_channel = nullptr;

    EITHelper::WriteEITCache();
    m_eitHelper->SetChannelID(0);
    m_eitHelper->SetSourceID(0);
    LOG(VB_EIT, LOG_INFO, LOC +
        QString("Stop EIT %1 scan")
            .arg(m_activeScanStopped ? "passive" : "active"));
}

/** \fn EITScanner::StartActiveScan(TVRec *rec, std::chrono::seconds)
 *  \brief Start active EIT scan.
 */
void EITScanner::StartActiveScan(TVRec *rec, std::chrono::seconds max_seconds_per_source)
{
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

        m_activeScanNextChan = m_activeScanChannels.begin();
    }

    {
        uint sourceid = m_rec->GetSourceID();
        QString sourcename = SourceUtil::GetSourceName(sourceid);
        LOG(VB_EIT, LOG_INFO, LOC +
            QString("StartActiveScan for source %1 '%2' with %3 multiplexes")
                .arg(sourceid).arg(sourcename).arg(m_activeScanChannels.size()));
    }

    // Start at a random channel. This is so that multiple cards with
    // the same source don't all scan the same channels in the same
    // order when the backend is first started up.
    if (!m_activeScanChannels.empty())
    {
        // The start channel is random.  From now on, start on the
        // next channel.  This makes sure the immediately following
        // channels get scanned in a timely manner if we keep erroring
        // out on the previous channel.
        auto nextChanIndex = MythRandom() % m_activeScanChannels.size();
        m_activeScanNextChan = m_activeScanChannels.begin() + nextChanIndex;

        // Start scan now
        m_activeScanNextTrig = MythDate::current();

        // Add a little randomness to trigger time so multiple
        // cards will have a staggered channel changing time.
        m_activeScanTrigTime = max_seconds_per_source;
        m_activeScanTrigTime += std::chrono::seconds(MythRandom(0, 28));

        m_activeScanStopped = false;
        m_activeScan = true;
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

    while (!m_activeScan && !m_activeScanStopped)
        m_activeScanCond.wait(&m_lock, 100);

    m_rec = nullptr;
}
