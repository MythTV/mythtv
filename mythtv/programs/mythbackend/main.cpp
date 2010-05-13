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

// C++ headers
#include <iostream>
#include <fstream>
using namespace std;

#ifndef _WIN32
#include <QCoreApplication>
#else
#include <QApplication>
#endif

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QRegExp>

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

#include "mediaserver.h"
#include "httpstatus.h"

#define LOC      QString("MythBackend: ")
#define LOC_WARN QString("MythBackend, Warning: ")
#define LOC_ERR  QString("MythBackend, Error: ")

#ifdef Q_OS_MACX
    // 10.6 uses some file handles for its new Grand Central Dispatch thingy
    #define UNUSED_FILENO 5
#else
    #define UNUSED_FILENO 3
#endif

QMap<int, EncoderLink *> tvList;
AutoExpire  *expirer      = NULL;
Scheduler   *sched        = NULL;
JobQueue    *jobqueue     = NULL;
QString      pidfile;
HouseKeeper *housekeeping = NULL;
QString      logfile      = QString::null;

MediaServer *g_pUPnp      = NULL;

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
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "No valid capture cards are defined in the database.\n\t\t\t"
                "Perhaps you should re-read the installation instructions?");

        gContext->LogEntry("mythbackend", LP_CRITICAL,
                           "No capture cards are defined",
                           "Please run the setup program.");
        return false;
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
        pginfo->pathname = pginfo->GetPlaybackURL(false, true);
    }
    else if (!infile.isEmpty())
    {
        pginfo = ProgramInfo::GetProgramFromBasename(infile);
        if (!pginfo)
        {
            if (!QFileInfo(infile).exists())
            {
                VERBOSE(VB_IMPORTANT, QString(
                            "Can not locate recording '%1'").arg(infile));
                return GENERIC_EXIT_NOT_OK;
            }
            else
            {
                pginfo = new ProgramInfo();
                pginfo->isVideo = true;

                QDir d(infile + "/VIDEO_TS");
                if ((infile.section('.', -1) == "iso") ||
                    (infile.section('.', -1) == "img") ||
                    d.exists())
                {
                    pginfo->pathname = QString("dvd:%1").arg(infile);
                }
                else
                {
                    pginfo->pathname = QFileInfo(infile).absoluteFilePath();
                }
            }

        }
        else
        {
            pginfo->pathname = pginfo->GetPlaybackURL(false, true);
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Can not locate recording for preview");
        return GENERIC_EXIT_NOT_OK;
    }

    PreviewGenerator *previewgen = new PreviewGenerator(
        pginfo, PreviewGenerator::kLocal);

    if (previewFrameNumber >= 0)
        previewgen->SetPreviewTimeAsFrameNumber(previewFrameNumber);

    if (previewSeconds >= 0)
        previewgen->SetPreviewTimeAsSeconds(previewSeconds);

    previewgen->SetOutputSize(previewSize);
    previewgen->SetOutputFilename(outfile);
    bool ok = previewgen->RunReal();
    previewgen->deleteLater();

    delete pginfo;

    return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_NOT_OK;
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

    int xat = param.indexOf("x", 0, Qt::CaseInsensitive);
    int aat = param.indexOf("@", 0);
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

    QString lastChar = param.at(param.length() - 1).toLower();
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

namespace
{
    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
}

void showUsage(const MythCommandLineParser &cmdlineparser, const QString &version)
{
    QString    help  = cmdlineparser.GetHelpString(false);
    QByteArray ahelp = help.toLocal8Bit();

    cerr << qPrintable(version) << endl <<
    "Valid options are: " << endl <<
    "-h or --help                   List valid command line parameters" << endl <<
    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
    "-p or --pidfile filename       Write PID of mythbackend to filename" << endl <<
    "-d or --daemon                 Runs mythbackend as a daemon" << endl <<
    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
    "--setverbose debug-level       Change debug level of running master backend" << endl <<
    "--user username                Drop permissions to username after starting"  << endl <<

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
    ahelp.constData() <<
    "--generate-preview             Generate a preview image" << endl <<
    "--upnprebuild                  Force an update of UPNP media" << endl <<
    "--infile                       Input file for preview generation" << endl <<
    "--outfile                      Optional output file for preview generation" << endl <<
    "--chanid                       Channel ID for preview generation" << endl <<
    "--starttime                    Recording start time for preview generation" << endl <<
    "--event EVENTTEXT              Send a backend event test message" << endl <<
    "--systemevent EVENTTEXT        Send a backend SYSTEM_EVENT test message" << endl
    << endl;

}

int main(int argc, char **argv)
{
    bool cmdline_err;
    MythCommandLineParser cmdline(
        kCLPOverrideSettingsFile |
        kCLPOverrideSettings     |
        kCLPQueryVersion);

    for (int argpos = 0; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
    }

#ifndef _WIN32
    for (int i = UNUSED_FILENO; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);
    QCoreApplication a(argc, argv);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    QApplication a(argc, argv);
#endif

    QString binname = basename(a.argv()[0]);
    extern const char *myth_source_version;
    extern const char *myth_source_path;
    QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
                                 .arg(binname)
                                 .arg(myth_source_path)
                                 .arg(myth_source_version);

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
    bool setverbose = false;
    QString newverbose = "";
    QString username = "";
    bool resched = false;
    bool nosched = false;
    bool noupnp = false;
    bool nojobqueue = false;
    bool nohousekeeper = false;
    bool noexpirer = false;
    QString printexpire = "";
    bool clearsettingscache = false;
    bool wantupnprebuild = false;
    QString eventString;

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
        else if (!strcmp(a.argv()[argpos],"--setverbose"))
        {
            setverbose = true;
            if (a.argc()-1 > argpos)
            {
                newverbose = a.argv()[argpos+1];
                ++argpos;
            } 
            else
            {
                cerr << "Missing argument to --setverbose option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--user"))
        {
            if (a.argc()-1 > argpos)
            {
                username = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                cerr << "Missing argument to --user option\n";
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
        else if (!strcmp(a.argv()[argpos],"--event"))
        {
            if ((a.argc()-1 > argpos) && a.argv()[argpos+1][0] != '-')
            {
                eventString = a.argv()[argpos+1];
                ++argpos;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--systemevent"))
        {
            if ((a.argc()-1 > argpos) && a.argv()[argpos+1][0] != '-')
            {
                eventString = QString("SYSTEM_EVENT ") + a.argv()[argpos+1];
                ++argpos;
            }
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
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
                showUsage(cmdline, versionStr);
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (((previewFrameNumber >= -1) || previewSeconds >= -1) &&
        (chanid.isEmpty() || starttime.isEmpty()) && infile.isEmpty())
    {
        cerr << "--generate-preview must be accompanied by either " <<endl
             << "\nboth --chanid and --starttime parameters, " << endl
             << "\nor the --infile parameter." << endl;
        return BACKEND_EXIT_INVALID_CMDLINE;
    }

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

    CleanupGuard callCleanup(cleanup);

    ofstream pidfs;
    if (pidfile.size())
    {
        pidfs.open(pidfile.toAscii().constData());
        if (!pidfs)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Could not open pid file" + ENO);
            return BACKEND_EXIT_OPENING_PIDFILE_ERROR;
        }
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to ignore SIGPIPE");

    if (daemonize && (daemon(0, 1) < 0))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to daemonize" + ENO);
        return BACKEND_EXIT_DAEMONIZING_ERROR;
    }

    if (!username.isEmpty())
    {
#ifdef _WIN32
        VERBOSE(VB_IMPORTANT, "--user option is not supported on Windows");
        return BACKEND_EXIT_INVALID_CMDLINE;
#else // ! _WIN32
        struct passwd *user_info = getpwnam(username.toLocal8Bit().constData());
        const uid_t user_id = geteuid();

        if (user_id && (!user_info || user_id != user_info->pw_uid))
        {
            VERBOSE(VB_IMPORTANT,
                    "You must be running as root to use the --user switch.");
            return BACKEND_EXIT_PERMISSIONS_ERROR;
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
                return BACKEND_EXIT_PERMISSIONS_ERROR;
            }
            if (setgid(user_info->pw_gid) == -1)
            {
                VERBOSE(VB_IMPORTANT, "Error setting effective group.");
                return BACKEND_EXIT_PERMISSIONS_ERROR;
            }
            if (initgroups(user_info->pw_name, user_info->pw_gid) == -1)
            {
                VERBOSE(VB_IMPORTANT, "Error setting groups.");
                return BACKEND_EXIT_PERMISSIONS_ERROR;
            }
            if (setuid(user_info->pw_uid) == -1)
            {
                VERBOSE(VB_IMPORTANT, "Error setting effective user.");
                return BACKEND_EXIT_PERMISSIONS_ERROR;
            }
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    QString("Invalid user '%1' specified with --user")
                    .arg(username));
            return BACKEND_EXIT_PERMISSIONS_ERROR;
        }
#endif // ! _WIN32
    }

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    VERBOSE(VB_IMPORTANT, versionStr);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return BACKEND_EXIT_NO_MYTHCONTEXT;
    }
    gContext->SetBackend(true);

    if (!eventString.isEmpty())
    {
        gContext->SetBackend(false);
        if (gContext->ConnectToMasterServer())
        {
            if (eventString.startsWith("SYSTEM_EVENT"))
                eventString += QString(" SENDER %1")
                                       .arg(gContext->GetHostName());

            RemoteSendMessage(eventString);
            return BACKEND_EXIT_OK;
        }
        return BACKEND_EXIT_NO_MYTHCONTEXT;
    }

    if (wantupnprebuild)
    {
        VERBOSE(VB_GENERAL, "Rebuilding UPNP Media Map");

        UPnpMedia *rebuildit = new UPnpMedia(false,false);
        rebuildit->BuildMediaMap();

        return BACKEND_EXIT_OK;
    }

    if (setverbose)
    {
        gContext->SetBackend(false);

        if (gContext->ConnectToMasterServer())
        {
            QString message = "SET_VERBOSE "; 
            message += newverbose;

            RemoteSendMessage(message);
            VERBOSE(VB_IMPORTANT, QString("Sent '%1' message").arg(message));
            return BACKEND_EXIT_OK;
        }
        else
        {
            VERBOSE(VB_IMPORTANT,
                    "Unable to connect to backend, verbose level unchanged ");
            return BACKEND_EXIT_NO_CONNECT;
        }
    }

    if (clearsettingscache)
    {
        gContext->SetBackend(false);

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

    QMap<QString,QString> settingsOverride = cmdline.GetSettingsOverride();
    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(*it));
            gContext->OverrideSettingForSession(it.key(), *it);
        }
    }

    if (!gContext->IsMasterHost())
    {
        MythSocket *tempMonitorConnection = new MythSocket();
        if (tempMonitorConnection->connect(
                        gContext->GetSetting("MasterServerIP", "127.0.0.1"),
                        gContext->GetNumSetting("MasterServerPort", 6543)))
        {
            if (!gContext->CheckProtoVersion(tempMonitorConnection))
            {
                VERBOSE(VB_IMPORTANT, "Master backend is incompatible with "
                                      "this backend.\nCannot become a slave.");
                return BACKEND_EXIT_NO_CONNECT;
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
                return BACKEND_EXIT_INVALID_TIMEZONE;
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
    }

    // Don't allow upgrade for --printsched, --testsched, --resched,
    // --printexpire, --generate-preview
    bool allowUpgrade = (!(printsched || testsched || resched)) &&
                        (printexpire.isEmpty()) &&
                        ((previewFrameNumber < -1) && (previewSeconds < -1));
    if (!UpgradeTVDatabaseSchema(allowUpgrade, allowUpgrade))
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema");
        return BACKEND_EXIT_DB_OUTOFDATE;
    }

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

        return (ok) ? BACKEND_EXIT_OK : BACKEND_EXIT_NO_CONNECT;
    }

    if (!printexpire.isEmpty())
    {
        expirer = new AutoExpire();
        expirer->PrintExpireList(printexpire);
        return BACKEND_EXIT_OK;
    }

    if ((previewFrameNumber >= -1) || (previewSeconds >= -1))
    {
        int ret = preview_helper(
            chanid, starttime,
            previewFrameNumber, previewSeconds, previewSize,
            infile, outfile);
        return ret;
    }

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    int port = gContext->GetNumSetting("BackendServerPort", 6543);

    QString myip = gContext->GetSetting("BackendServerIP");
    if (myip.isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return BACKEND_EXIT_NO_IP_ADDRESS;
    }

    bool ismaster = gContext->IsMasterHost();

    if (ismaster)
    {
        VERBOSE(VB_GENERAL, LOC + "Starting up as the master server.");
        gContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as master server", "");

        if (nosched)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "********** The Scheduler has been DISABLED with "
                    "the --nosched option **********");
        }

        // kill -USR1 mythbackendpid will force a upnpmedia rebuild
        signal(SIGUSR1, &upnp_rebuild);
    }
    else
    {
        VERBOSE(VB_GENERAL, LOC + "Running as a slave backend.");
        gContext->LogEntry("mythbackend", LP_INFO,
                           "MythBackend started as a slave backend", "");
    }

    bool fatal_error = false;
    bool runsched = setupTVs(ismaster, fatal_error);
    if (fatal_error)
    {
        return BACKEND_EXIT_CAP_CARD_SETUP_ERROR;
    }

    if (ismaster && runsched)
    {
        sched = new Scheduler(true, &tvList);
        int err = sched->GetError();
        if (err)
        {
            return err;
        }

        if (nosched)
            sched->DisableScheduling();
    }

    // Get any initial housekeeping done before we fire up anything else
    if (nohousekeeper)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "****** The Housekeeper has been DISABLED with "
                "the --nohousekeeper option ******");
    }
    else
        housekeeping = new HouseKeeper(true, ismaster, sched);

    if (ismaster)
    {
        if (noexpirer)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "********* Auto-Expire has been DISABLED with "
                    "the --noautoexpire option ********");
        }
        else
            expirer = new AutoExpire(&tvList);
    }

    if (sched && expirer)
        sched->SetExpirer(expirer);

    if (nojobqueue)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "********* The JobQueue has been DISABLED with "
                "the --nojobqueue option *********");
    }
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

    VERBOSE(VB_IMPORTANT, QString("Enabled verbose msgs: %1").arg(verboseString));

    MainServer *mainServer = new MainServer(ismaster, port, &tvList, sched,
            expirer);

    int exitCode = mainServer->GetExitCode();
    if (exitCode != BACKEND_EXIT_OK)
    {
        VERBOSE(VB_IMPORTANT, "Backend exiting, MainServer initialization "
                "error.");
        return exitCode;
    }

    StorageGroup::CheckAllStorageGroupDirs();

    if (gContext->IsMasterBackend())
        SendMythSystemEvent("MASTER_STARTED");

    exitCode = a.exec();

    if (gContext->IsMasterBackend())
    {
        SendMythSystemEvent("MASTER_SHUTDOWN");
        a.processEvents();
    }

    gContext->LogEntry("mythbackend", LP_INFO, "MythBackend exiting", "");

    if (sysEventHandler)
        delete sysEventHandler;

    return exitCode ? exitCode : BACKEND_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
