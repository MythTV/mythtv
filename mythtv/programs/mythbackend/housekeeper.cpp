#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
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

bool HouseKeeper_filldb_running = false;

void reapChild(int /* sig */)
{
    (void)wait(0);
    HouseKeeper_filldb_running = false;
}

HouseKeeper::HouseKeeper(bool runthread, bool master)
{
    isMaster = master;

    threadrunning = runthread;
    filldbRunning = false;

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

            if ((now.toTime_t() - lastrun.toTime_t()) > period * oneday)
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

                    if (wantToRun("MythFillDB", period, minhr, maxhr))
                    {
                        QString msg = "Running mythfilldatabase";
                        gContext->LogEntry("mythbackend", LP_DEBUG, msg, "");
                        VERBOSE(VB_GENERAL, msg);
                        runFillDatabase();
                        updateLastrun("MythFillDB");
                    }
                }
            }

            if (wantToRun("JobQueueCleanup", 1, 0, 24))
            {
                JobQueue::CleanupOldJobsInQueue();
                updateLastrun("JobQueueCleanup");
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

void HouseKeeper::runFillDatabase()
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
    
    signal(SIGCHLD, &reapChild);
    HouseKeeper_filldb_running = true;
    if (fork() == 0)
    {
        for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
            close(i);
        system(command.ascii());
        _exit(0); // this exit is ok, non-error exit from system command.
    }
}

void *HouseKeeper::doHouseKeepingThread(void *param)
{
    HouseKeeper *hkeeper = (HouseKeeper*)param;
    hkeeper->RunHouseKeeping();
 
    return NULL;
}
