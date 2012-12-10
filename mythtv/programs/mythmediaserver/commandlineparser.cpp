#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythMediaServerCommandLineParser::MythMediaServerCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHMEDIASERVER)
{ LoadArguments(); }

QString MythMediaServerCommandLineParser::GetHelpHeader(void) const
{
    return "MythMediaServer is a daemon implementing the backend file server.\n"
           "It is intended to allow access to remote file storage on machines\n"
           "that do not have tuners, and as such cannot run a backend.";
}

void MythMediaServerCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addSettingsOverride();
    addPIDFile();
    addDaemon();
    addLogging();
}

