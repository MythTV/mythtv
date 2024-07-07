// C++ headers
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/time.h>     // for setpriority
#include <sys/types.h>
#include <unistd.h>

// Qt
#include <QtGlobal>
#ifndef _WIN32
#include <QCoreApplication>
#else
#include <QApplication>
#endif
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/cleanupguard.h"
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdb.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/signalhandling.h"
#include "libmythbase/storagegroup.h"
#include "libmythtv/dbcheck.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythtv/previewgenerator.h"

//MythPreviewGen
#include "mythpreviewgen_commandlineparser.h"

#define LOC      QString("MythPreviewGen: ")
#define LOC_WARN QString("MythPreviewGen, Warning: ")
#define LOC_ERR  QString("MythPreviewGen, Error: ")

#ifdef Q_OS_MACOS
// 10.6 uses some file handles for its new Grand Central Dispatch thingy
static constexpr long UNUSED_FILENO { 5 };
#else
static constexpr long UNUSED_FILENO { 3 };
#endif

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = nullptr;
        SignalHandler::Done();
    }
}

int preview_helper(uint chanid, QDateTime starttime,
                   long long previewFrameNumber, std::chrono::seconds previewSeconds,
                   const QSize previewSize,
                   const QString &infile, const QString &outfile)
{
    // Lower scheduling priority, to avoid problems with recordings.
    if (setpriority(PRIO_PROCESS, 0, 9))
        LOG(VB_GENERAL, LOG_ERR, "Setting priority failed." + ENO);

    if (!QFileInfo(infile).isReadable() && ((chanid == 0U) || !starttime.isValid()))
        ProgramInfo::QueryKeyFromPathname(infile, chanid, starttime);

    ProgramInfo *pginfo = nullptr;
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
            infile, ""/*plot*/, ""/*title*/, ""/*sortTitle*/, ""/*subtitle*/,
            ""/*sortSubtitle*/, ""/*director*/, 0/*season*/, 0/*episode*/,
            ""/*inetref*/, 120min/*length_in_minutes*/, 1895/*year*/, ""/*id*/);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Cannot locate recording to preview");
        return GENERIC_EXIT_NOT_OK;
    }

    auto *previewgen = new PreviewGenerator(pginfo, QString(),
                                            PreviewGenerator::kLocal);

    if (previewFrameNumber >= 0)
        previewgen->SetPreviewTimeAsFrameNumber(previewFrameNumber);

    if (previewSeconds >= 0s)
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
        MythPreviewGeneratorCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

#ifndef _WIN32
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
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHPREVIEWGEN);

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if ((!cmdline.toBool("chanid") || !cmdline.toBool("starttime")) &&
        !cmdline.toBool("inputfile"))
    {
        std::cerr << "--generate-preview must be accompanied by either " <<std::endl
                  << "\nboth --chanid and --starttime parameters, " << std::endl
                  << "\nor the --infile parameter." << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    ///////////////////////////////////////////////////////////////////////

    // Don't listen to console input
    close(0);

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    SignalHandler::Init();
#endif

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Unable to ignore SIGPIPE");

    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    int ret = preview_helper(
        cmdline.toUInt("chanid"), cmdline.toDateTime("starttime"),
        cmdline.toLongLong("frame"), std::chrono::seconds(cmdline.toLongLong("seconds")),
        cmdline.toSize("size"),
        cmdline.toString("inputfile"), cmdline.toString("outputfile"));
    return ret;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
