#include <qapplication.h>
#include <qfile.h>
#include <qtextstream.h>

#include <iostream>
using namespace std;
#include <unistd.h>

#include <sys/wait.h>   // For WIFEXITED on Mac OS X

#include <exitcodes.h>
#include <mythcontext.h>
#include <mythdbcon.h>

void setGlobalSetting(const QString &key, const QString &value)
{
    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("DELETE FROM settings WHERE value = :KEY;");
        query.bindValue(":KEY", key);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Clear setting", query);

        query.prepare("INSERT INTO settings ( value, data ) "
                "VALUES ( :VALUE, :DATA );");
        query.bindValue(":VALUE", key);
        query.bindValue(":DATA", value);

        if (!query.exec() || !query.isActive())
            MythContext::DBError("Save new global setting", query);
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Database not open while trying to save setting: %1")
                        .arg(key));
    }
}

QString getGlobalSetting(const QString &key, const QString &defaultval)
{
    QString value = defaultval;

    MSqlQuery query(MSqlQuery::InitCon());
    if (query.isConnected())
    {
        query.prepare("SELECT data FROM settings WHERE value = :KEY AND "
                      "hostname IS NULL;");
        query.bindValue(":KEY", key);
        query.exec();
        if (query.isActive() && query.size() > 0)
        {
            query.next();
            value = QString::fromUtf8(query.value(0).toString());
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, 
                QString("Database not open while trying to load setting: %1")
                        .arg(key));
    }

    return value;
}

int lockShutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --lock");

    setGlobalSetting("MythShutdownLock", "1");

    return 0;
}

int unlockShutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --unlock");

    setGlobalSetting("MythShutdownLock", "0");

    return 0;
}

bool isRunning(QString program)
{
    QString sCommand = QString("ret=`ps cax | grep -c %1`; exit $ret").arg(program);
    int res = system(sCommand.ascii());
    if (WIFEXITED(res))
        res = WEXITSTATUS(res);
    
    return res;    
}

QDateTime getDailyWakeupTime(QString sPeriod)
{
    QString sTime = getGlobalSetting(sPeriod, "00:00");
    QTime tTime = QTime::fromString(sTime);
    QDateTime dtDateTime = QDateTime(QDate::currentDate(), tTime);

    return dtDateTime;
}

int getStatus()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --status");
    
    int res = 0;         
    
    if (isRunning("mythtranscode"))
    {    
        VERBOSE(VB_IMPORTANT, "Transcoding in progress...");
        res += 1;
    }    
        
    if (isRunning("mythcommflag"))
    {    
        VERBOSE(VB_IMPORTANT, "Commercial Flagging in progress...");
        res += 2;
    }    

    if (isRunning("mythfilldatabas"))
    {    
        VERBOSE(VB_IMPORTANT, "Grabbing EPG data in progress...");
        res += 4;
    }    

    if (getGlobalSetting("MythShutdownLock", "0") == "1")
    {    
        VERBOSE(VB_IMPORTANT, "Shutdown is locked");
        res += 16;
    }    
    
    QDateTime dtPeriod1Start = getDailyWakeupTime("DailyWakeupStartPeriod1");
    QDateTime dtPeriod1End = getDailyWakeupTime("DailyWakeupEndPeriod1");
    QDateTime dtPeriod2Start = getDailyWakeupTime("DailyWakeupStartPeriod2");
    QDateTime dtPeriod2End = getDailyWakeupTime("DailyWakeupEndPeriod2");
    QDateTime dtCurrent = QDateTime::currentDateTime();
    
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
    
    VERBOSE(VB_GENERAL, "Mythshutdown: --status returned: " << res);

    return res;
}

int checkOKShutdown()
{
    // mythbackend wants 0=ok to shutdown, 1=reset idle count, 2=wait for frontend 

    VERBOSE(VB_GENERAL, "Mythshutdown: --check");
    
    int res = getStatus();        
    
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

int setWakeupTime(QString sWakeupTime)
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --setwakeup");
    
    VERBOSE(VB_IMPORTANT, "Mythshutdown: wakeup time given is: " << sWakeupTime);
    
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

    setGlobalSetting("MythShutdownNextScheduled", dtWakeupTime.toString(Qt::ISODate));

    return 0;
}

int shutdown()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --shutdown");
    
    // get next daily wakeup times if any are set
    QDateTime dtPeriod1Start = getDailyWakeupTime("DailyWakeupStartPeriod1");
    QDateTime dtPeriod1End = getDailyWakeupTime("DailyWakeupEndPeriod1");
    QDateTime dtPeriod2Start = getDailyWakeupTime("DailyWakeupStartPeriod2");
    QDateTime dtPeriod2End = getDailyWakeupTime("DailyWakeupEndPeriod2");
    QDateTime dtCurrent = QDateTime::currentDateTime();
    QDateTime dtNextDailyWakeup = QDateTime();
    
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
    
    // if we have at least one valid daily wakeup time and dtNextDailyWakeup is 
    // still not valid then next daily wakeup is tomorrow 
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
    if (s != "")    
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
            VERBOSE(VB_IMPORTANT, "scheduled recording time has already passed. " 
                                  "schedule deleted"); 
        
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
    setGlobalSetting("MythShutdownWakeupTime", dtWakeupTime.toString(Qt::ISODate));
    
    // stop here to debug  
    //return 0;
      
    int shutdownmode = 0; // default to poweroff no reboot 
    QString nvramRestartCmd = 
            gContext->GetSetting("MythShutdownNvramRestartCmd", "");

    if (dtWakeupTime.isValid())
    {
        // dont't shutdown if we are within 15 mins of the next wakeup time
        if (dtCurrent.secsTo(dtWakeupTime) > 15 * 60)
        {    
            QString nvramCommand = gContext->GetSetting("MythShutdownNvramCmd",
                     "/usr/bin/nvram-wakeup");
            nvramCommand += QString(" --settime %1")
                            .arg(dtWakeupTime.toTime_t());
                              
            VERBOSE(VB_IMPORTANT, QString("sending command to set time in bios\n\t\t\t%1")
                            .arg(nvramCommand));
            
            shutdownmode = system(nvramCommand);          
            if (WIFEXITED(shutdownmode))
                shutdownmode = WEXITSTATUS(shutdownmode);

            VERBOSE(VB_IMPORTANT, QString("%1 exited with code %2").arg(nvramCommand)
                                                             .arg(shutdownmode)); 

            if (shutdownmode == 2)
            {
                VERBOSE(VB_IMPORTANT, "nvram-wakeup failed to set time in bios");
                return 1;
            }
            
            // we don't trust the return code from nvram-wakeup so only reboot
            // if the user has set a restart command
            if (nvramRestartCmd == "")
                shutdownmode = 0;
            else
                shutdownmode = 1;
        }    
        else
        {    
            VERBOSE(VB_IMPORTANT, "The next wakeup time is less than 15 mins away," 
                                  "not shutting down"); 
            return 0;  
        }   
    } 

    int res = 0;
    
    switch (shutdownmode) 
    {
        case 0:
        { 
            VERBOSE(VB_IMPORTANT, "everything looks fine, shutting down ..."); 
            QString poweroffCmd = 
                    gContext->GetSetting("MythShutdownPoweroff", "/sbin/poweroff");
            VERBOSE(VB_IMPORTANT, ".."); 
            VERBOSE(VB_IMPORTANT, "."); 
            VERBOSE(VB_IMPORTANT, "shutting down ..."); 

            system(poweroffCmd); 
            res = 0;
            break;
        }
        case 1:     
        {    
            VERBOSE(VB_IMPORTANT, "everything looks fine, but reboot is needed"); 
            VERBOSE(VB_IMPORTANT, "sending command to bootloader ..."); 
            VERBOSE(VB_IMPORTANT, nvramRestartCmd);
            
            system(nvramRestartCmd);
            
            VERBOSE(VB_IMPORTANT, ".."); 
            VERBOSE(VB_IMPORTANT, "."); 
            VERBOSE(VB_IMPORTANT, "rebooting ..."); 
            
            QString rebootCmd = 
                    gContext->GetSetting("MythShutdownReboot", "/sbin/reboot");
            system(rebootCmd);
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

int startup()
{
    VERBOSE(VB_GENERAL, "Mythshutdown: --startup");
    
    int res = 0;
    QDateTime startupTime = QDateTime();
    QString s = getGlobalSetting("MythshutdownWakeupTime", "");
    if (s != "")
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

void showUsage()
{
  cout << "Usage of mythshutdown\n"; 
  cout << "-w/--setwakeup time (sets the wakeup time. time=yyyy-MM-ddTmm:hh:ss.\n"; 
  cout << "                     doesn't write it into nvram)\n";  
  cout << "-q/--shutdown       (set nvram-wakeup time and shutdown)\n"; 
  cout << "-p/--startup        (check startup. check will return 0 if automatic\n";
  cout << "                                                      1 for manually)\n"; 
  cout << "-c/--check          (check shutdown possible returns 0 ok to shutdown\n"; 
  cout << "                                                     1 reset idle check)\n"; 
  cout << "-l/--lock           (disable shutdown. check will return 1.)\n"; 
  cout << "-u/--unlock         (enable shutdown. check will return 0)\n";
  cout << "-s/--status         (returns a code indicating the current status)\n";
  cout << "         0 - Idle\n";
  cout << "         1 - Transcoding\n";
  cout << "         2 - Commercial Flagging\n";
  cout << "         4 - Grabbing EPG data\n";
  cout << "         8 - Not used\n";
  cout << "        16 - Locked\n";
  cout << "        32 - Not used\n";
  cout << "        64 - In a daily wakeup/shutdown period\n";
  cout << "       128 - Less than 15 minutes to next wakeup period\n";
  cout << "-v/--verbose debug-level  (Use '-v help' for level info\n";
  cout << "-h/--help                 (shows this usage)\n";
}

int main(int argc, char **argv)
{
    // by default we don't output any messages
    print_verbose_messages = VB_NONE;
    
    QApplication a(argc, argv, false);
        
    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cout << "mythshutdown: Could not initialize myth context. "
                "Exiting." << endl;;
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    bool bLockShutdown = false;
    bool bUnlockShutdown = false;
    bool bCheckOKShutdown = false;
    bool bStartup = false;
    bool bShutdown = false;
    bool bGetStatus = false;
    bool bSetWakeupTime = false;
    QString sWakeupTime = "";
    bool bShowUsage = false;
        
    //  Check command line arguments
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return FRONTEND_EXIT_INVALID_CMDLINE;                        
                ++argpos;
            } 
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }

        else if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--lock"))
        {
            bLockShutdown = true;
        }
        else if (!strcmp(a.argv()[argpos],"-u") ||
            !strcmp(a.argv()[argpos],"--unlock"))
        {
            bUnlockShutdown = true;
        }
        else if (!strcmp(a.argv()[argpos],"-c") ||
            !strcmp(a.argv()[argpos],"--check"))
        {
            bCheckOKShutdown = true;
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--status"))
        {
            bGetStatus = true;
        }
        else if (!strcmp(a.argv()[argpos],"-w") ||
            !strcmp(a.argv()[argpos],"--setwakeup"))
        {
            if (a.argc() - 1 > argpos)
            {
                sWakeupTime = a.argv()[argpos+1];
                ++argpos;
            } 
            else 
            {
                cout << "mythshutdown: Missing argument to "
                                "-w/--setwakeup option" << endl;
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }

            bSetWakeupTime = true;
        }
        else if (!strcmp(a.argv()[argpos],"-q") ||
            !strcmp(a.argv()[argpos],"--shutdown"))
        {
            bShutdown = true;
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
            !strcmp(a.argv()[argpos],"--startup"))
        {
            bStartup = true;
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
            !strcmp(a.argv()[argpos],"--help"))
        {
            bShowUsage = true;
        }
        else
        {
            cout << "Invalid argument: " << a.argv()[argpos] << endl;
            showUsage();
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }
    
    int res = 0;
    
    if (bShowUsage)
        showUsage();
    else if (bLockShutdown)
        res = lockShutdown();
    else if (bUnlockShutdown)
        res = unlockShutdown();
    else if (bCheckOKShutdown)
        res = checkOKShutdown();
    else if (bStartup)
        res = startup();
    else if (bShutdown)
        res = shutdown();
    else if (bGetStatus)
        res = getStatus();
    else if (bSetWakeupTime)
        res = setWakeupTime(sWakeupTime);
    else 
        showUsage();
    
    return res;
}

