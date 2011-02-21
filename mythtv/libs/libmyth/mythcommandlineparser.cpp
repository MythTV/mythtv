#include <iostream>
using namespace std;

#include <QFile>
#include <QFileInfo>
#include <QSize>

#include "mythcommandlineparser.h"
#include "exitcodes.h"
#include "mythconfig.h"
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"

MythCommandLineParser::MythCommandLineParser(uint64_t things_to_parse) :
    parseTypes(things_to_parse),
    display(), geometry(),
    logfile(),
    pidfile(),
    infile(),
    outfile(),
    newverbose(),
    username(),
    printexpire(),
    eventString(),
    previewSize(0,0),
    starttime(),
    chanid(0),
    previewFrameNumber(-2),
    previewSeconds(-2),
    daemonize(false),
    printsched(false),
    testsched(false),
    setverbose(false),
    resched(false),
    nosched(false),
    scanvideos(false),
    noupnp(false),
    nojobqueue(false),
    nohousekeeper(false),
    noexpirer(false),
    clearsettingscache(false),
    wantupnprebuild(false),

    wantsToExit(false)
{
}

bool MythCommandLineParser::PreParse(
    int argc, const char * const * argv, int &argpos, bool &err)
{
    err = false;

    binname = QFileInfo(argv[0]).baseName();

    if (argpos >= argc)
        return false;

    if ((parseTypes & kCLPDisplay) &&
             (!strcmp(argv[argpos],"-display") ||
              !strcmp(argv[argpos],"--display")))
    {
        if ((argc - 1) > argpos)
        {
            display = argv[argpos+1];
            if (display.startsWith("-"))
            {
                cerr << "Invalid or missing argument to -display option\n";
                err = true;
                return true;
            }
            else
                ++argpos;
        }
        else
        {
            cerr << "Missing argument to -display option\n";
            err = true;
            return true;
        }

        return true;
    }
    else if ((parseTypes & kCLPGeometry) &&
             (!strcmp(argv[argpos],"-geometry") ||
              !strcmp(argv[argpos],"--geometry")))
    {
        if ((argc - 1) > argpos)
        {
            geometry = argv[argpos+1];
            if (geometry.startsWith("-"))
            {
                cerr << "Invalid or missing argument to -geometry option\n";
                err = true;
                return true;
            }
            else
                ++argpos;
        }
        else
        {
            cerr << "Missing argument to -geometry option\n";
            err = true;
            return true;
        }

        return true;
    }
#ifdef Q_WS_MACX
    else if (!strncmp(argv[argpos],"-psn_",5))
    {
        cerr << "Ignoring Process Serial Number from command line\n";
        return true;
    }
#endif
    else if ((parseTypes & kCLPVerbose) &&
             (!strcmp(argv[argpos],"-v") ||
              !strcmp(argv[argpos],"--verbose")))
    {
        if ((argc - 1) > argpos)
        {
            if (parse_verbose_arg(argv[argpos+1]) ==
                GENERIC_EXIT_INVALID_CMDLINE)
            {
                wantsToExit = err = true;
            }
            ++argpos;
        }
        else
        {
            cerr << "Missing argument to -v/--verbose option";
            wantsToExit = err = true;
        }
        return true;
    }
    else if ((parseTypes & kCLPSetVerbose) &&
             !strcmp(argv[argpos],"--setverbose"))
    {
        setverbose = true;
        if ((argc - 1) > argpos)
        {
            newverbose = argv[argpos+1];
            ++argpos;
        }
        else
        {
            cerr << "Missing argument to --setverbose option\n";
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        return true;
    }
    else if ((parseTypes & kCLPHelp) &&
             (!strcmp(argv[argpos],"-h") ||
              !strcmp(argv[argpos],"--help") ||
              !strcmp(argv[argpos],"--usage")))
    {
        QString help = GetHelpString(true);
        QByteArray ahelp = help.toLocal8Bit();
        cout << ahelp.constData();
        wantsToExit = true;
        return true;
    }
    else if ((parseTypes & kCLPQueryVersion) &&
             !strcmp(argv[argpos],"--version"))
    {
        cout << "Please attach all output as a file in bug reports." << endl;
        cout << "MythTV Version   : " << MYTH_SOURCE_VERSION << endl;
        cout << "MythTV Branch    : " << MYTH_SOURCE_PATH << endl;
        cout << "Network Protocol : " << MYTH_PROTO_VERSION << endl;
        cout << "Library API      : " << MYTH_BINARY_VERSION << endl;
        cout << "QT Version       : " << QT_VERSION_STR << endl;
#ifdef MYTH_BUILD_CONFIG
        cout << "Options compiled in:" <<endl;
        cout << MYTH_BUILD_CONFIG << endl;
#endif
        wantsToExit = true;
        return true;
    }
    else if ((parseTypes & kCLPExtra) &&
             argv[argpos][0] != '-')
        // Though it's allowed (err = false), we didn't handle the arg
        return false;

    return false;
}

bool MythCommandLineParser::Parse(
    int argc, const char * const * argv, int &argpos, bool &err)
{
    err = false;

    if (argpos >= argc)
        return false;

    if ((parseTypes & kCLPWindowed) &&
        (!strcmp(argv[argpos],"-w") ||
         !strcmp(argv[argpos],"--windowed")))
    {
        settingsOverride["RunFrontendInWindow"] = "1";
        return true;
    }
    else if ((parseTypes & kCLPNoWindowed) &&
             (!strcmp(argv[argpos],"-nw") ||
              !strcmp(argv[argpos],"--no-windowed")))
    {
        settingsOverride["RunFrontendInWindow"] = "0";
        return true;
    }
    else if ((parseTypes & kCLPDaemon) &&
             (!strcmp(argv[argpos],"-d") ||
              !strcmp(argv[argpos],"--daemon")))
    {
        daemonize = true;
        return true;
    }
    else if ((parseTypes && kCLPPrintSchedule) &&
             !strcmp(argv[argpos],"--printsched"))
    {
        printsched = true;
        return true;
    }
    else if ((parseTypes && kCLPTestSchedule) &&
             !strcmp(argv[argpos],"--testsched"))
    {
        testsched = true;
        return true;
    }
    else if ((parseTypes && kCLPReschedule) &&
             !strcmp(argv[argpos],"--resched"))
    {
        resched = true;
        return true;
    }
    else if ((parseTypes && kCLPNoSchedule) &&
             !strcmp(argv[argpos],"--nosched"))
    {
        nosched = true;
        return true;
    }
    else if ((parseTypes && kCLPReschedule) &&
             !strcmp(argv[argpos],"--scanvideos"))
    {
        scanvideos = true;
        return true;
    }
    else if ((parseTypes && kCLPNoUPnP) &&
             !strcmp(argv[argpos],"--noupnp"))
    {
        noupnp = true;
        return true;
    }
    else if ((parseTypes && kCLPUPnPRebuild) &&
             !strcmp(argv[argpos],"--upnprebuild"))
    {
        wantupnprebuild = true;
        return true;
    }
    else if ((parseTypes && kCLPNoJobqueue) &&
             !strcmp(argv[argpos],"--nojobqueue"))
    {
        nojobqueue = true;
        return true;
    }
    else if ((parseTypes && kCLPNoHousekeeper) &&
             !strcmp(argv[argpos],"--nohousekeeper"))
    {
        nohousekeeper = true;
        return true;
    }
    else if ((parseTypes && kCLPNoAutoExpire) &&
             !strcmp(argv[argpos],"--noautoexpire"))
    {
        noexpirer = true;
        return true;
    }
    else if ((parseTypes && kCLPClearCache) &&
             !strcmp(argv[argpos],"--clearcache"))
    {
        clearsettingscache = true;
        return true;
    }
    else if ((parseTypes & kCLPOverrideSettingsFile) &&
             (!strcmp(argv[argpos],"--override-settings-file")))
    {
        if (argc <= argpos)
        {
            cerr << "Missing argument to --override-settings-file option\n";
            err = true;
            return true;
        }

        QString filename = QString::fromLocal8Bit(argv[++argpos]);
        QFile f(filename);
        if (!f.open(QIODevice::ReadOnly))
        {
            QByteArray tmp = filename.toAscii();
            cerr << "Failed to open the override settings file: '"
                 << tmp.constData() << "'" << endl;
            err = true;
            return true;
        }

        char buf[1024];
        int64_t len = f.readLine(buf, sizeof(buf) - 1);
        while (len != -1)
        {
            if (len >= 1 && buf[len-1]=='\n')
                buf[len-1] = 0;
            QString line(buf);
            QStringList tokens = line.split("=", QString::SkipEmptyParts);
            if (tokens.size() == 1)
                tokens.push_back("");
            if (tokens.size() >= 2)
            {
                tokens[0].replace(QRegExp("^[\"']"), "");
                tokens[0].replace(QRegExp("[\"']$"), "");
                tokens[1].replace(QRegExp("^[\"']"), "");
                tokens[1].replace(QRegExp("[\"']$"), "");
                if (!tokens[0].isEmpty())
                    settingsOverride[tokens[0]] = tokens[1];
            }
            len = f.readLine(buf, sizeof(buf) - 1);
        }
        return true;
    }
    else if ((parseTypes & kCLPOverrideSettings) &&
             (!strcmp(argv[argpos],"-O") ||
              !strcmp(argv[argpos],"--override-setting")))
    {
        if ((argc - 1) > argpos)
        {
            QString tmpArg = argv[argpos+1];
            if (tmpArg.startsWith("-"))
            {
                cerr << "Invalid or missing argument to "
                     << "-O/--override-setting option\n";
                err = true;
                return true;
            }

            QStringList pairs = tmpArg.split(",", QString::SkipEmptyParts);
            for (int index = 0; index < pairs.size(); ++index)
            {
                QStringList tokens = pairs[index].split(
                    "=", QString::SkipEmptyParts);
                if (tokens.size() == 1)
                    tokens.push_back("");
                if (tokens.size() >= 2)
                {
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    if (!tokens[0].isEmpty())
                        settingsOverride[tokens[0]] = tokens[1];
                }
            }
        }
        else
        {
            cerr << "Invalid or missing argument to "
                 << "-O/--override-setting option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPGetSettings) && gContext &&
             (!strcmp(argv[argpos],"-G") ||
              !strcmp(argv[argpos],"--get-setting") ||
              !strcmp(argv[argpos],"--get-settings")))
    {
        if ((argc - 1) > argpos)
        {
            QString tmpArg = argv[argpos+1];
            if (tmpArg.startsWith("-"))
            {
                cerr << "Invalid argument to "
                     << "-G/--get-setting option\n";
                err = true;
                return true;
            }

            settingsQuery = tmpArg.split(",", QString::SkipEmptyParts);
        }
        else
        {
            cerr << "Missing argument to "
                 << "-G/--get-setting option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPLogFile) &&
             (!strcmp(argv[argpos],"-l") ||
              !strcmp(argv[argpos],"--logfile")))
    {
        if ((argc - 1) > argpos)
        {
            logfile = argv[argpos+1];
            if (logfile.startsWith("-"))
            {
                cerr << "Invalid argument to -l/--logfile option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to -l/--logfile option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPPidFile) &&
             (!strcmp(argv[argpos],"-p") ||
              !strcmp(argv[argpos],"--pidfile")))
    {
        if ((argc - 1) > argpos)
        {
            pidfile = argv[argpos+1];
            if (pidfile.startsWith("-"))
            {
                cerr << "Invalid argument to -p/--pidfile option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to -p/--pidfile option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPInFile) &&
             !strcmp(argv[argpos],"--infile"))
    {
        if ((argc - 1) > argpos)
        {
            infile = argv[argpos+1];
            if (infile.startsWith("-"))
            {
                cerr << "Invalid argument to --infile option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to --infile option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPOutFile) &&
             !strcmp(argv[argpos],"--outfile"))
    {
        if ((argc - 1) > argpos)
        {
            outfile = argv[argpos+1];
            if (outfile.startsWith("-"))
            {
                cerr << "Invalid argument to --outfile option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to --outfile option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPUsername) &&
             !strcmp(argv[argpos],"--user"))
    {
        if ((argc - 1) > argpos)
        {
            username = argv[argpos+1];
            if (username.startsWith("-"))
            {
                cerr << "Invalid argument to --user option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to --user option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPEvent) &&
             (!strcmp(argv[argpos],"--event")))
    {
        if ((argc - 1) > argpos)
        {
            eventString = argv[argpos+1];
            if (eventString.startsWith("-"))
            {
                cerr << "Invalid argument to --event option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to --event option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPSystemEvent) &&
             (!strcmp(argv[argpos],"--systemevent")))
    {
        if ((argc - 1) > argpos)
        {
            eventString = argv[argpos+1];
            if (eventString.startsWith("-"))
            {
                cerr << "Invalid argument to --systemevent option\n";
                err = true;
                return true;
            }
            eventString = QString("SYSTEM_EVENT ") + eventString;
        }
        else
        {
            cerr << "Missing argument to --systemevent option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPChannelId) &&
             (!strcmp(argv[argpos],"-c") ||
              !strcmp(argv[argpos],"--chanid")))
    {
        if ((argc - 1) > argpos)
        {
            chanid = QString(argv[argpos+1]).toUInt();
            if (!chanid)
            {
                cerr << "Invalid argument to -c/--chanid option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to -c/--chanid option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPStartTime) &&
             (!strcmp(argv[argpos],"-s") ||
              !strcmp(argv[argpos],"--starttime")))
    {
        if ((argc - 1) > argpos)
        {
            QString tmp = QString(argv[argpos+1]).trimmed();
            starttime = QDateTime::fromString(tmp, "yyyyMMddhhmmss");
            if (!starttime.isValid() && tmp.length() == 19)
            {
                starttime = QDateTime::fromString(tmp, Qt::ISODate);
            }
            if (!starttime.isValid())
            {
                cerr << "Invalid argument to -s/--starttime option\n";
                err = true;
                return true;
            }
        }
        else
        {
            cerr << "Missing argument to -s/--starttime option\n";
            err = true;
            return true;
        }

        ++argpos;
        return true;
    }
    else if ((parseTypes & kCLPPrintExpire) &&
             (!strcmp(argv[argpos],"--printexpire")))
    {
        printexpire = "ALL";
        if (((argc - 1) > argpos) &&
            QString(argv[argpos+1]).startsWith("-"))
        {
            printexpire = argv[argpos+1];
            ++argpos;
        }
        return true;
    }
    else if (parseTypes & kCLPGeneratePreview &&
             (!strcmp(argv[argpos],"--seconds")))
    {
        QString tmp;
        if ((argc - 1) > argpos)
        {
            tmp = argv[argpos+1];
            bool ok = true;
            long long seconds = tmp.toInt(&ok);
            if (ok)
            {
                previewSeconds = seconds;
                ++argpos;
            }
            else
                tmp.clear();
        }

        return true;
    }
    else if (parseTypes & kCLPGeneratePreview &&
             (!strcmp(argv[argpos],"--frame")))
    {
        QString tmp;
        if ((argc - 1) > argpos)
        {
            tmp = argv[argpos+1];
            VERBOSE(VB_IMPORTANT, QString("--frame: %1").arg(tmp));
            bool ok = true;
            long long frame = tmp.toInt(&ok);
            if (ok)
            {
                previewFrameNumber = frame;
                ++argpos;
            }
            else
                tmp.clear();
        }

        return true;
    }
    else if (parseTypes & kCLPGeneratePreview &&
             (!strcmp(argv[argpos],"--size")))
    {
        QString tmp;
        if ((argc - 1) > argpos)
        {
            tmp = argv[argpos+1];
            int xat = tmp.indexOf("x", 0, Qt::CaseInsensitive);
            if (xat > 0)
            {
                QString widthStr  = tmp.left(xat);
                QString heightStr;
                heightStr = tmp.mid(xat + 1);

                bool ok1, ok2;
                previewSize = QSize(widthStr.toInt(&ok1), heightStr.toInt(&ok2));
                if (!ok1 || !ok2)
                {
                    VERBOSE(VB_IMPORTANT, QString(
                                "Error: Failed to parse preview generator "
                                "param '%1'").arg(tmp));
                }
            }
            ++argpos;
        }

        return true;
    }
    else
    {
        return PreParse(argc, argv, argpos, err);
    }
}

QString MythCommandLineParser::GetHelpString(bool with_header) const
{
    QString str;
    QTextStream msg(&str, QIODevice::WriteOnly);

    if (with_header)
    {
        QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
            .arg(binname).arg(MYTH_SOURCE_PATH).arg(MYTH_SOURCE_VERSION);
        msg << versionStr << endl;
        msg << "Valid options are: " << endl;
    }

    if (parseTypes & kCLPHelp)
    {
        msg << "-h or --help                   "
            << "List valid command line parameters" << endl;
    }

    if (parseTypes & kCLPLogFile)
    {
        msg << "-l or --logfile filename       "
            << "Writes STDERR and STDOUT messages to filename" << endl;
    }

    if (parseTypes & kCLPPidFile)
    {
        msg << "-p or --pidfile filename       "
            << "Write PID of mythbackend to filename" << endl;
    }

    if (parseTypes & kCLPDaemon)
    {
        msg << "-d or --daemon                 "
            << "Runs program as a daemon" << endl;
    }

    if (parseTypes & kCLPDisplay)
    {
        msg << "-display X-server              "
            << "Create GUI on X-server, not localhost" << endl;
    }

    if (parseTypes & kCLPGeometry)
    {
        msg << "-geometry or --geometry WxH    "
            << "Override window size settings" << endl;
        msg << "-geometry WxH+X+Y              "
            << "Override window size and position" << endl;
    }

    if (parseTypes & kCLPWindowed)
    {
        msg << "-w or --windowed               Run in windowed mode" << endl;
    }

    if (parseTypes & kCLPNoWindowed)
    {
        msg << "-nw or --no-windowed           Run in non-windowed mode "
            << endl;
    }

    if (parseTypes & kCLPOverrideSettings)
    {
        msg << "-O or --override-setting KEY=VALUE " << endl
            << "                               "
            << "Force the setting named 'KEY' to value 'VALUE'" << endl
            << "                               "
            << "This option may be repeated multiple times" << endl;
    }

    if (parseTypes & kCLPOverrideSettingsFile)
    {
        msg << "--override-settings-file <file> " << endl
            << "                               "
            << "File containing KEY=VALUE pairs for settings" << endl
            << "                               Use a comma separated list to return multiple values"
            << endl;
    }

    if (parseTypes & kCLPGetSettings)
    {
        msg << "-G or --get-setting KEY[,KEY2,etc] " << endl
            << "                               "
            << "Returns the current database setting for 'KEY'" << endl;
    }

    if (parseTypes & kCLPQueryVersion)
    {
        msg << "--version                      Version information" << endl;
    }

    if (parseTypes & kCLPVerbose)
    {
        msg << "-v or --verbose debug-level    Use '-v help' for level info" << endl;
    }

    if (parseTypes & kCLPSetVerbose)
    {
        msg << "--setverbose debug-level       "
            << "Change debug level of running master backend" << endl;
    }

    if (parseTypes & kCLPUsername)
    {
        msg << "--user username                "
            << "Drop permissions to username after starting"  << endl;
    }

    if (parseTypes & kCLPPrintExpire)
    {
        msg << "--printexpire                  "
            << "List of auto-expire programs" << endl;
    }

    if (parseTypes & kCLPPrintSchedule)
    {
        msg << "--printsched                   "
            << "Upcoming scheduled programs" << endl;
    }

    if (parseTypes & kCLPTestSchedule)
    {
        msg << "--testsched                    "
            << "Test run scheduler (ignore existing schedule)" << endl;
    }

    if (parseTypes & kCLPReschedule)
    {
        msg << "--resched                      "
            << "Force the scheduler to update" << endl;
    }

    if (parseTypes & kCLPNoSchedule)
    {
        msg << "--nosched                      "
            << "Do not perform any scheduling" << endl;
    }

    if (parseTypes & kCLPScanVideos)
    {
        msg << "--scanvideos                   "
            << "Scan for new video content" << endl;
    }

    if (parseTypes & kCLPNoUPnP)
    {
        msg << "--noupnp                       "
            << "Do not enable the UPNP server" << endl;
    }

    if (parseTypes & kCLPNoJobqueue)
    {
        msg << "--nojobqueue                   "
            << "Do not start the JobQueue" << endl;
    }

    if (parseTypes & kCLPNoHousekeeper)
    {
        msg << "--nohousekeeper                "
            << "Do not start the Housekeeper" << endl;
    }

    if (parseTypes & kCLPNoAutoExpire)
    {
        msg << "--noautoexpire                 "
            << "Do not start the AutoExpire thread" << endl;
    }

    if (parseTypes & kCLPClearCache)
    {
        msg << "--clearcache                   "
            << "Clear the settings cache on all MythTV servers" << endl;
    }

    if (parseTypes & kCLPGeneratePreview)
    {
        msg << "--seconds                      "
            << "Number of seconds into video that preview should be taken" << endl;
        msg << "--frame                        "
            << "Number of frames into video that preview should be taken" << endl;
        msg << "--size                         "
            << "Dimensions of preview image" << endl;
    }

    if (parseTypes & kCLPUPnPRebuild)
    {
        msg << "--upnprebuild                  "
            << "Force an update of UPNP media" << endl;
    }

    if (parseTypes & kCLPInFile)
    {
        msg << "--infile                       "
            << "Input file for preview generation" << endl;
    }

    if (parseTypes & kCLPOutFile)
    {
        msg << "--outfile                      "
            << "Optional output file for preview generation" << endl;
    }

    if (parseTypes & kCLPChannelId)
    {
        msg << "--chanid                       "
            << "Channel ID for preview generation" << endl;
    }

    if (parseTypes & kCLPStartTime)
    {
        msg << "--starttime                    "
            << "Recording start time for preview generation" << endl;
    }

    if (parseTypes & kCLPEvent)
    {
        msg << "--event EVENTTEXT              "
            << "Send a backend event test message" << endl;
    }

    if (parseTypes & kCLPSystemEvent)
    {
        msg << "--systemevent EVENTTEXT        "
            << "Send a backend SYSTEM_EVENT test message" << endl;
    }

    msg.flush();

    return str;
}

