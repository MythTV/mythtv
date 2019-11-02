// -*- Mode: c++ -*-

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
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
#include "signalhandling.h"
#include "loggingserver.h"
#include "cleanupguard.h"

namespace {
    void cleanup()
    {
        delete gContext;
        gContext = nullptr;
        SignalHandler::Done();
    }
}

static int RunCCExtract(const ProgramInfo &program_info, const QString & destdir)
{
    QString filename = program_info.GetPlaybackURL();
    if (filename.startsWith("myth://"))
    {
        QString msg =
            QString("Only locally accessible files are supported (%1).")
            .arg(program_info.GetPathname());
        cerr << qPrintable(msg) << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!QFile::exists(filename))
    {
        cerr << qPrintable(
            QString("Could not open input file (%1).").arg(filename)) << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    RingBuffer *tmprbuf = RingBuffer::Create(filename, false);
    if (!tmprbuf)
    {
        cerr << qPrintable(QString("Unable to create RingBuffer for %1")
                           .arg(filename)) << endl;
        return GENERIC_EXIT_PERMISSIONS_ERROR;
    }

    if (program_info.GetRecordingEndTime() > MythDate::current())
    {
        cout << "Program will end @ "
             << qPrintable(program_info.GetRecordingEndTime(MythDate::ISODate))
             << endl;
        tmprbuf->SetWaitForWrite();
    }

    PlayerFlags flags = (PlayerFlags)(kVideoIsNull | kAudioMuted  |
                                      kDecodeNoLoopFilter | kDecodeFewBlocks |
                                      kDecodeLowRes | kDecodeSingleThreaded |
                                      kDecodeNoDecode);
    MythCCExtractorPlayer *ccp = new MythCCExtractorPlayer(flags, true,
                                                           filename, destdir);
    PlayerContext *ctx = new PlayerContext(kCCExtractorInUseID);
    ctx->SetPlayingInfo(&program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(ccp);

    ccp->SetPlayerInfo(nullptr, nullptr, ctx);
    if (ccp->OpenFile() < 0)
    {
        cerr << "Failed to open " << qPrintable(filename) << endl;
        return GENERIC_EXIT_NOT_OK;
    }
    if (!ccp->run())
    {
        cerr << "Failed to decode " << qPrintable(filename) << endl;
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
        cerr << "The input file --infile is required" << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QString destdir = cmdline.toString("destdir");

    bool useDB = !QFile::exists(infile);

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(
            false/*use gui*/, false/*prompt for backend*/,
            false/*bypass auto discovery*/, !useDB/*ignoreDB*/))
    {
        cerr << "Failed to init MythContext, exiting." << endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    ProgramInfo pginfo(infile);
    return RunCCExtract(pginfo, destdir);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
