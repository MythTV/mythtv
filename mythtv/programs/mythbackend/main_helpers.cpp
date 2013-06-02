// POSIX headers
#include <sys/time.h>     // for setpriority
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>

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
#include "backendhousekeeper.h"

#include "mythcontext.h"
#include "mythversion.h"
#include "mythdb.h"
#include "dbutil.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "dbcheck.h"
#include "jobqueue.h"
#include "previewgenerator.h"
#include "commandlineparser.h"
#include "mythsystemevent.h"
#include "main_helpers.h"
#include "backendcontext.h"
#include "mythtranslation.h"
#include "mythtimezone.h"
#include "signalhandling.h"
#include "hardwareprofile.h"

#include "mediaserver.h"
#include "httpstatus.h"
#include "mythlogging.h"

#define LOC      QString("MythBackend: ")
#define LOC_WARN QString("MythBackend, Warning: ")
#define LOC_ERR  QString("MythBackend, Error: ")

static MainServer *mainServer = NULL;

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
            MythDB::DBError("Updating record basename", query);

        // Hack to make sure record.station gets set if the user
        // downgrades to a prior version and creates new entries
        // without it.
        if (!query.exec("UPDATE channel SET callsign=chanid "
                        "WHERE callsign IS NULL OR callsign='';"))
            MythDB::DBError("Updating channel callsign", query);

        if (query.exec("SELECT MIN(chanid) FROM channel;"))
        {
            query.first();
            int min_chanid = query.value(0).toInt();
            if (!query.exec(QString("UPDATE record SET chanid = %1 "
                                    "WHERE chanid IS NULL;").arg(min_chanid)))
                MythDB::DBError("Updating record chanid", query);
        }
        else
            MythDB::DBError("Querying minimum chanid", query);

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
                    MythDB::DBError("Updating record station", update_record);
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
            LOG(VB_GENERAL, LOG_ERR, cidmsg +
                " does not have a hostname defined.\n"
                "Please run setup and confirm all of the capture cards.\n");
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
                    LOG(VB_GENERAL, LOG_ERR, "Problem with capture cards. " +
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
                    LOG(VB_GENERAL, LOG_ERR, "Problem with capture cards" +
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                "No valid capture cards are defined in the database.");
    }

    return true;
}

void cleanup(void)
{
    if (mainServer)
        mainServer->Stop();

    delete housekeeping;
    housekeeping = NULL;

    if (gCoreContext)
    {
        delete gCoreContext->GetScheduler();
        gCoreContext->SetScheduler(NULL);
    }

    delete expirer;
    expirer = NULL;

    delete jobqueue;
    jobqueue = NULL;

    delete g_pUPnp;
    g_pUPnp = NULL;

    if (SSDP::Instance())
    {
        SSDP::Instance()->RequestTerminate();
        SSDP::Instance()->wait();
    }

    if (TaskQueue::Instance())
    {
        TaskQueue::Instance()->RequestTerminate();
        TaskQueue::Instance()->wait();
    }

    while (!TVRec::cards.empty())
    {
        TVRec *rec = *TVRec::cards.begin();
        delete rec;
    }

    delete gContext;
    gContext = NULL;

    delete mainServer;
    mainServer = NULL;

    if (pidfile.size())
    {
        unlink(pidfile.toLatin1().constData());
        pidfile.clear();
    }

    SignalHandler::Done();
}

int handle_command(const MythBackendCommandLineParser &cmdline)
{
    QString eventString;

    if (cmdline.toBool("event"))
        eventString = cmdline.toString("event");
    else if (cmdline.toBool("systemevent"))
        eventString = "SYSTEM_EVENT " +
                      cmdline.toString("systemevent") +
                      QString(" SENDER %1").arg(gCoreContext->GetHostName());

    if (!eventString.isEmpty())
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            gCoreContext->SendMessage(eventString);
            return GENERIC_EXIT_OK;
        }
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (cmdline.toBool("setverbose"))
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            QString message = "SET_VERBOSE ";
            message += cmdline.toString("setverbose");

            gCoreContext->SendMessage(message);
            LOG(VB_GENERAL, LOG_INFO,
                QString("Sent '%1' message").arg(message));
            return GENERIC_EXIT_OK;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Unable to connect to backend, verbose mask unchanged ");
            return GENERIC_EXIT_CONNECT_ERROR;
        }
    }

    if (cmdline.toBool("setloglevel"))
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            QString message = "SET_LOG_LEVEL ";
            message += cmdline.toString("setloglevel");

            gCoreContext->SendMessage(message);
            LOG(VB_GENERAL, LOG_INFO,
                QString("Sent '%1' message").arg(message));
            return GENERIC_EXIT_OK;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR,
                "Unable to connect to backend, log level unchanged ");
            return GENERIC_EXIT_CONNECT_ERROR;
        }
    }

    if (cmdline.toBool("clearcache"))
    {
        if (gCoreContext->ConnectToMasterServer())
        {
            gCoreContext->SendMessage("CLEAR_SETTINGS_CACHE");
            LOG(VB_GENERAL, LOG_INFO, "Sent CLEAR_SETTINGS_CACHE message");
            return GENERIC_EXIT_OK;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "Unable to connect to backend, settings "
                    "cache will not be cleared.");
            return GENERIC_EXIT_CONNECT_ERROR;
        }
    }

    if (cmdline.toBool("printsched") ||
        cmdline.toBool("testsched"))
    {
        Scheduler *sched = new Scheduler(false, &tvList);
        if (!cmdline.toBool("testsched") &&
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
            ProgramInfo::CheckProgramIDAuthorities();
            sched->FillRecordListFromDB();
        }

        verboseMask |= VB_SCHEDULE;
        LogLevel_t oldLogLevel = logLevel;
        logLevel = LOG_DEBUG;
        sched->PrintList(true);
        logLevel = oldLogLevel;
        delete sched;
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("resched"))
    {
        bool ok = false;
        if (gCoreContext->ConnectToMasterServer())
        {
            LOG(VB_GENERAL, LOG_INFO, "Connected to master for reschedule");
            ScheduledRecording::RescheduleMatch(0, 0, 0, QDateTime(),
                                                "MythBackendCommand");
            ok = true;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, "Cannot connect to master for reschedule");

        return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_CONNECT_ERROR;
    }

    if (cmdline.toBool("scanvideos"))
    {
        bool ok = false;
        if (gCoreContext->ConnectToMasterServer())
        {
            gCoreContext->SendReceiveStringList(QStringList() << "SCAN_VIDEOS");
            LOG(VB_GENERAL, LOG_INFO, "Requested video scan");
            ok = true;
        }
        else
            LOG(VB_GENERAL, LOG_ERR, "Cannot connect to master for video scan");

        return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_CONNECT_ERROR;
    }

    if (cmdline.toBool("printexpire"))
    {
        expirer = new AutoExpire();
        expirer->PrintExpireList(cmdline.toString("printexpire"));
        return GENERIC_EXIT_OK;
    }

    // This should never actually be reached..
    return GENERIC_EXIT_OK;
}
using namespace MythTZ;

int connect_to_master(void)
{
    MythSocket *tempMonitorConnection = new MythSocket();
    if (tempMonitorConnection->ConnectToHost(
            gCoreContext->GetSetting("MasterServerIP", "127.0.0.1"),
            gCoreContext->GetNumSetting("MasterServerPort", 6543)))
    {
        if (!gCoreContext->CheckProtoVersion(tempMonitorConnection))
        {
            LOG(VB_GENERAL, LOG_ERR, "Master backend is incompatible with "
                    "this backend.\nCannot become a slave.");
            return GENERIC_EXIT_CONNECT_ERROR;
        }

        QStringList tempMonitorDone("DONE");

        QStringList tempMonitorAnnounce("ANN Monitor tzcheck 0");
        tempMonitorConnection->SendReceiveStringList(tempMonitorAnnounce);
        if (tempMonitorAnnounce.empty() ||
            tempMonitorAnnounce[0] == "ERROR")
        {
            tempMonitorConnection->DecrRef();
            tempMonitorConnection = NULL;
            if (tempMonitorAnnounce.empty())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to open event socket, timeout");
            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "Failed to open event socket" +
                    ((tempMonitorAnnounce.size() >= 2) ?
                    QString(", error was %1").arg(tempMonitorAnnounce[1]) :
                    QString(", remote error")));
            }
        }

        QStringList timeCheck;
        if (tempMonitorConnection)
        {
            timeCheck.push_back("QUERY_TIME_ZONE");
            tempMonitorConnection->SendReceiveStringList(timeCheck);
        }
        if (timeCheck.size() < 3)
        {
            return GENERIC_EXIT_SOCKET_ERROR;
        }
        tempMonitorConnection->WriteStringList(tempMonitorDone);

        QDateTime our_time = MythDate::current();
        QDateTime master_time = MythDate::fromString(timeCheck[2]);
        int timediff = abs(our_time.secsTo(master_time));

        if (timediff > 300)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Current time on the master backend differs by "
                        "%1 seconds from time on this system. Exiting.")
                .arg(timediff));
            tempMonitorConnection->DecrRef();
            return GENERIC_EXIT_INVALID_TIME;
        }

        if (timediff > 20)
        {
            LOG(VB_GENERAL, LOG_WARNING,
                    QString("Time difference between the master "
                            "backend and this system is %1 seconds.")
                .arg(timediff));
        }
    }
    if (tempMonitorConnection)
        tempMonitorConnection->DecrRef();

    return GENERIC_EXIT_OK;
}


void print_warnings(const MythBackendCommandLineParser &cmdline)
{
    if (cmdline.toBool("nohousekeeper"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "****** The Housekeeper has been DISABLED with "
            "the --nohousekeeper option ******");
    }
    if (cmdline.toBool("nosched"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "********** The Scheduler has been DISABLED with "
            "the --nosched option **********");
    }
    if (cmdline.toBool("noautoexpire"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "********* Auto-Expire has been DISABLED with "
            "the --noautoexpire option ********");
    }
    if (cmdline.toBool("nojobqueue"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "********* The JobQueue has been DISABLED with "
            "the --nojobqueue option *********");
    }
}

int run_backend(MythBackendCommandLineParser &cmdline)
{
    if (!DBUtil::CheckTimeZoneSupport())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "MySQL time zone support is missing.  "
            "Please install it and try again.  "
            "See 'mysql_tzinfo_to_sql' for assistance.");
        return GENERIC_EXIT_DB_NOTIMEZONE;
    }

    bool ismaster = gCoreContext->IsMasterHost();

    if (!UpgradeTVDatabaseSchema(ismaster, ismaster))
    {
        LOG(VB_GENERAL, LOG_ERR, "Couldn't upgrade database to new schema");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    MythTranslation::load("mythfrontend");

    if (!ismaster)
    {
        int ret = connect_to_master();
        if (ret != GENERIC_EXIT_OK)
            return ret;
    }

    int     port = gCoreContext->GetNumSetting("BackendServerPort", 6543);
    if (gCoreContext->GetSetting("BackendServerIP").isEmpty() &&
        gCoreContext->GetSetting("BackendServerIP6").isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return GENERIC_EXIT_SETUP_ERROR;
    }

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    if (ismaster)
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Starting up as the master server.");
    }
    else
    {
        LOG(VB_GENERAL, LOG_NOTICE, LOC + "Running as a slave backend.");
    }

    print_warnings(cmdline);

    bool fatal_error = false;
    bool runsched = setupTVs(ismaster, fatal_error);
    if (fatal_error)
    {
        delete sysEventHandler;
        return GENERIC_EXIT_SETUP_ERROR;
    }

    Scheduler *sched = NULL;
    if (ismaster)
    {
        if (runsched)
        {
            sched = new Scheduler(true, &tvList);
            int err = sched->GetError();
            if (err)
                return err;

            if (cmdline.toBool("nosched"))
                sched->DisableScheduling();
        }

        if (!cmdline.toBool("noautoexpire"))
        {
            expirer = new AutoExpire(&tvList);
            if (sched)
                sched->SetExpirer(expirer);
        }
        gCoreContext->SetScheduler(sched);
    }

    if (!cmdline.toBool("nohousekeeper"))
    {
        housekeeping = new HouseKeeper();

        if (ismaster)
        {
            housekeeping->RegisterTask(new LogCleanerTask());
            housekeeping->RegisterTask(new CleanupTask());
            housekeeping->RegisterTask(new ThemeUpdateTask());
            housekeeping->RegisterTask(new ArtworkTask());
            housekeeping->RegisterTask(new MythFillDatabaseTask());
        }

        housekeeping->RegisterTask(new JobQueueRecoverTask());
#ifdef __linux__
 #ifdef CONFIG_BINDINGS_PYTHON
        housekeeping->RegisterTask(new HardwareProfileTask());
 #endif
#endif

        housekeeping->Start();
    }

    if (!cmdline.toBool("nojobqueue"))
        jobqueue = new JobQueue(ismaster);

    // ----------------------------------------------------------------------
    //
    // ----------------------------------------------------------------------

    if (g_pUPnp == NULL)
    {
        g_pUPnp = new MediaServer();

        g_pUPnp->Init(ismaster, cmdline.toBool("noupnp"));
    }

    // ----------------------------------------------------------------------
    // Setup status server
    // ----------------------------------------------------------------------

    HttpStatus *httpStatus = NULL;
    HttpServer *pHS = g_pUPnp->GetHttpServer();

    if (pHS)
    {
        LOG(VB_GENERAL, LOG_INFO, "Main::Registering HttpStatus Extension");

        httpStatus = new HttpStatus( &tvList, sched, expirer, ismaster );
        pHS->RegisterExtension( httpStatus );
    }

    mainServer = new MainServer(
        ismaster, port, &tvList, sched, expirer);

    int exitCode = mainServer->GetExitCode();
    if (exitCode != GENERIC_EXIT_OK)
    {
        LOG(VB_GENERAL, LOG_CRIT,
            "Backend exiting, MainServer initialization error.");
        cleanup();
	return exitCode;
    }

    if (httpStatus && mainServer)
        httpStatus->SetMainServer(mainServer);

    StorageGroup::CheckAllStorageGroupDirs();

    if (gCoreContext->IsMasterBackend())
        gCoreContext->SendSystemEvent("MASTER_STARTED");

    ///////////////////////////////
    ///////////////////////////////
    exitCode = qApp->exec();
    ///////////////////////////////
    ///////////////////////////////

    if (gCoreContext->IsMasterBackend())
    {
        gCoreContext->SendSystemEvent("MASTER_SHUTDOWN");
        qApp->processEvents();
    }

    LOG(VB_GENERAL, LOG_NOTICE, "MythBackend exiting");

    delete sysEventHandler;

    return exitCode;
}
