// POSIX headers
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>

// ANSI C headers
#include <cstdlib>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QStringList>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

// MythTV headers
#include "housekeeper.h"
#include "mythsystemlegacy.h"
#include "jobqueue.h"
#include "mythcorecontext.h"
#include "mythdb.h"
#include "mythdate.h"
#include "compat.h"
#include "mythdirs.h"
#include "programinfo.h"
#include "eitcache.h"
#include "scheduler.h"
#include "mythcoreutil.h"
#include "mythdownloadmanager.h"
#include "exitcodes.h"
#include "mythversion.h"
#include "mythlogging.h"

void HouseKeepingThread::run(void)
{
    RunProlog();
    m_parent->RunHouseKeeping();
    RunEpilog();
}

void MythFillDatabaseThread::run(void)
{
    RunProlog();
    m_parent->RunMFD();
    RunEpilog();
}

HouseKeeper::HouseKeeper(bool runthread, bool master, Scheduler *lsched) :
    isMaster(master),           sched(lsched),
    houseKeepingRun(runthread), houseKeepingThread(NULL),
    fillDBThread(NULL),         fillDBStarted(false),
    fillDBMythSystemLegacy(NULL)
{
    CleanupMyOldRecordings();

    if (runthread)
    {
        houseKeepingThread = new HouseKeepingThread(this);
        houseKeepingThread->start();

        QMutexLocker locker(&houseKeepingLock);
        while (houseKeepingRun && !houseKeepingThread->isRunning())
            houseKeepingWait.wait(locker.mutex());
    }
}

HouseKeeper::~HouseKeeper()
{
    if (houseKeepingThread)
    {
        {
            QMutexLocker locker(&houseKeepingLock);
            houseKeepingRun = false;
            houseKeepingWait.wakeAll();
        }
        houseKeepingThread->wait();
        delete houseKeepingThread;
        houseKeepingThread = NULL;
    }

    if (fillDBThread)
    {
        KillMFD();
        delete fillDBThread;
        fillDBThread = NULL;
    }
}

bool HouseKeeper::wantToRun(const QString &dbTag, int period, int minhour,
                            int maxhour, bool nowIfPossible)
{
    bool runOK = false;
    unsigned int oneday = 60 * 60 * 24;
    int longEnough = 0;

    if (period)
        longEnough = ((period * oneday) - oneday/2);
    else
        longEnough = oneday / 8;

    QDateTime now = MythDate::current();
    QDateTime lastrun = MythDate::fromTime_t(0);

    if (minhour < 0)
        minhour = 0;
    if (maxhour > 23)
        maxhour = 23;

    MSqlQuery result(MSqlQuery::InitCon());
    if (result.isConnected())
    {
        result.prepare("SELECT lastrun FROM housekeeping WHERE tag = :TAG ;");
        result.bindValue(":TAG", dbTag);

        if (result.exec() && result.next())
        {
            lastrun = MythDate::as_utc(result.value(0).toDateTime());

            if ((lastrun.secsTo(now) > longEnough) &&
                (now.date().day() != lastrun.date().day()))
            {
                int hour = now.time().hour();

                if (((minhour > maxhour) &&
                     ((hour <= maxhour) || (hour >= minhour))) ||
                    ((hour >= minhour) && (hour <= maxhour)))
                {
                    int minute = now.time().minute();
                    // Allow the job run if
                    // a) we want to run now rather than at a random time
                    // b) we have reached the last half hour of the window, or
                    // c) we win a random draw with a probability of 1/N.
                    //
                    // N gets smaller the nearer we are to the end of the
                    // window. The "(24 + ...) % 24" makes sure the calculation
                    // is correct even for the case hour > minhour > maxhour.
                    if ((nowIfPossible) ||
                        (hour == maxhour && minute > 30) ||
                        ((random()%((((24+maxhour-hour)%24)*12+(60-minute)/5 - 6) + 1)) == 0))
                        runOK = true;
                }
            }
        }
        else
        {
            result.prepare("INSERT INTO housekeeping(tag,lastrun) "
                           "values(:TAG ,now());");
            result.bindValue(":TAG", dbTag);
            if (!result.exec())
                MythDB::DBError("HouseKeeper::wantToRun -- insert", result);

            runOK = true;
        }
    }

    return runOK;
}

void HouseKeeper::updateLastrun(const QString &dbTag)
{
    MSqlQuery result(MSqlQuery::InitCon());
    if (result.isConnected())
    {
        result.prepare("DELETE FROM housekeeping WHERE tag = :TAG ;");
        result.bindValue(":TAG", dbTag);
        if (!result.exec())
            MythDB::DBError("HouseKeeper::updateLastrun -- delete", result);

        result.prepare("INSERT INTO housekeeping(tag,lastrun) "
                       "values(:TAG ,now()) ;");
        result.bindValue(":TAG", dbTag);
        if (!result.exec())
            MythDB::DBError("HouseKeeper::updateLastrun -- insert", result);
    }
}

QDateTime HouseKeeper::getLastRun(const QString &dbTag)
{
    MSqlQuery result(MSqlQuery::InitCon());

    QDateTime lastRun = MythDate::fromTime_t(0);

    result.prepare("SELECT lastrun FROM housekeeping WHERE tag = :TAG ;");
    result.bindValue(":TAG", dbTag);

    if (result.exec() && result.next())
    {
        lastRun = MythDate::as_utc(result.value(0).toDateTime());
    }

    return lastRun;
}

void HouseKeeper::RunHouseKeeping(void)
{
    // tell constructor that we've started..
    {
        QMutexLocker locker(&houseKeepingLock);
        houseKeepingWait.wakeAll();
    }

    QString dbTag;
    bool initialRun = true;

    // wait a little for main server to come up and things to settle down
    {
        QMutexLocker locker(&houseKeepingLock);
        houseKeepingWait.wait(locker.mutex(), 10 * 1000);
    }

    RunStartupTasks();

    QMutexLocker locker(&houseKeepingLock);
    while (houseKeepingRun)
    {
        locker.unlock();

        LOG(VB_GENERAL, LOG_INFO, "Running housekeeping thread");

        // These tasks are only done from the master backend
        if (isMaster)
        {
            // Clean out old database logging entries
            if (wantToRun("LogClean", 1, 0, 24))
            {
                LOG(VB_GENERAL, LOG_INFO, "Running LogClean");
                flushDBLogs();
                updateLastrun("LogClean");
            }

            // Run mythfilldatabase to grab the TV listings
            if (gCoreContext->GetNumSetting("MythFillEnabled", 1))
            {
                if (fillDBThread && fillDBThread->isRunning())
                {
                    LOG(VB_GENERAL, LOG_INFO,
                        "mythfilldatabase still running, skipping checks.");
                }
                else
                {
                    bool grabberSupportsNextTime = false;
                    MSqlQuery result(MSqlQuery::InitCon());
                    if (result.isConnected())
                    {
                        result.prepare("SELECT COUNT(*) FROM videosource "
                                       "WHERE xmltvgrabber IN "
                                           "( 'datadirect', 'technovera',"
                                           " 'schedulesdirect1' );");

                        if ((result.exec()) &&
                            (result.next()) &&
                            (result.value(0).toInt() > 0))
                        {
                            grabberSupportsNextTime =
                                gCoreContext->GetNumSetting(
                                    "MythFillGrabberSuggestsTime", 1);
                        }
                    }

                    bool runMythFill = false;
                    if (grabberSupportsNextTime)
                    {
                        QDateTime nextRun = MythDate::fromString(
                            gCoreContext->GetSetting("MythFillSuggestedRunTime",
                                                     "1970-01-01T00:00:00"));
                        QDateTime lastRun = getLastRun("MythFillDB");
                        QDateTime now = MythDate::current();

                        if ((nextRun < now) &&
                            (lastRun.secsTo(now) > (3 * 60 * 60)))
                            runMythFill = true;
                    }
                    else
                    {
                        QDate date = QDate::currentDate();
                        int minhr = gCoreContext->GetNumSetting(
                            "MythFillMinHour", -1);
                        int maxhr = gCoreContext->GetNumSetting(
                            "MythFillMaxHour", 23) % 24;
                        if (minhr == -1)
                        {
                            minhr = 0;
                            maxhr = 23;
                        }
                        else
                        {
                            minhr %= 24;
                        }

                        QDateTime umin =
                            QDateTime(date, QTime(minhr,0)).toUTC();
                        QDateTime umax =
                            QDateTime(date, QTime(maxhr,0)).toUTC();

                        if (umax == umin)
                            umax.addSecs(60*60);

                        runMythFill = wantToRun(
                            "MythFillDB", 1,
                            umin.time().hour(), umax.time().hour(),
                            initialRun);
                    }

                    if (runMythFill)
                    {
                        LOG(VB_GENERAL, LOG_INFO, "Running mythfilldatabase");
                        StartMFD();
                        updateLastrun("MythFillDB");
                    }
                }
            }

            if (wantToRun("DailyCleanup", 1, 0, 24)) {
                JobQueue::CleanupOldJobsInQueue();
                CleanupAllOldInUsePrograms();
                CleanupOrphanedLivetvChains();
                CleanupRecordedTables();
                CleanupProgramListings();
                updateLastrun("DailyCleanup");
            }

            if ((gCoreContext->GetNumSetting("ThemeUpdateNofications", 1)) &&
                (wantToRun("ThemeChooserInfoCacheUpdate", 1, 0, 24, true)))
            {
                UpdateThemeChooserInfoCache();
                updateLastrun("ThemeChooserInfoCacheUpdate");
            }

#if CONFIG_BINDINGS_PYTHON
            if ((gCoreContext->GetNumSetting("DailyArtworkUpdates", 0)) &&
                (wantToRun("RecordedArtworkUpdate", 1, 0, 24, true)))
            {
                UpdateRecordedArtwork();
                updateLastrun("RecordedArtworkUpdate");
            }
#endif
        }

        dbTag = QString("JobQueueRecover-%1").arg(gCoreContext->GetHostName());
        if (wantToRun(dbTag, 1, 0, 24))
        {
            JobQueue::RecoverOldJobsInQueue();
            updateLastrun(dbTag);
        }

        initialRun = false;

        locker.relock();
        if (houseKeepingRun)
            houseKeepingWait.wait(locker.mutex(), (300 + (random()%8)) * 1000);
    }
}

void HouseKeeper::flushDBLogs()
{
    int numdays = 14;
    uint64_t maxrows = 10000 * numdays;  // likely high enough to keep numdays

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        // Remove less-important logging after 1/2 * numdays days
        QDateTime days = MythDate::current();
        days = days.addDays(0 - (numdays / 2));
        QString sql = "DELETE FROM logging "
                      " WHERE application NOT IN (:MYTHBACKEND, :MYTHFRONTEND) "
                      "   AND msgtime < :DAYS ;";
        query.prepare(sql);
        query.bindValue(":MYTHBACKEND", MYTH_APPNAME_MYTHBACKEND);
        query.bindValue(":MYTHFRONTEND", MYTH_APPNAME_MYTHFRONTEND);
        query.bindValue(":DAYS", days);
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting helper application database log entries "
                    "from before %1.") .arg(days.toString(Qt::ISODate)));
        if (!query.exec())
            MythDB::DBError("Delete helper application log entries", query);

        // Remove backend/frontend logging after numdays days
        days = MythDate::current();
        days = days.addDays(0 - numdays);
        sql = "DELETE FROM logging WHERE msgtime < :DAYS ;";
        query.prepare(sql);
        query.bindValue(":DAYS", days);
        LOG(VB_GENERAL, LOG_DEBUG,
            QString("Deleting database log entries from before %1.")
            .arg(days.toString(Qt::ISODate)));
        if (!query.exec())
            MythDB::DBError("Delete old log entries", query);

        sql = "SELECT COUNT(id) FROM logging;";
        query.prepare(sql);
        if (query.exec())
        {
            uint64_t totalrows = 0;
            while (query.next())
            {
                totalrows = query.value(0).toLongLong();
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Database has %1 log entries.").arg(totalrows));
            }
            if (totalrows > maxrows)
            {
                sql = "DELETE FROM logging ORDER BY msgtime LIMIT :ROWS;";
                query.prepare(sql);
                quint64 extrarows = totalrows - maxrows;
                query.bindValue(":ROWS", extrarows);
                LOG(VB_GENERAL, LOG_DEBUG,
                    QString("Deleting oldest %1 database log entries.")
                        .arg(extrarows));
                if (!query.exec())
                    MythDB::DBError("Delete excess log entries", query);
            }
        }
        else
            MythDB::DBError("Query logging table size", query);
    }
}

void HouseKeeper::RunMFD(void)
{
    {
        QMutexLocker locker(&fillDBLock);
        fillDBStarted = true;
        fillDBWait.wakeAll();
    }

    QString mfpath = gCoreContext->GetSetting("MythFillDatabasePath",
                                          "mythfilldatabase");
    QString mfarg = gCoreContext->GetSetting("MythFillDatabaseArgs", "");

    if (mfpath == "mythfilldatabase")
        mfpath = GetInstallPrefix() + "/bin/mythfilldatabase";

    QString command = QString("%1 %2 %3").arg(mfpath).arg(logPropagateArgs)
                        .arg(mfarg);

    {
        QMutexLocker locker(&fillDBLock);
        fillDBMythSystemLegacy = new MythSystemLegacy(command, kMSRunShell |
                                                   kMSAutoCleanup);
        fillDBMythSystemLegacy->Run(0);
        fillDBWait.wakeAll();
    }

    MythFillDatabaseThread::setTerminationEnabled(true);

    uint result = fillDBMythSystemLegacy->Wait(0);

    MythFillDatabaseThread::setTerminationEnabled(false);

    {
        QMutexLocker locker(&fillDBLock);
        fillDBMythSystemLegacy->deleteLater();
        fillDBMythSystemLegacy = NULL;
        fillDBWait.wakeAll();
    }

    if (result != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythFillDatabase command '%1' failed")
                .arg(command));
    }
}

void HouseKeeper::StartMFD(void)
{
    if (fillDBThread)
    {
        KillMFD();
        delete fillDBThread;
        fillDBThread = NULL;
        fillDBStarted = false;
    }

    fillDBThread = new MythFillDatabaseThread(this);
    fillDBThread->start();

    QMutexLocker locker(&fillDBLock);
    while (!fillDBStarted)
        fillDBWait.wait(locker.mutex());
}

void HouseKeeper::KillMFD(void)
{
    if (!fillDBThread->isRunning())
        return;

    QMutexLocker locker(&fillDBLock);
    if (fillDBMythSystemLegacy && fillDBThread->isRunning())
    {
        fillDBMythSystemLegacy->Term(false);
        fillDBWait.wait(locker.mutex(), 50);
    }

    if (fillDBMythSystemLegacy && fillDBThread->isRunning())
    {
        fillDBMythSystemLegacy->Term(true);
        fillDBWait.wait(locker.mutex(), 50);
    }

    if (fillDBThread->isRunning())
    {
        fillDBThread->terminate();
        usleep(5000);
    }

    if (fillDBThread->isRunning())
    {
        locker.unlock();
        fillDBThread->wait();
    }
}

void HouseKeeper::CleanupMyOldRecordings(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME AND "
                    "( recusage = 'recorder' OR recusage LIKE 'Unknown %' );");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("HouseKeeper::CleanupMyOldRecordings", query);
}

void HouseKeeper::CleanupAllOldInUsePrograms(void)
{
    QDateTime fourHoursAgo = MythDate::current().addSecs(-4 * 60 * 60);
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE lastupdatetime < :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);
    if (!query.exec())
        MythDB::DBError("HouseKeeper::CleanupAllOldInUsePrograms", query);
}

void HouseKeeper::CleanupOrphanedLivetvChains(void)
{
    QDateTime fourHoursAgo = MythDate::current().addSecs(-4 * 60 * 60);
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deleteQuery(MSqlQuery::InitCon());

    // Keep these tvchains, they may be in use.
    query.prepare("SELECT DISTINCT chainid FROM tvchain "
                  "WHERE endtime > :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);

    if (!query.exec() || !query.isActive())
    {
        MythDB::DBError("HouseKeeper Cleaning TVChain Table", query);
        return;
    }

    QString msg, keepChains;
    while (query.next())
        if (keepChains.isEmpty())
            keepChains = "'" + query.value(0).toString() + "'";
        else
            keepChains += ", '" + query.value(0).toString() + "'";

    if (keepChains.isEmpty())
        msg = "DELETE FROM tvchain WHERE endtime < now();";
    else
    {
        msg = QString("DELETE FROM tvchain "
                      "WHERE chainid NOT IN ( %1 ) AND endtime < now();")
                      .arg(keepChains);
    }
    deleteQuery.prepare(msg);
    if (!deleteQuery.exec())
        MythDB::DBError("HouseKeeper Cleaning TVChain Table", deleteQuery);
}

void HouseKeeper::CleanupRecordedTables(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deleteQuery(MSqlQuery::InitCon());
    int tableIndex = 0;
    // tables[tableIndex][0] is the table name
    // tables[tableIndex][1] is the name of the column on which the join is
    // performed
    QString tables[][2] = {
        { "recordedprogram", "progstart" },
        { "recordedrating", "progstart" },
        { "recordedcredits", "progstart" },
        { "recordedmarkup", "starttime" },
        { "recordedseek", "starttime" },
        { "", "" } }; // This blank entry must exist, do not remove.
    QString table = tables[tableIndex][0];
    QString column = tables[tableIndex][1];

    // Because recordedseek can have millions of rows, we don't want to JOIN it
    // with recorded.  Instead, pull out DISTINCT chanid and starttime into a
    // temporary table (resulting in tens, hundreds, or--at most--a few
    // thousand rows) for the JOIN
    QString querystr;
    querystr = "CREATE TEMPORARY TABLE IF NOT EXISTS temprecordedcleanup ( "
                   "chanid int(10) unsigned NOT NULL default '0', "
                   "starttime datetime NOT NULL default '0000-00-00 00:00:00' "
                   ");";

    if (!query.exec(querystr))
    {
        MythDB::DBError("Housekeeper Creating Temporary Table", query);
        return;
    }

    while (!table.isEmpty())
    {
        query.prepare(QString("TRUNCATE TABLE temprecordedcleanup;"));
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("Housekeeper Truncating Temporary Table", query);
            return;
        }

        query.prepare(QString("INSERT INTO temprecordedcleanup "
                              "( chanid, starttime ) "
                              "SELECT DISTINCT chanid, starttime "
                              "FROM %1;")
                              .arg(table));

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("HouseKeeper Cleaning Recorded Tables", query);
            return;
        }

        query.prepare(QString("SELECT DISTINCT p.chanid, p.starttime "
                              "FROM temprecordedcleanup p "
                              "LEFT JOIN recorded r "
                              "ON p.chanid = r.chanid "
                              "AND p.starttime = r.%1 "
                              "WHERE r.chanid IS NULL;").arg(column));
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("HouseKeeper Cleaning Recorded Tables", query);
            return;
        }

        deleteQuery.prepare(QString("DELETE FROM %1 "
                                    "WHERE chanid = :CHANID "
                                    "AND starttime = :STARTTIME;")
                                    .arg(table));
        while (query.next())
        {
            deleteQuery.bindValue(":CHANID", query.value(0).toString());
            deleteQuery.bindValue(":STARTTIME", query.value(1));
            if (!deleteQuery.exec())
                MythDB::DBError("HouseKeeper Cleaning Recorded Tables",
                                deleteQuery);
        }

        tableIndex++;
        table = tables[tableIndex][0];
        column = tables[tableIndex][1];
    }

    if (!query.exec("DROP TABLE temprecordedcleanup;"))
        MythDB::DBError("Housekeeper Dropping Temporary Table", query);

}

void HouseKeeper::CleanupProgramListings(void)
{

    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;
    // Keep as many days of listings data as we keep matching, non-recorded
    // oldrecorded entries to allow for easier post-mortem analysis
    int offset = gCoreContext->GetNumSetting( "CleanOldRecorded", 10);
    // Also make sure to keep enough data so that we can flag the original
    // airdate, for when that isn't included in guide data
    int newEpiWindow = gCoreContext->GetNumSetting( "NewEpisodeWindow", 14);
    if (newEpiWindow > offset)
        offset = newEpiWindow;

    query.prepare("DELETE FROM oldprogram WHERE airdate < "
                  "DATE_SUB(CURRENT_DATE, INTERVAL 320 DAY);");
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("REPLACE INTO oldprogram (oldtitle,airdate) "
                  "SELECT title,starttime FROM program "
                  "WHERE starttime < NOW() AND manualid = 0 "
                  "GROUP BY title;");
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM program WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM programrating WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM programgenres WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM credits WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM record WHERE (type = :SINGLE "
                  "OR type = :OVERRIDE OR type = :DONTRECORD) "
                  "AND enddate < CURDATE();");
    query.bindValue(":SINGLE", kSingleRecord);
    query.bindValue(":OVERRIDE", kOverrideRecord);
    query.bindValue(":DONTRECORD", kDontRecord);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    MSqlQuery findq(MSqlQuery::InitCon());
    findq.prepare("SELECT record.recordid FROM record "
                  "LEFT JOIN oldfind ON oldfind.recordid = record.recordid "
                  "WHERE type = :FINDONE AND oldfind.findid IS NOT NULL;");
    findq.bindValue(":FINDONE", kOneRecord);

    if (findq.exec())
    {
        query.prepare("DELETE FROM record WHERE recordid = :RECORDID;");
        while (findq.next())
        {
            query.bindValue(":RECORDID", findq.value(0).toInt());
            if (!query.exec())
                MythDB::DBError("HouseKeeper Cleaning Program Listings", query);
        }
    }
    query.prepare("DELETE FROM oldfind WHERE findid < TO_DAYS(NOW()) - 14;");
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

    query.prepare("DELETE FROM oldrecorded WHERE "
                  "recstatus <> :RECORDED AND duplicate = 0 AND "
                  "endtime < DATE_SUB(CURRENT_DATE, INTERVAL :CLEAN DAY);");
    query.bindValue(":RECORDED", rsRecorded);
    query.bindValue(":CLEAN", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);

}

void HouseKeeper::UpdateThemeChooserInfoCache(void)
{
    QString MythVersion = MYTH_SOURCE_PATH;

    // FIXME: For now, treat git master the same as svn trunk
    if (MythVersion == "master")
        MythVersion = "trunk";

    if (MythVersion != "trunk")
    {
        MythVersion = MYTH_BINARY_VERSION; // Example: 0.25.20101017-1
        MythVersion.replace(QRegExp("\\.[0-9]{8,}.*"), "");
    }

    QString remoteThemesDir = GetConfDir();
    remoteThemesDir.append("/tmp/remotethemes");

    QDir dir(remoteThemesDir);
    if (!dir.exists() && !dir.mkpath(remoteThemesDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error creating %1"
                    "directory for remote themes info cache.")
                .arg(remoteThemesDir));
        return;
    }

    QString remoteThemesFile = remoteThemesDir;
    remoteThemesFile.append("/themes.zip");

    QString url = QString("%1/%2/themes.zip")
        .arg(gCoreContext->GetSetting("ThemeRepositoryURL",
             "http://themes.mythtv.org/themes/repository")).arg(MythVersion);

    bool result = GetMythDownloadManager()->download(url, remoteThemesFile);

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error downloading %1"
                    "remote themes info package.").arg(url));
        return;
    }

    if (!extractZIP(remoteThemesFile, remoteThemesDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error extracting %1"
                    "remote themes info package.").arg(remoteThemesFile));
        QFile::remove(remoteThemesFile);
        return;
    }
}

void HouseKeeper::UpdateRecordedArtwork(void)
{
    QString command = GetInstallPrefix() + "/bin/mythmetadatalookup";
    QStringList args;
    args << "--refresh-all-artwork";
    args << logPropagateArgs;

    LOG(VB_GENERAL, LOG_INFO, QString("Performing Artwork Refresh: %1 %2")
        .arg(command).arg(args.join(" ")));

    MythSystemLegacy artupd(command, args, kMSRunShell | kMSAutoCleanup);

    artupd.Run();
    artupd.Wait();

    LOG(VB_GENERAL, LOG_INFO, QString("Artwork Refresh Complete"));
}

void HouseKeeper::RunStartupTasks(void)
{
    if (isMaster)
        EITCache::ClearChannelLocks();
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
