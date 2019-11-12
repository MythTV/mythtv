#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include <QCoreApplication>
#include <QString>
#include <QDir>

#include "mythconfig.h"
#include "mythsocketmanager.h"
#include "mythcontext.h"
#include "exitcodes.h"
#include "dbcheck.h"
#include "mythdbcon.h"
#include "loggingserver.h"
#include "mythlogging.h"
#include "mythversion.h"
#include "mythsystemevent.h"
#include "commandlineparser.h"
#include "signalhandling.h"
#include "cleanupguard.h"

#include "controlrequesthandler.h"
#include "requesthandler/basehandler.h"
#include "requesthandler/fileserverhandler.h"
#include "requesthandler/messagehandler.h"
#if CONFIG_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
#define ms_sd_notify(x) \
    (void)sd_notify(0, x);
#else
#define ms_sd_notify(x)
#endif

#define LOC      QString("MythMediaServer: ")
#define LOC_WARN QString("MythMediaServer, Warning: ")
#define LOC_ERR  QString("MythMediaServer, Error: ")

using namespace std;

QString   pidfile;
QString   logfile  = "";

static void cleanup(void)
{
    delete gContext;
    gContext = nullptr;

    if (pidfile.size())
    {
        unlink(pidfile.toLatin1().constData());
        pidfile.clear();
    }

    SignalHandler::Done();
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
        MythMediaServerCommandLineParser::PrintVersion();
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

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
#endif

    ms_sd_notify("STATUS=Connecting to database.");
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (!UpgradeTVDatabaseSchema(false, false, true))
    {
        LOG(VB_GENERAL, LOG_ERR, "Exiting due to schema mismatch.");
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    cmdline.ApplySettingsOverride();

    gCoreContext->SetAsBackend(true); // blocks the event connection
    ms_sd_notify("STATUS=Connecting to master server.");
    if (!gCoreContext->ConnectToMasterServer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect to master server");
        return GENERIC_EXIT_CONNECT_ERROR;
    }

    int     port = gCoreContext->GetBackendServerPort();
    if (gCoreContext->GetBackendServerIP().isEmpty())
    {
        cerr << "No setting found for this machine's BackendServerIP.\n"
             << "Please run setup on this machine and modify the first page\n"
             << "of the general settings.\n";
        return GENERIC_EXIT_SETUP_ERROR;
    }

    ms_sd_notify("STATUS=Creating socket manager");
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
    sockmanager->RegisterHandler(new MessageHandler());

    ControlRequestHandler *controlRequestHandler = new ControlRequestHandler();
    sockmanager->RegisterHandler(controlRequestHandler);
    controlRequestHandler->ConnectToMaster();

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    // Provide systemd ready notification (for type=notify units)
    ms_sd_notify("STATUS=");
    ms_sd_notify("READY=1");

    int exitCode = QCoreApplication::exec();

    ms_sd_notify("STOPPING=1\nSTATUS=Exiting");
    delete sysEventHandler;

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
