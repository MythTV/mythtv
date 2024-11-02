#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythcommflag_commandlineparser.h"

MythCommFlagCommandLineParser::MythCommFlagCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHCOMMFLAG)
{ MythCommFlagCommandLineParser::LoadArguments(); }

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
         << add(QStringList{"-f", "--file"},
                "file", "",
                "Specify file to operate on.", "")
                    ->SetGroup("Input")
         << add("--video", "video", "", 
                "Rebuild the seek table for a video (non-recording) file.", "")
                    ->SetGroup("Input") );

    CommandLineArg::AllowOneOf( QList<CommandLineArg*>()
         << add("--gencutlist", "gencutlist", false,
                "Copy the commercial skip list to the cutlist.", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          cut lists. Please use mythutil.", "34")
         << add("--clearcutlist", "clearcutlist", false,
                "Clear the cutlist.", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          cut lists. Please use mythutil.", "34")
         << add("--clearskiplist", "clearskiplist", false,
                "Clear the commercial skip list.", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          skip lists. Please use mythutil.", "34")
         << add("--getcutlist", "getcutlist", false,
                "Display the current cutlist.", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          cut lists. Please use mythutil.", "34")
         << add("--getskiplist", "getskiplist", false,
                "Display the current commercial skip list.", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          skip lists. Please use mythutil.", "34")
         << add("--setcutlist", "setcutlist", "",
                "Set a new cutlist in the form:\n"
                "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "")
                    ->SetRemoved("mythcommflag is no longer used to manipulate\n"
                       "          cut lists. Please use mythutil.", "34")
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
}

