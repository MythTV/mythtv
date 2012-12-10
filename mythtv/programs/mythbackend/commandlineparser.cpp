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

    CommandLineArg::AllowOneOf(QList<CommandLineArg*>()
         << add("--printsched", "printsched", false,
                "Print upcoming list of scheduled recordings.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--testsched", "testsched", false,
                "do some scheduler testing.", "")
//                    ->SetDeprecated("use mythutil instead")
         << add("--resched", "resched", false,
                "Trigger a run of the recording scheduler on the existing "
                "master backend.",
                "This command will connect to the master backend and trigger "
                "a run of the recording scheduler. The call will return "
                "immediately, however the scheduler run may take several "
                "seconds to a minute or longer to complete.")
                    ->SetDeprecated("use mythutil instead")
         << add("--scanvideos", "scanvideos", false,
                "Trigger a rescan of media content in MythVideo.",
                "This command will connect to the master backend and trigger "
                "a run of the Video scanner. The call will return "
                "immediately, however the scanner may take several seconds "
                "to tens of minutes, depending on how much new or moved "
                "content it has to hash, and how quickly the scanner can "
                "access those files to do so. If enabled, this will also "
                "trigger the bulk metadata scanner upon completion.")
                    ->SetDeprecated("use mythutil instead")
         << add("--event", "event", "",
                "Send a backend event test message.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--systemevent", "systemevent", "",
                "Send a backend SYSTEM_EVENT test message.", "")
                    ->SetDeprecated("use mythutil instead")
         << add("--clearcache", "clearcache", false,
                "Trigger a cache clear on all connected MythTV systems.",
                "This command will connect to the master backend and trigger "
                "a cache clear event, which will subsequently be pushed to "
                "all other connected programs. This event will clear the "
                "local database settings cache used by each program, causing "
                "options to be re-read from the database upon next use.")
                    ->SetDeprecated("use mythutil instead")
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

    add("--upnprebuild", "upnprebuild", false, "", "")
            ->SetRemoved("The UPnP server no longer maintains its own list\n"
               "          of video content, and instead uses the shared\n"
               "          list of the Video Library. Update the Video Library\n"
               "          content list from within mythfrontend, or on the\n"
               "          command line using 'mythutil --scanvideos'.",
                         "0.25");
    add("--infile", "inputfile", "", "", "")
            ->SetRemoved("mythbackend is no longer used for preview\n"
               "          generation. Please use mythpreviewgen.", "0.25");
    add("--outfile", "outputfile", "", "", "")
            ->SetRemoved("mythbackend is no longer used for preview\n"
               "          generation. Please use mythpreviewgen.", "0.25");
    add("--chanid", "chanid", "", "", "")
            ->SetRemoved("mythbackend is no longer used for preview\n"
               "          generation. Please use mythpreviewgen.", "0.25");
    add("--starttime", "starttime", "", "", "")
            ->SetRemoved("mythbackend is no longer used for preview\n"
               "          generation. Please use mythpreviewgen.", "0.25");
}

QString MythBackendCommandLineParser::GetHelpHeader(void) const
{
    return "MythBackend is the primary server application for MythTV. It is \n"
           "used for recording and remote streaming access of media. Only one \n"
           "instance of this application is allowed to run on one host at a \n"
           "time, and one must be configured to operate as a master, performing \n"
           "additional scheduler and housekeeper tasks.";
}
