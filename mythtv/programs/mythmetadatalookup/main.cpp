// C headers
#include <unistd.h>

// C++ headers
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QEventLoop>

// libmyth headers
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdb.h"
#include "mythversion.h"
#include "util.h"
#include "jobqueue.h"
#include "mythtranslation.h"
#include "mythconfig.h"
#include "mythcommandlineparser.h"
#include "mythlogging.h"

#include "lookup.h"

bool inJobQueue = false;

class MPUBLIC MythMetadataLookupCommandLineParser : public MythCommandLineParser
{
  public:
    MythMetadataLookupCommandLineParser();
    void LoadArguments(void);
};

MythMetadataLookupCommandLineParser::MythMetadataLookupCommandLineParser() :
    MythCommandLineParser("mythmetadatalookup")
{ LoadArguments(); }

void MythMetadataLookupCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addRecording();
    addJob();
    addLogging();

    add("--refresh-all", "refresh-all", false,
            "Refresh all recorded programs and recording rules metadata", "");
}

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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("mythmetadatalookup");

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    ///////////////////////////////////////////////////////////////////////
    // Don't listen to console input
    close(0);

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        delete gContext;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    bool refreshall     = cmdline.toBool("refresh-all");
    bool usedchanid     = cmdline.toBool("chanid");
    bool usedstarttime  = cmdline.toBool("starttime");
    bool addjob         = cmdline.toBool("jobid");

    int jobid           = cmdline.toInt("jobid");
    uint chanid         = cmdline.toUInt("chanid");
    QDateTime starttime = cmdline.toDateTime("starttime");

    if (refreshall && (usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all must not be accompanied by --chanid or --starttime");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!addjob && !refreshall && !usedchanid && !usedstarttime)
    {
        refreshall = true;
    }

    if (addjob && (refreshall || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "The jobqueue (-j) command cannot be used with other options.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!refreshall && !addjob && !(usedchanid && usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--chanid and --starttime must be used together.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    myth_nice(19);

    MythTranslation::load("mythfrontend");

    LookerUpper *lookup = new LookerUpper();

    if (addjob)
    {
        int type = JOB_METADATA;
        JobQueue::GetJobInfoFromID(jobid, type, chanid, starttime);
    }

    if (refreshall)
        lookup->HandleAllRecordings();
    else
        lookup->HandleSingleRecording(chanid, starttime);

    while (lookup->StillWorking())
    {
        sleep(1);
        qApp->processEvents();
    }

    delete lookup;
    delete gContext;

    LOG(VB_GENERAL, LOG_NOTICE, "MythMetadataLookup run complete.");

    return GENERIC_EXIT_OK;
}
