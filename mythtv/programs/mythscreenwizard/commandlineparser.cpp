#include <QString>

#include "mythcorecontext.h"
#include "commandlineparser.h"

MythScreenWizardCommandLineParser::MythScreenWizardCommandLineParser() :
    MythCommandLineParser(MYTH_APPNAME_MYTHSCREENWIZARD)
{ LoadArguments(); }

QString MythScreenWizardCommandLineParser::GetHelpHeader(void) const
{
    return "MythScreenWizard is an external application implementing the Screen \n"
           "Setup Wizard. This should typically not be run manually, but instead \n"
           "called through the frontend.";
}

void MythScreenWizardCommandLineParser::LoadArguments(void)
{
    addHelp();
    addSettingsOverride();
    addVersion();
    addLogging();
    addMouse();
    addDisplay();
}

