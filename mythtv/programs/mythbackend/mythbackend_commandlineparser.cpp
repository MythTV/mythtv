#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythbackend_commandlineparser.h"

MythBackendCommandLineParser::MythBackendCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHBACKEND)
{ MythBackendCommandLineParser::LoadArguments(); }

void MythBackendCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addDaemon();
    addSettingsOverride();
    addUPnP();
    addDVBv3();
    addLogging();
    addPIDFile();

    CommandLineArg::AllowOneOf(QList<CommandLineArg*>()
         << add("--pidfile", "pidfile", "",
                "Filename to save the application pid.", "")
         << add("--printsched", "printsched", false,
                "Print upcoming list of scheduled recordings.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--testsched", "testsched", false,
                "do some scheduler testing.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--resched", "resched", false, "", "")
                    ->SetRemoved("mythbackend is no longer used to trigger\n"
                       "          rescheduling. Please use mythutil.", "34")
         << add("--scanvideos", "scanvideos", false,"", "")
                    ->SetRemoved("mythbackend is no longer used to trigger\n"
                       "          scanning media content. Please use mythutil.", "34")
         << add("--event", "event", "", "", "")
                    ->SetRemoved("mythbackend is no longer used send an event.", "34")
         << add("--systemevent", "systemevent", "", "", "")
                    ->SetRemoved("mythbackend is no longer used send a SYSTEM_EVENT.", "34")
         << add("--clearcache", "clearcache", false, "", "")
                    ->SetRemoved("mythbackend is no longer used to clear\n"
                       "          the database caches. Please use mythutil.", "34")
         << add("--printexpire", "printexpire", "ALL",
                "Print upcoming list of recordings to be expired.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--setverbose", "setverbose", "",
                "Change debug mask of the existing master backend.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--setloglevel", "setloglevel", "",
                "Change logging level of the existing master backend.", "")
//                    ->SetDeprecated("use mythutil instead");
    );

    add("--webonly", "webonly", false, "Start in web-server-only mode.",
            "Start the backend in web server mode, where only the "
            "web server is running. "
            "This is only for use when the backend crashes due "
            " to an invalid configuration, and you need to use setup "
            "to rectify it. Scheduler is disabled in this mode.");
    add("--nosched", "nosched", false, "",
            "Intended for debugging use only, disable the scheduler "
            "on this backend if it is the master backend, preventing "
            "any recordings from occuring until the backend is "
            "restarted without this option.");
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
    add("--user", "username", "",
            "Drop permissions to username after starting.", "");
#ifndef NDEBUG
    add("--upgradedbonly", "upgradedbonly", false,
        "Upgrade the database and then exit.",
        "Intended for development use only, Upgrade the database "
        "and then immediately exit.");
#endif
}

QString MythBackendCommandLineParser::GetHelpHeader(void) const
{
    return "MythBackend is the primary server application for MythTV. It is \n"
           "used for recording and remote streaming access of media. Only one \n"
           "instance of this application is allowed to run on one host at a \n"
           "time, and one must be configured to operate as a master, performing \n"
           "additional scheduler and housekeeper tasks.";
}
