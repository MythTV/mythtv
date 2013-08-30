#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythFrontendCommandLineParser::MythFrontendCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHFRONTEND)
{ LoadArguments(); }

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

    add(QStringList( QStringList() << "-r" << "--reset" ), "reset", false,
        "Resets appearance settings and language.", "");
    add(QStringList( QStringList() << "-p" << "--prompt" ), "prompt", false,
        "Always prompt for backend selection.", "");
    add(QStringList( QStringList() << "-d" << "--disable-autodiscovery" ),
        "noautodiscovery", false,
        "Prevent frontend from using UPnP autodiscovery.", "");

    add("--jumppoint", "jumppoint", "",
        "Start the frontend at specified jump point.", "")
            ->SetGroup("Startup Behavior");
    add("--runplugin", "runplugin", "",
        "Start the frontend within specified plugin.", "")
            ->SetGroup("Startup Behavior")
            ->SetBlocks("jumppoint");

    add(QStringList( QStringList() << "-G" << "--get-setting" ),
        "getsetting", "", "", "")
            ->SetRemoved("Use the Services API instead.", "0.25");
    add(QStringList( QStringList() << "-u" << "--upgrade-schema" ),
        "upgradeschema", "", "", "")
            ->SetRemoved("The frontend is no longer allowed to update\n"
               "          the primary database schema. Use mythtv-setup\n"
               "          or restart your primary backend to have it\n"
               "          perform the task automatically.", "0.25");
}

QString MythFrontendCommandLineParser::GetHelpHeader(void) const
{
    return "MythFrontend is the primary playback application for MythTV. It \n"
           "is used for playback of scheduled and live recordings, and management \n"
           "of recording rules.";
}

