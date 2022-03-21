#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythfilerecorder_commandlineparser.h"

MythFileRecorderCommandLineParser::MythFileRecorderCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{ MythFileRecorderCommandLineParser::LoadArguments(); }

QString MythFileRecorderCommandLineParser::GetHelpHeader(void) const
{
    return "MythFilelRecorder's allow a go-between app to interface "
        "with a recording device before the data is processed by mythbackend.";
}

void MythFileRecorderCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();

    addInFile();

    add("--inputid", "inputid", "", "MythTV input this app is attached to.", "")
        ->SetGroup("ExternalRecorder");

    add(QStringList{"--noloop"},
        "noloop", false,
        "Don't loop input back to beginning on EOF.", "");

    add(QStringList{"--data-rate"},
        "data_rate", 188*50000,
        "Rate at which to read data from the file.", "");
}
