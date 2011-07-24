
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
#include "mythlogging.h"
#include "mythversion.h"
#include "mythsystemevent.h"
#include "commandlineparser.h"

#include "controlrequesthandler.h"
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

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHMEDIASERVER);

    int retval = cmdline.Daemonize();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    bool daemonize = cmdline.toBool("daemon");
    QString mask("general");
    if ((retval = cmdline.ConfigureLogging(mask, daemonize)) != GENERIC_EXIT_OK)
        return retval;

    CleanupGuard callCleanup(cleanup);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    gCoreContext->SetBackend(false);
    if (!gCoreContext->ConnectToMasterServer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect to master server");
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
        LOG(VB_GENERAL, LOG_ERR,
            "Mediaserver exiting, failed to bind to listen port.");
        delete sockmanager;
        return GENERIC_EXIT_SOCKET_ERROR;
    }

    sockmanager->RegisterHandler(new BaseRequestHandler());
    sockmanager->RegisterHandler(new FileServerHandler());

    ControlRequestHandler *controlRequestHandler = new ControlRequestHandler();
    sockmanager->RegisterHandler(controlRequestHandler);
    controlRequestHandler->ConnectToMaster();

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    int exitCode = a.exec();

    if (sysEventHandler)
        delete sysEventHandler;

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
