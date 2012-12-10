#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythPreviewGeneratorCommandLineParser::MythPreviewGeneratorCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHPREVIEWGEN)
{ LoadArguments(); }

void MythPreviewGeneratorCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addRecording();
    addLogging();

    add("--seconds", "seconds", 0LL, "Number of seconds into video to take preview image.", "");
    add("--frame", "frame", 0LL, "Number of frames into video to take preview image.", "");
    add("--size", "size", QSize(0,0), "Dimensions of preview image.", "");
    add("--infile", "inputfile", "", "Input video for preview generation.", "");
    add("--outfile", "outputfile", "", "Optional output file for preview generation.", "");
}


