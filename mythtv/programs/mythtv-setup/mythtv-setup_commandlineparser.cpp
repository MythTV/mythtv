#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythtv-setup_commandlineparser.h"

MythTVSetupCommandLineParser::MythTVSetupCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHTV_SETUP)
{ MythTVSetupCommandLineParser::LoadArguments(); }

QString MythTVSetupCommandLineParser::GetHelpHeader(void) const
{
    return "Mythtv-setup is the setup application for the backend server. It is \n"
           "used to configure the backend, and manage tuner cards and storage. \n"
           "Most settings will require a restart of the backend before they take \n"
           "effect.";
}

void MythTVSetupCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addWindowed();
    addMouse();
    addGeometry();
    addDisplay();
    addLogging();
    addPlatform();

    add("--expert", "expert", false, "", "Expert mode.");
    add("--scan-list", "scanlist", false, "", "no help");
    add("--scan-save-only", "savescan", false, "", "no help");
    add("--scan-non-interactive", "scannoninteractive", false, "", "no help");

    add("--freq-std", "freqstd", "atsc", "",
            "Specify the frequency standard to be used with command "
            "line channel scanner")
        ->SetParentOf("modulation")
        ->SetParentOf("region");
    add("--modulation", "modulation", "vsb8", "",
            "Specify the frequency modulation to be used with command "
            "line channel scanner");
    add("--region", "region", "us", "",
           "Specify the region for the frequency standard to be used with command "
           "line channel scanner");

    add("--input-name", "inputname", "", "",
            "Specify which input to scan for, if specified card "
            "supports multiple.");
    add("--FTAonly", "ftaonly", false, "", "Only import 'Free To Air' channels.");
    add("--add-full-ts", "addfullts", false, "",
        "Create addition Transport Stream channels, "
        "which allow recording of the full, unaltered, transport stream.");
    add("--service-type", "servicetype", "all", "",
            "To be used with channel scanning or importing, specify "
            "the type of services to import. Select from the following, "
            "multiple can be added with '+':\n"
            "   all, tv, radio");

    add("--scan", "scan", 0U, "",
            "Run the command line channel scanner on a specified card ID.")
        ->SetParentOf("freqstd")
        ->SetParentOf("inputname")
        ->SetParentOf("ftaonly")
        ->SetParentOf("servicetype")
        ->SetParentOf("addfullts")
        ->SetBlocks("importscan");


    add("--scan-import", "importscan", 0U, "",
            "Import an existing scan from the database. Use --scan-list "
            "to enumerate scans available for import.")
        ->SetParentOf("ftaonly")
        ->SetParentOf("servicetype");
}
