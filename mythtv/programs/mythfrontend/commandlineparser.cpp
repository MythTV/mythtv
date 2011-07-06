
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythFrontendCommandLineParser::MythFrontendCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHFRONTEND)
{ LoadArguments(); }

void MythFrontendCommandLineParser::LoadArguments(void)
{
    allowArgs();
    addHelp();
    addVersion();
    addWindowed(false);
    addSettingsOverride();
    addGeometry();
    addDisplay();
    addUPnP();
    addLogging();

    add(QStringList( QStringList() << "-r" << "--reset" ), "reset", false,
        "Resets appearance, settings, and language.", "");
    add(QStringList( QStringList() << "-p" << "--prompt" ), "prompt", false,
        "Always prompt for backend selection.", "");
    add(QStringList( QStringList() << "-d" << "--disable-autodiscovery" ),
        "noautodiscovery", false, "Prevent frontend from using UPnP autodiscovery.", "");
}

QString MythFrontendCommandLineParser::GetHelpHeader(void) const
{
    return "MythFrontend is the primary playback application for MythTV. It \n"
           "is used for playback of scheduled and live recordings, and management \n"
           "of recording rules.";
}

