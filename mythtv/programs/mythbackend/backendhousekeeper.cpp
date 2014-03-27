// POSIX headers
#include <unistd.h>
#include <sys/types.h>

// ANSI C headers
#include <cstdlib>

// Qt headers
#include <QStringList>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

// MythTV headers
#include "backendhousekeeper.h"
#include "mythdb.h"
#include "mythdirs.h"
#include "jobqueue.h"
#include "exitcodes.h"
#include "mythsystemlegacy.h"
#include "mythversion.h"
#include "mythcoreutil.h"
#include "programtypes.h"
#include "recordingtypes.h"
#include "mythcorecontext.h"
#include "mythdownloadmanager.h"


bool LogCleanerTask::DoRun(void)
{
    int numdays = 14;
    uint64_t maxrows = 10000 * numdays;  // likely high enough to keep numdays
    bool res = true;

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
        {
            MythDB::DBError("Delete helper application log entries", query);
            res = false;
        }

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
        {
            MythDB::DBError("Delete old log entries", query);
            res = false;
        }

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
                {
                    MythDB::DBError("Delete excess log entries", query);
                    res = false;
                }
            }
        }
        else
        {
            MythDB::DBError("Query logging table size", query);
            res = false;
        }
    }

    return res;
}

bool CleanupTask::DoRun(void)
{
    JobQueue::CleanupOldJobsInQueue();
    CleanupOldRecordings();
    CleanupInUsePrograms();
    CleanupOrphanedLiveTV();
    CleanupRecordedTables();
    CleanupProgramListings();
    return true;
}

void CleanupTask::CleanupOldRecordings(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME AND "
                    "( recusage = 'recorder' OR recusage LIKE 'Unknown %' );");
    query.bindValue(":HOSTNAME", gCoreContext->GetHostName());
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupOldRecordings", query);
}

void CleanupTask::CleanupInUsePrograms(void)
{
    QDateTime fourHoursAgo = MythDate::current().addSecs(-4 * 60 * 60);
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE lastupdatetime < :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupInUsePrograms", query);
}

void CleanupTask::CleanupOrphanedLiveTV(void)
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
        MythDB::DBError("CleanupTask::CleanupOrphanedLiveTV", deleteQuery);
}

void CleanupTask::CleanupRecordedTables(void)
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
        MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                "(creating temporary table)", query);
        return;
    }

    while (!table.isEmpty())
    {
        query.prepare(QString("TRUNCATE TABLE temprecordedcleanup;"));
        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                  "(truncating temporary table)", query);
            return;
        }

        query.prepare(QString("INSERT INTO temprecordedcleanup "
                              "( chanid, starttime ) "
                              "SELECT DISTINCT chanid, starttime "
                              "FROM %1;")
                              .arg(table));

        if (!query.exec() || !query.isActive())
        {
            MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                    "(cleaning recorded tables)", query);
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
            MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                    "(cleaning recorded tables)", query);
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
            {
                MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                "(cleaning recorded tables)", deleteQuery);
                return;
            }
        }

        tableIndex++;
        table = tables[tableIndex][0];
        column = tables[tableIndex][1];
    }

    if (!query.exec("DROP TABLE temprecordedcleanup;"))
        MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                "(deleting temporary table)", query);
}

void CleanupTask::CleanupProgramListings(void)
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

bool ThemeUpdateTask::DoRun(void)
{
    QString MythVersion = MYTH_SOURCE_PATH;

    // Treat devel branches as master
    if (MythVersion.startsWith("devel/"))
        MythVersion = "master";

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
        return false;
    }

    QString remoteThemesFile = remoteThemesDir;
    remoteThemesFile.append("/themes.zip");

    m_url = QString("%1/%2/themes.zip")
        .arg(gCoreContext->GetSetting("ThemeRepositoryURL",
             "http://themes.mythtv.org/themes/repository")).arg(MythVersion);

    m_running = true;
    bool result = GetMythDownloadManager()->download(m_url, remoteThemesFile);
    m_running = false;

    if (!result)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error downloading %1"
                    "remote themes info package.").arg(m_url));
        return false;
    }

    if (!extractZIP(remoteThemesFile, remoteThemesDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error extracting %1"
                    "remote themes info package.").arg(remoteThemesFile));
        QFile::remove(remoteThemesFile);
        return false;
    }

    return true;
}

void ThemeUpdateTask::Terminate(void)
{
    if (m_running)
        GetMythDownloadManager()->cancelDownload(m_url);
    m_running = false;
}

ArtworkTask::ArtworkTask(void) : DailyHouseKeeperTask("RecordedArtworkUpdate",
                                         kHKGlobal, kHKRunOnStartup),
    m_msMML(NULL)
{
}

bool ArtworkTask::DoRun(void)
{
    if (m_msMML)
    {
        // this should never be defined, but terminate it anyway
        if (m_msMML->GetStatus() == GENERIC_EXIT_RUNNING)
            m_msMML->Term(true);
        delete m_msMML;
        m_msMML = NULL;
    }

    QString command = GetAppBinDir() + "mythmetadatalookup";
    QStringList args;
    args << "--refresh-all-artwork";
    args << logPropagateArgs;

    LOG(VB_GENERAL, LOG_INFO, QString("Performing Artwork Refresh: %1 %2")
        .arg(command).arg(args.join(" ")));

    m_msMML = new MythSystemLegacy(command, args, kMSRunShell | kMSAutoCleanup);

    m_msMML->Run();
    uint result = m_msMML->Wait();

    delete m_msMML;
    m_msMML = NULL;

    if (result != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Artwork command '%1' failed")
            .arg(command));
        return false;
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Artwork Refresh Complete"));
    return true;
}

ArtworkTask::~ArtworkTask(void)
{
    delete m_msMML;
    m_msMML = NULL;
}

bool ArtworkTask::DoCheckRun(QDateTime now)
{
    if (gCoreContext->GetNumSetting("DailyArtworkUpdates", 0) &&
            PeriodicHouseKeeperTask::DoCheckRun(now))
        return true;
    return false;
}

void ArtworkTask::Terminate(void)
{
    if (m_msMML && (m_msMML->GetStatus() == GENERIC_EXIT_RUNNING))
        // just kill it, the runner thread will handle any necessary cleanup
        m_msMML->Term(true);
}

bool JobQueueRecoverTask::DoRun(void)
{
    JobQueue::RecoverOldJobsInQueue();
    return true;
}

MythFillDatabaseTask::MythFillDatabaseTask(void) :
    DailyHouseKeeperTask("MythFillDB"), m_msMFD(NULL)
{
    SetHourWindowFromDB();
}

void MythFillDatabaseTask::SetHourWindowFromDB(void)
{
    // we need to set the time window from database settings, so we cannot
    // initialize these values in. grab them and set them afterwards
    int min = gCoreContext->GetNumSetting("MythFillMinHour", -1);
    int max = gCoreContext->GetNumSetting("MythFillMaxHour", 23);

    if (min == -1)
    {
        min = 0;
        max = 23;
    }
    else
    {
        // make sure they fall within the range of 0-23
        min %= 24;
        max %= 24;
    }

    DailyHouseKeeperTask::SetHourWindow(min, max);
}

bool MythFillDatabaseTask::UseSuggestedTime(void)
{
//     if (!gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime", 1))
//         // this feature is disabled, so don't bother with a deeper check
//         return false;
//
//     MSqlQuery result(MSqlQuery::InitCon());
//     if (result.isConnected())
//     {
//         // check to see if we have any of a list of supported grabbers in use
//         // TODO: this is really cludgy. there has to be a better way to test
//         result.prepare("SELECT COUNT(*) FROM videosource"
//                        " WHERE xmltvgrabber IN"
//                        "        ( 'datadirect',"
//                        "          'technovera',"
//                        "          'schedulesdirect1' );");
//         if ((result.exec()) &&
//             (result.next()) &&
//             (result.value(0).toInt() > 0))
//                 return true;
//     }

    return gCoreContext->GetNumSetting("MythFillGrabberSuggestsTime", 1);
}

bool MythFillDatabaseTask::DoCheckRun(QDateTime now)
{
    if (!gCoreContext->GetNumSetting("MythFillEnabled", 1))
    {
        // we don't want to run this manually, so abort early
        LOG(VB_GENERAL, LOG_DEBUG, "MythFillDatabase is disabled. Cannot run.");
        return false;
    }

//    if (m_running)
//        // we're still running from the previous pass, so abort early
//        return false;

    if (UseSuggestedTime())
    {
        QDateTime nextRun = MythDate::fromString(
            gCoreContext->GetSetting("MythFillSuggestedRunTime",
                                     "1970-01-01T00:00:00"));
        LOG(VB_GENERAL, LOG_DEBUG,
                QString("MythFillDatabase scheduled to run at %1.")
                    .arg(nextRun.toString()));
        if (nextRun > now)
            // not yet time
            return false;

        return true;
    }
    else if (InWindow(now))
        // we're inside our permitted window
        return true;

    else
    {
        // just let DailyHouseKeeperTask handle things
        LOG(VB_GENERAL, LOG_DEBUG, "Performing daily run check.");
        return DailyHouseKeeperTask::DoCheckRun(now);
    }
}

bool MythFillDatabaseTask::DoRun(void)
{
    if (m_msMFD)
    {
        // this should never be defined, but terminate it anyway
        if (m_msMFD->GetStatus() == GENERIC_EXIT_RUNNING)
            m_msMFD->Term(true);
        delete m_msMFD;
        m_msMFD = NULL;
    }

    QString mfpath = gCoreContext->GetSetting("MythFillDatabasePath",
                                        "mythfilldatabase");
    QString mfarg = gCoreContext->GetSetting("MythFillDatabaseArgs", "");

    uint opts = kMSRunShell | kMSAutoCleanup;
    if (mfpath == "mythfilldatabase")
    {
        opts |= kMSPropagateLogs;
        mfpath = GetAppBinDir() + "mythfilldatabase";
    }

    QString cmd = QString("%1 %2").arg(mfpath).arg(mfarg);

    m_msMFD = new MythSystemLegacy(cmd, opts);

    m_msMFD->Run();
    uint result = m_msMFD->Wait();

    delete m_msMFD;
    m_msMFD = NULL;

    if (result != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("MythFillDatabase command '%1' failed")
                .arg(cmd));
        return false;
    }

    return true;
}

MythFillDatabaseTask::~MythFillDatabaseTask(void)
{
    delete m_msMFD;
    m_msMFD = NULL;
}

void MythFillDatabaseTask::Terminate(void)
{
    if (m_msMFD && (m_msMFD->GetStatus() == GENERIC_EXIT_RUNNING))
        // just kill it, the runner thread will handle any necessary cleanup
        m_msMFD->Term(true);
}
