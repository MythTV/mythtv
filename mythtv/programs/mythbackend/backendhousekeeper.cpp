// POSIX headers
#include <unistd.h>
#include <sys/types.h>

// ANSI C headers
#include <algorithm>
#include <cstdlib>

// Qt headers
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

// MythTV headers
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythdownloadmanager.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsystemlegacy.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programtypes.h"
#include "libmythbase/recordingstatus.h"
#include "libmythbase/recordingtypes.h"
#include "libmythbase/unziputil.h"
#include "libmythmetadata/musicmetadata.h"
#include "libmythtv/jobqueue.h"

// MythBackend
#include "backendhousekeeper.h"

static constexpr int64_t kFourHours {4LL * 60 * 60};

bool CleanupTask::DoRun(void)
{
    JobQueue::CleanupOldJobsInQueue();
    CleanupOldRecordings();
    CleanupInUsePrograms();
    CleanupOrphanedLiveTV();
    CleanupRecordedTables();
    CleanupChannelTables();
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
    QDateTime fourHoursAgo = MythDate::current().addSecs(-kFourHours);
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE lastupdatetime < :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupInUsePrograms", query);
}

void CleanupTask::CleanupOrphanedLiveTV(void)
{
    QDateTime fourHoursAgo = MythDate::current().addSecs(-kFourHours);
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

    QString msg;
    QString keepChains;
    while (query.next())
    {
        if (keepChains.isEmpty())
            keepChains = "'" + query.value(0).toString() + "'";
        else
            keepChains += ", '" + query.value(0).toString() + "'";
    }

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
    // tables[tableIndex][0] is the table name
    // tables[tableIndex][1] is the name of the column on which the join is
    // performed
    std::array<std::array<QString,2>,5> tables {{
        { "recordedprogram", "progstart" },
        { "recordedrating", "progstart" },
        { "recordedcredits", "progstart" },
        { "recordedmarkup", "starttime" },
        { "recordedseek", "starttime" },
    }};

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

    for (const auto & [table,column] : tables)
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
    }

    if (!query.exec("DROP TABLE temprecordedcleanup;"))
        MythDB::DBError("CleanupTask::CleanupRecordedTables"
                                "(deleting temporary table)", query);
}

void CleanupTask::CleanupChannelTables(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deleteQuery(MSqlQuery::InitCon());

    // Delete all channels from the database that have already
    // been deleted for at least one day and that are not referenced
    // anymore by a recording.
    query.prepare(QString("DELETE channel "
                          "FROM channel "
                          "LEFT JOIN recorded r "
                          "    ON r.chanid = channel.chanid "
                          "LEFT JOIN oldrecorded o "
                          "    ON o.chanid = channel.chanid "
                          "WHERE channel.deleted IS NOT NULL "
                          "      AND channel.deleted < "
                          "          DATE_SUB(NOW(), INTERVAL 1 DAY) "
                          "      AND r.chanid IS NULL "
                          "      AND o.chanid IS NULL"));
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupChannelTables "
                        "(channel table)", query);

    // Delete all multiplexes from the database that are not
    // referenced anymore by a channel.
    query.prepare(QString("DELETE dtv_multiplex "
                          "FROM dtv_multiplex "
                          "LEFT JOIN channel c "
                          "    ON c.mplexid = dtv_multiplex.mplexid "
                          "WHERE c.chanid IS NULL"));
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupChannelTables "
                        "(dtv_multiplex table)", query);

    // Delete all IPTV channel data extension records from the database
    // when the channel it refers to does not exist anymore.
    query.prepare(QString("DELETE iptv_channel "
                          "FROM iptv_channel "
                          "LEFT JOIN channel c "
                          "    ON c.chanid = iptv_channel.chanid "
                          "WHERE c.chanid IS NULL"));
    if (!query.exec())
        MythDB::DBError("CleanupTask::CleanupChannelTables "
                        "(iptv_channel table)", query);
}

void CleanupTask::CleanupProgramListings(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    // Keep as many days of listings data as we keep matching, non-recorded
    // oldrecorded entries to allow for easier post-mortem analysis
    int offset = gCoreContext->GetNumSetting( "CleanOldRecorded", 10);
    // Also make sure to keep enough data so that we can flag the original
    // airdate, for when that isn't included in guide data
    int newEpiWindow = gCoreContext->GetNumSetting( "NewEpisodeWindow", 14);
    offset = std::max(newEpiWindow, offset);

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
    query.bindValue(":RECORDED", RecStatus::Recorded);
    query.bindValue(":CLEAN", offset);
    if (!query.exec())
        MythDB::DBError("HouseKeeper Cleaning Program Listings", query);
}

bool ThemeUpdateTask::DoCheckRun(const QDateTime& now)
{
    return gCoreContext->GetBoolSetting("ThemeUpdateNofications", true) &&
            PeriodicHouseKeeperTask::DoCheckRun(now);
}

bool ThemeUpdateTask::DoRun(void)
{
    uint major { 0 };
    uint minor { 0 };
    bool devel { false };
    bool parsed = ParseMythSourceVersion(devel, major, minor);
    bool result = false;

    if (!parsed || devel)
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Loading themes for devel"));
        result |= LoadVersion("trunk", LOG_ERR);
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, QString("Loading themes for %1").arg(major));
        result |= LoadVersion(QString::number(major), LOG_ERR);

        for (int i = minor ; i > 0; i--)
        {
            QString majmin = QString("%1.%2").arg(major).arg(i);
            LOG(VB_GENERAL, LOG_INFO,
                QString("Loading themes for %1").arg(majmin));
            result |= LoadVersion(majmin, LOG_INFO);
        }
    }
    return result;
}

bool ThemeUpdateTask::LoadVersion(const QString &version, int download_log_level)
{
    QString remoteThemesDir = GetConfDir();
    remoteThemesDir.append("/tmp/remotethemes");

    QDir dir(remoteThemesDir);
    if (!dir.exists() && !dir.mkpath(remoteThemesDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error creating %1 "
                    "directory for remote themes info cache.")
                .arg(remoteThemesDir));
        return false;
    }

    QString remoteThemesFile = remoteThemesDir;
    remoteThemesFile.append("/themes.zip");

    m_url = QString("%1/%2/themes.zip")
            .arg(gCoreContext->GetSetting("ThemeRepositoryURL",
                          "http://themes.mythtv.org/themes/repository"),
                 version);

    m_running = true;
    bool result = GetMythDownloadManager()->download(m_url, remoteThemesFile);
    m_running = false;

    if (!result)
    {
        LOG(VB_GENERAL, (LogLevel_t)download_log_level,
            QString("HouseKeeper: Failed to download %1 "
                    "remote themes info package.").arg(m_url));
        return false;
    }

    if (!extractZIP(remoteThemesFile, remoteThemesDir))
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("HouseKeeper: Error extracting %1 "
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

RadioStreamUpdateTask::RadioStreamUpdateTask(void)
    : DailyHouseKeeperTask("UpdateRadioStreams", kHKGlobal, kHKRunOnStartup)
{
}

bool RadioStreamUpdateTask::DoRun(void)
{
    if (m_msMU)
    {
        // this should never be defined, but terminate it anyway
        if (m_msMU->GetStatus() == GENERIC_EXIT_RUNNING)
            m_msMU->Term(true);
        delete m_msMU;
        m_msMU = nullptr;
    }

    QString command = GetAppBinDir() + "mythutil";
    QStringList args;
    args << "--updateradiostreams";
    args << logPropagateArgs;

    LOG(VB_GENERAL, LOG_INFO, QString("Performing Radio Streams Update: %1 %2")
        .arg(command, args.join(" ")));

    m_msMU = new MythSystemLegacy(command, args, kMSRunShell | kMSAutoCleanup);

    m_msMU->Run();
    uint result = m_msMU->Wait();

    delete m_msMU;
    m_msMU = nullptr;

    if (result != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Update Radio Streams command '%1' failed")
            .arg(command));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Radio Streams Update Complete"));
    return true;
}

RadioStreamUpdateTask::~RadioStreamUpdateTask(void)
{
    delete m_msMU;
    m_msMU = nullptr;
}

bool RadioStreamUpdateTask::DoCheckRun(const QDateTime& now)
{
    // we are only interested in the global setting so remove any local host setting just in case
    QString setting = GetMythDB()->GetSettingOnHost("MusicStreamListModified", gCoreContext->GetHostName(), "");
    if (!setting.isEmpty())
    {
        GetMythDB()->ClearSetting("MusicStreamListModified");
    }

    // check we are not already running a radio stream update
    return gCoreContext->GetSetting("MusicStreamListModified") == "Updating" &&
            PeriodicHouseKeeperTask::DoCheckRun(now);
}

void RadioStreamUpdateTask::Terminate(void)
{
    if (m_msMU && (m_msMU->GetStatus() == GENERIC_EXIT_RUNNING))
        // just kill it, the runner thread will handle any necessary cleanup
        m_msMU->Term(true);
}

ArtworkTask::ArtworkTask(void)
    : DailyHouseKeeperTask("RecordedArtworkUpdate", kHKGlobal, kHKRunOnStartup)
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
        m_msMML = nullptr;
    }

    QString command = GetAppBinDir() + "mythmetadatalookup";
    QStringList args;
    args << "--refresh-all-artwork";
    args << logPropagateArgs;

    LOG(VB_GENERAL, LOG_INFO, QString("Performing Artwork Refresh: %1 %2")
        .arg(command, args.join(" ")));

    m_msMML = new MythSystemLegacy(command, args, kMSRunShell | kMSAutoCleanup);

    m_msMML->Run();
    uint result = m_msMML->Wait();

    delete m_msMML;
    m_msMML = nullptr;

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
    m_msMML = nullptr;
}

bool ArtworkTask::DoCheckRun(const QDateTime& now)
{
    return gCoreContext->GetBoolSetting("DailyArtworkUpdates", false) &&
            PeriodicHouseKeeperTask::DoCheckRun(now);
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
    DailyHouseKeeperTask("MythFillDB")
{
    SetHourWindowFromDB();
}

void MythFillDatabaseTask::SetHourWindowFromDB(void)
{
    // we need to set the time window from database settings, so we cannot
    // initialize these values in. grab them and set them afterwards
    auto min = gCoreContext->GetDurSetting<std::chrono::hours>("MythFillMinHour", -1h);
    auto max = gCoreContext->GetDurSetting<std::chrono::hours>("MythFillMaxHour", 23h);

    if (min == -1h)
    {
        min = 0h;
        max = 23h;
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
//     if (!gCoreContext->GetBoolSetting("MythFillGrabberSuggestsTime", true))
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
//                        "        ( 'technovera' );");
//         if ((result.exec()) &&
//             (result.next()) &&
//             (result.value(0).toInt() > 0))
//                 return true;
//     }

    return gCoreContext->GetBoolSetting("MythFillGrabberSuggestsTime", true);
}

bool MythFillDatabaseTask::DoCheckRun(const QDateTime& now)
{
    if (!gCoreContext->GetBoolSetting("MythFillEnabled", true))
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

        // Delete the cached value immediately.  Necessary because the
        // value is written/read by different applications.
        GetMythDB()->ClearSettingsCache("MythFillSuggestedRunTime");

        // is it yet time
        return nextRun <= now;
    }
    if (InWindow(now))
        // we're inside our permitted window
        return true;

    // just let DailyHouseKeeperTask handle things
    LOG(VB_GENERAL, LOG_DEBUG, "Performing daily run check.");
    return DailyHouseKeeperTask::DoCheckRun(now);
}

bool MythFillDatabaseTask::DoRun(void)
{
    if (m_msMFD)
    {
        // this should never be defined, but terminate it anyway
        if (m_msMFD->GetStatus() == GENERIC_EXIT_RUNNING)
            m_msMFD->Term(true);
        delete m_msMFD;
        m_msMFD = nullptr;
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

    QString cmd = QString("%1 %2").arg(mfpath, mfarg);

    m_msMFD = new MythSystemLegacy(cmd, opts);

    m_msMFD->Run();
    uint result = m_msMFD->Wait();

    delete m_msMFD;
    m_msMFD = nullptr;

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
    m_msMFD = nullptr;
}

void MythFillDatabaseTask::Terminate(void)
{
    if (m_msMFD && (m_msMFD->GetStatus() == GENERIC_EXIT_RUNNING))
        // just kill it, the runner thread will handle any necessary cleanup
        m_msMFD->Term(true);
}
