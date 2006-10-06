#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdatetime.h>
#include <qstringlist.h>

#include <iostream>
using namespace std;

#include "housekeeper.h"
#include "jobqueue.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/util.h"

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
    unsigned int longEnough = 0;

    if (period)
        longEnough = ((period * oneday) - 600);
    else
        longEnough = oneday / 8;

    QDateTime now = QDateTime::currentDateTime();
    QDateTime lastrun;
    lastrun.setTime_t(0);

    MSqlQuery result(MSqlQuery::InitCon());
    if (result.isConnected())
    {
        result.prepare("SELECT lastrun FROM housekeeping WHERE tag = :TAG ;");
        result.bindValue(":TAG", dbTag);

        if (result.exec() && result.isActive() && result.size() > 0)
        {
            result.next();
            lastrun = result.value(0).toDateTime();

            if ((now.toTime_t() - lastrun.toTime_t()) > longEnough)
            {
                int hour = now.toString(QString("h")).toInt();
                if ((hour >= minhour) && (hour <= maxhour))
                    runOK = true;
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
                                           "( 'datadirect', 'technovera' );");

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
                        int hour = now.toString(QString("h")).toInt();

                        if ((nextRun < now) &&
                            (lastRun.secsTo(now) > (3 * 60 * 60)) &&
                            ((minhr <= hour) && (hour <= maxhr)))
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
                CleanupRecordedTables();
                updateLastrun("DailyCleanup");
            }
        }

        dbTag = QString("JobQueueRecover-%1").arg(gContext->GetHostName());
        if (wantToRun(dbTag, 1, 0, 24))
        {
            JobQueue::RecoverOldJobsInQueue();
            updateLastrun(dbTag);
        }

        sleep(300);
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
    QString command;

    QString mfpath = gContext->GetSetting("MythFillDatabasePath",
                                          "mythfilldatabase");
    QString mfarg = gContext->GetSetting("MythFillDatabaseArgs", "");
    QString mflog = gContext->GetSetting("MythFillDatabaseLog",
                                         "/var/log/mythfilldatabase.log");

    if (mflog == "")
        command = QString("%1 %2").arg(mfpath).arg(mfarg);
    else
        command = QString("%1 %2 >>%3 2>&1").arg(mfpath).arg(mfarg).arg(mflog);

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

void HouseKeeper::CleanupRecordedTables(void)
{
    MSqlQuery query(MSqlQuery::InitCon());
    MSqlQuery deleteQuery(MSqlQuery::InitCon());
    int tableIndex = 0;
    QString tables[] = {
        "recordedprogram",
        "recordedrating",
        "recordedcredits",
        "" }; // This blank entry must exist, do not remove.
    QString table = tables[tableIndex];
   
    while (table != "")
    {
        query.prepare(QString("SELECT DISTINCT p.chanid, p.starttime "
                              "FROM %1 p LEFT JOIN recorded r "
                              "ON p.chanid = r.chanid "
                              "AND p.starttime = r.progstart "
                              "WHERE r.chanid IS NULL;")
                              .arg(table));
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
        table = tables[tableIndex];
    }
}

void *HouseKeeper::doHouseKeepingThread(void *param)
{
    HouseKeeper *hkeeper = (HouseKeeper*)param;
    hkeeper->RunHouseKeeping();
 
    return NULL;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
