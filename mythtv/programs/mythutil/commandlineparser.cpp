// libmyth* headers
#include "mythcorecontext.h"

// local headers
#include "commandlineparser.h"

MythUtilCommandLineParser::MythUtilCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHUTIL)
{ LoadArguments(); }

void MythUtilCommandLineParser::LoadArguments(void)
{
    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
        // fileutils.cpp
        << add("--copyfile", "copyfile", false,
                "Copy a MythTV Storage Group file using RingBuffers", "")
                ->SetGroup("File")
                ->SetRequiredChild(QStringList("infile") << "outfile")
        << add("--download", "download", false,
                "Download a file using MythDownloadManager", "")
                ->SetGroup("File")
                ->SetRequiredChild(QStringList("infile") << "outfile")

        // mpegutils.cpp
        << add("--pidcounter", "pidcounter", false,
                "Count pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild("infile")
        << add("--pidfilter", "pidfilter", false,
                "Filter pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild(QStringList("infile") << "outfile")
        << add("--pidprinter", "pidprinter", false,
                "Print PSIP pids in a MythTV Storage Group file", "")
                ->SetGroup("MPEG-TS")
                ->SetRequiredChild("infile")
                ->SetChild("outfile")

        // markuputils.cpp
        << add("--gencutlist", "gencutlist", false,
                "Copy the commercial skip list to the cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--getcutlist", "getcutlist", false,
                "Display the current cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--setcutlist", "setcutlist", "",
                "Set a new cutlist in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--clearcutlist", "clearcutlist", false,
                "Clear the cutlist.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--getskiplist", "getskiplist", false,
                "Display the current commercial skip list.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--setskiplist", "setskiplist", "",
                "Set a new commercial skip list in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--clearskiplist", "clearskiplist", false,
                "Clear the commercial skip list.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")
        << add("--clearseektable", "clearseektable", false,
                "Clear the seek table.", "")
                ->SetGroup("Recording Markup")
                ->SetRequiredChild(QStringList("chanid") << "starttime")

        // backendutils.cpp
        << add("--resched", "resched", false,
                "Trigger a run of the recording scheduler on the existing "
                "master backend.",
                "This command will connect to the master backend and trigger "
                "a run of the recording scheduler. The call will return "
                "immediately, however the scheduler run may take several "
                "seconds to a minute or longer to complete.")
                ->SetGroup("Backend")
        << add("--scanvideos", "scanvideos", false,
                "Trigger a rescan of media content in MythVideo.",
                "This command will connect to the master backend and trigger "
                "a run of the Video scanner. The call will return "
                "immediately, however the scanner may take several seconds "
                "to tens of minutes, depending on how much new or moved "
                "content it has to hash, and how quickly the scanner can "
                "access those files to do so. If enabled, this will also "
                "trigger the bulk metadata scanner upon completion.")
                ->SetGroup("Backend")
        << add("--event", "event", QVariant::StringList, 
                "Send a backend event test message.", "")
                ->SetGroup("Backend")
        << add("--systemevent", "systemevent", "",
                "Send a backend SYSTEM_EVENT test message.", "")
                ->SetGroup("Backend")
        << add("--clearcache", "clearcache", false,
                "Trigger a cache clear on all connected MythTV systems.",
                "This command will connect to the master backend and trigger "
                "a cache clear event, which will subsequently be pushed to "
                "all other connected programs. This event will clear the "
                "local database settings cache used by each program, causing "
                "options to be re-read from the database upon next use.")
                ->SetGroup("Backend")
        << add("--parse-video-filename", "parsevideo", "", "",
                "Diagnostic tool for testing filename formats against what "
                "the Video Library name parser will detect them as.")
                ->SetGroup("Backend")

        // jobutils.cpp
        << add("--queuejob", "queuejob", "",
                "Insert a new job into the JobQueue.",
                "Schedule the specified job type (transcode, commflag, "
                "metadata, userjob1, userjob2, userjob3, userjob4) to run "
                "for the recording with the given chanid and starttime.")
                ->SetGroup("JobQueue")
                ->SetRequiredChild("chanid")
                ->SetRequiredChild("starttime")

        // messageutils.cpp
        << add("--message", "message", false,
                "Display a message on a frontend", "")
                ->SetGroup("Messaging")
        << add("--print-template", "printtemplate", false,
                "Print the template to be sent to the frontend", "")
                ->SetGroup("Messaging")
        );

    // mpegutils.cpp
    add("--pids", "pids", "", "Pids to process", "")
        ->SetRequiredChildOf("pidfilter")
        ->SetRequiredChildOf("pidprinter");
    add("--ptspids", "ptspids", "", "Pids to extract PTS from", "")
        ->SetGroup("MPEG-TS");
    add("--packetsize", "packetsize", 188, "TS Packet Size", "")
        ->SetChildOf("pidcounter")
        ->SetChildOf("pidfilter");
    add("--noautopts", "noautopts", false, "Disables PTS discovery", "")
        ->SetChildOf("pidprinter");
    add("--xml", "xml", false, "Enables XML output of PSIP", "")
        ->SetChildOf("pidprinter");

    // messageutils.cpp
    add("--udpport", "udpport", 6948, "(optional) UDP Port to send to", "")
        ->SetChildOf("message");
    add("--bcastaddr", "bcastaddr", "127.0.0.1", "(optional) IP address to send to", "")
        ->SetChildOf("message");

    // Generic Options used by more than one utility
    addRecording();
    addInFile(true);
    addSettingsOverride();
    addHelp();
    addVersion();
    addLogging();
    allowExtras();
}

QString MythUtilCommandLineParser::GetHelpHeader(void) const
{
    return "MythUtil is a command line utility application for MythTV.";
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
