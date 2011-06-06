
#include <fcntl.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>

#include <QCoreApplication>
#include <QString>
#include <QDir>

#include "mythsocketmanager.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythsystemevent.h"
#include "mythcommandlineparser.h"

#include "requesthandler/basehandler.h"
#include "requesthandler/fileserverhandler.h"

#define LOC      QString("MythMediaServer: ")
#define LOC_WARN QString("MythMediaServer, Warning: ")
#define LOC_ERR  QString("MythMediaServer, Error: ")

using namespace std;

QString   pidfile;
QString   logfile  = "";

static void cleanup(void)
{
    delete gContext;
    gContext = NULL;

    if (pidfile.size())
    {
        unlink(pidfile.toAscii().constData());
        pidfile.clear();
    }

    signal(SIGHUP, SIG_DFL);
}

static int log_rotate(int report_error)
{
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

static void log_rotate_handler(int)
{
    log_rotate(0);
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

int main(int argc, char *argv[])
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
                return GENERIC_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
    }

    QCoreApplication a(argc, argv);
    QMap<QString, QString> settingsOverride;
    int argpos = 1;
    bool daemonize = false;

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHJOBQUEUE);

    QString filename;

    while (argpos < a.argc())
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return GENERIC_EXIT_INVALID_CMDLINE;

                ++argpos;
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
                 !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc() > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return GENERIC_EXIT_INVALID_CMDLINE;
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
                    return GENERIC_EXIT_INVALID_CMDLINE;
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
            if ((a.argc() - 1) > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                            "-O/--override-setting option\n";
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }

                QStringList pairs = tmpArg.split(",");
                for (int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = pairs[index].split("=");
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            {
                cerr << "Invalid or missing argument to -O/--override-setting "
                        "option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else if (!strcmp(a.argv()[argpos],"-h") ||
                 !strcmp(a.argv()[argpos],"--help"))
        {
            cerr << "Valid Options are:" << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-p or --pidfile filename       Write PID of mythmediaserver to filename " << endl <<
                    "-d or --daemon                 Runs mythmediaserver as a daemon" << endl <<
                    endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return GENERIC_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return GENERIC_EXIT_OK;
        }
        else
        {
            printf("illegal option: '%s' (use --help)\n", a.argv()[argpos]);
            return GENERIC_EXIT_INVALID_CMDLINE;
        }

        ++argpos;
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
            return GENERIC_EXIT_PERMISSIONS_ERROR;
        }
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to ignore SIGPIPE");

    if (daemonize && (daemon(0, 1) < 0))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to daemonize" + ENO);
        return GENERIC_EXIT_DAEMONIZING_ERROR;
    }

    if (pidfs)
    {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 [%3] www.mythtv.org")
                            .arg(MYTH_APPNAME_MYTHJOBQUEUE)
                            .arg(MYTH_SOURCE_PATH)
                            .arg(MYTH_SOURCE_VERSION));

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

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

    if (!gCoreContext->ConnectToMasterServer())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to connect to master server");
        return GENERIC_EXIT_CONNECT_ERROR;
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

    MythSocketManager *sockmanager = new MythSocketManager();
    if (!sockmanager->Listen(port))
    {
        VERBOSE(VB_IMPORTANT, "Mediaserver exiting, failed to bind to listen port.");
        delete sockmanager;
        return GENERIC_EXIT_SOCKET_ERROR;
    }

    sockmanager->RegisterHandler(new BaseRequestHandler());
    sockmanager->RegisterHandler(new FileServerHandler());

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    int exitCode = a.exec();

    if (sysEventHandler)
        delete sysEventHandler;

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
