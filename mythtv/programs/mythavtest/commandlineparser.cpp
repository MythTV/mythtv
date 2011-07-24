
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythAVTestCommandLineParser::MythAVTestCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHAVTEST)
{ LoadArguments(); }

QString MythAVTestCommandLineParser::GetHelpHeader(void) const
{
    return "MythAVTest is a testing application that allows direct access \n"
           "to the MythTV internal video player.";
}

void MythAVTestCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addSettingsOverride();
    addVersion();
    addWindowed(false);
    addGeometry();
    addDisplay();
    addLogging();
}

