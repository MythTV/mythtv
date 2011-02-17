// POSIX headers
#include <sys/time.h>     // for setpriority
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#endif

#include "mythconfig.h"
#if CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

// C headers
#include <cstdlib>
#include <cerrno>

#include <QCoreApplication>
#include <QFileInfo>
#include <QRegExp>
#include <QFile>
#include <QDir>
#include <QMap>

#include "tv_rec.h"
#include "scheduledrecording.h"
#include "autoexpire.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"
#include "remoteutil.h"
#include "housekeeper.h"

#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdb.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "dbcheck.h"
#include "jobqueue.h"
#include "previewgenerator.h"
#include "mythcommandlineparser.h"
#include "mythsystemevent.h"
#include "main_helpers.h"
#include "backendcontext.h"

#include "mediaserver.h"
#include "httpstatus.h"

#define LOC      QString("MythBackend: ")
#define LOC_WARN QString("MythBackend, Warning: ")
#define LOC_ERR  QString("MythBackend, Error: ")

bool setupTVs(bool ismaster, bool &error)
{
    error = false;
    QString localhostname = gCoreContext->GetHostName();

    MSqlQuery query(MSqlQuery::InitCon());

    if (ismaster)
    {
        // Hack to make sure recorded.basename gets set if the user
        // downgrades to a prior version and creates new entries
        // without it.
        if (!query.exec("UPDATE recorded SET basename = CONCAT(chanid, '_', "
                        "DATE_FORMAT(starttime, '%Y%m%d%H%i00'), '_', "
                        "DATE_FORMAT(endtime, '%Y%m%d%H%i00'), '.nuv') "
                        "WHERE basename = '';"))
            MythDB::DBError("Updating record basename",
                                 query.lastQuery());

        // Hack to make sure record.station gets set if the user
        // downgrades to a prior version and creates new entries
        // without it.
        if (!query.exec("UPDATE channel SET callsign=chanid "
                        "WHERE callsign IS NULL OR callsign='';"))
            MythDB::DBError("Updating channel callsign", query.lastQuery());

        if (query.exec("SELECT MIN(chanid) FROM channel;"))
        {
            query.first();
            int min_chanid = query.value(0).toInt();
            if (!query.exec(QString("UPDATE record SET chanid = %1 "
                                    "WHERE chanid IS NULL;").arg(min_chanid)))
                MythDB::DBError("Updating record chanid", query.lastQuery());
        }
        else
            MythDB::DBError("Querying minimum chanid", query.lastQuery());

        MSqlQuery records_without_station(MSqlQuery::InitCon());
        records_without_station.prepare("SELECT record.chanid,"
                " channel.callsign FROM record LEFT JOIN channel"
                " ON record.chanid = channel.chanid WHERE record.station='';");
        if (records_without_station.exec() && records_without_station.next())
        {
            MSqlQuery update_record(MSqlQuery::InitCon());
            update_record.prepare("UPDATE record SET station = :CALLSIGN"
                    " WHERE chanid = :CHANID;");
            do
            {
                update_record.bindValue(":CALLSIGN",
                        records_without_station.value(1));
                update_record.bindValue(":CHANID",
                        records_without_station.value(0));
                if (!update_record.exec())
                {
                    MythDB::DBError("Updating record station",
                            update_record.lastQuery());
                }
            } while (records_without_station.next());
        }
    }

    if (!query.exec(
            "SELECT cardid, hostname "
            "FROM capturecard "
            "ORDER BY cardid"))
    {
        MythDB::DBError("Querying Recorders", query);
        return false;
    }

    vector<uint>    cardids;
    vector<QString> hosts;
    while (query.next())
    {
        uint    cardid = query.value(0).toUInt();
        QString host   = query.value(1).toString();
        QString cidmsg = QString("Card %1").arg(cardid);

        if (host.isEmpty())
        {
            QString msg = cidmsg + " does not have a hostname defined.\n"
                "Please run setup and confirm all of the capture cards.\n";

            VERBOSE(VB_IMPORTANT, msg);
            gCoreContext->LogEntry("mythbackend", LP_CRITICAL,
                               "Problem with capture cards", msg);
            continue;
        }

        cardids.push_back(cardid);
        hosts.push_back(host);
    }

    for (uint i = 0; i < cardids.size(); i++)
    {
        if (hosts[i] == localhostname)
            new TVRec(cardids[i]);
    }

    for (uint i = 0; i < cardids.size(); i++)
    {
        uint    cardid = cardids[i];
        QString host   = hosts[i];
        QString cidmsg = QString("Card %1").arg(cardid);

        if (!ismaster)
        {
            if (host == localhostname)
            {
                TVRec *tv = TVRec::GetTVRec(cardid);
                if (tv && tv->Init())
                {
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    gCoreContext->LogEntry("mythbackend", LP_CRITICAL,
                                       "Problem with capture cards",
                                       cidmsg + " failed init");
                    delete tv;
                    // The master assumes card comes up so we need to
                    // set error and exit if a non-master card fails.
                    error = true;
                }
            }
        }
        else
        {
            if (host == localhostname)
            {
                TVRec *tv = TVRec::GetTVRec(cardid);
                if (tv && tv->Init())
                {
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    gCoreContext->LogEntry("mythbackend", LP_CRITICAL,
                                       "Problem with capture cards",
                                       cidmsg + "failed init");
                    delete tv;
                }
            }
            else
            {
                EncoderLink *enc = new EncoderLink(cardid, NULL, host);
                tvList[cardid] = enc;
            }
        }
    }

    if (tvList.empty())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "No valid capture cards are defined in the database.");

        gCoreContext->LogEntry("mythbackend", LP_WARNING,
                           "No capture cards are defined",
                           "This backend will not be used for recording.");
    }

    return true;
}

bool setup_context(const MythCommandLineParser &cmdline)
{
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext.");
        return false;
    }
    gCoreContext->SetBackend(!cmdline.HasBackendCommand());

    QMap<QString,QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                    .arg(it.key()).arg(*it));
            gCoreContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    return true;
}

void cleanup(void)
{
    delete sched;
    sched = NULL;

    delete g_pUPnp;
    g_pUPnp = NULL;

    delete gContext;
    gContext = NULL;

    if (pidfile.size())
    {
        unlink(pidfile.toAscii().constData());
        pidfile.clear();
    }

    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
}

int log_rotate(int report_error)
{
    /* http://www.gossamer-threads.com/lists/mythtv/dev/110113 */

    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND|O_SYNC, 0664);
    if (new_logfd < 0)
    {
        // If we can't open the new logfile, send data to /dev/null
        if (report_error)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Cannot open logfile '%1'").arg(logfile));
            return -1;
        }
        new_logfd = open("/dev/null", O_WRONLY);
        if (new_logfd < 0)
        {
            // There's not much we can do, so punt.
            return -1;
        }
    }
    while (dup2(new_logfd, 1) < 0 && errno == EINTR) ;
    while (dup2(new_logfd, 2) < 0 && errno == EINTR) ;
    while (close(new_logfd) < 0 && errno == EINTR) ;
    return 0;
}

void log_rotate_handler(int)
{
    log_rotate(0);
}

void showUsage(const MythCommandLineParser &cmdlineparser, const QString &version)
{
    QString    help  = cmdlineparser.GetHelpString(false);
    QByteArray ahelp = help.toLocal8Bit();

    cerr << qPrintable(version) << endl <<
    "Valid options are: " << endl <<
    "-h or --help                   List valid command line parameters"
         << endl << ahelp.constData() << endl;
}

void setupLogfile(void)
{
    if (!logfile.isEmpty())
    {
        if (log_rotate(1) < 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Cannot open logfile; using stdout/stderr instead");
        }
        else
            signal(SIGHUP, &log_rotate_handler);
    }
}

bool openPidfile(ofstream &pidfs, const QString &pidfile)
{
    if (!pidfile.isEmpty())
    {
        pidfs.open(pidfile.toAscii().constData());
        if (!pidfs)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not open pid file" + ENO);
            return false;
        }
    }
    return true;
}

bool setUser(const QString &username)
{
    if (username.isEmpty())
        return true;

#ifdef _WIN32
    VERBOSE(VB_IMPORTANT, "--user option is not supported on Windows");
    return false;
#else // ! _WIN32
    struct passwd *user_info = getpwnam(username.toLocal8Bit().constData());
    const uid_t user_id = geteuid();

    if (user_id && (!user_info || user_id != user_info->pw_uid))
    {
        VERBOSE(VB_IMPORTANT,
                "You must be running as root to use the --user switch.");
        return false;
    }
    else if (user_info && user_id == user_info->pw_uid)
    {
        VERBOSE(VB_IMPORTANT,
                QString("Already running as '%1'").arg(username));
    }
    else if (!user_id && user_info)
    {
        if (setenv("HOME", user_info->pw_dir,1) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting home directory.");
            return false;
        }
        if (setgid(user_info->pw_gid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting effective group.");
            return false;
        }
        if (initgroups(user_info->pw_name, user_info->pw_gid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting groups.");
            return false;
        }
        if (setuid(user_info->pw_uid) == -1)
        {
            VERBOSE(VB_IMPORTANT, "Error setting effective user.");
            return false;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT,
                QString("Invalid user '%1' specified with --user")
                .arg(username));
        return false;
    }
    return true;
#endif // ! _WIN32
}

int handle_command(const MythCommandLineParser &cmdline)
{
    QString eventString = cmdline.GetEventString();
    if (!eventString.isEmpty())
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            if (eventString.startsWith("SYSTEM_EVENT"))
            {
                eventString += QString(" SENDER %1")
                    .arg(gCoreContext->GetHostName());
            }

            RemoteSendMessage(eventString);
            return GENERIC_EXIT_OK;
        }
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (cmdline.WantUPnPRebuild())
    {
        VERBOSE(VB_GENERAL, "Rebuilding UPNP Media Map is no longer supported.  Rescan videos using MythVideo.");

        return GENERIC_EXIT_OK;
    }

    if (cmdline.SetVerbose())
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            QString message = "SET_VERBOSE ";
            message += cmdline.GetNewVerbose();

            RemoteSendMessage(message);
            VERBOSE(VB_IMPORTANT, QString("Sent '%1' message").arg(message));
            return GENERIC_EXIT_OK;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    "Unable to connect to backend, verbose level unchanged ");
            return GENERIC_EXIT_CONNECT_ERROR;
        }
    }

    if (cmdline.ClearSettingsCache())
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            RemoteSendMessage("CLEAR_SETTINGS_CACHE");
            VERBOSE(VB_IMPORTANT, "Sent CLEAR_SETTINGS_CACHE message");
            return GENERIC_EXIT_OK;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Unable to connect to backend, settings "
                    "cache will not be cleared.");
            return GENERIC_EXIT_CONNECT_ERROR;
        }
    }

    if (cmdline.IsPrintScheduleEnabled() ||
        cmdline.IsTestSchedulerEnabled())
    {
        sched = new Scheduler(false, &tvList);
        if (!cmdline.IsTestSchedulerEnabled() &&
            gCoreContext->ConnectToMasterServer())
        {
            cout << "Retrieving Schedule from Master backend.\n";
            sched->FillRecordListFromMaster();
        }
        else
        {
            cout << "Calculating Schedule from database.\n" <<
                    "Inputs, Card IDs, and Conflict info may be invalid "
                    "if you have multiple tuners.\n";
            sched->FillRecordListFromDB();
        }

        print_verbose_messages |= VB_SCHEDULE;
        sched->PrintList(true);
        return GENERIC_EXIT_OK;
    }

    if (cmdline.Reschedule())
    {
        bool ok = false;
        if (gCoreContext->ConnectToMasterServer())
        {
            VERBOSE(VB_IMPORTANT, "Connected to master for reschedule");
            ScheduledRecording::signalChange(-1);
            ok = true;
        }
        else
            VERBOSE(VB_IMPORTANT, "Cannot connect to master for reschedule");

        return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_CONNECT_ERROR;
    }

    if (cmdline.ScanVideos())
    {
        bool ok = false;
        if (gCoreContext->ConnectToMasterServer())
        {
            gCoreContext->SendReceiveStringList(QStringList() << "SCAN_VIDEOS");
            VERBOSE(VB_IMPORTANT, "Requested video scan");
            ok = true;
        }
        else
            VERBOSE(VB_IMPORTANT, "Cannot connect to master for video scan");

        return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_CONNECT_ERROR;
    }

    if (!cmdline.GetPrintExpire().isEmpty())
    {
        expirer = new AutoExpire();
        expirer->PrintExpireList(cmdline.GetPrintExpire());
        return GENERIC_EXIT_OK;
    }

    // This should never actually be reached..
    return GENERIC_EXIT_OK;
}

int connect_to_master(void)
{
    MythSocket *tempMonitorConnection = new MythSocket();
    if (tempMonitorConnection->connect(
            gCoreContext->GetSetting("MasterServerIP", "127.0.0.1"),
            gCoreContext->GetNumSetting("MasterServerPort", 6543)))
    {
        if (!gCoreContext->CheckProtoVersion(tempMonitorConnection))
        {
            VERBOSE(VB_IMPORTANT, "Master backend is incompatible with "
                    "this backend.\nCannot become a slave.");
            return GENERIC_EXIT_CONNECT_ERROR;
        }

        QStringList tempMonitorDone("DONE");

        QStringList tempMonitorAnnounce("ANN Monitor tzcheck 0");
        tempMonitorConnection->writeStringList(tempMonitorAnnounce);
        tempMonitorConnection->readStringList(tempMonitorAnnounce);
        if (tempMonitorAnnounce.empty() ||
            tempMonitorAnnounce[0] == "ERROR")
        {
            tempMonitorConnection->DownRef();
            tempMonitorConnection = NULL;
            if (tempMonitorAnnounce.empty())
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to open event socket, timeout");
            }
            else
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to open event socket" +
                        ((tempMonitorAnnounce.size() >= 2) ?
                         QString(", error was %1").arg(tempMonitorAnnounce[1]) :
                         QString(", remote error")));
            }
        }

        QStringList tzCheck("QUERY_TIME_ZONE");
        if (tempMonitorConnection)
        {
            tempMonitorConnection->writeStringList(tzCheck);
            tempMonitorConnection->readStringList(tzCheck);
        }
        if (tzCheck.size() && !checkTimeZone(tzCheck))
        {
            // Check for different time zones, different offsets, different
            // times
            VERBOSE(VB_IMPORTANT, "The time and/or time zone settings on "
                    "this system do not match those in use on the master "
                    "backend. Please ensure all frontend and backend "
                    "systems are configured to use the same time zone and "
                    "have the current time properly set.");
            VERBOSE(VB_IMPORTANT,
                    "Unable to run with invalid time settings. Exiting.");
            tempMonitorConnection->writeStringList(tempMonitorDone);
            tempMonitorConnection->DownRef();
            return GENERIC_EXIT_INVALID_TIMEZONE;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Backend is running in %1 time zone.")
                    .arg(getTimeZoneID()));
        }
        if (tempMonitorConnection)
            tempMonitorConnection->writeStringList(tempMonitorDone);
    }
    if (tempMonitorConnection)
        tempMonitorConnection->DownRef();

    return GENERIC_EXIT_OK;
}

int setup_basics(const MythCommandLineParser &cmdline)
{
    ofstream pidfs;
    if (!openPidfile(pidfs, cmdline.GetPIDFilename()))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to ignore SIGPIPE");

    if (cmdline.IsDaemonizeEnabled() && (daemon(0, 1) < 0))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to daemonize" + ENO);
        return GENERIC_EXIT_DAEMONIZING_ERROR;
    }

    QString username = cmdline.GetUsername();
    if (!username.isEmpty() && !setUser(username))
        return GENERIC_EXIT_PERMISSIONS_ERROR;

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    return GENERIC_EXIT_OK;
}

void print_warnings(const MythCommandLineParser &cmdline)
{
    if (!cmdline.IsHouseKeeperEnabled())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "****** The Housekeeper has been DISABLED with "
                "the --nohousekeeper option ******");
    }
    if (!cmdline.IsSchedulerEnabled())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "********** The Scheduler has been DISABLED with "
                "the --nosched option **********");
    }
    if (!cmdline.IsAutoExpirerEnabled())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "********* Auto-Expire has been DISABLED with "
                "the --noautoexpire option ********");
    }
    if (!cmdline.IsJobQueueEnabled())
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "********* The JobQueue has been DISABLED with "
                "the --nojobqueue option *********");
    }
}

int run_backend(const MythCommandLineParser &cmdline)
{
    if (!setup_context(cmdline))
        return GENERIC_EXIT_NO_MYTHCONTEXT;

    if (!UpgradeTVDatabaseSchema(true, true))
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    ///////////////////////////////////////////

    bool ismaster = gCoreContext->IsMasterHost();

    g_pUPnp = new MediaServer(ismaster, !cmdline.IsUPnPEnabled() );

    if (!ismaster)
    {
        int ret = connect_to_master();
        if (ret != GENERIC_EXIT_OK)
            return ret;
    }

    QString myip = gCoreContext->GetSetting("BackendServerIP");
    int     port = gCoreContext->GetNumSetting("BackendServerPort", 6543);
    if (myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return GENERIC_EXIT_SETUP_ERROR;
    }

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    if (ismaster)
    {
        VERBOSE(VB_GENERAL, LOC + "Starting up as the master server.");
        gCoreContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as master server", "");
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + "Running as a slave backend.");
        gCoreContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as a slave backend", "");
    }

    print_warnings(cmdline);

    bool fatal_error = false;
    bool runsched = setupTVs(ismaster, fatal_error);
    if (fatal_error)
    {
        delete sysEventHandler;
        return GENERIC_EXIT_SETUP_ERROR;
    }

    if (ismaster)
    {
        if (runsched)
        {
            sched = new Scheduler(true, &tvList);
            int err = sched->GetError();
            if (err)
                return err;

            if (!cmdline.IsSchedulerEnabled())
                sched->DisableScheduling();
        }

        if (cmdline.IsHouseKeeperEnabled())
            housekeeping = new HouseKeeper(true, ismaster, sched);

        if (cmdline.IsAutoExpirerEnabled())
        {
            expirer = new AutoExpire(&tvList);
            if (sched)
                sched->SetExpirer(expirer);
        }
    }
    else if (cmdline.IsHouseKeeperEnabled())
    {
        housekeeping = new HouseKeeper(true, ismaster, NULL);
    }

    if (cmdline.IsJobQueueEnabled())
        jobqueue = new JobQueue(ismaster);

    // Setup status server
    HttpStatus *httpStatus = NULL;
    HttpServer *pHS = g_pUPnp->GetHttpServer();
    if (pHS)
    {
        VERBOSE(VB_IMPORTANT, "Main::Registering HttpStatus Extension");

        httpStatus = new HttpStatus(&tvList, sched, expirer, ismaster);
        if (httpStatus)
            pHS->RegisterExtension(httpStatus);
    }

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1")
            .arg(verboseString));

    MainServer *mainServer = new MainServer(
        ismaster, port, &tvList, sched, expirer);

    int exitCode = mainServer->GetExitCode();
    if (exitCode != GENERIC_EXIT_OK)
    {
        VERBOSE(VB_IMPORTANT, "Backend exiting, MainServer initialization "
                "error.");
        delete mainServer;
        return exitCode;
    }

    if (httpStatus && mainServer)
        httpStatus->SetMainServer(mainServer);

    StorageGroup::CheckAllStorageGroupDirs();

    if (gCoreContext->IsMasterBackend())
        SendMythSystemEvent("MASTER_STARTED");

    ///////////////////////////////
    ///////////////////////////////
    exitCode = qApp->exec();
    ///////////////////////////////
    ///////////////////////////////

    if (gCoreContext->IsMasterBackend())
    {
        SendMythSystemEvent("MASTER_SHUTDOWN");
        qApp->processEvents();
    }

    gCoreContext->LogEntry("mythbackend", LP_INFO, "MythBackend exiting", "");

    delete sysEventHandler;
    delete mainServer;

    return exitCode;
}
