/*
    main.cpp

    Starting point for the myth lcd server daemon

*/

// c/c++
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

// Qt
#include <QtGlobal>
#include <QCoreApplication>
#include <QFile>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/signalhandling.h"
#include "libmythtv/tv_play.h"

// mythlcdserver
#include "lcdserver.h"
#include "mythlcdserver_commandlineparser.h"

int main(int argc, char **argv)
{
    int  special_port = -1;
    QString startup_message = "";          // default to no startup message
    std::chrono::seconds message_time = 30s; // time to display startup message

    // TODO: check if this can use LOG_*
    debug_level = 0;  // don't show any debug messages by default

    MythLCDServerCommandLineParser cmdline;
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
        MythLCDServerCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHLCDSERVER);

    int retval = cmdline.Daemonize();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    bool daemonize = cmdline.toBool("daemon");
    QString mask("general");
    retval = cmdline.ConfigureLogging(mask, daemonize);
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("port"))
    {
        special_port = cmdline.toInt("port");
        if (special_port < 1 || special_port > 65534)
        {
            LOG(VB_GENERAL, LOG_ERR, "lcdserver: Bad port number");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }
    if (cmdline.toBool("message"))
        startup_message = cmdline.toString("message");
    if (cmdline.toBool("messagetime"))
    {
        message_time = std::chrono::seconds(cmdline.toInt("messagetime"));
        if (message_time < 1s || message_time > 1000s)
        {
            LOG(VB_GENERAL, LOG_ERR, "lcdserver: Bad message duration");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }
    if (cmdline.toBool("debug"))
    {
        debug_level = cmdline.toInt("debug");
        if (debug_level < 0 || debug_level > 10)
        {
            LOG(VB_GENERAL, LOG_ERR, "lcdserver: Bad debug level");
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

#ifndef _WIN32
    SignalHandler::Init();
#endif

    //  Get the MythTV context and db hooks
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "lcdserver: Could not initialize MythContext. Exiting.");
        SignalHandler::Done();
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    MythTranslation::load("mythfrontend");

    gCoreContext->ConnectToMasterServer(false);

    //  Figure out port to listen on
    int assigned_port = gCoreContext->GetNumSetting("LCDServerPort", 6545);
    if (special_port > 0)
    {
        assigned_port = special_port;
    }

    auto *server = new LCDServer(assigned_port, startup_message, message_time);

    QCoreApplication::exec();

    delete server;
    delete gContext;

    SignalHandler::Done();

    return GENERIC_EXIT_OK;
}
