#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythJobQueueCommandLineParser::MythJobQueueCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHJOBQUEUE)
{ LoadArguments(); }

QString MythJobQueueCommandLineParser::GetHelpHeader(void) const
{
    return "MythJobqueue is daemon implementing the job queue. It is intended \n"
           "for use as additional processing power without requiring a full backend.";
}

void MythJobQueueCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();
    addPIDFile();
    addDaemon();
}

