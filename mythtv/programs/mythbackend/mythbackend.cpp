#include "libmythbase/mythconfig.h"
#if CONFIG_SYSTEMD_NOTIFY
    #include <systemd/sd-daemon.h>
#endif

#include <csignal> // for signal
#include <cstdlib>
#include <thread>

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QtSystemDetection>
#endif
#ifndef Q_OS_WINDOWS
#include <QCoreApplication>
#else
#include <QApplication>
#endif

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#ifdef Q_OS_DARWIN
#include <QProcessEnvironment>
#endif

#include <unistd.h>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/configuration.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythdirs.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/jobqueue.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythtv/previewgenerator.h"
#include "libmythtv/scheduledrecording.h"
#include "libmythtv/tv_rec.h"

// MythBackend
#include "autoexpire.h"
#include "backendcontext.h"
#include "mainserver.h"
#include "mediaserver.h"
#include "mythbackend_commandlineparser.h"
#include "mythbackend_main_helpers.h"
#include "scheduler.h"

#include "servicesv2/v2myth.h"

#define LOC      QString("MythBackend: ")
#define LOC_WARN QString("MythBackend, Warning: ")
#define LOC_ERR  QString("MythBackend, Error: ")

#ifdef Q_OS_MACOS
// 10.6 uses some file handles for its new Grand Central Dispatch thingy
static constexpr long UNUSED_FILENO { 6 };
#else
static constexpr long UNUSED_FILENO { 3 };
#endif

int main(int argc, char **argv)
{
    MythBackendCommandLineParser cmdline;
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
        MythBackendCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

#ifndef Q_OS_WINDOWS
#if HAVE_CLOSE_RANGE
    close_range(UNUSED_FILENO, sysconf(_SC_OPEN_MAX) - 1, 0);
#else
    for (long i = UNUSED_FILENO; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);
#endif
    QCoreApplication a(argc, argv);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    QApplication a(argc, argv);
#endif
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHBACKEND);

#ifdef Q_OS_DARWIN
    QString path = QCoreApplication::applicationDirPath();
    setenv("PYTHONPATH",
           QString("%1/../Resources/lib/%2:/../Resources/lib/%2/site-packages:/../Resources/lib/%2/lib-dynload:%3")
           .arg(path)
           .arg(QFileInfo(PYTHON_EXE).fileName())
           .arg(QProcessEnvironment::systemEnvironment().value("PYTHONPATH"))
           .toUtf8().constData(), 1);
#endif

    int retval = cmdline.Daemonize();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    bool daemonize = cmdline.toBool("daemon");
    QString mask("general");
    retval = cmdline.ConfigureLogging(mask, daemonize);
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (daemonize)
        // Don't listen to console input if daemonized
        close(0);

#if CONFIG_SYSTEMD_NOTIFY
    (void)sd_notify(0, "STATUS=Connecting to database.");
#endif

    /*
    InitializeMythDirs() is called by MythContext(), but we need to call it
    first so XmlConfiguration() can find the configuration file.
    */
    InitializeMythDirs();
    // If setup has not been done (ie. the config.xml does not exist),
    // set the ignoreDB flag, which will cause only the web-app to
    // start, so that setup can be done.
    bool ignoreDB = !(XmlConfiguration().FileExists());
    if (ignoreDB)
        V2Myth::s_WebOnlyStartup = V2Myth::kWebOnlyDBSetup;

    // Init Parameters:
    // bool Init(bool gui = true,
    //           bool promptForBackend = false,
    //           bool disableAutoDiscovery = false,
    //           bool ignoreDB = false);
    MythContext context {MYTH_BINARY_VERSION};
    if (!context.Init(false,false,false,ignoreDB))
    {
        LOG(VB_GENERAL, LOG_CRIT, "Failed to init MythContext.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }
    context.setCleanup(cleanup);

    MythTranslation::load("mythfrontend");

    setHttpProxy();

    cmdline.ApplySettingsOverride();

    if (cmdline.toBool("setverbose")    || cmdline.toBool("printsched") ||
        cmdline.toBool("testsched")     ||
        cmdline.toBool("printexpire")   || cmdline.toBool("setloglevel"))
    {
        gCoreContext->SetAsBackend(false);
        return handle_command(cmdline);
    }

    gCoreContext->SetAsBackend(true);
    retval = run_backend(cmdline);
    // Retcode 258 is a special value to signal to mythbackend to restart
    // This is used by the V2Myth/Shutdown?Restart=true API call
    // Retcode 259 is a special value to signal to mythbackend to restart
    // in webonly mode
    if (retval == 258 || retval == 259)
    {
        char ** newargv = new char * [argc + 2];
        std::string webonly = "--webonly";
        newargv[0] = argv[0];
        int newargc = 1;
        for (int ix = 1 ; ix < argc ; ++ix)
        {
            if (webonly != argv[ix])
                newargv[newargc++] = argv[ix];
        }
        if (retval == 259)
            newargv[newargc++] = webonly.data();
        newargv[newargc] = nullptr;
        LOG(VB_GENERAL, LOG_INFO,
            QString("Restarting mythbackend"));
        std::this_thread::sleep_for(50ms);
        int rc = execvp(newargv[0], newargv);
        LOG(VB_GENERAL, LOG_ERR,
            QString("execvp failed prog %1 rc=%2 errno=%3").arg(argv[0]).arg(rc).arg(errno));
        delete[] newargv;
    }
    return retval;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
