
using namespace std;

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

    add(QStringList( QStringList() << "-f" << "--file" ), "file", "",
            "Specify file to operate on.", "");
    add("--video", "video", "", "Rebuild the seek table for a video (non-recording) file.", "");
    add("--method", "commmethod", "", "Commercial flagging method[s] to employ:\n"
                                      "off, blank, scene, blankscene, logo, all, "
                                      "d2, d2_logo, d2_blank, d2_scene, d2_all", "");
    add("--outputmethod", "outputmethod", "",
            "Format of output written to outputfile, essentials, full.", "");
    add("--gencutlist", "gencutlist", false, "Copy the commercial skip list to the cutlist.", "");
    add("--clearcutlist", "clearcutlist", false, "Clear the cutlist.", "");
    add("--clearskiplist", "clearskiplist", false, "Clear the commercial skip list.", "");
    add("--getcutlist", "getcutlist", false, "Display the current cutlist.", "");
    add("--getskiplist", "getskiplist", false, "Display the current commercial skip list.", "");
    add("--setcutlist", "setcutlist", "", "Set a new cutlist in the form:\n"
                                          "#-#[,#-#]... (ie, 1-100,1520-3012,4091-5094)", "");
    add("--skipdb", "skipdb", false, "", "Intended for external 3rd party use.");
    add("--queue", "queue", false, "Insert flagging job into the JobQueue, rather than "
                            "running flagging in the foreground.", "");
    add("--noprogress", "noprogress", false, "Don't print progress on stdout.", "");
    add("--rebuild", "rebuild", false, "Do not flag commercials, just rebuild the seektable.", "");
    add("--force", "force", false, "Force operation, even if program appears to be in use.", "");
    add("--dontwritetodb", "dontwritedb", false, "", "Intended for external 3rd party use.");
    add("--onlydumpdb", "dumpdb", false, "", "?");
    add("--outputfile", "outputfile", "", "File to write commercial flagging output [debug].", "");
    add("--dry-run", "dryrun", false, "Don't actually queue operation, just list what would be done", "");
}

