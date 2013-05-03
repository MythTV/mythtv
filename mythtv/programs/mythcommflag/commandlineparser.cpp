#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythCommFlagCommandLineParser::MythCommFlagCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHCOMMFLAG)
{ LoadArguments(); }

void MythCommFlagCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addJob();
    addRecording();
    addLogging();

    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
         << new CommandLineArg("chanid")
         << new CommandLineArg("jobid")
         << add(QStringList( QStringList() << "-f" << "--file" ),
                "file", "",
                "Specify file to operate on.", "")
                    ->SetGroup("Input")
         << add("--video", "video", "", 
                "Rebuild the seek table for a video (non-recording) file.", "")
                    ->SetGroup("Input") );

    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
         << add("--gencutlist", "gencutlist", false,
                "Copy the commercial skip list to the cutlist.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--clearcutlist", "clearcutlist", false,
                "Clear the cutlist.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--clearskiplist", "clearskiplist", false,
                "Clear the commercial skip list.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--getcutlist", "getcutlist", false,
                "Display the current cutlist.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--getskiplist", "getskiplist", false,
                "Display the current commercial skip list.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--setcutlist", "setcutlist", "",
                "Set a new cutlist in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--skipdb", "skipdb", false, "",
                "Intended for external 3rd party use.")
                    ->SetGroup("Advanced")
                    ->SetRequires("file")
         << add("--rebuild", "rebuild", false,
                "Do not flag commercials, just rebuild the seektable.", "")
                    ->SetGroup("Commflagging")
                    ->SetBlocks("commmethod") );

    add("--method", "commmethod", "",
        "Commercial flagging method[s] to employ:\n"
        "off, blank, scene, blankscene, logo, all, "
        "d2, d2_logo, d2_blank, d2_scene, d2_all", "")
            ->SetGroup("Commflagging");
    add("--outputmethod", "outputmethod", "",
        "Format of output written to outputfile, essentials, full.", "")
            ->SetGroup("Commflagging");
    add("--queue", "queue", false,
        "Insert flagging job into the JobQueue, rather than "
        "running flagging in the foreground.", "");
    add("--noprogress", "noprogress", false,
        "Don't print progress on stdout.", "")
            ->SetGroup("Logging");
    add("--force", "force", false,
        "Force operation, even if program appears to be in use.", "")
            ->SetGroup("Advanced");
    add("--dontwritetodb", "dontwritedb", false, "",
        "Intended for external 3rd party use.")
            ->SetGroup("Advanced");
    add("--onlydumpdb", "dumpdb", false, "", "?")
            ->SetGroup("Advanced");
    add("--outputfile", "outputfile", "",
        "File to write commercial flagging output [debug].", "")
            ->SetGroup("Advanced");
    add("--dry-run", "dryrun", false,
        "Don't actually queue operation, just list what would be done", "");

    add("--sleep", "fullspeed", false, "", "")
            ->SetRemoved("If your system is incapable of performing\n"
               "          commercial detection without disrupting other\n"
               "          operations, use the jobqueue execution window\n"
               "          settings to ensure tasks do not run during the\n"
               "          time you may be running such other operations.",
                         "0.25");
    add("--nopercentage", "nopercentage", false, "", "")
            ->SetRemoved("Use --noprogress instead.", "0.25");
    add("--very-quiet", "veryquiet", false, "", "")
            ->SetRemoved("Use --quiet instead. Can be used multiple times\n"
               "          for increased effect.", "0.25");
    add("--all", "runall", false, "", "")
            ->SetRemoved("Use --queue with no content definition for\n"
               "          similar behavior. Will queue all tasks to be\n"
               "          run through the jobqueue, rather than run them\n"
               "          all synchronously within this instance.", "0.25");
    add("--allstart", "allstart", "", "", "")
            ->SetRemoved("and is no longer available in this version.", "0.25");
    add("--allend", "allend", "", "", "")
            ->SetRemoved("and is no longer available in this version.", "0.25");
    add("--hogcpu", "hogcpu", "", "", "")
            ->SetRemoved("and is no longer available in this version.", "0.25");
}

