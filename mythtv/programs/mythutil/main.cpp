// Qt includes
#ifndef _WIN32
#include <QCoreApplication>
#else
#include <QApplication>
#endif

// libmyth* includes
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythlogging.h"

// Local includes
#include "mythutil.h"
#include "commandlineparser.h"
#include "backendutils.h"
#include "fileutils.h"
#include "mpegutils.h"
#include "jobutils.h"
#include "markuputils.h"
#include "messageutils.h"
#include "signalhandling.h"


int main(int argc, char *argv[])
{
    MythUtilCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    // default to quiet operation for pidcounter and pidfilter
    QString defaultVerbose = "general";
    LogLevel_t defaultLevel = LOG_INFO;
    if (cmdline.toBool("pidcounter") || cmdline.toBool("pidfilter"))
    {
        if (!cmdline.toBool("verbose"))
        {
            verboseString = defaultVerbose = "";
            verboseMask = 0;
        }
        if (!cmdline.toBool("loglevel"))
            logLevel = defaultLevel = LOG_ERR;
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

#ifndef _WIN32
    QCoreApplication a(argc, argv);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    QApplication a(argc, argv);
#endif

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHUTIL);

    int retval;
    if ((retval = cmdline.ConfigureLogging(defaultVerbose)) != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toBool("loglevel"))
        logLevel = defaultLevel;

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL << SIGRTMIN;
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    UtilMap utilMap;

    registerBackendUtils(utilMap);
    registerFileUtils(utilMap);
    registerMPEGUtils(utilMap);
    registerJobUtils(utilMap);
    registerMarkupUtils(utilMap);
    registerMessageUtils(utilMap);

    bool cmdFound = false;
    int cmdResult = GENERIC_EXIT_OK;
    UtilMap::iterator i;
    for (i = utilMap.begin(); i != utilMap.end(); ++i)
    {
        if (cmdline.toBool(i.key()))
        {
            cmdResult = (i.value())(cmdline);
            cmdFound = true;
            break;
        }
    }

    if(!cmdFound)
    {
        cmdline.PrintHelp();
        cmdResult = GENERIC_EXIT_INVALID_CMDLINE;
    }

    delete gContext;

    SignalHandler::Done();

    return cmdResult;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
