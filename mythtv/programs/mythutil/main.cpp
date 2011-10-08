// Qt includes
#include <QApplication>

// libmyth* includes
#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythlogging.h"

// Local includes
#include "mythutil.h"
#include "commandlineparser.h"
#include "backendutils.h"
#include "fileutils.h"
#include "jobutils.h"
#include "markuputils.h"
#include "messageutils.h"


int main(int argc, char *argv[])
{
    MythUtilCommandLineParser cmdline;
    if (!cmdline.Parse(argc, argv))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_INVALID_CMDLINE;
    }

    if (cmdline.toBool("showhelp"))
    {
        cmdline.PrintHelp();
        return GENERIC_EXIT_OK;
    }

    if (cmdline.toBool("showversion"))
    {
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHUTIL);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    UtilMap utilMap;

    registerBackendUtils(utilMap);
    registerFileUtils(utilMap);
    registerJobUtils(utilMap);
    registerMarkupUtils(utilMap);
    registerMessageUtils(utilMap);

    bool cmdFound = false;
    int cmdResult = GENERIC_EXIT_OK;
    UtilMap::iterator i;
    for (i = utilMap.begin(); i != utilMap.end(); i++)
    {
        if (cmdline.toBool(i.key()))
        {
            cmdResult = (i.value())(cmdline);
            cmdFound = true;
            break;
        }
    }

    if(!cmdFound)
    {
        cmdline.PrintHelp();
        cmdResult = GENERIC_EXIT_INVALID_CMDLINE;
    }

    delete gContext;

    return cmdResult;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
