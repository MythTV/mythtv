/*
    main.cpp

    Starting point for the myth lcd server daemon

*/

#include <iostream>
using namespace std;
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <QCoreApplication>
#include <QFile>

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "tv_play.h"
#include "compat.h"
#include "mythtranslation.h"

#include "lcdserver.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);
    bool daemon_mode = false;
    int  special_port = -1;
    QString startup_message = "";          // default to no startup message
    int message_time = 30;                 // time to display startup message
    print_verbose_messages = VB_IMPORTANT; // only show important messages
    QString logfile = "";

    debug_level = 0;  // don't show any debug messages by default

    //  Check command line arguments
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-d") ||
            !strcmp(a.argv()[argpos],"--daemon"))
        {
            daemon_mode = true;
        }
        else if (!strcmp(a.argv()[argpos],"-n") ||
            !strcmp(a.argv()[argpos],"--nodaemon"))
        {
            daemon_mode = false;
        }
        else if (!strcmp(a.argv()[argpos],"-p") ||
            !strcmp(a.argv()[argpos],"--port"))
        {
            if (a.argc() > argpos)
            {
                QString port_number = a.argv()[argpos+1];
                ++argpos;
                special_port = port_number.toInt();
                if (special_port < 1 || special_port > 65534)
                {
                    VERBOSE(VB_IMPORTANT, "lcdserver: Bad port number");
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "lcdserver: Missing argument to "
                                "-p/--port option");
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-m") ||
            !strcmp(a.argv()[argpos],"--startupmessage"))
        {
            if (a.argc() > argpos)
            {
                startup_message = a.argv()[argpos+1];
                ++argpos;
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "lcdserver: Missing argument to "
                                "-m/--startupmessage");
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-t") ||
            !strcmp(a.argv()[argpos],"--messagetime"))
        {
            if (a.argc() > argpos)
            {
                QString sTime = a.argv()[argpos+1];
                ++argpos;
                message_time = sTime.toInt();
                if (message_time < 1 || message_time > 1000)
                {
                    VERBOSE(VB_IMPORTANT, "lcdserver: Bad show message time");
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "lcdserver: Missing argument to "
                                "-t/--messagetime");
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return GENERIC_EXIT_INVALID_CMDLINE;

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc()-1 > argpos)
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
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-x") ||
            !strcmp(a.argv()[argpos],"--debuglevel"))
        {
            if (a.argc() > argpos)
            {
                QString sTemp = a.argv()[argpos+1];

                ++argpos;
                debug_level = sTemp.toInt();
                if (debug_level < 0 || debug_level > 10)
                {
                    VERBOSE(VB_IMPORTANT, "lcdserver: Bad debug level");
                    return GENERIC_EXIT_INVALID_CMDLINE;
                }
            }
            else
            {
                VERBOSE(VB_IMPORTANT, "lcdserver: Missing argument to "
                                "-x/--debuglevel");
                return GENERIC_EXIT_INVALID_CMDLINE;
            }
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl
                 << "Valid options are: " << endl <<
"-p or --port number           A port number to listen on (default is 6545) "
                 << endl <<
"-d or --daemon                Runs lcd server as a daemon "
                 << endl <<
"-n or --nodaemon              Does not run lcd server as a daemon (default)"
                 << endl <<
"-m or --startupmessage        Message to show at startup"
                 << endl <<
"-t or --messagetime           How long to show startup message (default 30 seconds)"
                 << endl <<
"-l or --logfile filename      Writes STDERR and STDOUT messages to filename"
                 << endl <<
"-v or --verbose debug-level   Use '-v help' for level info"
                 << endl <<
"-x or --debuglevel level      Control how much debug messages to show"
                 << endl <<
"                              [number between 0 and 10] (default 0)"
                 << endl;
            return GENERIC_EXIT_INVALID_CMDLINE;
        }
    }

    // set up log file
    int logfd = -1;

    if (!logfile.isEmpty())
    {
        QByteArray tmp = logfile.toAscii();
        logfd = open(tmp.constData(), O_WRONLY|O_CREAT|O_APPEND, 0664);

        if (logfd < 0)
        {
            perror("open(logfile)");
            return GENERIC_EXIT_PERMISSIONS_ERROR;
        }
    }

    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);

        // Close the unduplicated logfd
        if (logfd != 1 && logfd != 2)
            close(logfd);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    //  Switch to daemon mode?
    if (daemon_mode)
    {
        if (daemon(0, 1) < 0)
        {
            VERBOSE(VB_IMPORTANT, "lcdserver: Failed to run as a daemon. "
                            "Bailing out.");
            return GENERIC_EXIT_DAEMONIZING_ERROR;
        }
        cout << endl;
    }

    //  Get the MythTV context and db hooks
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "lcdserver: Could not initialize MythContext. "
                        "Exiting.");
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

    new LCDServer(assigned_port, startup_message, message_time);

    a.exec();

    delete gContext;
    return GENERIC_EXIT_OK;
}

