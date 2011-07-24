
using namespace std;

#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythBackendCommandLineParser::MythBackendCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHBACKEND)
{ LoadArguments(); }

void MythBackendCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addDaemon();
    addSettingsOverride();
    addUPnP();
    addLogging();
    addPIDFile();

    add("--printsched", "printsched", false,
            "Print upcoming list of scheduled recordings.", "");
    add("--testsched", "testsched", false, "do some scheduler testing.", "");
    add("--resched", "resched", false,
            "Trigger a run of the recording scheduler on the existing "
            "master backend.",
            "This command will connect to the master backend and trigger "
            "a run of the recording scheduler. The call will return "
            "immediately, however the scheduler run may take several "
            "seconds to a minute or longer to complete.");
    add("--nosched", "nosched", false, "",
            "Intended for debugging use only, disable the scheduler "
            "on this backend if it is the master backend, preventing "
            "any recordings from occuring until the backend is "
            "restarted without this option.");
    add("--scanvideos", "scanvideos", false,
            "Trigger a rescan of media content in MythVideo.",
            "This command will connect to the master backend and trigger "
            "a run of the Video scanner. The call will return "
            "immediately, however the scanner may take several seconds "
            "to tens of minutes, depending on how much new or moved "
            "content it has to hash, and how quickly the scanner can "
            "access those files to do so. If enabled, this will also "
            "trigger the bulk metadata scanner upon completion.");
    add("--nojobqueue", "nojobqueue", false, "",
            "Intended for debugging use only, disable the jobqueue "
            "on this backend. As each jobqueue independently selects "
            "jobs, this will only have any affect on this local "
            "backend.");
    add("--nohousekeeper", "nohousekeeper", false, "",
            "Intended for debugging use only, disable the housekeeper "
            "on this backend if it is the master backend, preventing "
            "any guide processing, recording cleanup, or any other "
            "task performed by the housekeeper.");
    add("--noautoexpire", "noautoexpire", false, "",
            "Intended for debugging use only, disable the autoexpirer "
            "on this backend if it is the master backend, preventing "
            "recordings from being expired to clear room for new "
            "recordings.");
    add("--event", "event", "", "Send a backend event test message.", "");
    add("--systemevent", "systemevent", "",
            "Send a backend SYSTEM_EVENT test message.", "");
    add("--clearcache", "clearcache", false,
            "Trigger a cache clear on all connected MythTV systems.",
            "This command will connect to the master backend and trigger "
            "a cache clear event, which will subsequently be pushed to "
            "all other connected programs. This event will clear the "
            "local database settings cache used by each program, causing "
            "options to be re-read from the database upon next use.");
    add("--printexpire", "printexpire", "ALL",
            "Print upcoming list of recordings to be expired.", "");
    add("--setverbose", "setverbose", "",
            "Change debug mask of the existing master backend.", "");
    add("--setloglevel", "setloglevel", "",
            "Change logging level of the existing master backend.", "");
    add("--user", "username", "",
            "Drop permissions to username after starting.", "");
}

QString MythBackendCommandLineParser::GetHelpHeader(void) const
{
    return "MythBackend is the primary server application for MythTV. It is \n"
           "used for recording and remote streaming access of media. Only one \n"
           "instance of this application is allowed to run on one host at a \n"
           "time, and one must be configured to operate as a master, performing \n"
           "additional scheduler and housekeeper tasks.";
}
