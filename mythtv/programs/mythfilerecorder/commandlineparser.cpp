
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythFileRecorderCommandLineParser::MythFileRecorderCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{ LoadArguments(); }

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

    add(QStringList(QStringList() << "--noloop"),
        "noloop", false,
        "Don't loop input back to beginning on EOF.", "");
}

