// -*- Mode: c++ -*-

// C++ headers
#include <unistd.h>
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QApplication>
#include <QString>
#include <QtCore>
#include <QtGui>

// MythTV headers
#include "mythccextractorplayer.h"
#include "commandlineparser.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "programinfo.h"
#include "ringbuffer.h"
#include "exitcodes.h"
#include "loggingserver.h"
#include "signalhandling.h"

namespace {
    void cleanup()
    {
        logStop();
        logServerStop();

        if (gContext)
        {
            delete gContext;
            gContext = NULL;
        }
    }

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
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHLOGSERVER);

    MythLogServerCommandLineParser cmdline;
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

    //QString pidfile = cmdline.toString("pidfile");
    int retval = cmdline.Daemonize();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    bool daemonize = cmdline.toBool("daemon");
    QString mask("general");
    if ((retval = cmdline.ConfigureLogging(mask, daemonize)) != GENERIC_EXIT_OK)
        return retval;

    bool logging = logServerStart();

    if (daemonize)
        // Don't listen to console input if daemonized
        close(0);

    gContext = NULL;

    CleanupGuard callCleanup(cleanup);

    if (!logging)
        return GENERIC_EXIT_OK;

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL << SIGRTMIN;
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        cerr << "Failed to init MythContext, exiting." << endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    qApp->exec();

    SignalHandler::Done();

    return GENERIC_EXIT_OK;
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
