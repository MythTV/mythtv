#include "commandlineparser.h"
#include "mythcorecontext.h"

MythLogServerCommandLineParser::MythLogServerCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHLOGSERVER)
{
    LoadArguments();
}

void MythLogServerCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addDaemon();
    addLogging();
}

QString MythLogServerCommandLineParser::GetHelpHeader(void) const
{
    return
        "This is the common logserver that accepts logging messages from\n"
        "all the clients via ZeroMQ.";
}
