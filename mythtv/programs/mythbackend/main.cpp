// POSIX headers
#include <sys/time.h>     // for setpriority
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>

#include "mythconfig.h"
#if CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

// C headers
#include <cstdlib>
#include <cerrno>

// C++ headers
#include <iostream>
#include <fstream>
using namespace std;

#ifndef _WIN32
#include <QCoreApplication>
#else
#include <QApplication>
#endif

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <QRegExp>

#include "tv_rec.h"
#include "scheduledrecording.h"
#include "autoexpire.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"
#include "remoteutil.h"
#include "housekeeper.h"

#include "mythcorecontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdb.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "dbcheck.h"
#include "jobqueue.h"
#include "previewgenerator.h"
#include "mythcommandlineparser.h"
#include "mythsystemevent.h"

#include "mediaserver.h"
#include "httpstatus.h"
#include "backendcontext.h"
#include "main_helpers.h"

#define LOC      QString("MythBackend: ")
#define LOC_WARN QString("MythBackend, Warning: ")
#define LOC_ERR  QString("MythBackend, Error: ")

#ifdef Q_OS_MACX
    // 10.6 uses some file handles for its new Grand Central Dispatch thingy
    #define UNUSED_FILENO 5
#else
    #define UNUSED_FILENO 3
#endif

int main(int argc, char **argv)
{
    bool cmdline_err;
    MythCommandLineParser cmdline(
        kCLPDaemon               |
        kCLPHelp                 |
        kCLPOverrideSettingsFile |
        kCLPOverrideSettings     |
        kCLPQueryVersion         |
        kCLPPrintSchedule        |
        kCLPTestSchedule         |
        kCLPReschedule           |
        kCLPNoSchedule           |
        kCLPNoUPnP               |
        kCLPUPnPRebuild          |
        kCLPNoJobqueue           |
        kCLPNoHousekeeper        |
        kCLPNoAutoExpire         |
        kCLPClearCache           |
        kCLPVerbose              |
        kCLPSetVerbose           |
        kCLPLogFile              |
        kCLPPidFile              |
        kCLPInFile               |
        kCLPOutFile              |
        kCLPUsername             |
        kCLPEvent                |
        kCLPSystemEvent          |
        kCLPChannelId            |
        kCLPStartTime            |
        kCLPPrintExpire          |
        kCLPGeneratePreview);

    for (int argpos = 0; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
    }

#ifndef _WIN32
    for (int i = UNUSED_FILENO; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);
    QCoreApplication a(argc, argv);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    QApplication a(argc, argv);
#endif

    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            QByteArray help = cmdline.GetHelpString(true).toLocal8Bit();
            cout << help.constData();
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
    }

    if (cmdline.HasInvalidPreviewGenerationParams())
    {
        cerr << "--generate-preview must be accompanied by either " <<endl
             << "\nboth --chanid and --starttime parameters, " << endl
             << "\nor the --infile parameter." << endl;
        return BACKEND_EXIT_INVALID_CMDLINE;
    }

    logfile = cmdline.GetLogFilename();
    pidfile = cmdline.GetPIDFilename();

    ///////////////////////////////////////////////////////////////////////

    // Don't listen to console input
    close(0);

    setupLogfile();

    CleanupGuard callCleanup(cleanup);

    int exitCode = setup_basics(cmdline);
    if (BACKEND_EXIT_OK != exitCode)
        return exitCode;

    {
        extern const char *myth_source_version;
        extern const char *myth_source_path;
        QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
            .arg(basename(argv[0])).arg(myth_source_path)
            .arg(myth_source_version);
        VERBOSE(VB_IMPORTANT, versionStr);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (cmdline.HasBackendCommand())
    {
        if (!setup_context(cmdline))
            return BACKEND_EXIT_NO_MYTHCONTEXT;
        return handle_command(cmdline);
    }

    ///////////////////////////////////////////

    return run_backend(cmdline);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
