#include <QString>

#include "mythjobqueue_commandlineparser.h"

#include "libmythbase/mythappname.h"

MythJobQueueCommandLineParser::MythJobQueueCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHJOBQUEUE)
{ MythJobQueueCommandLineParser::LoadArguments(); }

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

