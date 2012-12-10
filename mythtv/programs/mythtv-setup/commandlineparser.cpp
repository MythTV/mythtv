#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythTVSetupCommandLineParser::MythTVSetupCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHTV_SETUP)
{ LoadArguments(); }

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

    add("--expert", "expert", false, "", "Expert mode.");
    add("--scan-list", "scanlist", false, "", "no help");
    add("--scan-save-only", "savescan", false, "", "no help");
    add("--scan-non-interactive", "scannoninteractive", false, "", "no help");

    add("--frequency-table", "freqtable", "atsc-vsb8-us", "",
            "Specify frequency table to be used with command "
            "line channel scanner.");
    add("--input-name", "inputname", "", "",
            "Specify which input to scan for, if specified card "
            "supports multiple.");
    add("--FTAonly", "ftaonly", false, "", "Only import 'Free To Air' channels.");
    add("--service-type", "servicetype", "all", "",
            "To be used with channel scanning or importing, specify "
            "the type of services to import. Select from the following, "
            "multiple can be added with '+':\n"
            "   all, tv, radio");

    add("--scan", "scan", 0U, "", 
            "Run the command line channel scanner on a specified card ID.")
        ->SetParentOf("freqtable")
        ->SetParentOf("inputname")
        ->SetParentOf("ftaonly")
        ->SetParentOf("servicetype")
        ->SetBlocks("importscan");

    add("--scan-import", "importscan", 0U, "",
            "Import an existing scan from the database. Use --scan-list "
            "to enumerate scans available for import.")
        ->SetParentOf("ftaonly")
        ->SetParentOf("servicetype");
}

