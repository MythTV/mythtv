#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <cerrno>

#include "mythconfig.h"
#ifdef CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

#include <iostream>
#include <fstream>
using namespace std;

#include "tv.h"
#include "autoexpire.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"
#include "remoteutil.h"
#include "housekeeper.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/exitcodes.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/jobqueue.h"

QMap<int, EncoderLink *> tvList;
AutoExpire *expirer = NULL;
Scheduler *sched = NULL;
JobQueue *jobqueue = NULL;
QString pidfile;
QString lockfile_location;
HouseKeeper *housekeeping = NULL;
QString logfile = "";

bool setupTVs(bool ismaster, bool &error)
{
    error = false;
    QString localhostname = gContext->GetHostName();

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
            MythContext::DBError("Updating record basename",
                                 query.lastQuery());

        // Hack to make sure record.station gets set if the user
        // downgrades to a prior version and creates new entries
        // without it.
        if (!query.exec("UPDATE channel SET callsign=chanid "
                        "WHERE callsign IS NULL OR callsign='';"))
            MythContext::DBError("Updating channel callsign",
                                 query.lastQuery());

        if (query.exec("SELECT MIN(chanid) FROM channel;"))
        {
            query.first();
            int min_chanid = query.value(0).toInt();
            if (!query.exec(QString("UPDATE record SET chanid = %1 "
                                    "WHERE chanid IS NULL;").arg(min_chanid)))
                MythContext::DBError("Updating record chanid",
                                     query.lastQuery());
        }
        else
            MythContext::DBError("Querying minimum chanid",
                                 query.lastQuery());

        MSqlQuery records_without_station(MSqlQuery::InitCon());
        records_without_station.prepare("SELECT record.chanid,"
                " channel.callsign FROM record LEFT JOIN channel"
                " ON record.chanid = channel.chanid WHERE record.station='';");
        records_without_station.exec(); 
        if (records_without_station.first())
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
                    MythContext::DBError("Updating record station",
                            update_record.lastQuery());
                }
            } while (records_without_station.next());
        }
    }

    query.exec("SELECT cardid, hostname "
               "FROM capturecard "
               "WHERE parentid = '0' "
               "ORDER BY cardid");

    if (query.isActive() && query.size())
    {
        while (query.next())
        {
            int cardid = query.value(0).toInt();
            QString host = query.value(1).toString();

            if (host.isNull() || host.isEmpty())
            {
                QString msg = "One of your capturecard entries does not have a "
                              "hostname.\n  Please run setup and confirm all "
                              "of the capture cards.\n";

                cerr << msg;
                gContext->LogEntry("mythbackend", LP_CRITICAL,
                                   "Problem with capture cards", msg);
                error = true;
            }

            if (!ismaster)
            {
                if (host == localhostname)
                {
                    TVRec *tv = new TVRec(cardid);
                    tv->Init();
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
            }
            else
            {
                if (host == localhostname)
                {
                    TVRec *tv = new TVRec(cardid);
                    tv->Init();
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    EncoderLink *enc = new EncoderLink(cardid, NULL, host);
                    tvList[cardid] = enc;
                }
            }
        }
    }
    else
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        cerr << "Perhaps you should read the installation instructions?\n";
        gContext->LogEntry("mythbackend", LP_CRITICAL,
                           "No capture cards are defined", 
                           "Please run the setup program.");
        return false;
    }

    return true;
}

void cleanup(void) 
{
    delete gContext;

    if (sched)
        delete sched;

    if (pidfile != "")
        unlink(pidfile.ascii());

    unlink(lockfile_location.ascii());

    signal(SIGHUP, SIG_DFL);
}

int log_rotate(int report_error)
{
    /* http://www.gossamer-threads.com/lists/mythtv/dev/110113 */

    int new_logfd = open(logfile, O_WRONLY|O_CREAT|O_APPEND|O_SYNC, 0664);
    if (new_logfd < 0)
    {
        // If we can't open the new logfile, send data to /dev/null
        if (report_error)
        {
            cerr << "cannot open logfile " << logfile << endl;
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


int main(int argc, char **argv)
{
    for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);

    QApplication a(argc, argv, false);

    QMap<QString, QString> settingsOverride;
    QString binname = basename(a.argv()[0]);

    bool daemonize = false;
    bool printsched = false;
    bool testsched = false;
    bool resched = false;
    bool nosched = false;
    bool nojobqueue = false;
    bool nohousekeeper = false;
    bool noexpirer = false;
    bool printexpire = false;
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc() > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
        } 
        else if (!strcmp(a.argv()[argpos],"-p") ||
                 !strcmp(a.argv()[argpos],"--pidfile"))
        {
            if (a.argc() > argpos)
            {
                pidfile = a.argv()[argpos+1];
                if (pidfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -p/--pidfile option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                } 
                else
                {
                   ++argpos;
                }
            }
        } 
        else if (!strcmp(a.argv()[argpos],"-d") ||
                 !strcmp(a.argv()[argpos],"--daemon"))
        {
            daemonize = true;

        }
        else if (!strcmp(a.argv()[argpos],"-O") ||
                 !strcmp(a.argv()[argpos],"--override-setting"))
        {
            if (a.argc()-1 > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -O/--override-setting option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                } 
 
                QStringList pairs = QStringList::split(",", tmpArg);
                for (unsigned int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = QStringList::split("=", pairs[index]);
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            {
                cerr << "Invalid or missing argument to -O/--override-setting option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                 !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return BACKEND_EXIT_INVALID_CMDLINE;

                ++argpos;
            } 
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        } 
        else if (!strcmp(a.argv()[argpos],"--printsched"))
        {
            printsched = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--testsched"))
        {
            testsched = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--resched"))
        {
            resched = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--nosched"))
        {
            nosched = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--nojobqueue"))
        {
            nojobqueue = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--nohousekeeper"))
        {
            nohousekeeper = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--noautoexpire"))
        {
            noexpirer = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--printexpire"))
        {
            printexpire = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--version"))
        {
            extern const char *myth_source_version;
            cout << "Library API version: " << MYTH_BINARY_VERSION << endl;
            cout << "Source code version: " << myth_source_version << endl;
#ifdef MYTH_BUILD_CONFIG
            cout << "Options compiled in:" <<endl;
            cout << MYTH_BUILD_CONFIG << endl;
#endif
            return BACKEND_EXIT_OK;
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: " << endl <<
                    "-h or --help                   List valid command line parameters" << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-p or --pidfile filename       Write PID of mythbackend " <<
                                                    "to filename" << endl <<
                    "-d or --daemon                 Runs mythbackend as a daemon" << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
                    "--printexpire                  List of auto-expire programs" << endl <<
                    "--printsched                   Upcoming scheduled programs" << endl <<
                    "--testsched                    Test run scheduler (ignore existing schedule)" << endl <<
                    "--resched                      Force the scheduler to update" << endl <<
                    "--nosched                      Do not perform any scheduling" << endl <<
                    "--nojobqueue                   Do not start the JobQueue" << endl <<
                    "--nohousekeeper                Do not start the Housekeeper" << endl <<
                    "--noautoexpire                 Do not start the AutoExpire thread" << endl <<
                    "--version                      Version information" << endl;
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (logfile != "" )
    {
        if (log_rotate(1) < 0)
            cerr << "cannot open logfile; using stdout/stderr" << endl;
        else
            signal(SIGHUP, &log_rotate_handler);
    }
    
    ofstream pidfs;
    if (pidfile != "")
    {
        pidfs.open(pidfile.ascii());
        if (!pidfs)
        {
            perror(pidfile.ascii());
            cerr << "Error opening pidfile";
            return BACKEND_EXIT_OPENING_PIDFILE_ERROR;
        }
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    if (daemonize)
        if (daemon(0, 1) < 0)
        {
            perror("daemon");
            return BACKEND_EXIT_DEAMONIZING_ERROR;
        }


    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return BACKEND_EXIT_NO_MYTHCONTEXT;
    }
    gContext->SetBackend(true);

    close(0);

    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(it.data()));
            gContext->OverrideSettingForSession(it.key(), it.data());
        }
    }

    gContext->ActivateSettingsCache(false);
    if (!UpgradeTVDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema");
        return BACKEND_EXIT_DB_OUTOFDATE;
    }    
    gContext->ActivateSettingsCache(true);

    if (printsched || testsched)
    {
        gContext->SetBackend(false);
        sched = new Scheduler(false, &tvList);
        if (!testsched && gContext->ConnectToMasterServer())
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
        cleanup();
        return BACKEND_EXIT_OK;
    }

    if (resched)
    {
        gContext->SetBackend(false);

        bool ok = false;
        if (gContext->ConnectToMasterServer())
        {
            VERBOSE(VB_IMPORTANT, "Connected to master for reschedule");
            ScheduledRecording::signalChange(-1);
            ok = true;
        }
        else
            VERBOSE(VB_IMPORTANT, "Cannot connect to master for reschedule");

        cleanup();
        return (ok) ? BACKEND_EXIT_OK : BACKEND_EXIT_NO_CONNECT;
    }

    if (printexpire)
    {
        expirer = new AutoExpire(false, false);
        expirer->FillExpireList();
        expirer->PrintExpireList();
        cleanup();
        return BACKEND_EXIT_OK;
    }

    int port = gContext->GetNumSetting("BackendServerPort", 6543);
    int statusport = gContext->GetNumSetting("BackendStatusPort", 6544);

    QString myip = gContext->GetSetting("BackendServerIP");
    QString masterip = gContext->GetSetting("MasterServerIP");

    bool ismaster = false;

    if (myip.isNull() || myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return BACKEND_EXIT_NO_IP_ADDRESS;
    }

    if (masterip == myip)
    {
        cerr << "Starting up as the master server.\n";
        gContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as master server", "");
        ismaster = true;

        if (nosched)
            cerr << "********** The Scheduler has been DISABLED with "
                    "the --nosched option **********\n";
    }
    else
    {
        cerr << "Running as a slave backend.\n";
        gContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as a slave backend", "");
    }

    // Get any initial housekeeping done before we fire up anything else
    if (nohousekeeper)
        cerr << "****** The Housekeeper has been DISABLED with "
                "the --nohousekeeper option ******\n";
    else
        housekeeping = new HouseKeeper(true, ismaster);

    bool fatal_error = false;
    bool runsched = setupTVs(ismaster, fatal_error);
    if (fatal_error)
        return BACKEND_EXIT_CAP_CARD_SETUP_ERROR;

    if (ismaster && runsched && !nosched)
    {
        sched = new Scheduler(true, &tvList);
    }

    if (noexpirer)
        cerr << "********* Auto-Expire has been DISABLED with "
                "the --noautoexpire option ********\n";
    else
        expirer = new AutoExpire(true, ismaster);

    if (nojobqueue)
        cerr << "********* The JobQueue has been DISABLED with "
                "the --nojobqueue option *********\n";
    else
        jobqueue = new JobQueue(ismaster);

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 www.mythtv.org")
                            .arg(binname).arg(MYTH_BINARY_VERSION));

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1").arg(verboseString));

    lockfile_location = gContext->GetSetting("RecordFilePrefix") + "/nfslockfile.lock";

    if (ismaster)
// Create a file in the recording directory.  A slave encoder will 
// look for this file and return 0 bytes free if it finds it when it's
// queried about its available/used diskspace.  This will prevent double (or
// more) counting of available disk space.
// If the slave doesn't find this file then it will assume that it has
// a seperate store.
    {
        if (creat(lockfile_location.ascii(), 0664) == -1)
        {
            perror(lockfile_location.ascii()); 
            cerr << "Unable to open lockfile!\n"
                 << "Be sure that \'" << gContext->GetSetting("RecordFilePrefix")
                 << "\' exists and that both \nthe directory and that "
                 << "file are writeable by this user.\n";
            return BACKEND_EXIT_OPENING_VLOCKFILE_ERROR;
        }
    }

    new MainServer(ismaster, port, statusport, &tvList, sched, expirer);

    if (ismaster)
    {
        QString WOLslaveBackends
            = gContext->GetSetting("WOLslaveBackendsCommand","");
        if (!WOLslaveBackends.isEmpty())
        {
            VERBOSE(VB_IMPORTANT, "Waking slave Backends now.");
            system(WOLslaveBackends.ascii());
        }
    }

    a.exec();

    gContext->LogEntry("mythbackend", LP_INFO, "MythBackend exiting", "");
    cleanup();

    return BACKEND_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
