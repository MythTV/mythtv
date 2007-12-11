#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qstringlist.h>
#include <qfileinfo.h>

#include <iostream>
#include <cstdlib>
using namespace std;

#include "housekeeper.h"
#include "jobqueue.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/util.h"
#include "libmyth/compat.h"

#include "programinfo.h"

#include "libmythtv/eitcache.h"

static bool HouseKeeper_filldb_running = false;

HouseKeeper::HouseKeeper(bool runthread, bool master)
{
    isMaster = master;

    threadrunning = runthread;
    filldbRunning = false;

    CleanupMyOldRecordings();

    if (runthread)
    {
        pthread_t hkthread;
        pthread_create(&hkthread, NULL, doHouseKeepingThread, this);
    }
}

HouseKeeper::~HouseKeeper()
{
}

bool HouseKeeper::wantToRun(const QString &dbTag, int period, int minhour,
                            int maxhour)
{
    bool runOK = false;
    unsigned int oneday = 60 * 60 * 24;
    int longEnough = 0;

    if (period)
        longEnough = ((period * oneday) - oneday/2);
    else
        longEnough = oneday / 8;

    QDateTime now = QDateTime::currentDateTime();
    QDateTime lastrun;
    lastrun.setTime_t(0);

    if (minhour < 0)
        minhour = 0;
    if (maxhour > 23)
        maxhour = 23;

    MSqlQuery result(MSqlQuery::InitCon());
    if (result.isConnected())
    {
        result.prepare("SELECT lastrun FROM housekeeping WHERE tag = :TAG ;");
        result.bindValue(":TAG", dbTag);

        if (result.exec() && result.isActive() && result.size() > 0)
        {
            result.next();
            lastrun = result.value(0).toDateTime();

            if ((lastrun.secsTo(now) > longEnough) &&
                (now.date().day() != lastrun.date().day()))
            {
                int hour = now.time().hour();

                if (((minhour > maxhour) &&
                     ((hour <= maxhour) || (hour >= minhour))) ||
                    ((hour >= minhour) && (hour <= maxhour)))
                {
                    int minute = now.time().minute();
                    if ((hour == maxhour && minute > 30) ||
                        ((random()%(((maxhour-hour)*12+(60-minute)/5 - 6) + 1)) == 0))
                        runOK = true;
                }
            }
        }
        else
        {
            result.prepare("INSERT INTO housekeeping(tag,lastrun) "
                           "values(:TAG ,now());");
            result.bindValue(":TAG", dbTag);
            result.exec();

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
        result.exec();

        result.prepare("INSERT INTO housekeeping(tag,lastrun) "
                       "values(:TAG ,now()) ;");
        result.bindValue(":TAG", dbTag);
        result.exec();
    }
}

QDateTime HouseKeeper::getLastRun(const QString &dbTag)
{
    QDateTime lastRun;
    MSqlQuery result(MSqlQuery::InitCon());

    lastRun.setTime_t(0);

    result.prepare("SELECT lastrun FROM housekeeping WHERE tag = :TAG ;");
    result.bindValue(":TAG", dbTag);

    if (result.exec() && result.isActive() && result.size() > 0)
    {
        result.next();
        lastRun = result.value(0).toDateTime();
    }

    return lastRun;
}

void HouseKeeper::RunHouseKeeping(void)
{
    int period, maxhr, minhr;
    QString dbTag;
    // wait a little for main server to come up and things to settle down
    sleep(10);

    RunStartupTasks();

    while (1)
    {
        gContext->LogEntry("mythbackend", LP_DEBUG,
                           "Running housekeeping thread", "");

        // These tasks are only done from the master backend
        if (isMaster)
        {
            // Clean out old database entries
            if (gContext->GetNumSetting("LogEnabled", 0) &&
                gContext->GetNumSetting("LogCleanEnabled", 0))
            {
                period = gContext->GetNumSetting("LogCleanPeriod",1);
                if (wantToRun("LogClean", period, 0, 24))
                {
                    VERBOSE(VB_GENERAL, "Running LogClean");
                    flushLogs();
                    updateLastrun("LogClean");
                }
            }

            // Run mythfilldatabase to grab the TV listings
            if (gContext->GetNumSetting("MythFillEnabled", 0))
            {
                if (HouseKeeper_filldb_running)
                {
                    VERBOSE(VB_GENERAL, "mythfilldatabase still running, "
                                        "skipping checks.");
                } 
                else 
                {
                    period = gContext->GetNumSetting("MythFillPeriod", 1);
                    minhr = gContext->GetNumSetting("MythFillMinHour", -1);
                    if (minhr == -1)
                    {
                        minhr = 0;
                        maxhr = 24;
                    } 
                    else 
                    {
                        maxhr = gContext->GetNumSetting("MythFillMaxHour", 24);
                    }

                    bool grabberSupportsNextTime = false;
                    MSqlQuery result(MSqlQuery::InitCon());
                    if (result.isConnected())
                    {
                        result.prepare("SELECT COUNT(*) FROM videosource "
                                       "WHERE xmltvgrabber IN "
                                           "( 'datadirect', 'technovera',"
                                           " 'schedulesdirect1' );");

                        if ((result.exec()) &&
                            (result.isActive()) &&
                            (result.size() > 0) &&
                            (result.next()) &&
                            (result.value(0).toInt() > 0))
                            grabberSupportsNextTime = true;
                    }

                    bool runMythFill = false;
                    if (grabberSupportsNextTime &&
                        gContext->GetNumSetting("MythFillGrabberSuggestsTime", 1))
                    {
                        QDateTime nextRun = QDateTime::fromString(
                            gContext->GetSetting("MythFillSuggestedRunTime",
                            "1970-01-01T00:00:00"), Qt::ISODate);
                        QDateTime lastRun = getLastRun("MythFillDB");
                        QDateTime now = QDateTime::currentDateTime();

                        if ((nextRun < now) &&
                            (lastRun.secsTo(now) > (3 * 60 * 60)))
                            runMythFill = true;
                    }
                    else if (wantToRun("MythFillDB", period, minhr, maxhr))
                    {
                        runMythFill = true;
                    }

                    if (runMythFill)
                    {
                        QString msg = "Running mythfilldatabase";
                        gContext->LogEntry("mythbackend", LP_DEBUG, msg, "");
                        VERBOSE(VB_GENERAL, msg);
                        runFillDatabase();
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
        }

        dbTag = QString("JobQueueRecover-%1").arg(gContext->GetHostName());
        if (wantToRun(dbTag, 1, 0, 24))
        {
            JobQueue::RecoverOldJobsInQueue();
            updateLastrun(dbTag);
        }

        sleep(300 + (random()%8));
    }
} 

void HouseKeeper::flushLogs()
{
    int numdays = gContext->GetNumSetting("LogCleanDays", 14);
    int maxdays = gContext->GetNumSetting("LogCleanMax", 30);

    QDateTime days = QDateTime::currentDateTime();
    days = days.addDays(0 - numdays);
    QDateTime max = QDateTime::currentDateTime();
    max = max.addDays(0 - maxdays);

    MSqlQuery result(MSqlQuery::InitCon());
    if (result.isConnected())
    {
        result.prepare("DELETE FROM mythlog WHERE "
                       "acknowledged=1 and logdate < :DAYS ;");
        result.bindValue(":DAYS", days);
        result.exec();

        result.prepare("DELETE FROM mythlog WHERE logdate< :MAX ;");
        result.bindValue(":MAX", max);
        result.exec();
    }
}

void *HouseKeeper::runMFDThread(void *param)
{
    HouseKeeper *keep = (HouseKeeper *)param;
    keep->RunMFD();
    return NULL;
}

void HouseKeeper::RunMFD(void)
{
    QString mfpath = gContext->GetSetting("MythFillDatabasePath",
                                          "mythfilldatabase");
    QString mfarg = gContext->GetSetting("MythFillDatabaseArgs", "");
    QString mflog = gContext->GetSetting("MythFillDatabaseLog",
                                         "/var/log/mythfilldatabase.log");

    if (mfpath == "mythfilldatabase")
        mfpath = gContext->GetInstallPrefix() + "/bin/mythfilldatabase";

    QString command = QString("%1 %2").arg(mfpath).arg(mfarg);

    if (mflog.length())
    {
        bool dir_writable = false;
        QFileInfo testFile(mflog);
        if (testFile.exists() && testFile.isDir() && testFile.isWritable())
        {
            mflog += "/mythfilldatabase.log";
            testFile.setFile(mflog);
            dir_writable = true;
        }

        if (!dir_writable && !testFile.exists())
        {
            dir_writable = QFileInfo(testFile.dirPath()).isWritable();
        }

        if (dir_writable || (testFile.exists() && testFile.isWritable()))
        {
            command = QString("%1 %2 >>%3 2>&1").arg(mfpath).arg(mfarg)
                    .arg(mflog);
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Invalid mythfilldatabase log path: %1 is not "
                            "writable.").arg(mflog));
        }
    }

    myth_system(command.ascii(), MYTH_SYSTEM_DONT_BLOCK_LIRC | 
                                 MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);

    HouseKeeper_filldb_running = false;
}

void HouseKeeper::runFillDatabase()
{
    if (HouseKeeper_filldb_running)
        return;

    HouseKeeper_filldb_running = true;

    pthread_t housekeep_thread;
    pthread_create(&housekeep_thread, NULL, runMFDThread, this);
    pthread_detach(housekeep_thread);
}

void HouseKeeper::CleanupMyOldRecordings(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME AND "
                    "( recusage = 'recorder' OR recusage LIKE 'Unknown %' );");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.exec();
}

void HouseKeeper::CleanupAllOldInUsePrograms(void)
{
    QDateTime fourHoursAgo = QDateTime::currentDateTime().addSecs(-4 * 60 * 60);
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE lastupdatetime < :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);
    query.exec();
}

void HouseKeeper::CleanupOrphanedLivetvChains(void)
{
    QDateTime fourHoursAgo = QDateTime::currentDateTime().addSecs(-4 * 60 * 60);
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deleteQuery(MSqlQuery::InitCon());

    // Keep these tvchains, they may be in use.
    query.prepare("SELECT DISTINCT chainid FROM tvchain "
                  "WHERE endtime > :FOURHOURSAGO ;");
    query.bindValue(":FOURHOURSAGO", fourHoursAgo);

    if (!query.exec() || !query.isActive())
    {
        MythContext::DBError("HouseKeeper Cleaning TVChain Table", query);
        return;
    }

    QString msg, keepChains = "";
    while (query.next())
        if (keepChains == "")
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
    deleteQuery.exec();
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
        MythContext::DBError("Housekeeper Creating Temporary Table", query);
        return;
    }

    while (table != "")
    {
        query.prepare(QString("TRUNCATE TABLE temprecordedcleanup;"));
        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("Housekeeper Truncating Temporary Table",
                                 query);
            return;
        }

        query.prepare(QString("INSERT INTO temprecordedcleanup "
                              "( chanid, starttime ) "
                              "SELECT DISTINCT chanid, starttime "
                              "FROM %1;")
                              .arg(table));

        if (!query.exec() || !query.isActive())
        {
            MythContext::DBError("HouseKeeper Cleaning Recorded Tables", query);
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
            MythContext::DBError("HouseKeeper Cleaning Recorded Tables", query);
            return;
        }

        deleteQuery.prepare(QString("DELETE FROM %1 "
                                    "WHERE chanid = :CHANID "
                                    "AND starttime = :STARTTIME;")
                                    .arg(table));
        while (query.next())
        {
            deleteQuery.bindValue(":CHANID", query.value(0).toString());
            deleteQuery.bindValue(":STARTTIME", query.value(1).toString());
            deleteQuery.exec();
        }

        tableIndex++;
        table = tables[tableIndex][0];
        column = tables[tableIndex][1];
    }

    if (!query.exec("DROP TABLE temprecordedcleanup;"))
        MythContext::DBError("Housekeeper Dropping Temporary Table", query);

}

void HouseKeeper::CleanupProgramListings(void)
{

    MSqlQuery query(MSqlQuery::InitCon());
    QString querystr;
    // We keep seven days of guide data
    int offset = 7;

    query.prepare("DELETE FROM oldprogram WHERE airdate < "
                  "DATE_SUB(CURRENT_DATE, INTERVAL 320 DAY);");
    query.exec();

    query.prepare("REPLACE INTO oldprogram (oldtitle,airdate) "
                  "SELECT title,starttime FROM program "
                  "WHERE starttime < NOW() AND manualid = 0 "
                  "GROUP BY title;");
    query.exec();

    query.prepare("DELETE FROM program WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM programrating WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM programgenres WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM credits WHERE starttime <= "
                  "DATE_SUB(CURRENT_DATE, INTERVAL :OFFSET DAY);");
    query.bindValue(":OFFSET", offset);
    query.exec();

    query.prepare("DELETE FROM record WHERE (type = :SINGLE "
                  "OR type = :OVERRIDE OR type = :DONTRECORD) "
                  "AND enddate < CURDATE();");
    query.bindValue(":SINGLE", kSingleRecord);
    query.bindValue(":OVERRIDE", kOverrideRecord);
    query.bindValue(":DONTRECORD", kDontRecord);
    query.exec();

    MSqlQuery findq(MSqlQuery::InitCon());
    findq.prepare("SELECT record.recordid FROM record "
                  "LEFT JOIN oldfind ON oldfind.recordid = record.recordid "
                  "WHERE type = :FINDONE AND oldfind.findid IS NOT NULL;");
    findq.bindValue(":FINDONE", kFindOneRecord);
    findq.exec();

    if (findq.isActive() && findq.size() > 0)
    {
        while (findq.next())
        {
            query.prepare("DELETE FROM record WHERE recordid = :RECORDID;");
            query.bindValue(":RECORDID", findq.value(0).toInt());
            query.exec();
        }
    }
    query.prepare("DELETE FROM oldfind WHERE findid < TO_DAYS(NOW()) - 14;");
    query.exec();

    int cleanOldRecorded = gContext->GetNumSetting( "CleanOldRecorded", 10);

    query.prepare("DELETE FROM oldrecorded WHERE "
                  "recstatus <> :RECORDED AND duplicate = 0 AND "
                  "endtime < DATE_SUB(CURRENT_DATE, INTERVAL :CLEAN DAY);");
    query.bindValue(":RECORDED", rsRecorded);
    query.bindValue(":CLEAN", cleanOldRecorded);
    query.exec();

}


void HouseKeeper::RunStartupTasks(void)
{
    if (isMaster)
        EITCache::ClearChannelLocks();
}


void *HouseKeeper::doHouseKeepingThread(void *param)
{
    HouseKeeper *hkeeper = (HouseKeeper*)param;
    hkeeper->RunHouseKeeping();
 
    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
