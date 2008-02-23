// POSIX headers
#include <sys/time.h>     // for setpriority

#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <qregexp.h>
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
#include <cstdlib>
using namespace std;

#include "tv_rec.h"
#include "autoexpire.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"
#include "remoteutil.h"
#include "housekeeper.h"

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/exitcodes.h"
#include "libmyth/compat.h"
#include "libmyth/storagegroup.h"
#include "libmythtv/programinfo.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/previewgenerator.h"

#include "mediaserver.h"
#include "httpstatus.h"

QMap<int, EncoderLink *> tvList;
AutoExpire *expirer = NULL;
Scheduler *sched = NULL;
JobQueue *jobqueue = NULL;
QString pidfile;
HouseKeeper *housekeeping = NULL;
QString logfile = "";

MediaServer *g_pUPnp       = NULL;

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

    if (!query.exec(
            "SELECT cardid, hostname "
            "FROM capturecard "
            "ORDER BY cardid"))
    {
        MythContext::DBError("Querying Recorders", query);
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
            gContext->LogEntry("mythbackend", LP_CRITICAL,
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
                if (tv->Init())
                {
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    gContext->LogEntry("mythbackend", LP_CRITICAL,
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
                if (tv->Init())
                {
                    EncoderLink *enc = new EncoderLink(cardid, tv);
                    tvList[cardid] = enc;
                }
                else
                {
                    gContext->LogEntry("mythbackend", LP_CRITICAL,
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
        cerr << "ERROR: no valid capture cards are defined in the database.\n";
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

    if (g_pUPnp)
        delete g_pUPnp;

    if (pidfile != "")
        unlink(pidfile.ascii());

    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1, SIG_DFL);
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

void upnp_rebuild(int)
{
    if (gContext->IsMasterHost())
    {
        g_pUPnp->RebuildMediaMap();
    }

}

int preview_helper(const QString &chanid, const QString &starttime,
                   long long previewFrameNumber, long long previewSeconds,
                   const QSize &previewSize,
                   const QString &infile, const QString &outfile)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        VERBOSE(VB_GENERAL, "Setting priority failed." + ENO);

    ProgramInfo *pginfo = NULL;
    if (!chanid.isEmpty() && !starttime.isEmpty())
    {
        pginfo = ProgramInfo::GetProgramFromRecorded(chanid, starttime);
        if (!pginfo)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Can not locate recording made on '%1' at '%2'")
                    .arg(chanid).arg(starttime));
            return GENERIC_EXIT_NOT_OK;
        }
    }
    else if (!infile.isEmpty())
    {
        pginfo = ProgramInfo::GetProgramFromBasename(infile);
        if (!pginfo)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Can not locate recording '%1'").arg(infile));
            return GENERIC_EXIT_NOT_OK;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Can not locate recording for preview");
        return GENERIC_EXIT_NOT_OK;
    }

    PreviewGenerator *previewgen = new PreviewGenerator(pginfo, true);

    if (previewFrameNumber >= 0)
        previewgen->SetPreviewTimeAsFrameNumber(previewFrameNumber);

    if (previewSeconds >= 0)
        previewgen->SetPreviewTimeAsSeconds(previewSeconds);

    previewgen->SetOutputSize(previewSize);
    previewgen->SetOutputFilename(outfile);
    previewgen->RunReal();
    previewgen->deleteLater();

    delete pginfo;

    return GENERIC_EXIT_OK;
}

// [WxH] | [WxH@]seconds[S] | [WxH@]frame_numF
bool parse_preview_info(const QString &param,
                        long long     &previewFrameNumber,
                        long long     &previewSeconds,
                        QSize         &previewSize)
{
    previewFrameNumber = -1;
    previewSeconds = -1;
    previewSize = QSize(0,0);
    if (param.isEmpty())
        return true;

    int xat = param.find("x", 0, false);
    int aat = param.find("@", 0, false);
    if (xat > 0)
    {
        QString widthStr  = param.left(xat);
        QString heightStr = QString::null;
        if (aat > xat)
            heightStr = param.mid(xat + 1, aat - xat - 1);
        else
            heightStr = param.mid(xat + 1);

        bool ok1, ok2;
        previewSize = QSize(widthStr.toInt(&ok1), heightStr.toInt(&ok2));
        if (!ok1 || !ok2)
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Error: Failed to parse --generate-preview "
                        "param '%1'").arg(param));
        }
    }
    if ((xat > 0) && (aat < xat))
        return true;

    QString lastChar = param.at(param.length() - 1).lower();
    QString frameNumStr = QString::null;
    QString secsStr = QString::null;
    if (lastChar == "f")
        frameNumStr = param.mid(aat + 1, param.length() - aat - 2);
    else if (lastChar == "s")
        secsStr = param.mid(aat + 1, param.length() - aat - 2);
    else
        secsStr = param.mid(aat + 1, param.length() - aat - 1);

    bool ok = false;
    if (!frameNumStr.isEmpty())
        previewFrameNumber = frameNumStr.toUInt(&ok);
    else if (!secsStr.isEmpty())
        previewSeconds = secsStr.toUInt(&ok);

    if (!ok)
    {
        VERBOSE(VB_IMPORTANT, QString(
                    "Error: Failed to parse --generate-preview "
                    "param '%1'").arg(param));
    }

    return ok;
}

int main(int argc, char **argv)
{
    bool need_gui = false;
#ifndef _WIN32
    for (int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    need_gui = true;
#endif

    QApplication a(argc, argv, need_gui);

    QMap<QString, QString> settingsOverride;
    QString binname = basename(a.argv()[0]);

    long long previewFrameNumber = -2;
    long long previewSeconds     = -2;
    QSize previewSize(0,0);
    QString chanid    = QString::null;
    QString starttime = QString::null;
    QString infile    = QString::null;
    QString outfile   = QString::null;

    bool daemonize = false;
    bool printsched = false;
    bool testsched = false;
    bool resched = false;
    bool nosched = false;
    bool noupnp = false;
    bool nojobqueue = false;
    bool nohousekeeper = false;
    bool noexpirer = false;
    QString printexpire = "";
    bool clearsettingscache = false;
    bool wantupnprebuild = false;

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
        else if (!strcmp(a.argv()[argpos],"--noupnp"))
        {
            noupnp = true;
        }
        else if (!strcmp(a.argv()[argpos],"--upnprebuild"))
        {
            wantupnprebuild = true;
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
            printexpire = "ALL";
            if ((a.argc()-1 > argpos) && a.argv()[argpos+1][0] != '-')
            {
                printexpire = a.argv()[argpos+1];
                ++argpos;
            }
        } 
        else if (!strcmp(a.argv()[argpos],"--clearcache"))
        {
            clearsettingscache = true;
        } 
        else if (!strcmp(a.argv()[argpos],"--version"))
        {
            extern const char *myth_source_version;
            extern const char *myth_source_path;
            cout << "Please include all output in bug reports." << endl;
            cout << "MythTV Version   : " << myth_source_version << endl;
            cout << "MythTV Branch    : " << myth_source_path << endl;
            cout << "Library API      : " << MYTH_BINARY_VERSION << endl;
            cout << "Network Protocol : " << MYTH_PROTO_VERSION << endl;
#ifdef MYTH_BUILD_CONFIG
            cout << "Options compiled in:" <<endl;
            cout << MYTH_BUILD_CONFIG << endl;
#endif
            return BACKEND_EXIT_OK;
        }
        else if (!strcmp(a.argv()[argpos],"--generate-preview"))
        {
            QString tmp = QString::null;
            if ((argpos + 1) < a.argc())
            {
                tmp = a.argv()[argpos+1];
                bool ok = true;
                if (tmp.left(1) == "-")
                    tmp.left(2).toInt(&ok);
                if (ok)
                    argpos++;
                else
                    tmp = QString::null;
            }

            if (!parse_preview_info(tmp, previewFrameNumber, previewSeconds,
                                    previewSize))
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Unable to parse --generate-preview "
                                "option '%1'").arg(tmp));

                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-c") ||
                 !strcmp(a.argv()[argpos],"--chanid"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "-", 1))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --chanid option");

                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            chanid = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
                 !strcmp(a.argv()[argpos],"--starttime"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "-", 1))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --starttime option");
                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            starttime = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"--infile"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "-", 1))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --infile option");

                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            infile = a.argv()[++argpos];
        }
        else if (!strcmp(a.argv()[argpos],"--outfile"))
        {
            if (((argpos + 1) >= a.argc()) ||
                !strncmp(a.argv()[argpos + 1], "-", 1))
            {
                VERBOSE(VB_IMPORTANT,
                        "Missing or invalid parameters for --outfile option");

                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            outfile = a.argv()[++argpos];
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
                    "--noupnp                       Do not enable the UPNP server" << endl <<
                    "--nojobqueue                   Do not start the JobQueue" << endl <<
                    "--nohousekeeper                Do not start the Housekeeper" << endl <<
                    "--noautoexpire                 Do not start the AutoExpire thread" << endl <<
                    "--clearcache                   Clear the settings cache on all myth servers" << endl <<
                    "--version                      Version information" << endl <<
                    "--generate-preview             Generate a preview image" << endl <<
                    "--upnprebuild                  Force an update of UPNP media" << endl <<
                    "--infile                       Input file for preview generation" << endl <<
                    "--outfile                      Optional output file for preview generation" << endl <<
                    "--chanid                       Channel ID for preview generation" << endl <<
                    "--starttime                    Recording start time for preview generation" << endl;
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (((previewFrameNumber >= -1) || previewSeconds >= -1) &&
        (chanid.isEmpty() || starttime.isEmpty()) && infile.isEmpty())
    {
        cerr << "--generate-preview must be accompanied by either " <<endl
             << "\nboth --chanid and --starttime paramaters, " << endl
             << "\nor the --infile paramater." << endl;
        return BACKEND_EXIT_INVALID_CMDLINE;
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
            return BACKEND_EXIT_DAEMONIZING_ERROR;
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

    if (wantupnprebuild)
    {
        VERBOSE(VB_GENERAL, "Rebuilding UPNP Media Map");

        UPnpMedia *rebuildit = new UPnpMedia(false,false);
        rebuildit->BuildMediaMap();

        return BACKEND_EXIT_OK;
    }

    if (clearsettingscache)
    {
        if (gContext->ConnectToMasterServer())
        {
            RemoteSendMessage("CLEAR_SETTINGS_CACHE");
            VERBOSE(VB_IMPORTANT, "Sent CLEAR_SETTINGS_CACHE message");
            return BACKEND_EXIT_OK;
        }
        else
        {
            VERBOSE(VB_IMPORTANT, "Unable to connect to backend, settings "
                    "cache will not be cleared.");
            return BACKEND_EXIT_NO_CONNECT;
        }
    }

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
    if ((CompareTVDatabaseSchemaVersion() > 0) &&
        // and Not MythContext::PromptForSchemaUpgrade() expertMode
        (gContext->GetNumSetting("DBSchemaAutoUpgrade") != -1))
    {
        VERBOSE(VB_IMPORTANT, "The schema version of your existing database "
                "is newer than this version of MythTV understands. Please "
                "ensure that you have selected the proper database server or "
                "upgrade this and all other frontends and backends to the "
                "same MythTV version and revision.");
        return BACKEND_EXIT_DB_OUTOFDATE;
    }
    if (!UpgradeTVDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema");
        return BACKEND_EXIT_DB_OUTOFDATE;
    }    
    gContext->ActivateSettingsCache(true);

    close(0);

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

    if (printexpire != "")
    {
        expirer = new AutoExpire();
        expirer->PrintExpireList(printexpire);
        cleanup();
        return BACKEND_EXIT_OK;
    }

    if ((previewFrameNumber >= -1) || (previewSeconds >= -1))
    {
        return preview_helper(
            chanid, starttime,
            previewFrameNumber, previewSeconds, previewSize,
            infile, outfile);
    }

    int port = gContext->GetNumSetting("BackendServerPort", 6543);

    QString myip = gContext->GetSetting("BackendServerIP");
    if (myip.isNull() || myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return BACKEND_EXIT_NO_IP_ADDRESS;
    }

    bool ismaster = gContext->IsMasterHost();

    if (ismaster)
    {
        cerr << "Starting up as the master server.\n";
        gContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as master server", "");

        if (nosched)
            cerr << "********** The Scheduler has been DISABLED with "
                    "the --nosched option **********\n";

        // kill -USR1 mythbackendpid will force a upnpmedia rebuild
        signal(SIGUSR1, &upnp_rebuild);
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

    if (ismaster && runsched)
    {
        sched = new Scheduler(true, &tvList);

        if (nosched)
            sched->DisableScheduling();
    }

    if (ismaster)
    {
        if (noexpirer)
            cerr << "********* Auto-Expire has been DISABLED with "
                    "the --noautoexpire option ********\n";
        else
            expirer = new AutoExpire(&tvList);
    }

    if (sched && expirer)
        sched->SetExpirer(expirer);

    if (nojobqueue)
        cerr << "********* The JobQueue has been DISABLED with "
                "the --nojobqueue option *********\n";
    else
        jobqueue = new JobQueue(ismaster);

    // Start UPnP Services 

    g_pUPnp = new MediaServer( ismaster, noupnp );

    HttpServer *pHS = g_pUPnp->GetHttpServer();
    if (pHS)
    {
        VERBOSE(VB_IMPORTANT, "Main::Registering HttpStatus Extension");

        pHS->RegisterExtension( new HttpStatus( &tvList, sched,
                                                expirer, ismaster ));
    }

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 www.mythtv.org")
                            .arg(binname).arg(MYTH_BINARY_VERSION));

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1").arg(verboseString));

    new MainServer(ismaster, port, &tvList, sched, expirer);

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

    StorageGroup::CheckAllStorageGroupDirs();
 
    a.exec();

    gContext->LogEntry("mythbackend", LP_INFO, "MythBackend exiting", "");
    cleanup();

    return BACKEND_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
