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
#include "mythverbose.h"
#include "mythversion.h"
#include "mythdb.h"
#include "exitcodes.h"
#include "compat.h"
#include "storagegroup.h"
#include "programinfo.h"
#include "dbcheck.h"
#include "previewgenerator.h"
#include "mythcommandlineparser.h"
#include "mythsystemevent.h"
#include "mythlogging.h"

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

int preview_helper(const QString &_chanid, const QString &starttime,
                   long long previewFrameNumber, long long previewSeconds,
                   const QSize &previewSize,
                   const QString &infile, const QString &outfile)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        VERBOSE(VB_GENERAL, "Setting priority failed." + ENO);

    uint chanid = _chanid.toUInt();
    QDateTime recstartts = myth_dt_from_string(starttime);
    if (!chanid || !recstartts.isValid())
        ProgramInfo::ExtractKeyFromPathname(infile, chanid, recstartts);

    ProgramInfo *pginfo = NULL;
    if (chanid && recstartts.isValid())
    {
        pginfo = new ProgramInfo(chanid, recstartts);
        if (!pginfo->GetChanID())
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Cannot locate recording made on '%1' at '%2'")
                    .arg(chanid).arg(starttime));
            delete pginfo;
            return GENERIC_EXIT_NOT_OK;
        }
        pginfo->SetPathname(pginfo->GetPlaybackURL(false, true));
    }
    else if (!infile.isEmpty())
    {
        if (!QFileInfo(infile).isReadable())
        {
            VERBOSE(VB_IMPORTANT, QString(
                        "Cannot read this file '%1'").arg(infile));
            return GENERIC_EXIT_NOT_OK;
        }
        pginfo = new ProgramInfo(
            infile, ""/*plot*/, ""/*title*/, ""/*subtitle*/, ""/*director*/,
            0/*season*/, 0/*episode*/, 120/*length_in_minutes*/,
            1895/*year*/);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Cannot locate recording to preview");
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
    int quiet = 0;

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

    if ((!cmdline.toBool("chanid") || !cmdline.toBool("starttime")) &&
        !cmdline.toBool("inputfile"))
    {
        cerr << "--generate-preview must be accompanied by either " <<endl
             << "\nboth --chanid and --starttime parameters, " << endl
             << "\nor the --infile parameter." << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("quiet"))
    {
        quiet = cmdline.toUInt("quiet");
        if (quiet > 1)
        {
            print_verbose_messages = VB_NONE;
            parse_verbose_arg("none");
        }
    }

    int facility = cmdline.GetSyslogFacility();

    ///////////////////////////////////////////////////////////////////////

    // Don't listen to console input
    close(0);

    CleanupGuard callCleanup(cleanup);

    QString logfile = cmdline.GetLogFilePath();
    logStart(logfile, quiet, facility);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Unable to ignore SIGPIPE");

    {
        QString versionStr = QString("%1 version: %2 [%3] www.mythtv.org")
            .arg(MYTH_APPNAME_MYTHPREVIEWGEN).arg(MYTH_SOURCE_PATH)
            .arg(MYTH_SOURCE_VERSION);
        VERBOSE(VB_IMPORTANT, versionStr);
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(false))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }
    gCoreContext->SetBackend(false); // TODO Required?

    int ret = preview_helper(
        cmdline.toString("chanid"), cmdline.toString("starttime"),
        cmdline.toLongLong("frame"), cmdline.toLongLong("seconds"),
        cmdline.toSize("size"),
        cmdline.toString("inputfile"), cmdline.toString("outputfile"));
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
