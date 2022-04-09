// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
#include <memory>

// Qt headers
#include <QtGlobal>
#include <QCoreApplication>
#include <QEventLoop>
#ifdef Q_OS_DARWIN
#include <QProcessEnvironment>
#endif

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythversion.h"
#include "mythmiscutil.h"
#include "jobqueue.h"
#include "mythtranslation.h"
#include "mythconfig.h"
#include "mythmetadatalookup_commandlineparser.h"
#include "mythlogging.h"
#include "signalhandling.h"
#include "cleanupguard.h"

#include "lookup.h"

namespace
{
    void cleanup()
    {
        delete gContext;
        gContext = nullptr;
        SignalHandler::Done();
    }
}

bool inJobQueue = false;

int main(int argc, char *argv[])
{
    MythMetadataLookupCommandLineParser cmdline;
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
        MythMetadataLookupCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHMETADATALOOKUP);

#ifdef Q_OS_DARWIN
    QString path = QCoreApplication::applicationDirPath();
    setenv("PYTHONPATH",
           QString("%1/../Resources/lib/%2/site-packages:%3")
           .arg(path)
           .arg(QFileInfo(PYTHON_EXE).fileName())
           .arg(QProcessEnvironment::systemEnvironment().value("PYTHONPATH"))
           .toUtf8().constData(), 1);
#endif

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    SignalHandler::Init();
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    myth_nice(19);

    MythTranslation::load("mythfrontend");

    std::unique_ptr<LookerUpper> lookup {new LookerUpper};

    LOG(VB_GENERAL, LOG_INFO,
            "Testing grabbers and metadata sites for functionality...");
    if (!LookerUpper::AllOK())
        return GENERIC_EXIT_NOT_OK;
    LOG(VB_GENERAL, LOG_INFO,
            "All grabbers tested and working.  Continuing...");

    if (cmdline.toBool("jobid"))
    {
        uint chanid = 0;
        QDateTime starttime;
        int jobType = JOB_METADATA;

        if (!JobQueue::GetJobInfoFromID(cmdline.toInt("jobid"), jobType,
                                        chanid, starttime))
        {
            LOG(VB_GENERAL, LOG_INFO,
                        QString("No valid job found for jobid: %1")
                                    .arg(cmdline.toInt("jobid")));
            return GENERIC_EXIT_NOT_OK;
        }

        lookup->HandleSingleRecording(chanid, starttime,
                                      cmdline.toBool("refresh-rules"));
    }
    else if (cmdline.toBool("chanid") && cmdline.toBool("starttime"))
    {
        lookup->HandleSingleRecording(cmdline.toUInt("chanid"),
                                      cmdline.toDateTime("starttime"),
                                      cmdline.toBool("refresh-rules"));
    }
    else if (cmdline.toBool("refresh-all-rules"))
    {
        lookup->HandleAllRecordingRules();
        LookerUpper::CopyRuleInetrefsToRecordings();
    }
    else if (cmdline.toBool("refresh-all-artwork"))
        lookup->HandleAllArtwork(false);
    else if (cmdline.toBool("refresh-all-artwork-dangerously"))
        lookup->HandleAllArtwork(true);
    else
    {
        // refresh-all is default behavior if no other arguments given
        LookerUpper::CopyRuleInetrefsToRecordings();
        lookup->HandleAllRecordings(cmdline.toBool("refresh-rules"));
    }

    while (lookup->StillWorking())
    {
        sleep(1);
        qApp->processEvents();
    }

    LOG(VB_GENERAL, LOG_NOTICE, "MythMetadataLookup run complete.");

    return GENERIC_EXIT_OK;
}
