#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythWelcomeCommandLineParser::MythWelcomeCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHWELCOME)
{ LoadArguments(); }

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

    add(QStringList( QStringList() << "-s" << "--setup" ), "setup", false,
            "Run setup for mythshutdown.", "");
}

