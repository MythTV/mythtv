#include <QString>

#include "libmythbase/mythcorecontext.h"
#include "mythfrontend_commandlineparser.h"

MythFrontendCommandLineParser::MythFrontendCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHFRONTEND)
{ MythFrontendCommandLineParser::LoadArguments(); }

void MythFrontendCommandLineParser::LoadArguments(void)
{
    addHelp();
    addVersion();
    addWindowed();
    addMouse();
    addSettingsOverride();
    addGeometry();
    addDisplay();
    addUPnP();
    addLogging();
    addPlatform();

    add(QStringList{"-r", "--reset"}, "reset", false,
        "Resets appearance settings and language.", "");
    add(QStringList{"-p", "--prompt"}, "prompt", false,
        "Always prompt for backend selection.", "");
    add(QStringList{"-d", "--disable-autodiscovery"},
        "noautodiscovery", false,
        "Prevent frontend from using UPnP autodiscovery.", "");

    add("--jumppoint", "jumppoint", "",
        "Start the frontend at specified jump point.", "")
            ->SetGroup("Startup Behavior");
    add("--runplugin", "runplugin", "",
        "Start the frontend within specified plugin.", "")
            ->SetGroup("Startup Behavior")
            ->SetBlocks("jumppoint");

    add(QStringList{"-vrr", "--vrr"}, "vrr", 0U,
                    "Try to enable (1) or disable (0) variable refresh rate (FreeSync or GSync)","");
}

QString MythFrontendCommandLineParser::GetHelpHeader(void) const
{
    return "MythFrontend is the primary playback application for MythTV. It \n"
           "is used for playback of scheduled and live recordings, and management \n"
           "of recording rules.";
}
