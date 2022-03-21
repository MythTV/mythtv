#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythwelcome_commandlineparser.h"

MythWelcomeCommandLineParser::MythWelcomeCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHWELCOME)
{ MythWelcomeCommandLineParser::LoadArguments(); }

QString MythWelcomeCommandLineParser::GetHelpHeader(void) const
{
    return "MythWelcome is a graphical launcher application to allow MythFrontend \n"
           "to disconnect from the backend, and allow automatic shutdown to occur.";
}

void MythWelcomeCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();
    addDisplay();
    addPlatform();
    addWindowed();
    addGeometry();
    addMouse();
    add(QStringList{"-s", "--setup"}, "setup", false,
            "Run setup for mythshutdown.", "");
}

