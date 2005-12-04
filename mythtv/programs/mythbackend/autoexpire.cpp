#include <cstdlib>
#include <unistd.h>
#include <signal.h>


#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qregexp.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qfileinfo.h>

#include <iostream>
#include <algorithm>
using namespace std;

#include <sys/stat.h>
#ifdef linux
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "autoexpire.h"
#include "programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/util.h"
#include "libmythtv/remoteutil.h"
#include "libmythtv/remoteencoder.h"
#include "encoderlink.h"

#define LOC QString("AutoExpire: ")

extern AutoExpire *expirer;

/** If calculated desired space for 10 min freq is > SPACE_TOO_BIG_KB
 *  then we use 5 min expire frequency.
 */
#define SPACE_TOO_BIG_KB 3*1024*1024

/** \class AutoExpire
 *  \brief Used to expire recordings to make space for new recordings.
 */

/** \fn AutoExpire::AutoExpire(bool, bool)
 *  \brief Creates AutoExpire class, starting the thread if runthread is true.
 *
 *  \param runthread If true, the auto delete thread is started.
 *  \param master    If true, the auto delete thread will call
 *                   ExpireEpisodesOverMax().
 */
AutoExpire::AutoExpire(bool runthread, bool master)
    : record_file_prefix("/"), desired_space(3*1024*1024),
      desired_freq(10), expire_thread_running(runthread),
      is_master_backend(master), update_pending(false)    
{
    if (runthread)
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        pthread_create(&expire_thread, &attr, ExpirerThread, this);
        gContext->addListener(this);
    }
}

/** \fn AutoExpire::~AutoExpire()
 *  \brief AutoExpire destructor stops auto delete thread if it is running.
 */
AutoExpire::~AutoExpire()
{
    while (update_pending)
        usleep(500);
    if (expire_thread_running)
    {
        gContext->removeListener(this);
        expire_thread_running = false;
        pthread_kill(expire_thread, SIGALRM); // try to speed up join..
        VERBOSE(VB_IMPORTANT, LOC + "Warning: Stopping auto expire thread "
                "can take several seconds. Please be patient.");
        pthread_join(expire_thread, NULL);
    }
}

//#define DBG_CALC_PARAM(X) VERBOSE(VB_IMPORTANT, LOC + X);
#define DBG_CALC_PARAM(X) 

/** \fn AutoExpire::CalcParams()
 *   Calcualtes how much space needs to be cleared, and how often.
 */
void AutoExpire::CalcParams(vector<EncoderLink*> recs)
{
    DBG_CALC_PARAM("CalcParams() -- begin");
    QString recordFilePrefix = gContext->GetSetting("RecordFilePrefix");

    DBG_CALC_PARAM("CalcParams() -- A recs.size():"<<recs.size());

    uint rec_cnt = 0;
    size_t totalKBperMin = 0;
    // Determine total bitrate in KB/min for recorders sharing our disk
    for (vector<EncoderLink*>::iterator it = recs.begin(); it != recs.end(); ++it)
    {
        DBG_CALC_PARAM("CalcParams() -- A 1 rec("<<*it<<")");
        EncoderLink *enc = *it;
        volatile long long beginSize = -1, remoteSize = -1, endSize = -1;

        bool sharing = false;
        DBG_CALC_PARAM(QString("CalcParams() -- local(%1) con(%2)")
                       .arg(enc->IsLocal()).arg(enc->IsConnected()));
        if (enc->IsLocal())
            sharing = true;
        else if (enc->IsConnected())
        {
            long long total, used;
            beginSize  = getDiskSpace(recordFilePrefix, total, used);
            remoteSize = enc->GetFreeDiskSpace();
            endSize    = getDiskSpace(recordFilePrefix, total, used);

            DBG_CALC_PARAM("CalcParams() -- A 2");
            if (beginSize<0 || remoteSize<0 || endSize<0)
            {
                VERBOSE(VB_IMPORTANT, LOC + "CalcParams(): "
                        "Error, can not determine free space.");
                VERBOSE(VB_IMPORTANT, QString("    beg: %1 rem: %2 end: %3")
                        .arg(beginSize).arg(remoteSize).arg(endSize));
                sharing = true; // assume shared disk
            }
            else
            {
                long long minSize = (long long)(min(beginSize, endSize) * 0.98f);
                long long maxSize = (long long)(max(beginSize, endSize) * 1.02f);
                DBG_CALC_PARAM(QString("min: %1 rem: %2 max: %3")
                               .arg(minSize).arg(remoteSize).arg(maxSize));
                sharing = (minSize < remoteSize && remoteSize < maxSize);
            }
        }
        DBG_CALC_PARAM("CalcParams() -- A 3");
        if (sharing)
        {
            rec_cnt++;
            long long maxBitrate = enc->GetMaxBitrate();
            if (maxBitrate<=0)
                maxBitrate = 19500000LL;
            totalKBperMin += (((size_t)maxBitrate)*((size_t)15))>>11;
            DBG_CALC_PARAM("enc->GetMaxBitrate(): "<<enc->GetMaxBitrate());
            DBG_CALC_PARAM("totalKBperMin:        "<<totalKBperMin);
        }
        DBG_CALC_PARAM("CalcParams() -- A 4");
    }
    DBG_CALC_PARAM("CalcParams() -- B");

    VERBOSE(VB_IMPORTANT, LOC +
            QString("Found %1 recorders w/max rate of %1 MiB/min")
            .arg(rec_cnt).arg(totalKBperMin/1024));

    // Determine GB needed to account for recordings if autoexpire
    // is run every ten minutes. If this is too big, calculate space
    // needed to run autoexpire every five minutes.
    long long tot10min = totalKBperMin * 15, tot5min = totalKBperMin * 8;
    size_t expireMinKB;
    uint expireFreq;
    if (tot10min < SPACE_TOO_BIG_KB)
        (expireMinKB = tot10min), (expireFreq = 10);
    else
        (expireMinKB = tot5min), (expireFreq = 5);
    expireMinKB += gContext->GetNumSetting("AutoExpireExtraSpace", 0) * 1024*1024;

    DBG_CALC_PARAM("CalcParams() -- C");

    double expireMinGB = expireMinKB/(1024.0*1024.0);
    VERBOSE(VB_IMPORTANT, LOC +
            QString("Required Free Space: %2 GB w/freq: %2 min")
            .arg(expireMinGB, 0, 'f', 1).arg(expireFreq));

    // lock class and save these parameters.
    instance_lock.lock();
    desired_space      = expireMinKB;
    desired_freq       = expireFreq;
    record_file_prefix = recordFilePrefix;
    instance_lock.unlock();
    DBG_CALC_PARAM("CalcParams() -- end");
}

/** \fn UniqueFileSystemID(const QString&)
 *  \brief Returns unique ID of disk containing file,
 *         or a random ID if it does not succeed.
 *
 *   This may not succeed on some systems even if the file does exist.
 *   This is because the fsid on some systems (SunOS?), is also the
 *   NSF handle and can be used to compromise these systems. In practice
 *   this means MythTV will never delete symlinked recordings on such a
 *   system, even if the file is on the same filesystem.
 *  \param file_on_disk file on the file system we wish to stat.
 */
static fsid_t UniqueFileSystemID(const QString &file_on_disk)
{
    struct statfs statbuf;
    bzero(&statbuf, sizeof(statbuf));
    if (statfs(file_on_disk.local8Bit(), &statbuf) != 0)
        for (uint i = 0; i < sizeof(fsid_t); ++i)
            ((char*)(&statbuf.f_fsid))[i] = random()|0xff;
    return statbuf.f_fsid;
}

static bool operator== (const fsid_t &a, const fsid_t &b)
{
    for (uint i = 0; i < sizeof(fsid_t); ++i)
        if ( ((char*)(&a))[i] != ((char*)(&b))[i] )
            return false;
    return true;
}
static inline bool operator!= (const fsid_t &a, const fsid_t &b) { return !(a==b); }

/** \fn AutoExpire::RunExpirer()
 *  \brief This contains the main loop for the auto expire process.
 *
 *   Every "AutoExpireFrequency" minutes this will delete as many
 *   files as needed to free "AutoExpireDiskThreshold" gigabytes of
 *   space on the filesystem containing "RecordFilePrefix".
 *
 *   If this is run on the master backend this will also delete
 *   programs exceeding the maximum number of episodes of that
 *   program desired.
 */
void AutoExpire::RunExpirer(void)
{
    QTime curTime;
    QTime timer;

    // wait a little for main server to come up and things to settle down
    sleep(20);

    timer.start();

    while (expire_thread_running)
    {
        timer.restart();

        instance_lock.lock();

        UpdateDontExpireSet();

        curTime = QTime::currentTime();

        // Expire Short LiveTV files for this backend every 2 minutes
        if ((curTime.minute() % 2) == 0)
            ExpireLiveTV(emShortLiveTVPrograms);

        // Expire normal recordings depending on frequency calculated
        if ((curTime.minute() % desired_freq) == 0)
        {
            ExpireLiveTV(emNormalLiveTVPrograms);

            if (is_master_backend)
                ExpireEpisodesOverMax();

            ExpireRecordings();
        }

        instance_lock.unlock();

        Sleep(60 - (timer.elapsed() / 1000));
    }
} 

/** \fn AutoExpire::Sleep(int sleepTime)
 *  \brief Sleeps for sleepTime minutes; unless the expire thread
 *         is told to quit, then stops sleeping within 5 seconds.
 */
void AutoExpire::Sleep(int sleepTime)
{
    int minSleep = 5, timeExpended = 0;
    while (expire_thread_running && timeExpended < sleepTime)
    {
        if (timeExpended > (sleepTime - minSleep))
            minSleep = sleepTime - timeExpended;
        timeExpended += minSleep - (int)sleep(minSleep);
    }
}

/** \fn AutoExpire::ExpireLiveTV(int type)
 *  \brief This expires LiveTV programs.
 */
void AutoExpire::ExpireLiveTV(int type)
{
    long long availFreeKB = 0;
    long long tKB, uKB;

    if ((availFreeKB = getDiskSpace(record_file_prefix, tKB, uKB)) < 0)
    {
        QString msg = QString("ERROR: Could not calculate free space.");
        VERBOSE(VB_IMPORTANT, LOC + msg);
        gContext->LogEntry("mythbackend", LP_WARNING,
                           "Autoexpire Recording", msg);
    }
    else
    {
        VERBOSE(VB_FILE, LOC + QString("ExpireLiveTV(%1)").arg(type));
        ClearExpireList();
        FillDBOrdered(type);
        SendDeleteMessages(availFreeKB, 0, true);
    }
}

/** \fn AutoExpire::ExpireRecordings()
 *  \brief This expires normal recordings.
 *
 */
void AutoExpire::ExpireRecordings(void)
{
    long long availFreeKB = 0;
    long long tKB, uKB;

    if ((availFreeKB = getDiskSpace(record_file_prefix, tKB, uKB)) < 0)
    {
        QString msg = QString("ERROR: Could not calculate free space.");
        VERBOSE(VB_IMPORTANT, LOC + msg);
        gContext->LogEntry("mythbackend", LP_WARNING,
                           "Autoexpire Recording", msg);
    }
    else if (((size_t)availFreeKB) < desired_space)
    {
        VERBOSE(VB_FILE, LOC +
                QString("Running autoexpire, we want %1 MB free, "
                        "but only have %2 MB.")
                .arg(desired_space/1024).arg(availFreeKB/1024));
        
        FillExpireList();
        SendDeleteMessages(availFreeKB, desired_space);
    }
}

/** \fn CheckFile(const ProgramInfo*, const QString&, const fsid_t&)
 *  \brief Returns true iff file can be deleted.
 *
 *   CheckFile makes sure the file exists and is stored on the same file
 *   system as the recordfileprefix.
 *  \param pginfo           ProgramInfo for the program we wish to delete.
 *  \param recordfileprefix Path where new recordings are stored.
 *  \param fsid             Unique ID of recordfileprefix's filesystem.
 */
static bool CheckFile(const ProgramInfo *pginfo,
                      const QString &recordfileprefix,
                      const fsid_t& fsid)
{
    QString filename = pginfo->GetRecordFilename(recordfileprefix);
    QFileInfo checkFile(filename);
    if (!checkFile.exists())
    {
        // this file doesn't exist on this machine
        if (pginfo->hostname.isEmpty())
        {
            QString msg =
                QString("Don't know how to delete %1, "
                        "no hostname.").arg(filename);
            VERBOSE(VB_IMPORTANT, LOC + msg);
            gContext->LogEntry("mythbackend", LP_WARNING,
                               "Autoexpire Recording", msg); 
        }
        else if (pginfo->hostname == gContext->GetHostName())
        {
            QString msg =
                QString("ERROR when trying to autoexpire file: %1. "
                        "File doesn't exist.  Database metadata "
                        "will not be removed.").arg(filename);
            VERBOSE(VB_IMPORTANT, LOC + msg);
            gContext->LogEntry("mythbackend", LP_WARNING,
                               "Autoexpire Recording", msg); 
        }
        return false;
    }
    if (checkFile.isSymLink() &&
        UniqueFileSystemID(checkFile.readLink()) != fsid)
    {
        QString msg =
            QString("File '%1' is a symlink and does not appear to be on "
                    "the same file system as new recordings. "
                    "Not autoexpiring.").arg(filename);
        VERBOSE(VB_FILE, LOC + msg);
        return false;
    }
    return true;
}

/** \fn AutoExpire::SendDeleteMessages(size_t, size_t, bool)
 *  \brief This sends delete message to main event thread.
 */
void AutoExpire::SendDeleteMessages(size_t availFreeKB, size_t desiredFreeKB,
                                    bool deleteAll)
{
    QString msg;
    fsid_t fsid = UniqueFileSystemID(record_file_prefix);

    if (expire_list.size() == 0)
    {
        if ((!deleteAll) &&
            (desiredFreeKB > 0) &&
            (availFreeKB < desiredFreeKB))
            VERBOSE(VB_FILE, LOC + "SendDeleteMessages. WARNING: below free "
                    "disk space threshold, but nothing to expire~");
        else
            VERBOSE(VB_FILE, LOC + "SendDeleteMessages. Nothing to expire.");

        return;
    }

    VERBOSE(VB_FILE, LOC + "SendDeleteMessages, cycling through expire list.");
    pginfolist_t::iterator it = expire_list.begin();
    while (it != expire_list.end() &&
           (deleteAll || availFreeKB < desiredFreeKB))
    {
        VERBOSE(VB_FILE, QString("    Checking %1 @ %2")
                         .arg((*it)->chanid).arg((*it)->startts.toString()));
        if (CheckFile(*it, record_file_prefix, fsid))
        {
            // Print informative message 
            msg = QString("Expiring: %1 %2 %3 MBytes")
                .arg((*it)->title).arg((*it)->startts.toString())
                .arg((int)((*it)->filesize/1024/1024));
            VERBOSE(VB_FILE, QString("    ") +  msg);
            gContext->LogEntry("autoexpire", LP_NOTICE,
                               "Expiring Program", msg);                

            // send auto expire message to backend's event thread.
            MythEvent me(QString("AUTO_EXPIRE %1 %2").arg((*it)->chanid)
                         .arg((*it)->recstartts.toString(Qt::ISODate)));
            gContext->dispatch(me);

            availFreeKB += ((*it)->filesize/1024); // add size to avail size
            VERBOSE(VB_FILE,
                    QString("    After unlink we will have %1 MB free.")
                    .arg(availFreeKB/1024));

        }
        else
        {
            VERBOSE(VB_FILE, QString("    CheckFile failed for %1 @ %2, "
                             "unable to expire.")
                             .arg((*it)->chanid).arg((*it)->startts.toString()));
        }
        ++it; // move on to next program
    }
    
    if (!deleteAll && availFreeKB < desiredFreeKB)
    {
        msg = QString("ERROR when trying to autoexpire files.  "
                      "No recordings available to expire.");
        VERBOSE(VB_IMPORTANT, LOC + msg); 
        gContext->LogEntry("mythbackend", LP_WARNING,
                           "Autoexpire Recording", msg);
    }
}

/** \fn AutoExpire::ExpirerThread(void *)
 *  \brief This calls RunExpirer() from within a new pthread.
 */
void *AutoExpire::ExpirerThread(void *param)
{
    AutoExpire *expirer = (AutoExpire *)param;
    expirer->RunExpirer();
 
    return NULL;
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

    QString fileprefix = gContext->GetFilePrefix();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT recordid, maxepisodes, title "
                  "FROM record WHERE maxepisodes > 0 "
                  "ORDER BY recordid ASC, maxepisodes DESC");

    if (query.exec() && query.isActive() && query.size() > 0)
    {
        VERBOSE(VB_FILE, LOC + QString("Found %1 record profiles using max "
                                 "episode expiration")
                                 .arg(query.size()));
        while (query.next())
        {
            VERBOSE(VB_FILE, QString("    %1 (%2 for rec id %3)")
                                     .arg(query.value(2).toString())
                                     .arg(query.value(1).toInt())
                                     .arg(query.value(0).toInt()));
            maxEpisodes[query.value(0).toString()] = query.value(1).toInt();
        }
    }

    VERBOSE(VB_FILE, LOC + "Checking episode count for each recording "
                           "profile using max episodes");
    for (maxIter = maxEpisodes.begin(); maxIter != maxEpisodes.end(); maxIter++)
    {
        query.prepare("SELECT chanid, starttime, title, progstart, progend "
                      "FROM recorded "
                      "WHERE recordid = :RECID AND preserve = 0 "
                      "AND recgroup <> 'LiveTV' "
                      "ORDER BY starttime DESC;");
        query.bindValue(":RECID", maxIter.key());

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("AutoExpire query failed!", query);
            continue;
        }

        VERBOSE(VB_FILE, QString("    Recordid %1 has %2 recordings.")
                                 .arg(maxIter.key())
                                 .arg(query.size()));
        if (query.size() > 0)
        {
            int found = 1;
            while (query.next())
            {
                QString chanid = query.value(0).toString();
                QDateTime startts = query.value(1).toDateTime();
                QString title = QString::fromUtf8(query.value(2).toString());
                QDateTime progstart = query.value(3).toDateTime();
                QDateTime progend = query.value(4).toDateTime();

                episodeKey = QString("%1_%2_%3")
                             .arg(chanid)
                             .arg(progstart.toString(Qt::ISODate))
                             .arg(progend.toString(Qt::ISODate));

                if ((!IsInDontExpireSet(chanid, startts)) && 
                    (!episodeParts.contains(episodeKey)) &&
                    (found > maxIter.data()))
                {
                    QString msg = QString("Expiring \"%1\" from %2, "
                                          "too many episodes.")
                                          .arg(title)
                                          .arg(startts.toString());
                    VERBOSE(VB_FILE, QString("    ") + msg);
                    gContext->LogEntry("autoexpire", LP_NOTICE,
                                       "Expired program", msg);

                    msg = QString("AUTO_EXPIRE %1 %2")
                                  .arg(chanid)
                                  .arg(startts.toString(Qt::ISODate));

                    MythEvent me(msg);
                    gContext->dispatchNow(me);
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
                        found++;
                    }
                }
            }
        }
    }
}

/** \fn AutoExpire::FillExpireList()
 *  \brief Uses the "AutoExpireMethod" setting in the database to
 *         fill the list of files that are deletable.
 *
 *   At the moment "Delete Oldest First" is the only available method.
 */
void AutoExpire::FillExpireList(void)
{
    int expMethod = gContext->GetNumSetting("AutoExpireMethod", 1);

    ClearExpireList();

    switch(expMethod)
    {
        case emOldestFirst:
        case emLowestPriorityFirst:
                FillDBOrdered(expMethod);
                break;
        // default falls through so list is empty so no AutoExpire
    }
}

/** \fn AutoExpire::PrintExpireList(void)
 *  \brief Prints a summary of the files that can be deleted.
 */
void AutoExpire::PrintExpireList(void)
{
    cout << "MythTV AutoExpire List (programs listed in order of expiration)\n";

    pginfolist_t::iterator i = expire_list.begin();
    for (; i != expire_list.end(); i++)
    {
        ProgramInfo *first = (*i);
        QString title = first->title;

        if (first->subtitle != "")
            title += ": \"" + first->subtitle + "\"";

        cout << title.local8Bit().leftJustify(39, ' ', true) << " "
             << QString("%1").arg(first->filesize / 1024 / 1024).local8Bit()
                .rightJustify(5, ' ', true) << "MiB  "
             << first->startts.toString().local8Bit().leftJustify(24, ' ', true)
             << " [" << QString("%1").arg(first->recpriority).local8Bit()
                .rightJustify(3, ' ', true) << "]"
             << endl;
    }
}

/** \fn AutoExpire::ClearExpireList()
 *  \brief Clears expire_list, freeing any ProgramInfo's.
 */
void AutoExpire::ClearExpireList(void)
{
    while (expire_list.size() > 0)
    {
        ProgramInfo *pginfo = expire_list.back();
        expire_list.pop_back();
        delete pginfo;
    }
}

/** \fn AutoExpire::FillDBOrdered(int)
 *  \brief Creates a list of programs to delete using the database to 
 *         order list.
 */
void AutoExpire::FillDBOrdered(int expMethod)
{
    QString fileprefix = gContext->GetFilePrefix();
    QString where;
    QString orderby;

    switch (expMethod)
    {
        default:
        case emOldestFirst:
            where = "autoexpire > 0";
            orderby = "starttime ASC";
            break;
        case emLowestPriorityFirst:
            where = "autoexpire > 0";
            orderby = "recorded.recpriority ASC, starttime ASC";
            break;
        case emShortLiveTVPrograms:
            where = QString("recgroup = 'LiveTV' AND hostname = '%1' "
                    "AND endtime < DATE_ADD(starttime, INTERVAL '2' MINUTE) "
                    "AND endtime <= DATE_ADD(NOW(), INTERVAL '-1' MINUTE) ")
                    .arg(gContext->GetHostName());
            orderby = "starttime ASC";
            break;
        case emNormalLiveTVPrograms:
            int LiveTVMaxAge =
                    gContext->GetNumSetting("AutoExpireLiveTVMaxAge", 7);
            where = QString("recgroup = 'LiveTV' AND hostname = '%1' "
                    "AND endtime <= DATE_ADD(NOW(), INTERVAL '-%2' DAY) ")
                    .arg(gContext->GetHostName()).arg(LiveTVMaxAge);
            orderby = "starttime ASC";
            break;
    }

    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr = QString(
               "SELECT recorded.chanid, starttime,   endtime,     "
               "       title,           subtitle,    description, "
               "       hostname,        channum,     name,        "
               "       callsign,        seriesid,    programid,   "
               "       recorded.        recpriority, progstart, "
               "       progend,         filesize "
               "FROM recorded "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "WHERE %1 "
               "ORDER BY autoexpire DESC, %2").arg(where).arg(orderby);

    query.prepare(querystr);

    if (!query.exec() || !query.isActive() || !query.size())
        return;

    while (query.next())
    {
        ProgramInfo *proginfo = new ProgramInfo;

        proginfo->chanid = query.value(0).toString();
        proginfo->startts = query.value(13).toDateTime();
        proginfo->endts = query.value(14).toDateTime();
        proginfo->recstartts = query.value(1).toDateTime();
        proginfo->recendts = query.value(2).toDateTime();
        proginfo->title = QString::fromUtf8(query.value(3).toString());
        proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
        proginfo->description = QString::fromUtf8(query.value(5).toString());
        proginfo->hostname = query.value(6).toString();

        if (!query.value(7).toString().isEmpty())
        {
            proginfo->chanstr = query.value(7).toString();
            proginfo->channame = QString::fromUtf8(query.value(8).toString());
            proginfo->chansign = QString::fromUtf8(query.value(9).toString());
        }
        else
        {
            proginfo->chanstr = "#" + proginfo->chanid;
            proginfo->channame = "#" + proginfo->chanid;
            proginfo->chansign = "#" + proginfo->chanid;
        }

        proginfo->seriesid = query.value(10).toString();
        proginfo->programid = query.value(11).toString();
        proginfo->recpriority = query.value(12).toInt();

        proginfo->pathname = proginfo->GetRecordFilename(fileprefix);

        proginfo->filesize = stringToLongLong(query.value(15).toString());

        if (IsInDontExpireSet(proginfo->chanid, proginfo->recstartts))
        {
            VERBOSE(VB_FILE, LOC + QString("FillDBOrdered: Chanid "
                                     "%1 @ %2 is in Don't Expire List")
                                     .arg(proginfo->chanid)
                                     .arg(proginfo->recstartts.toString()));
            delete proginfo;
        }
        else
        {
            VERBOSE(VB_FILE, LOC + QString("FillDBOrdered: Adding chanid "
                                     "%1 @ %2 to expire list")
                                     .arg(proginfo->chanid)
                                     .arg(proginfo->recstartts.toString()));
            expire_list.push_back(proginfo);
        }
    }
}

/** \fn SpawnUpdateThread(void*)
 *  \brief This is used by Update(QMap<int, EncoderLink*> *, bool)
 *         to run CalcParams(vector<EncoderLink*>).
 *
 *  \param autoExpireInstance AutoExpire instance on which to call CalcParams.
 */
void *SpawnUpdateThread(void *autoExpireInstance)
{
    sleep(5);
    AutoExpire *ae = (AutoExpire*) autoExpireInstance;
    ae->CalcParams(ae->encoder_links);
    ae->instance_lock.lock();
    ae->update_pending = false;
    ae->instance_lock.unlock();
    return NULL;
}

/** \fn AutoExpire::Update(QMap<int, EncoderLink*> *, bool)
 *  \brief This is used to update the global AutoExpire instance "expirer".
 *
 *  \param encoderList These are the encoders you want "expirer" to know about.
 *  \param immediately If true CalcParams() is called directly. If false a thread
 *                     is spawned to call CalcParams(), this is for use in the
 *                     MainServer event thread where calling CalcParams() directly
 *                     would deadlock the event thread.
 */
void AutoExpire::Update(QMap<int, EncoderLink*> *encoderList, bool immediately)
{
    if (!expirer)
        return;

    // make sure there is only one update pending
    expirer->instance_lock.lock();
    while (expirer->update_pending)
    {
        expirer->instance_lock.unlock();
        usleep(500);
        expirer->instance_lock.lock();
    }
    expirer->update_pending = true;
        
    // create vector of encoders
    expirer->encoder_links.clear();
    QMap<int, EncoderLink*>::Iterator it = encoderList->begin();
    for (; it != encoderList->end(); ++it)
        expirer->encoder_links.push_back(it.data());
    expirer->instance_lock.unlock();

    // do it..
    if (immediately)
    {
        expirer->CalcParams(expirer->encoder_links);
        expirer->instance_lock.lock();
        expirer->update_pending = false;
        expirer->instance_lock.unlock();
    }
    else
    {
        // create thread to do work
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&expirer->update_thread, &attr,
                       SpawnUpdateThread, expirer);
    }
}

void AutoExpire::UpdateDontExpireSet(void)
{
    dont_expire_set.clear();

    MSqlQuery query(MSqlQuery::InitCon());
    query.prepare("SELECT chanid, starttime, lastupdatetime, recusage, "
                  " hostname "
                  "FROM inuseprograms;");

    if (!query.exec() || !query.isActive() || !query.size())
        return;

    QDateTime curTime = QDateTime::currentDateTime();

    VERBOSE(VB_FILE, LOC + "Adding Programs to 'Do Not Expire' List");
    while (query.next())
    {
        QString chanid = query.value(0).toString();
        QDateTime startts = query.value(1).toDateTime();
        QDateTime lastupdate = query.value(2).toDateTime();

        if (lastupdate.secsTo(curTime) < 2 * 60 * 60)
        {
            QString key = chanid + startts.toString(Qt::ISODate);
            dont_expire_set.insert(key);
            VERBOSE(VB_FILE, QString("    %1 @ %2 in use by %3 on %4")
                                     .arg(chanid).arg(startts.toString())
                                     .arg(query.value(3).toString())
                                     .arg(query.value(4).toString()));
        }
    }
}

bool AutoExpire::IsInDontExpireSet(QString chanid, QDateTime starttime)
{
    QString key = chanid + starttime.toString(Qt::ISODate);

    return (dont_expire_set.count(key));
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
