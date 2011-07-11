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
            "Batch update recorded program metadata. If a recording's "
            "rule has an inetref but the recording does not, it will "
            "be inherited.", "");
    add("--refresh-rules", "refresh-rules", false,
            "Also update inetref for recording rules when metadata is "
            "found for a recording (Best effort only, imperfect)", "");
    add("--refresh-all-rules", "refresh-all-rules", false,
            "Batch update metadata for recording rules. This will "
            "set inetrefs for your recording rules based on an automated "
            "lookup. This is a best effort, and not guaranteed! If your "
            "recordings lack inetrefs but one is found for the rule, it "
            "will be inherited.", "");
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

    bool refreshall      = cmdline.toBool("refresh-all");
    bool refreshrules    = cmdline.toBool("refresh-rules");
    bool refreshallrules = cmdline.toBool("refresh-all-rules");
    bool usedchanid      = cmdline.toBool("chanid");
    bool usedstarttime   = cmdline.toBool("starttime");
    bool addjob          = cmdline.toBool("jobid");

    int jobid            = cmdline.toInt("jobid");
    uint chanid          = cmdline.toUInt("chanid");
    QDateTime starttime  = cmdline.toDateTime("starttime");

    if (refreshallrules && (refreshall || usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all-rules must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (refreshall && (usedchanid || usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "--refresh-all must not be accompanied by any other argument.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!addjob && !refreshall && !refreshallrules &&
        !usedchanid && !usedstarttime)
    {
        refreshall = true;
    }

    if (addjob && (refreshall || refreshallrules || usedchanid ||
                   usedstarttime))
    {
        LOG(VB_GENERAL, LOG_ERR,
            "The jobqueue (-j) command cannot be used with other options.");
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (!refreshall && !refreshallrules && !addjob &&
        !(usedchanid && usedstarttime))
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
    {
        lookup->CopyRuleInetrefsToRecordings();
        lookup->HandleAllRecordings(refreshrules);
    }
    else if (refreshallrules)
    {
        lookup->HandleAllRecordingRules();
        lookup->CopyRuleInetrefsToRecordings();
    }
    else
        lookup->HandleSingleRecording(chanid, starttime, refreshrules);

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
