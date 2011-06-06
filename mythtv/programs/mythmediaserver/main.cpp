
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
    QCoreApplication a(argc, argv);
    QMap<QString, QString> settingsOverride;
    bool daemonize = false;
    int quiet = 0;

    QString filename;

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHMEDIASERVER);

    print_verbose_messages = VB_IMPORTANT;
    verboseString = "important";

    MythMediaServerCommandLineParser cmdline;
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

    if (cmdline.toBool("pidfile"))
        pidfile = cmdline.toUInt("pidfile");
    daemonize = cmdline.toBool("daemonize");

    if (parse_verbose_arg(cmdline.toString("verbose")) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
        return GENERIC_EXIT_INVALID_CMDLINE;
    if (cmdline.toBool("verboseint"))
        print_verbose_messages = cmdline.toUInt("verboseint");

    if (cmdline.toBool("quiet"))
    {
        quiet = cmdline.toUInt("quiet");
        if (quiet > 1)
        {
            print_verbose_messages = VB_NONE;
            parse_verbose_arg("none");
        }
    }

    int facility = cmdline.GetSyslogFacility();
    bool dblog = !cmdline.toBool("nodblog");

    CleanupGuard callCleanup(cleanup);

    QString logfile = cmdline.GetLogFilePath();
    logStart(logfile, quiet, facility, dblog);

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
                            .arg(MYTH_APPNAME_MYTHMEDIASERVER)
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
