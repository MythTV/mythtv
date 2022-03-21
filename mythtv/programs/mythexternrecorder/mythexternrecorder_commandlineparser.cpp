#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythexternrecorder_commandlineparser.h"

MythExternRecorderCommandLineParser::MythExternRecorderCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHEXTERNRECORDER)
{ MythExternRecorderCommandLineParser::LoadArguments(); }

QString MythExternRecorderCommandLineParser::GetHelpHeader(void) const
{
    return "mythexternrecorder is a go-between app which interfaces "
        "between a recording device and mythbackend.";
}

void MythExternRecorderCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();

    add("--conf", "conf", "", "Path to a configuration file in INI format.", "")
        ->SetGroup("ExternalRecorder");

    add("--inputid", "inputid", "", "MythTV input this app is attached to.", "")
        ->SetGroup("ExternalRecorder");

    add(QStringList{"--exec"},
        "exec", false,
        "Execute a program to retrieve Transport Stream from. "
        "Data is expected to be on stdout.", "")
        ->SetGroup("ExternalRecorder");

}
