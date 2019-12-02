#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QString>
#include <QRegExp>
#include <QFileInfo>
#include <QDir>

#include "exitcodes.h"
#include "mythcontext.h"
#include "jobqueue.h"
#include "mythdbcon.h"
#include "mythversion.h"
#include "commandlineparser.h"
#include "compat.h"
#include "mythsystemevent.h"
#include "loggingserver.h"
#include "mythlogging.h"
#include "signalhandling.h"
#include "housekeeper.h"
#include "hardwareprofile.h"
#include "cleanupguard.h"

#define LOC      QString("MythJobQueue: ")
#define LOC_WARN QString("MythJobQueue, Warning: ")
#define LOC_ERR  QString("MythJobQueue, Error: ")

using namespace std;

JobQueue *jobqueue = nullptr;
QString   pidfile;
QString   logfile;

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
    MythJobQueueCommandLineParser cmdline;
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
        MythJobQueueCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHJOBQUEUE);

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

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    if (!gCoreContext->ConnectToMasterServer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect to master server");
        return GENERIC_EXIT_CONNECT_ERROR;
    }

    jobqueue = new JobQueue(false);

    auto *sysEventHandler = new MythSystemEventHandler();

    auto *housekeeping = new HouseKeeper();
#ifdef __linux__
 #ifdef CONFIG_BINDINGS_PYTHON
    housekeeping->RegisterTask(new HardwareProfileTask());
 #endif
#endif
    housekeeping->Start();

    int exitCode = QCoreApplication::exec();

    delete sysEventHandler;

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
