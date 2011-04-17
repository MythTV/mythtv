
#include <iostream>
#include <cstdlib>
using namespace std;
#include <unistd.h>

#include <QCoreApplication>
#include <QFile>

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythsystem.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "programdetail.h"
#include "jobqueue.h"
#include "tv.h"
#include "remoteutil.h"
#include "tvremoteutil.h"
#include "compat.h"
#include "mythcommandlineparser.h"

static void setGlobalSetting(const QString &key, const QString &value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("DELETE FROM settings WHERE value = :KEY;");
        query.bindValue(":KEY", key);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Clear setting", query);

        query.prepare("INSERT INTO settings ( value, data ) "
                "VALUES ( :VALUE, :DATA );");
        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", value);

        if (!query.exec() || !query.isActive())
            MythDB::DBError("Save new global setting", query);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Database not open while trying to save setting: %1")
                        .arg(key));
    }
}

static QString getGlobalSetting(const QString &key, const QString &defaultval)
{
    QString value = defaultval;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value = :KEY AND "
                      "hostname IS NULL;");
        query.bindValue(":KEY", key);
        if (query.exec() && query.next())
            value = query.value(0).toString();
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Database not open while trying to load setting: %1")
                        .arg(key));
    }

    return value;
}

static int lockShutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --lock");

    MSqlQuery query(MSqlQuery::InitCon());

    // lock setting table
    int tries = 0;
    while (!query.exec("LOCK TABLE settings WRITE;") && tries < 5)
    {
        VERBOSE(VB_GENERAL, "Waiting for lock on setting table");
        sleep(1);
        tries++;
    }

    if (tries >= 5)
    {
        VERBOSE(VB_GENERAL, "Waited too long to obtain lock on setting table");
        return 1;
    }

    // does the setting already exist?
    query.prepare("SELECT * FROM settings "
                  "WHERE value = 'MythShutdownLock' AND hostname IS NULL;");
    if (!query.exec())
        MythDB::DBError("lockShutdown -- select", query);

    if (query.size() < 1)
    {
        // add the lock setting
        query.prepare("INSERT INTO settings (value, data) "
                      "VALUES ('MythShutdownLock', '1');");
        if (!query.exec())
            MythDB::DBError("lockShutdown -- insert", query);
    }
    else
    {
        // update the lock setting
        query.prepare("UPDATE settings SET data = data + 1 "
                      "WHERE value = 'MythShutdownLock' "
                      "AND hostname IS NULL;");
        if (!query.exec())
            MythDB::DBError("lockShutdown -- update", query);
    }

    // unlock settings table
    if (!query.exec("UNLOCK TABLES;"))
        MythDB::DBError("lockShutdown -- unlock", query);

    return 0;
}

static int unlockShutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --unlock");

    MSqlQuery query(MSqlQuery::InitCon());

    // lock setting table
    int tries = 0;
    while (!query.exec("LOCK TABLE settings WRITE;") && tries < 5)
    {
        VERBOSE(VB_GENERAL, "Waiting for lock on setting table");
        sleep(1);
        tries++;
    }

    if (tries >= 5)
    {
        VERBOSE(VB_GENERAL, "Waited too long to obtain lock on setting table");
        return 1;
    }

    // does the setting exist?
    query.prepare("SELECT * FROM settings "
                  "WHERE value = 'MythShutdownLock' AND hostname IS NULL;");
    if (!query.exec())
        MythDB::DBError("unlockShutdown -- select", query);

    if (query.size() < 1)
    {
        // add the lock setting
        query.prepare("INSERT INTO settings (value, data) "
                      "VALUES ('MythShutdownLock', '0');");
        if (!query.exec())
            MythDB::DBError("unlockShutdown -- insert", query);
    }
    else
    {
        // update lock setting
        query.prepare("UPDATE settings SET data = GREATEST(0,  data - 1) "
                      "WHERE value = 'MythShutdownLock' "
                      "AND hostname IS NULL;");
        if (!query.exec())
            MythDB::DBError("unlockShutdown -- update", query);
    }

    // unlock table
    if (!query.exec("UNLOCK TABLES;"))
        MythDB::DBError("unlockShutdown -- unlock", query);

    // tell the master BE to reset its idle time
    RemoteSendMessage("RESET_IDLETIME");

    return 0;
}

/** \brief Returns true if a program containing the specified
 *         string is running on this machine
 *
 *  Since many Linux distributions rename executables, and they
 *  also have different names by default on different operating
 *  systems, and this function is POSIX only use of this function
 *  is discouraged.
 *
 *  Warning: This function should never be passed a value
 *  which is not specified explicitly in the code, it does
 *  no checking for shell script injection.
 *
 */
static bool isRunning(const char *program)
{
    QString command = QString("ps ch -C %1 -o pid > /dev/null").arg(program);
    return (myth_system(command) == GENERIC_EXIT_OK);
}

static QDateTime getDailyWakeupTime(QString sPeriod)
{
    QString sTime = getGlobalSetting(sPeriod, "00:00");
    QTime tTime = QTime::fromString(sTime, "hh:mm");
    QDateTime dtDateTime = QDateTime(QDate::currentDate(), tTime);

    return dtDateTime;
}

static bool isRecording()
{
    if (!gCoreContext->IsConnectedToMaster())
    {
        VERBOSE(VB_IMPORTANT,
                "isRecording: Attempting to connect to master server...");
        if (!gCoreContext->ConnectToMasterServer(false))
        {
            VERBOSE(VB_IMPORTANT,
                    "isRecording: Could not connect to master server!");
            return false;
        }
    }

    return RemoteGetRecordingStatus(NULL, false);
}

static int getStatus(bool bWantRecStatus)
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --status");

    int res = 0;

    if (isRunning("mythtranscode"))
    {
        VERBOSE(VB_IMPORTANT, "Transcoding in progress...");
        res |= 1;
    }

    if (isRunning("mythcommflag"))
    {
        VERBOSE(VB_IMPORTANT, "Commercial Detection in progress...");
        res |= 2;
    }

    if (isRunning("mythfilldatabase"))
    {
        VERBOSE(VB_IMPORTANT, "Grabbing EPG data in progress...");
        res |= 4;
    }

    if (bWantRecStatus && isRecording())
    {
        VERBOSE(VB_IMPORTANT, "Recording in progress...");
        res |= 8;
    }

    if (getGlobalSetting("MythShutdownLock", "0") != "0")
    {
        VERBOSE(VB_IMPORTANT, "Shutdown is locked");
        res |= 16;
    }

    if (JobQueue::HasRunningOrPendingJobs(15))
    {
        VERBOSE(VB_IMPORTANT, "Has queued or pending jobs");
        res |= 32;
    }

    QDateTime dtPeriod1Start = getDailyWakeupTime("DailyWakeupStartPeriod1");
    QDateTime dtPeriod1End = getDailyWakeupTime("DailyWakeupEndPeriod1");
    QDateTime dtPeriod2Start = getDailyWakeupTime("DailyWakeupStartPeriod2");
    QDateTime dtPeriod2End = getDailyWakeupTime("DailyWakeupEndPeriod2");
    QDateTime dtCurrent = QDateTime::currentDateTime();

    // Check for time periods that cross midnight
    if (dtPeriod1End < dtPeriod1Start)
    {
        if (dtCurrent > dtPeriod1End)
            dtPeriod1End = dtPeriod1End.addDays(1);
        else
            dtPeriod1Start = dtPeriod1Start.addDays(-1);
    }

    if (dtPeriod2End < dtPeriod2Start)
    {
        if (dtCurrent > dtPeriod2End)
            dtPeriod2End = dtPeriod2End.addDays(1);
        else
            dtPeriod2Start = dtPeriod2Start.addDays(-1);
    }

    // Check for one of the daily wakeup periods
    if (dtPeriod1Start != dtPeriod1End)
    {
        if (dtCurrent >= dtPeriod1Start && dtCurrent <= dtPeriod1End)
        {
            VERBOSE(VB_IMPORTANT, "In a daily wakeup period (1).");
            res |= 64;
        }
    }

    if (dtPeriod2Start != dtPeriod2End)
    {
        if (dtCurrent >= dtPeriod2Start && dtCurrent <= dtPeriod2End)
        {
            VERBOSE(VB_IMPORTANT, "In a daily wakeup period (2).");
            res |= 64;
        }
    }

    // Are we about to start a daily wakeup period
    // allow for a 15 minute window
    if (dtPeriod1Start != dtPeriod1End)
    {
        int delta = dtCurrent.secsTo(dtPeriod1Start);
        if (delta >= 0 && delta <= 15 * 60)
        {
            VERBOSE(VB_IMPORTANT, "About to start daily wakeup period (1)");
            res |= 128;
        }
    }

    if (dtPeriod2Start != dtPeriod2End)
    {
        int delta = dtCurrent.secsTo(dtPeriod2Start);
        if (delta >= 0 && delta <= 15 * 60)
        {
            VERBOSE(VB_IMPORTANT, "About to start daily wakeup period (2)");
            res |= 128;
        }
    }

    if (isRunning("mythtv-setup"))
    {
        VERBOSE(VB_IMPORTANT, "Setup is running...");
        res = 255;
    }

    VERBOSE(VB_GENERAL, "Mythshutdown: --status returned: " << res);

    return res;
}

static int checkOKShutdown(bool bWantRecStatus)
{
    // mythbackend wants 0=ok to shutdown,
    // 1=reset idle count, 2=wait for frontend

    VERBOSE(VB_GENERAL, "Mythshutdown: --check");

    int res = getStatus(bWantRecStatus);

    if (res > 0)
    {
        VERBOSE(VB_IMPORTANT, "Not OK to shutdown");
        res = 1;
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "OK to shutdown");
        res = 0;
    }

    VERBOSE(VB_GENERAL, "Mythshutdown: --check returned: " << res);

    return res;
}

static int setWakeupTime(QString sWakeupTime)
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --setwakeup");

    VERBOSE(VB_IMPORTANT,
            "Mythshutdown: wakeup time given is: " << sWakeupTime);

    // check time given is valid
    QDateTime dtWakeupTime;
    dtWakeupTime = QDateTime::fromString(sWakeupTime, Qt::ISODate);

    if (!dtWakeupTime.isValid())
    {
        VERBOSE(VB_IMPORTANT, QString("Mythshutdown: --setwakeup invalid date "
                                      "format (%1)\n\t\t\t"
                                      "must be yyyy-MM-ddThh:mm:ss")
                                      .arg(sWakeupTime));
        return 1;
    }

    setGlobalSetting("MythShutdownNextScheduled",
                     dtWakeupTime.toString(Qt::ISODate));

    return 0;
}

static int setScheduledWakeupTime()
{
    if (!gCoreContext->IsConnectedToMaster())
    {
        VERBOSE(VB_IMPORTANT, "setScheduledWakeupTime: "
                              "Attempting to connect to master server...");
        if (!gCoreContext->ConnectToMasterServer(false))
        {
            VERBOSE(VB_IMPORTANT, "setScheduledWakeupTime: "
                                  "Could not connect to master server!");
            return 1;
        }
    }

    QDateTime nextRecordingStart;
    GetProgramDetailList(nextRecordingStart);

    // set the wakeup time for the next scheduled recording
    if (!nextRecordingStart.isNull())
    {
        int m_preRollSeconds = gCoreContext->GetNumSetting("RecordPreRoll");
        QDateTime restarttime = nextRecordingStart
            .addSecs((-1) * m_preRollSeconds);

        int add = gCoreContext->GetNumSetting("StartupSecsBeforeRecording", 240);
        if (add)
            restarttime = restarttime.addSecs((-1) * add);

        setWakeupTime(restarttime.toString(Qt::ISODate));

        return 0;
    }
    return 1;
}

static int shutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --shutdown");

    // get next daily wakeup times if any are set
    QDateTime dtPeriod1Start = getDailyWakeupTime("DailyWakeupStartPeriod1");
    QDateTime dtPeriod1End = getDailyWakeupTime("DailyWakeupEndPeriod1");
    QDateTime dtPeriod2Start = getDailyWakeupTime("DailyWakeupStartPeriod2");
    QDateTime dtPeriod2End = getDailyWakeupTime("DailyWakeupEndPeriod2");
    QDateTime dtCurrent = QDateTime::currentDateTime();
    QDateTime dtNextDailyWakeup = QDateTime();

    // Make sure Period1 is before Period2
    if (dtPeriod2Start < dtPeriod1Start)
    {
        QDateTime temp = dtPeriod1Start;
        dtPeriod1Start = dtPeriod2Start;
        dtPeriod2Start = temp;
        temp = dtPeriod1End;
        dtPeriod1End = dtPeriod2End;
        dtPeriod2End = temp;
    }

    // Check for time periods that cross midnight
    if (dtPeriod1End < dtPeriod1Start)
    {
        if (dtCurrent > dtPeriod1End)
            dtPeriod1End = dtPeriod1End.addDays(1);
        else
            dtPeriod1Start = dtPeriod1Start.addDays(-1);
    }

    if (dtPeriod2End < dtPeriod2Start)
    {
        if (dtCurrent > dtPeriod2End)
            dtPeriod2End = dtPeriod2End.addDays(1);
        else
            dtPeriod2Start = dtPeriod2Start.addDays(-1);
    }

    // have we passed the first wakeup time today
    if (dtPeriod1Start != dtPeriod1End)
    {
        if (dtCurrent < dtPeriod1Start)
        {
            VERBOSE(VB_IMPORTANT, "daily wakeup today at " <<
                        dtPeriod1Start.toString("hh:mm:ss"));
            dtNextDailyWakeup = dtPeriod1Start;
        }
    }

    // have we passed the second wakeup time today
    if (!dtNextDailyWakeup.isValid() && dtPeriod2Start != dtPeriod2End)
    {
        if (dtCurrent < dtPeriod2Start)
        {
            VERBOSE(VB_IMPORTANT, "daily wakeup today at " <<
                        dtPeriod2Start.toString("hh:mm:ss"));
            dtNextDailyWakeup = dtPeriod2Start;
        }
    }

    // if we have at least one valid daily wakeup time
    // and dtNextDailyWakeup is still not valid
    // then next daily wakeup is tomorrow
    if (!dtNextDailyWakeup.isValid())
    {
        if (dtPeriod1Start != dtPeriod1End)
            dtNextDailyWakeup = dtPeriod1Start;
        else if (dtPeriod2Start != dtPeriod2End)
            dtNextDailyWakeup = dtPeriod2Start;

        if (dtNextDailyWakeup.isValid())
        {
            dtNextDailyWakeup = dtNextDailyWakeup.addDays(1);

            VERBOSE(VB_IMPORTANT, "next daily wakeup is tomorrow at " <<
                        dtNextDailyWakeup.toString("hh:mm:ss"));
        }
    }

    // if dtNextDailyWakeup is still not valid then no daily wakeups are set
    if (!dtNextDailyWakeup.isValid())
        VERBOSE(VB_IMPORTANT,"no daily wakeup times are set");

    // get next scheduled wake up for a recording if any
    QDateTime dtNextRecordingStart = QDateTime();
    QString s = getGlobalSetting("MythShutdownNextScheduled", "");
    if (!s.isEmpty())
        dtNextRecordingStart = QDateTime::fromString(s, Qt::ISODate);

    if (!dtNextRecordingStart.isValid())
        VERBOSE(VB_IMPORTANT,"no recording time is set");
    else
        VERBOSE(VB_IMPORTANT, "recording scheduled at: " <<
                    dtNextRecordingStart.toString(Qt::ISODate));

    // check if scheduled recording time has already passed
    if (dtNextRecordingStart.isValid())
    {
        int delta = dtCurrent.secsTo(dtNextRecordingStart);

        if (delta < 0)
        {
            VERBOSE(VB_IMPORTANT, "Scheduled recording time has"
                                  " already passed. Schedule deleted");

            dtNextRecordingStart = QDateTime();
            setGlobalSetting("MythShutdownNextScheduled", "");
        }
    }

    QDateTime dtWakeupTime = QDateTime();

    // simple case
    // no daily wakeup set
    // no scheduled program set
    // just shut down
    if (!dtNextRecordingStart.isValid() && !dtNextDailyWakeup.isValid())
    {
        dtWakeupTime = QDateTime();
        VERBOSE(VB_IMPORTANT, "no wake up time set and no scheduled program");
    }

    // no daily wakeup set
    // scheduled program is set
    if (dtNextRecordingStart.isValid() && !dtNextDailyWakeup.isValid())
    {
        dtWakeupTime = dtNextRecordingStart;
        VERBOSE(VB_IMPORTANT, "will wake up at next scheduled program");
    }

    // daily wakeup is set
    // no scheduled program is set
    if (!dtNextRecordingStart.isValid() && dtNextDailyWakeup.isValid())
    {
        dtWakeupTime = dtNextDailyWakeup;
        VERBOSE(VB_IMPORTANT, "will wake up at next daily wakeup");
    }

    // daily wakeup is set
    // scheduled program is set
    // wake up at which ever is the earliest
    if (dtNextRecordingStart.isValid() && dtNextDailyWakeup.isValid())
    {
        if (dtNextDailyWakeup < dtNextRecordingStart)
        {
            VERBOSE(VB_IMPORTANT,  "program is scheduled but will wake up "
                                   "at next daily wakeup");
            dtWakeupTime = dtNextDailyWakeup;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "daily wakeup is set but will wake up "
                                  "at next scheduled program");
            dtWakeupTime = dtNextRecordingStart;
        }
    }

    // save the next wakuptime in the db
    setGlobalSetting("MythShutdownWakeupTime",
                     dtWakeupTime.toString(Qt::ISODate));

    // stop here to debug
    //return 0;

    int shutdownmode = 0; // default to poweroff no reboot
    QString nvramRestartCmd =
            gCoreContext->GetSetting("MythShutdownNvramRestartCmd", "");

    if (dtWakeupTime.isValid())
    {
        // dont't shutdown if we are within 15 mins of the next wakeup time
        if (dtCurrent.secsTo(dtWakeupTime) > 15 * 60)
        {
            QString nvramCommand = gCoreContext->GetSetting("MythShutdownNvramCmd",
                     "/usr/bin/nvram-wakeup --settime $time");

            QString wakeup_timeformat = gCoreContext->GetSetting(
                "MythShutdownWakeupTimeFmt", "time_t");

            if (wakeup_timeformat == "time_t")
            {
                QString time_ts;
                nvramCommand.replace(
                    "$time", time_ts.setNum(dtWakeupTime.toTime_t()));
            }
            else
                nvramCommand.replace(
                    "$time", dtWakeupTime.toString(wakeup_timeformat));

            VERBOSE(VB_IMPORTANT,
                    "sending command to set time in bios\n\t\t\t"
                    + nvramCommand);

            shutdownmode = myth_system(nvramCommand);

            VERBOSE(VB_IMPORTANT, (nvramCommand + " exited with code %2")
                                  .arg(shutdownmode));

            if (shutdownmode == 2)
            {
                VERBOSE(VB_IMPORTANT, "nvram-wakeup failed to set time in bios");
                return 1;
            }

            // we don't trust the return code from nvram-wakeup so only reboot
            // if the user has set a restart command
            if (nvramRestartCmd.isEmpty())
                shutdownmode = 0;
            else
                shutdownmode = 1;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "The next wakeup time is less than"
                                  " 15 mins away, not shutting down");
            return 0;
        }
    }

    int res = 0;

    switch (shutdownmode)
    {
        case 0:
        {
            VERBOSE(VB_IMPORTANT, "everything looks fine, shutting down ...");
            QString poweroffCmd = gCoreContext->GetSetting(
                "MythShutdownPoweroff", "/sbin/poweroff");
            VERBOSE(VB_IMPORTANT, "..");
            VERBOSE(VB_IMPORTANT, ".");
            VERBOSE(VB_IMPORTANT, "shutting down ...");

            myth_system(poweroffCmd);
            res = 0;
            break;
        }
        case 1:
        {
            VERBOSE(VB_IMPORTANT,
                    "everything looks fine, but reboot is needed");
            VERBOSE(VB_IMPORTANT, "sending command to bootloader ...");
            VERBOSE(VB_IMPORTANT, nvramRestartCmd);

            myth_system(nvramRestartCmd);

            VERBOSE(VB_IMPORTANT, "..");
            VERBOSE(VB_IMPORTANT, ".");
            VERBOSE(VB_IMPORTANT, "rebooting ...");

            QString rebootCmd =
                    gCoreContext->GetSetting("MythShutdownReboot", "/sbin/reboot");
            myth_system(rebootCmd);
            res = 0;
            break;
        }
        default:
            VERBOSE(VB_IMPORTANT, "panic. invalid shutdown mode, do nothing");
            res = 1;
            break;
    }

    return res;
}

static int startup()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --startup");

    int res = 0;
    QDateTime startupTime = QDateTime();
    QString s = getGlobalSetting("MythshutdownWakeupTime", "");
    if (!s.isEmpty())
        startupTime = QDateTime::fromString(s, Qt::ISODate);

    // if we don't have a valid startup time assume we were started manually
    if (!startupTime.isValid())
        res = 1;
    else
    {
        // if we started within 15mins of the saved wakeup time assume we started
        // automatically to record or for a daily wakeup/shutdown period

        int delta = startupTime.secsTo(QDateTime::currentDateTime());
        if (delta < 0)
            delta = -delta;

        if (delta < 15 * 60)
            res = 0;
        else
            res = 1;
    }

    if (res)
        VERBOSE(VB_IMPORTANT, "looks like we were started manually" << res);
    else
        VERBOSE(VB_IMPORTANT, "looks like we were started automatically" << res);


    VERBOSE(VB_GENERAL, "Mythshutdown: --startup returned: " << res);

    return res;
}

int main(int argc, char **argv)
{
    // by default we don't output any messages
    print_verbose_messages = VB_NONE;

    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHSHUTDOWN);

    bool bLockShutdown = false;
    bool bUnlockShutdown = false;
    bool bCheckOKShutdown = false;
    bool bStartup = false;
    bool bShutdown = false;
    bool bGetStatus = false;
    bool bSetWakeupTime = false;
    QString sWakeupTime = "";
    bool bSetScheduledWakeupTime = false;
    bool bCheckAndShutdown = false;
    bool bWantRecStatus = true;

    MythShutdownCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("verbose"))
        if (parse_verbose_arg(cmdline.toString("verbose")) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
            return GENERIC_EXIT_INVALID_CMDLINE;
    if (cmdline.toBool("lock"))
        bLockShutdown = true;
    if (cmdline.toBool("unlock"))
        bUnlockShutdown = true;
    if (cmdline.toBool("shutdown"))
        bShutdown = true;
    if (cmdline.toBool("startup"))
        bStartup = true;
    if (cmdline.toBool("setschedwakeup"))
        bSetScheduledWakeupTime = true;
    if (cmdline.toBool("safeshutdown"))
        bCheckAndShutdown = true;
    if (cmdline.toBool("setwakeup"))
        sWakeupTime = cmdline.toString("setwakeup");

    if (cmdline.toBool("check"))
    {
        bCheckOKShutdown = true;
        bWantRecStatus = (bool)(cmdline.toInt("check") == 1);
    }
    if (cmdline.toBool("status"))
    {
        bGetStatus = true;
        bWantRecStatus = (bool)(cmdline.toInt("status") == 1);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cout << "mythshutdown: Could not initialize MythContext. "
                "Exiting." << endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }


    int res = 0;

    if (bLockShutdown)
        res = lockShutdown();
    else if (bUnlockShutdown)
        res = unlockShutdown();
    else if (bCheckOKShutdown)
        res = checkOKShutdown(bWantRecStatus);
    else if (bSetScheduledWakeupTime)
        res = setScheduledWakeupTime();
    else if (bStartup)
        res = startup();
    else if (bShutdown)
        res = shutdown();
    else if (bGetStatus)
        res = getStatus(bWantRecStatus);
    else if (bSetWakeupTime)
        res = setWakeupTime(sWakeupTime);
    else if (bCheckAndShutdown)
    {
        res = checkOKShutdown(true);
        if (res == 0)     // Nothing to stop a shutdown (eg. recording in progress).
        {
             res = setScheduledWakeupTime();
             res = shutdown();
        }
    }
    else
        cmdline.PrintHelp();

    delete gContext;

    return res;
}

