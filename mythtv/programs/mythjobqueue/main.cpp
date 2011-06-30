
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cmath>

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
#include "mythlogging.h"

#define LOC      QString("MythJobQueue: ")
#define LOC_WARN QString("MythJobQueue, Warning: ")
#define LOC_ERR  QString("MythJobQueue, Error: ")

using namespace std;

JobQueue *jobqueue = NULL;
QString   pidfile;
QString   logfile;

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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHJOBQUEUE);

    bool daemonize = cmdline.toBool("daemon");
    int retval;
    QString mask("important general");
    if ((retval = cmdline.ConfigureLogging(mask, daemonize)) != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("pidfile"))
        pidfile = cmdline.toString("pidfile");

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

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    if (!gCoreContext->ConnectToMasterServer())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to connect to master server");
        return GENERIC_EXIT_CONNECT_ERROR;
    }

    jobqueue = new JobQueue(false);

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();

    int exitCode = a.exec();

    if (sysEventHandler)
        delete sysEventHandler;

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
