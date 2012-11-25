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

#include "mythcontext.h"
#include "mythcorecontext.h"
#include "mythversion.h"
#include "mythdb.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "dbcheck.h"
#include "previewgenerator.h"
#include "commandlineparser.h"
#include "mythsystemevent.h"
#include "mythlogging.h"
#include "signalhandling.h"

#define LOC      QString("MythPreviewGen: ")
#define LOC_WARN QString("MythPreviewGen, Warning: ")
#define LOC_ERR  QString("MythPreviewGen, Error: ")

#ifdef Q_OS_MACX
    // 10.6 uses some file handles for its new Grand Central Dispatch thingy
    #define UNUSED_FILENO 5
#else
    #define UNUSED_FILENO 3
#endif

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = NULL;
        SignalHandler::Done();
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

int preview_helper(uint chanid, QDateTime starttime,
                   long long previewFrameNumber, long long previewSeconds,
                   const QSize &previewSize,
                   const QString &infile, const QString &outfile)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        LOG(VB_GENERAL, LOG_ERR, "Setting priority failed." + ENO);

    if (!QFileInfo(infile).isReadable() && (!chanid || !starttime.isValid()))
        ProgramInfo::QueryKeyFromPathname(infile, chanid, starttime);

    ProgramInfo *pginfo = NULL;
    if (chanid && starttime.isValid())
    {
        pginfo = new ProgramInfo(chanid, starttime);
        if (!pginfo->GetChanID())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Cannot locate recording made on '%1' at '%2'")
                .arg(chanid).arg(starttime.toString(Qt::ISODate)));
            delete pginfo;
            return GENERIC_EXIT_NOT_OK;
        }
        pginfo->SetPathname(pginfo->GetPlaybackURL(false, true));
    }
    else if (!infile.isEmpty())
    {
        if (!QFileInfo(infile).isReadable())
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("Cannot read this file '%1'").arg(infile));
            return GENERIC_EXIT_NOT_OK;
        }
        pginfo = new ProgramInfo(
            infile, ""/*plot*/, ""/*title*/, ""/*subtitle*/, ""/*director*/,
            0/*season*/, 0/*episode*/, ""/*inetref*/, 120/*length_in_minutes*/,
            1895/*year*/, ""/*id*/);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot locate recording to preview");
        return GENERIC_EXIT_NOT_OK;
    }

    PreviewGenerator *previewgen = new PreviewGenerator(
        pginfo, QString(), PreviewGenerator::kLocal);

    if (previewFrameNumber >= 0)
        previewgen->SetPreviewTimeAsFrameNumber(previewFrameNumber);

    if (previewSeconds >= 0)
        previewgen->SetPreviewTimeAsSeconds(previewSeconds);

    previewgen->SetOutputSize(previewSize);
    previewgen->SetOutputFilename(outfile);
    bool ok = previewgen->RunReal();
    previewgen->deleteLater();

    delete pginfo;

    return (ok) ? GENERIC_EXIT_OK : GENERIC_EXIT_NOT_OK;
}

int main(int argc, char **argv)
{
    MythPreviewGeneratorCommandLineParser cmdline;
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

#ifndef _WIN32
    for (int i = UNUSED_FILENO; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
        close(i);
    QCoreApplication a(argc, argv);
#else
    // MINGW application needs a window to receive messages
    // such as socket notifications :[
    QApplication a(argc, argv);
#endif
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHPREVIEWGEN);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    if ((!cmdline.toBool("chanid") || !cmdline.toBool("starttime")) &&
        !cmdline.toBool("inputfile"))
    {
        cerr << "--generate-preview must be accompanied by either " <<endl
             << "\nboth --chanid and --starttime parameters, " << endl
             << "\nor the --infile parameter." << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    ///////////////////////////////////////////////////////////////////////

    // Don't listen to console input
    close(0);

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to ignore SIGPIPE");

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }
    gCoreContext->SetBackend(false); // TODO Required?

    int ret = preview_helper(
        cmdline.toUInt("chanid"), cmdline.toDateTime("starttime"),
        cmdline.toLongLong("frame"), cmdline.toLongLong("seconds"),
        cmdline.toSize("size"),
        cmdline.toString("inputfile"), cmdline.toString("outputfile"));
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
