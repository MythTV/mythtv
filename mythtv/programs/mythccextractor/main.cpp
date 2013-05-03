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

namespace {
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

static int RunCCExtract(const ProgramInfo &program_info)
{
    if (!program_info.IsLocal())
    {
        QString msg =
            QString("Only locally accessible files are supported (%1).")
            .arg(program_info.GetPathname());
        cerr << qPrintable(msg) << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    QString filename = program_info.GetPathname();
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


    PlayerFlags flags = (PlayerFlags)(kVideoIsNull | kAudioMuted  |
                                      kDecodeNoLoopFilter | kDecodeFewBlocks |
                                      kDecodeLowRes | kDecodeSingleThreaded |
                                      kDecodeNoDecode);
    MythCCExtractorPlayer *ccp = new MythCCExtractorPlayer(flags, true, filename);
    PlayerContext *ctx = new PlayerContext(kCCExtractorInUseID);
    ctx->SetPlayingInfo(&program_info);
    ctx->SetRingBuffer(tmprbuf);
    ctx->SetPlayer(ccp);

    ccp->SetPlayerInfo(NULL, NULL, ctx);
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

    bool useDB = false;

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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QString infile = cmdline.toString("inputfile");
    if (infile.isEmpty())
    {
        cerr << "The input file --infile is required" << endl;
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

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

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(
            false/*use gui*/, false/*prompt for backend*/,
            false/*bypass auto discovery*/, !useDB/*ignoreDB*/))
    {
        cerr << "Failed to init MythContext, exiting." << endl;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    ProgramInfo pginfo(infile);
    return RunCCExtract(pginfo);
}


/* vim: set expandtab tabstop=4 shiftwidth=4: */
