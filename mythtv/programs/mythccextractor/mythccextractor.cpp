// -*- Mode: c++ -*-

// C++ headers
#include <iostream>

// Qt headers
#include <QtGlobal>
#include <QCoreApplication>
#include <QString>

// MythTV headers
#include "libmyth/mythcontext.h"
#include "libmythbase/cleanupguard.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/programinfo.h"
#include "libmythbase/signalhandling.h"
#include "libmythtv/io/mythmediabuffer.h"
#include "libmythtv/mythccextractorplayer.h"

// MythCCExtractor
#include "mythccextractor_commandlineparser.h"

namespace {
    void cleanup()
    {
        delete gContext;
        gContext = nullptr;
        SignalHandler::Done();
    }
}

static int RunCCExtract(ProgramInfo &program_info, const QString & destdir)
{
    QString filename = program_info.GetPlaybackURL();
    if (filename.startsWith("myth://"))
    {
        QString msg =
            QString("Only locally accessible files are supported (%1).")
            .arg(program_info.GetPathname());
        std::cerr << qPrintable(msg) << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!QFile::exists(filename))
    {
        std::cerr << qPrintable(
            QString("Could not open input file (%1).").arg(filename)) << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    MythMediaBuffer *tmprbuf = MythMediaBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        std::cerr << qPrintable(QString("Unable to create RingBuffer for %1")
                                .arg(filename)) << std::endl;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (program_info.GetRecordingEndTime() > MythDate::current())
    {
        std::cout << "Program will end @ "
                  << qPrintable(program_info.GetRecordingEndTime(MythDate::ISODate))
                  << std::endl;
        tmprbuf->SetWaitForWrite();
    }

    auto flags = (PlayerFlags)(kVideoIsNull | kAudioMuted  |
                               kDecodeNoLoopFilter | kDecodeFewBlocks |
                               kDecodeLowRes | kDecodeSingleThreaded |
                               kDecodeNoDecode);
    auto *ctx = new PlayerContext(kCCExtractorInUseID);
    auto *ccp = new MythCCExtractorPlayer(ctx, flags, true, filename, destdir);
    ctx->SetPlayingInfo(&program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(ccp);
    if (ccp->OpenFile() < 0)
    {
        std::cerr << "Failed to open " << qPrintable(filename) << std::endl;
        return GENERIC_EXIT_NOT_OK;
    }
    if (!ccp->run())
    {
        std::cerr << "Failed to decode " << qPrintable(filename) << std::endl;
        return GENERIC_EXIT_NOT_OK;
    }

    delete ctx;

    return GENERIC_EXIT_OK;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHCCEXTRACTOR);

    MythCCExtractorCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    int retval = cmdline.ConfigureLogging("none");
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        MythCCExtractorCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QString infile = cmdline.toString("inputfile");
    if (infile.isEmpty())
    {
        std::cerr << "The input file --infile is required" << std::endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QString destdir = cmdline.toString("destdir");

    bool useDB = !QFile::exists(infile);

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    SignalHandler::Init();
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(
            false/*use gui*/, false/*prompt for backend*/,
            false/*bypass auto discovery*/, !useDB/*ignoreDB*/))
    {
        std::cerr << "Failed to init MythContext, exiting." << std::endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    ProgramInfo pginfo(infile);
    return RunCCExtract(pginfo, destdir);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
