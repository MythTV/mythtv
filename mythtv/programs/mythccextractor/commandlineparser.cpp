#include "commandlineparser.h"
#include "mythcorecontext.h"

MythCCExtractorCommandLineParser::MythCCExtractorCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHCCEXTRACTOR)
{
    LoadArguments();
}

void MythCCExtractorCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging("none", LOG_ERR);
    add(QStringList( QStringList() << "-i" << "--infile" ), "inputfile", "",
            "input file", "");
}

QString MythCCExtractorCommandLineParser::GetHelpHeader(void) const
{
    return
        "This is a command for generating srt files for\n"
        "DVB and ATSC recordings.";
}
