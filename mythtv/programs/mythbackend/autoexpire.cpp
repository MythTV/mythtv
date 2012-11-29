// System headers
#include <sys/stat.h>
#ifdef __linux__
#  include <sys/vfs.h>
#else // if !__linux__
#  include <sys/param.h>
#  ifndef USING_MINGW
#    include <sys/mount.h>
#  endif // USING_MINGW
#endif // !__linux__

// POSIX headers
#include <unistd.h>

// C headers
#include <cstdlib>

// C++ headers
#include <iostream>
#include <algorithm>
using namespace std;

// Qt headers
#include <QDateTime>
#include <QFileInfo>
#include <QList>

// MythTV headers
#include "filesysteminfo.h"
#include "autoexpire.h"
#include "programinfo.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythdate.h"
#include "storagegroup.h"
#include "remoteutil.h"
#include "remoteencoder.h"
#include "encoderlink.h"
#include "backendutil.h"
#include "mainserver.h"
#include "compat.h"
#include "mythlogging.h"

#define LOC     QString("AutoExpire: ")
#define LOC_ERR QString("AutoExpire Error: ")

extern AutoExpire *expirer;

/** If calculated desired space for 10 min freq is > SPACE_TOO_BIG_KB
 *  then we use 5 min expire frequency.
 */
#define SPACE_TOO_BIG_KB 3*1024*1024

/// \brief This calls AutoExpire::RunExpirer() from within a new thread.
void ExpireThread::run(void)
{
    RunProlog();
    m_parent->RunExpirer();
    RunEpilog();
}

/// \brief This calls AutoExpire::RunUpdate() from within a new thread.
void UpdateThread::run(void)
{
    RunProlog();
    m_parent->RunUpdate();
    RunEpilog();
}

/** \class AutoExpire
 *  \brief Used to expire recordings to make space for new recordings.
 */

/** \fn AutoExpire::AutoExpire(QMap<int, EncoderLink *> *tvList)
 *  \brief Creates AutoExpire class, starting the thread.
 *
 *  \param tvList    EncoderLink list of all recorders
 */
AutoExpire::AutoExpire(QMap<int, EncoderLink *> *tvList) :
    encoderList(tvList),
    expire_thread(new ExpireThread(this)),
    desired_freq(15),
    expire_thread_run(true),
    main_server(NULL),
    update_pending(false),
    update_thread(NULL)
{
    expire_thread->start();
    gCoreContext->addListener(this);
}

/** \fn AutoExpire::AutoExpire()
 *  \brief Creates AutoExpire class
 */
AutoExpire::AutoExpire() :
    encoderList(NULL),
    expire_thread(NULL),
    desired_freq(15),
    expire_thread_run(false),
    main_server(NULL),
    update_pending(false),
    update_thread(NULL)
{
}

/** \fn AutoExpire::~AutoExpire()
 *  \brief AutoExpire destructor stops auto delete thread if it is running.
 */
AutoExpire::~AutoExpire()
{
    {
        QMutexLocker locker(&instance_lock);
        expire_thread_run = false;
        instance_cond.wakeAll();
    }

    {
        QMutexLocker locker(&instance_lock);
        while (update_pending)
            instance_cond.wait(&instance_lock);
    }

    if (expire_thread)
    {
        gCoreContext->removeListener(this);
        expire_thread->wait();
        delete expire_thread;
        expire_thread = NULL;
    }
}

/**
 *   \brief  Used by the scheduler to select the next recording dir
 *   \return the desired free space for each file system
 */

uint64_t AutoExpire::GetDesiredSpace(int fsID) const
{
    QMutexLocker locker(&instance_lock);
    if (desired_space.contains(fsID))
        return desired_space[fsID];
    return 0;
}

/** \fn AutoExpire::CalcParams()
 *   Calculates how much space needs to be cleared, and how often.
 */
void AutoExpire::CalcParams()
{
    LOG(VB_FILE, LOG_INFO, LOC + "CalcParams()");

    QList<FileSystemInfo> fsInfos;

    instance_lock.lock();
    if (main_server)
        main_server->GetFilesystemInfos(fsInfos);
    instance_lock.unlock();

    if (fsInfos.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Filesystem Info cache is empty, unable "                                       "to calculate necessary parameters.");

        return;
    }

    uint64_t maxKBperMin = 0;
    uint64_t extraKB = static_cast<uint64_t>
                        (gCoreContext->GetNumSetting("AutoExpireExtraSpace", 0))
                          << 20;

    QMap<int, uint64_t> fsMap;
    QMap<int, vector<int> > fsEncoderMap;

    // we use this copying on purpose. The used_encoders map ensures
    // that every encoder writes only to one fs.
    // Copying the data minimizes the time the lock is held
    instance_lock.lock();
    QMap<int, int>::const_iterator ueit = used_encoders.begin();
    while (ueit != used_encoders.end())
    {
        fsEncoderMap[*ueit].push_back(ueit.key());
        ++ueit;
    }
    instance_lock.unlock();

    QList<FileSystemInfo>::iterator fsit;
    for (fsit = fsInfos.begin(); fsit != fsInfos.end(); ++fsit)
    {
        if (fsMap.contains(fsit->getFSysID()))
            continue;

        fsMap[fsit->getFSysID()] = 0;
        uint64_t thisKBperMin = 0;

        // append unknown recordings to all fsIDs
        vector<int>::iterator unknownfs_it = fsEncoderMap[-1].begin();
        for (; unknownfs_it != fsEncoderMap[-1].end(); ++unknownfs_it)
            fsEncoderMap[fsit->getFSysID()].push_back(*unknownfs_it);

        if (fsEncoderMap.contains(fsit->getFSysID()))
        {
            LOG(VB_FILE, LOG_INFO,
                QString("fsID #%1: Total: %2 GB   Used: %3 GB   Free: %4 GB")
                    .arg(fsit->getFSysID())
                .arg(fsit->getTotalSpace() / 1024.0 / 1024.0, 7, 'f', 1)
                .arg(fsit->getUsedSpace() / 1024.0 / 1024.0, 7, 'f', 1)
                .arg(fsit->getFreeSpace() / 1024.0 / 1024.0, 7, 'f', 1));

            vector<int>::iterator encit =
                fsEncoderMap[fsit->getFSysID()].begin();
            for (; encit != fsEncoderMap[fsit->getFSysID()].end(); ++encit)
            {
                EncoderLink *enc = *(encoderList->find(*encit));

                if (!enc->IsConnected() || !enc->IsBusy())
                {
                    // remove encoder since it can't write to any file system
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Cardid %1: is not recoding, removing it "
                                "from used list.").arg(*encit));
                    instance_lock.lock();
                    used_encoders.remove(*encit);
                    instance_lock.unlock();
                    continue;
                }

                uint64_t maxBitrate = enc->GetMaxBitrate();
                if (maxBitrate<=0)
                    maxBitrate = 19500000LL;
                thisKBperMin += (((uint64_t)maxBitrate)*((uint64_t)15))>>11;
                LOG(VB_FILE, LOG_INFO, QString("    Cardid %1: max bitrate "
                        "%2 Kb/sec, fsID %3 max is now %4 KB/min")
                        .arg(enc->GetCardID())
                        .arg(enc->GetMaxBitrate() >> 10)
                        .arg(fsit->getFSysID())
                        .arg(thisKBperMin));
            }
        }
        fsMap[fsit->getFSysID()] = thisKBperMin;

        if (thisKBperMin > maxKBperMin)
        {
            LOG(VB_FILE, LOG_INFO,
                QString("  Max of %1 KB/min for fsID %2 is higher "
                    "than the existing Max of %3 so we'll use this Max instead")
                    .arg(thisKBperMin).arg(fsit->getFSysID()).arg(maxKBperMin));
            maxKBperMin = thisKBperMin;
        }
    }

    // Determine frequency to run autoexpire so it doesn't have to free
    // too much space
    uint expireFreq = 15;
    if (maxKBperMin > 0)
    {
        expireFreq = SPACE_TOO_BIG_KB / (maxKBperMin + maxKBperMin/3);
        expireFreq = max(3U, min(expireFreq, 15U));
    }

    double expireMinGB = ((maxKBperMin + maxKBperMin/3)
                          * expireFreq + extraKB) >> 20;
    LOG(VB_GENERAL, LOG_NOTICE, LOC +
        QString("CalcParams(): Max required Free Space: %1 GB w/freq: %2 min")
            .arg(expireMinGB, 0, 'f', 1).arg(expireFreq));

    // lock class and save these parameters.
    instance_lock.lock();
    desired_freq = expireFreq;
    // write per file system needed space back, use safety of 33%
    QMap<int, uint64_t>::iterator it = fsMap.begin();
    while (it != fsMap.end())
    {
        desired_space[it.key()] = (*it + *it/3) * expireFreq + extraKB;
        ++it;
    }
    instance_lock.unlock();
}

/** \brief This contains the main loop for the auto expire process.
 *
 *   Responsible for cleanup of old LiveTV programs as well as deleting as
 *   many recordings that are expirable as necessary to
 *   maintain enough free space on all directories in MythTV Storage Groups.
 *   The thread deletes short LiveTV programs every 2 minutes and long
 *   LiveTV and regular programs as needed every "desired_freq" minutes.
 */
void AutoExpire::RunExpirer(void)
{
    QTime timer;
    QDateTime curTime;
    QDateTime next_expire = MythDate::current().addSecs(60);

    QMutexLocker locker(&instance_lock);

    // wait a little for main server to come up and things to settle down
    Sleep(20 * 1000);

    timer.start();

    while (expire_thread_run)
    {
        curTime = MythDate::current();
        // recalculate auto expire parameters
        if (curTime >= next_expire)
        {
            locker.unlock();
            CalcParams();
            locker.relock();
            if (!expire_thread_run)
                break;
        }
        timer.restart();

        UpdateDontExpireSet();

        // Expire Short LiveTV files for this backend every 2 minutes
        if ((curTime.time().minute() % 2) == 0)
            ExpireLiveTV(emShortLiveTVPrograms);

        // Expire normal recordings depending on frequency calculated
        if (curTime >= next_expire)
        {
            LOG(VB_FILE, LOG_INFO, LOC + "Running now!");
            next_expire =
                MythDate::current().addSecs(desired_freq * 60);

            ExpireLiveTV(emNormalLiveTVPrograms);

            int maxAge = gCoreContext->GetNumSetting("DeletedMaxAge", 0);
            if (maxAge > 0)
                ExpireOldDeleted();
            else if (maxAge == 0)
                ExpireQuickDeleted();

            ExpireEpisodesOverMax();

            ExpireRecordings();
        }

        Sleep(60 * 1000 - timer.elapsed());
    }
}

/** \fn AutoExpire::Sleep(int sleepTime)
 *  \brief Sleeps for sleepTime milliseconds; unless the expire thread
 *         is told to quit. Must be called with instance_lock held.
 *
 *  \note Will release instance_lock!
 */
void AutoExpire::Sleep(int sleepTime)
{
    if (sleepTime <= 0)
        return;

    QDateTime little_tm = MythDate::current().addMSecs(sleepTime);
    int timeleft = sleepTime;
    while (expire_thread_run && (timeleft > 0))
    {
        instance_cond.wait(&instance_lock, timeleft);
        timeleft = MythDate::current().secsTo(little_tm) * 1000;
    }
}

/** \fn AutoExpire::ExpireLiveTV(int type)
 *  \brief This expires LiveTV programs.
 */
void AutoExpire::ExpireLiveTV(int type)
{
    pginfolist_t expireList;

    LOG(VB_FILE, LOG_INFO, LOC + QString("ExpireLiveTV(%1)").arg(type));
    FillDBOrdered(expireList, type);
    SendDeleteMessages(expireList);
    ClearExpireList(expireList);
}

/** \fn AutoExpire::ExpireOldDeleted(void)
 *  \brief This expires deleted programs older than DeletedMaxAge.
 */
void AutoExpire::ExpireOldDeleted(void)
{
    pginfolist_t expireList;

    LOG(VB_FILE, LOG_INFO, LOC + QString("ExpireOldDeleted()"));
    FillDBOrdered(expireList, emOldDeletedPrograms);
    SendDeleteMessages(expireList);
    ClearExpireList(expireList);
}

/**
 *  \brief This expires deleted programs within a few minutes
 */
void AutoExpire::ExpireQuickDeleted(void)
{
    pginfolist_t expireList;

    LOG(VB_FILE, LOG_INFO, LOC + QString("ExpireQuickDeleted()"));
    FillDBOrdered(expireList, emQuickDeletedPrograms);
    SendDeleteMessages(expireList);
    ClearExpireList(expireList);
}

/** \fn AutoExpire::ExpireRecordings()
 *  \brief This expires normal recordings.
 *
 */
void AutoExpire::ExpireRecordings(void)
{
    pginfolist_t expireList;
    pginfolist_t deleteList;
    QList<FileSystemInfo> fsInfos;
    QList<FileSystemInfo>::iterator fsit;

    LOG(VB_FILE, LOG_INFO, LOC + "ExpireRecordings()");

    if (main_server)
        main_server->GetFilesystemInfos(fsInfos);

    if (fsInfos.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Filesystem Info cache is empty, unable "
                                      "to determine what Recordings to expire");

        return;
    }

    FillExpireList(expireList);

    QMap <int, bool> truncateMap;
    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT DISTINCT rechost, recdir "
                  "FROM inuseprograms "
                  "WHERE recusage = 'truncatingdelete' "
                   "AND lastupdatetime > DATE_ADD(NOW(), INTERVAL -2 MINUTE);");

    if (!query.exec())
    {
        MythDB::DBError(LOC + "ExpireRecordings", query);
    }
    else
    {
        while (query.next())
        {
            QString rechost = query.value(0).toString();
            QString recdir  = query.value(1).toString();

            LOG(VB_FILE, LOG_INFO, LOC +
                QString("%1:%2 has an in-progress truncating delete.")
                    .arg(rechost).arg(recdir));

            for (fsit = fsInfos.begin(); fsit != fsInfos.end(); ++fsit)
            {
                if ((fsit->getHostname() == rechost) &&
                    (fsit->getPath() == recdir))
                {
                    truncateMap[fsit->getFSysID()] = true;
                    break;
                }
            }
        }
    }

    QMap <int, bool> fsMap;
    for (fsit = fsInfos.begin(); fsit != fsInfos.end(); ++fsit)
    {
        if (fsMap.contains(fsit->getFSysID()))
            continue;

        fsMap[fsit->getFSysID()] = true;

        LOG(VB_FILE, LOG_INFO,
            QString("fsID #%1: Total: %2 GB   Used: %3 GB   Free: %4 GB")
                .arg(fsit->getFSysID())
                .arg(fsit->getTotalSpace() / 1024.0 / 1024.0, 7, 'f', 1)
                .arg(fsit->getUsedSpace() / 1024.0 / 1024.0, 7, 'f', 1)
                .arg(fsit->getFreeSpace() / 1024.0 / 1024.0, 7, 'f', 1));

        if ((fsit->getTotalSpace() == -1) || (fsit->getUsedSpace() == -1))
        {
            LOG(VB_FILE, LOG_ERR, LOC +
                QString("fsID #%1 has invalid info, AutoExpire cannot run for "
                        "this filesystem.  Continuing on to next...")
                    .arg(fsit->getFSysID()));
            LOG(VB_FILE, LOG_INFO, QString("Directories on filesystem ID %1:")
                    .arg(fsit->getFSysID()));
            QList<FileSystemInfo>::iterator fsit2;
            for (fsit2 = fsInfos.begin(); fsit2 != fsInfos.end(); ++fsit2)
            {
                if (fsit2->getFSysID() == fsit->getFSysID())
                {
                    LOG(VB_FILE, LOG_INFO, QString("    %1:%2")
                            .arg(fsit2->getHostname()).arg(fsit2->getPath()));
                }
            }

            continue;
        }

        if (truncateMap.contains(fsit->getFSysID()))
        {
            LOG(VB_FILE, LOG_INFO,
                QString("    fsid %1 has a truncating delete in progress,  "
                        "AutoExpire cannot run for this filesystem until the "
                        "delete has finished.  Continuing on to next...")
                    .arg(fsit->getFSysID()));
            continue;
        }

        if (max((int64_t)0LL, fsit->getFreeSpace()) <
            desired_space[fsit->getFSysID()])
        {
            LOG(VB_FILE, LOG_INFO,
                QString("    Not Enough Free Space!  We want %1 MB")
                    .arg(desired_space[fsit->getFSysID()] / 1024));

            QMap<QString, int> dirList;
            QList<FileSystemInfo>::iterator fsit2;

            LOG(VB_FILE, LOG_INFO,
                QString("    Directories on filesystem ID %1:")
                    .arg(fsit->getFSysID()));

            for (fsit2 = fsInfos.begin(); fsit2 != fsInfos.end(); ++fsit2)
            {
                if (fsit2->getFSysID() == fsit->getFSysID())
                {
                    LOG(VB_FILE, LOG_INFO, QString("        %1:%2")
                            .arg(fsit2->getHostname()).arg(fsit2->getPath()));
                    dirList[fsit2->getHostname() + ":" + fsit2->getPath()] = 1;
                }
            }

            LOG(VB_FILE, LOG_INFO,
                "    Searching for files expirable in these directories");
            QString myHostName = gCoreContext->GetHostName();
            pginfolist_t::iterator it = expireList.begin();
            while ((it != expireList.end()) &&
                   (max((int64_t)0LL, fsit->getFreeSpace()) <
                    desired_space[fsit->getFSysID()]))
            {
                ProgramInfo *p = *it;
                ++it;

                LOG(VB_FILE, LOG_INFO, QString("        Checking %1 => %2")
                        .arg(p->toString(ProgramInfo::kRecordingKey))
                        .arg(p->GetTitle()));

                if (!p->IsLocal())
                {
                    bool foundFile = false;
                    QMap<int, EncoderLink *>::Iterator eit =
                         encoderList->begin();
                    while (eit != encoderList->end())
                    {
                        EncoderLink *el = *eit;
                        eit++;

                        if ((p->GetHostname() == el->GetHostName()) ||
                            ((p->GetHostname() == myHostName) &&
                             (el->IsLocal())))
                        {
                            if (el->IsConnected())
                                foundFile = el->CheckFile(p);

                            eit = encoderList->end();
                        }
                    }

                    if (!foundFile && (p->GetHostname() != myHostName))
                    {
                        // Wasn't found so check locally
                        QString file = GetPlaybackURL(p);

                        if (file.left(1) == "/")
                        {
                            p->SetPathname(file);
                            p->SetHostname(myHostName);
                            foundFile = true;
                        }
                    }

                    if (!foundFile)
                    {
                        LOG(VB_FILE, LOG_ERR, LOC +
                            QString("        ERROR: Can't find file for %1")
                                .arg(p->toString(ProgramInfo::kRecordingKey)));
                        continue;
                    }
                }

                QFileInfo vidFile(p->GetPathname());
                if (dirList.contains(p->GetHostname() + ':' + vidFile.path()))
                {
                    fsit->setUsedSpace(fsit->getUsedSpace()
                                                - (p->GetFilesize() / 1024));
                    deleteList.push_back(p);

                    LOG(VB_FILE, LOG_INFO,
                        QString("        FOUND file expirable. "
                                "%1 is located at %2 which is on fsID #%3. "
                                "Adding to deleteList.  After deleting we "
                                "should have %4 MB free on this filesystem.")
                            .arg(p->toString(ProgramInfo::kRecordingKey))
                            .arg(p->GetPathname()).arg(fsit->getFSysID())
                            .arg(fsit->getFreeSpace() / 1024));
                }
            }
        }
    }

    SendDeleteMessages(deleteList);

    ClearExpireList(deleteList, false);
    ClearExpireList(expireList);
}

/**
 *  \brief This sends delete message to main event thread.
 */
void AutoExpire::SendDeleteMessages(pginfolist_t &deleteList)
{
    QString msg;

    if (deleteList.empty())
    {
        LOG(VB_FILE, LOG_INFO, LOC + "SendDeleteMessages. Nothing to expire.");
        return;
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        "SendDeleteMessages, cycling through deleteList.");
    pginfolist_t::iterator it = deleteList.begin();
    while (it != deleteList.end())
    {
        msg = QString("%1Expiring %2 MB for %3 => %4")
            .arg(VERBOSE_LEVEL_CHECK(VB_FILE, LOG_ANY) ? "    " : "")
            .arg(((*it)->GetFilesize() >> 20))
            .arg((*it)->toString(ProgramInfo::kRecordingKey))
            .arg((*it)->toString(ProgramInfo::kTitleSubtitle));

        LOG(VB_GENERAL, LOG_NOTICE, msg);

        // send auto expire message to backend's event thread.
        MythEvent me(QString("AUTO_EXPIRE %1 %2").arg((*it)->GetChanID())
                     .arg((*it)->GetRecordingStartTime(MythDate::ISODate)));
        gCoreContext->dispatch(me);

        ++it; // move on to next program
    }
}

/** \fn AutoExpire::ExpireEpisodesOverMax()
 *  \brief This deletes programs exceeding the maximum
 *         number of episodes of that program desired.
 *         Excludes recordings in the LiveTV Recording Group.
 */
void AutoExpire::ExpireEpisodesOverMax(void)
{
    QMap<QString, int> maxEpisodes;
    QMap<QString, int>::Iterator maxIter;
    QMap<QString, int> episodeParts;
    QString episodeKey;

    QString fileprefix = gCoreContext->GetFilePrefix();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recordid, maxepisodes, title "
                  "FROM record WHERE maxepisodes > 0 "
                  "ORDER BY recordid ASC, maxepisodes DESC");

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Found %1 record profiles using max episode expiration")
                .arg(query.size()));
        while (query.next())
        {
            LOG(VB_FILE, LOG_INFO, QString("    %1 (%2 for rec id %3)")
                                     .arg(query.value(2).toString())
                                     .arg(query.value(1).toInt())
                                     .arg(query.value(0).toInt()));
            maxEpisodes[query.value(0).toString()] = query.value(1).toInt();
        }
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        "Checking episode count for each recording profile using max episodes");
    for (maxIter = maxEpisodes.begin(); maxIter != maxEpisodes.end(); ++maxIter)
    {
        query.prepare("SELECT chanid, starttime, title, progstart, progend, "
                          "filesize, duplicate "
                      "FROM recorded "
                      "WHERE recordid = :RECID AND preserve = 0 "
                      "AND recgroup NOT IN ('LiveTV', 'Deleted') "
                      "ORDER BY starttime DESC;");
        query.bindValue(":RECID", maxIter.key());

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("AutoExpire query failed!", query);
            continue;
        }

        LOG(VB_FILE, LOG_INFO, QString("    Recordid %1 has %2 recordings.")
                                 .arg(maxIter.key())
                                 .arg(query.size()));
        if (query.size() > 0)
        {
            int found = 1;
            while (query.next())
            {
                uint chanid = query.value(0).toUInt();
                QDateTime startts = MythDate::as_utc(query.value(1).toDateTime());
                QString title = query.value(2).toString();
                QDateTime progstart = MythDate::as_utc(query.value(3).toDateTime());
                QDateTime progend = MythDate::as_utc(query.value(4).toDateTime());
                int duplicate = query.value(6).toInt();

                episodeKey = QString("%1_%2_%3")
                             .arg(chanid)
                             .arg(progstart.toString(Qt::ISODate))
                             .arg(progend.toString(Qt::ISODate));

                if ((!IsInDontExpireSet(chanid, startts)) &&
                    (!episodeParts.contains(episodeKey)) &&
                    (found > *maxIter))
                {
                    uint64_t spaceFreed = query.value(5).toLongLong() >> 20;
                    QString msg =
                        QString("%1Expiring %2 MBytes for %3 at %4 => %5.  "
                                "Too many episodes, we only want to keep %6.")
                        .arg(VERBOSE_LEVEL_CHECK(VB_FILE, LOG_ANY) ?
                             "    " : "")
                        .arg(spaceFreed)
                        .arg(chanid).arg(startts.toString(Qt::ISODate))
                        .arg(title).arg(*maxIter);

                    LOG(VB_GENERAL, LOG_NOTICE, msg);

                    msg = QString("AUTO_EXPIRE %1 %2")
                                  .arg(chanid)
                                  .arg(startts.toString(Qt::ISODate));

                    MythEvent me(msg);
                    gCoreContext->dispatch(me);
                }
                else
                {
                    // keep track of shows we haven't expired so we can
                    // make sure we don't expire another part of the same
                    // episode.
                    if (episodeParts.contains(episodeKey))
                    {
                        episodeParts[episodeKey] = episodeParts[episodeKey] + 1;
                    }
                    else
                    {
                        episodeParts[episodeKey] = 1;
                        if( duplicate )
                            found++;
                    }
                }
            }
        }
    }
}

/** \fn AutoExpire::FillExpireList(pginfolist_t&)
 *  \brief Uses the "AutoExpireMethod" setting in the database to
 *         fill the list of files that are deletable.
 */
void AutoExpire::FillExpireList(pginfolist_t &expireList)
{
    int expMethod = gCoreContext->GetNumSetting("AutoExpireMethod", 1);

    ClearExpireList(expireList);

    FillDBOrdered(expireList, emNormalDeletedPrograms);

    switch(expMethod)
    {
        case emOldestFirst:
        case emLowestPriorityFirst:
        case emWeightedTimePriority:
                FillDBOrdered(expireList, expMethod);
                break;
        // default falls through so list is empty so no AutoExpire
    }
}

/** \fn AutoExpire::PrintExpireList(QString)
 *  \brief Prints a summary of the files that can be deleted.
 */
void AutoExpire::PrintExpireList(QString expHost)
{
    pginfolist_t expireList;

    FillExpireList(expireList);

    QString msg = "MythTV AutoExpire List ";
    if (expHost != "ALL")
        msg += QString("for '%1' ").arg(expHost);
    msg += "(programs listed in order of expiration)";
    cout << msg.toLocal8Bit().constData() << endl;

    pginfolist_t::iterator i = expireList.begin();
    for (; i != expireList.end(); ++i)
    {
        ProgramInfo *first = (*i);

        if (expHost != "ALL" && first->GetHostname() != expHost)
            continue;

        QString title = first->toString(ProgramInfo::kTitleSubtitle);
        title = title.leftJustified(39, ' ', true);

        QString outstr = QString("%1 %2 MB %3 [%4]")
            .arg(title)
            .arg(QString::number(first->GetFilesize() >> 20)
                 .rightJustified(5, ' ', true))
            .arg(first->GetRecordingStartTime(MythDate::ISODate)
                 .leftJustified(24, ' ', true))
            .arg(QString::number(first->GetRecordingPriority())
                 .rightJustified(3, ' ', true));
        QByteArray out = outstr.toLocal8Bit();

        cout << out.constData() << endl;
    }

    ClearExpireList(expireList);
}

/** \fn AutoExpire::GetAllExpiring(QStringList&)
 *  \brief Gets the full list of programs that can expire in expiration order
 */
void AutoExpire::GetAllExpiring(QStringList &strList)
{
    QMutexLocker lockit(&instance_lock);
    pginfolist_t expireList;

    UpdateDontExpireSet();

    FillDBOrdered(expireList, emShortLiveTVPrograms);
    FillDBOrdered(expireList, emNormalLiveTVPrograms);
    FillDBOrdered(expireList, emNormalDeletedPrograms);
    FillDBOrdered(expireList, gCoreContext->GetNumSetting("AutoExpireMethod",
                  emOldestFirst));

    strList << QString::number(expireList.size());

    pginfolist_t::iterator it = expireList.begin();
    for (; it != expireList.end(); ++it)
        (*it)->ToStringList(strList);

    ClearExpireList(expireList);
}

/** \fn AutoExpire::GetAllExpiring(pginfolist_t&)
 *  \brief Gets the full list of programs that can expire in expiration order
 */
void AutoExpire::GetAllExpiring(pginfolist_t &list)
{
    QMutexLocker lockit(&instance_lock);
    pginfolist_t expireList;

    UpdateDontExpireSet();

    FillDBOrdered(expireList, emShortLiveTVPrograms);
    FillDBOrdered(expireList, emNormalLiveTVPrograms);
    FillDBOrdered(expireList, emNormalDeletedPrograms);
    FillDBOrdered(expireList, gCoreContext->GetNumSetting("AutoExpireMethod",
                  emOldestFirst));

    pginfolist_t::iterator it = expireList.begin();
    for (; it != expireList.end(); ++it)
        list.push_back( new ProgramInfo( *(*it) ));

    ClearExpireList(expireList);
}

/** \fn AutoExpire::ClearExpireList(pginfolist_t&, bool)
 *  \brief Clears expireList, freeing any ProgramInfo's if necessary.
 */
void AutoExpire::ClearExpireList(pginfolist_t &expireList, bool deleteProg)
{
    ProgramInfo *pginfo = NULL;
    while (!expireList.empty())
    {
        if (deleteProg)
            pginfo = expireList.back();

        expireList.pop_back();

        if (deleteProg)
            delete pginfo;
    }
}

/** \fn AutoExpire::FillDBOrdered(pginfolist_t&, int)
 *  \brief Creates a list of programs to delete using the database to
 *         order list.
 */
void AutoExpire::FillDBOrdered(pginfolist_t &expireList, int expMethod)
{
    QString where;
    QString orderby;
    QString msg;
    int maxAge;

    switch (expMethod)
    {
        default:
        case emOldestFirst:
            msg = "Adding programs expirable in Oldest First order";
            where = "autoexpire > 0";
            if (gCoreContext->GetNumSetting("AutoExpireWatchedPriority", 0))
                orderby = "recorded.watched DESC, ";
            orderby += "starttime ASC";
            break;
        case emLowestPriorityFirst:
            msg = "Adding programs expirable in Lowest Priority First order";
            where = "autoexpire > 0";
            if (gCoreContext->GetNumSetting("AutoExpireWatchedPriority", 0))
                orderby = "recorded.watched DESC, ";
            orderby += "recorded.recpriority ASC, starttime ASC";
            break;
        case emWeightedTimePriority:
            msg = "Adding programs expirable in Weighted Time Priority order";
            where = "autoexpire > 0";
            if (gCoreContext->GetNumSetting("AutoExpireWatchedPriority", 0))
                orderby = "recorded.watched DESC, ";
            orderby += QString("DATE_ADD(starttime, INTERVAL '%1' * "
                                        "recorded.recpriority DAY) ASC")
                      .arg(gCoreContext->GetNumSetting("AutoExpireDayPriority", 3));
            break;
        case emShortLiveTVPrograms:
            msg = "Adding Short LiveTV programs in starttime order";
            where = "recgroup = 'LiveTV' "
                    "AND endtime < DATE_ADD(starttime, INTERVAL '2' MINUTE) "
                    "AND endtime <= DATE_ADD(NOW(), INTERVAL '-1' MINUTE) ";
            orderby = "starttime ASC";
            break;
        case emNormalLiveTVPrograms:
            msg = "Adding LiveTV programs in starttime order";
            where = QString("recgroup = 'LiveTV' "
                    "AND endtime <= DATE_ADD(NOW(), INTERVAL '-%1' DAY) ")
                    .arg(gCoreContext->GetNumSetting("AutoExpireLiveTVMaxAge", 1));
            orderby = "starttime ASC";
            break;
        case emOldDeletedPrograms:
            if ((maxAge = gCoreContext->GetNumSetting("DeletedMaxAge", 0)) <= 0)
                return;
            msg = QString("Adding programs deleted more than %1 days ago")
                          .arg(maxAge);
            where = QString("recgroup = 'Deleted' "
                    "AND lastmodified <= DATE_ADD(NOW(), INTERVAL '-%1' DAY) ")
                    .arg(maxAge);
            orderby = "starttime ASC";
            break;
        case emQuickDeletedPrograms:
            if (gCoreContext->GetNumSetting("DeletedMaxAge", 0) != 0)
                return;
            msg = QString("Adding programs deleted more than 5 minutes ago");
            where = QString("recgroup = 'Deleted' "
                    "AND lastmodified <= DATE_ADD(NOW(), INTERVAL '-5' MINUTE) ");
            orderby = "lastmodified ASC";
            break;
        case emNormalDeletedPrograms:
            msg = "Adding deleted programs in FIFO order";
            where = "recgroup = 'Deleted'";
            orderby = "lastmodified ASC";
            break;
    }

    LOG(VB_FILE, LOG_INFO, LOC + "FillDBOrdered: " + msg);

    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr = QString(
        "SELECT recorded.chanid, starttime "
        "FROM recorded "
        "LEFT JOIN channel ON recorded.chanid = channel.chanid "
        "WHERE %1 AND deletepending = 0 "
        "ORDER BY autoexpire DESC, %2").arg(where).arg(orderby);

    query.prepare(querystr);

    if (!query.exec())
        return;

    while (query.next())
    {
        uint chanid = query.value(0).toUInt();
        QDateTime recstartts = MythDate::as_utc(query.value(1).toDateTime());

        if (IsInDontExpireSet(chanid, recstartts))
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("    Skipping %1 at %2 because it is in Don't Expire "
                        "List")
                    .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
        }
        else if (IsInExpireList(expireList, chanid, recstartts))
        {
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("    Skipping %1 at %2 because it is already in Expire "
                        "List")
                    .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
        }
        else
        {
            ProgramInfo *pginfo = new ProgramInfo(chanid, recstartts);
            if (pginfo->GetChanID())
            {
                LOG(VB_FILE, LOG_INFO, LOC + QString("    Adding   %1 at %2")
                        .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
                expireList.push_back(pginfo);
            }
            else
            {
                LOG(VB_FILE, LOG_INFO, LOC +
                    QString("    Skipping %1 at %2 "
                            "because it could not be loaded from the DB")
                        .arg(chanid).arg(recstartts.toString(Qt::ISODate)));
                delete pginfo;
            }
        }
    }
}

/** \brief This is used by Update(QMap<int, EncoderLink*> *, bool)
 *         to run CalcParams(vector<EncoderLink*>).
 *
 *  \param autoExpireInstance AutoExpire instance on which to call CalcParams.
 */
void AutoExpire::RunUpdate(void)
{
    QMutexLocker locker(&instance_lock);
    Sleep(5 * 1000);
    locker.unlock();
    CalcParams();
    locker.relock();
    update_pending = false;
    update_thread->deleteLater();
    update_thread = NULL;
    instance_cond.wakeAll();
}

/**
 *  \brief This is used to update the global AutoExpire instance "expirer".
 *
 *  \param encoder     This recorder starts a recording now
 *  \param fsID        file system ID of the writing directory
 *  \param immediately If true CalcParams() is called directly.
 *                     If false, a thread is spawned to call CalcParams(),
 *                     this is for use in the MainServer event thread
 *                     where calling CalcParams() directly
 *                     would deadlock the event thread.
 */
void AutoExpire::Update(int encoder, int fsID, bool immediately)
{
    if (!expirer)
        return;

    // make sure there is only one update pending
    QMutexLocker locker(&expirer->instance_lock);
    while (expirer->update_pending)
        expirer->instance_cond.wait(&expirer->instance_lock);
    expirer->update_pending = true;

    if (encoder > 0)
    {
        QString msg = QString("Cardid %1: is starting a recording on")
                      .arg(encoder);
        if (fsID == -1)
            msg.append(" an unknown fsID soon.");
        else
            msg.append(QString(" fsID %2 soon.").arg(fsID));

        LOG(VB_FILE, LOG_INFO, LOC + msg);
        expirer->used_encoders[encoder] = fsID;
    }

    // do it..
    if (immediately)
    {
        locker.unlock();
        expirer->CalcParams();
        locker.relock();
        expirer->update_pending = false;
        expirer->instance_cond.wakeAll();
    }
    else
    {
        // create thread to do work, unless one is running still
        if (!expirer->update_thread)
        {
            expirer->update_thread = new UpdateThread(expirer);
            expirer->update_thread->start();
        }
    }
}

void AutoExpire::UpdateDontExpireSet(void)
{
    dont_expire_set.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare(
        "SELECT chanid, starttime, lastupdatetime, recusage, hostname "
        "FROM inuseprograms");

    if (!query.exec() || !query.next())
        return;

    LOG(VB_FILE, LOG_INFO, LOC + "Adding Programs to 'Do Not Expire' List");
    QDateTime curTime = MythDate::current();

    do
    {
        uint chanid = query.value(0).toUInt();
        QDateTime recstartts = MythDate::as_utc(query.value(1).toDateTime());
        QDateTime lastupdate = MythDate::as_utc(query.value(2).toDateTime());

        if (lastupdate.secsTo(curTime) < 2 * 60 * 60)
        {
            QString key = QString("%1_%2")
                .arg(chanid).arg(recstartts.toString(Qt::ISODate));
            dont_expire_set.insert(key);
            LOG(VB_FILE, LOG_INFO, QString("    %1 at %2 in use by %3 on %4")
                    .arg(chanid)
                    .arg(recstartts.toString(Qt::ISODate))
                    .arg(query.value(3).toString())
                    .arg(query.value(4).toString()));
        }
    }
    while (query.next());
}

bool AutoExpire::IsInDontExpireSet(
    uint chanid, const QDateTime &recstartts) const
{
    QString key = QString("%1_%2")
        .arg(chanid).arg(recstartts.toString(Qt::ISODate));

    return (dont_expire_set.find(key) != dont_expire_set.end());
}

bool AutoExpire::IsInExpireList(
    const pginfolist_t &expireList, uint chanid, const QDateTime &recstartts)
{
    pginfolist_t::const_iterator it;

    for (it = expireList.begin(); it != expireList.end(); ++it)
    {
        if (((*it)->GetChanID()             == chanid) &&
            ((*it)->GetRecordingStartTime() == recstartts))
        {
            return true;
        }
    }
    return false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
