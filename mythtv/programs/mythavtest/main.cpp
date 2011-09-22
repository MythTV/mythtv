#include <unistd.h>
#include <iostream>

using namespace std;

#include <QString>
#include <QRegExp>
#include <QDir>
#include <QApplication>

#include "tv_play.h"
#include "programinfo.h"
#include "commandlineparser.h"

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythversion.h"
#include "mythdbcon.h"
#include "compat.h"
#include "dbcheck.h"
#include "mythlogging.h"

// libmythui
#include "mythuihelper.h"
#include "mythmainwindow.h"


int main(int argc, char *argv[])
{
    MythAVTestCommandLineParser cmdline;
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
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHAVTEST);

    int retval;
    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toString("display").isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.toString("display"));
    }

    if (!cmdline.toString("geometry").isEmpty())
    {
        MythUIHelper::ParseGeometryOverride(cmdline.toString("geometry"));
    }

    QString filename = "";
    if (cmdline.GetArgs().size() >= 1)
        filename = cmdline.GetArgs()[0];

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    setuid(getuid());

    QString themename = gCoreContext->GetSetting("Theme");
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        QString msg = QString("Fatal Error: Couldn't find theme '%1'.")
            .arg(themename);
        LOG(VB_GENERAL, LOG_ERR, msg);
        return GENERIC_EXIT_NO_THEME;
    }

    GetMythUI()->LoadQtConfig();

#if defined(Q_OS_MACX)
    // Mac OS X doesn't define the AudioOutputDevice setting
#else
    QString auddevice = gCoreContext->GetSetting("AudioOutputDevice");
    if (auddevice.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal Error: Audio not configured, you need "
                                 "to run 'mythfrontend', not 'mythtv'.");
        return GENERIC_EXIT_SETUP_ERROR;
    }
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();

    TV::InitKeys();

    if (!UpgradeTVDatabaseSchema(false))
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal Error: Incorrect database schema.");
        delete gContext;
        return GENERIC_EXIT_DB_OUTOFDATE;
    }

    if (filename.isEmpty())
    {
        TV::StartTV(NULL, kStartTVNoFlags);
    }
    else
    {
        ProgramInfo pginfo(filename);
        TV::StartTV(&pginfo, kStartTVNoFlags);
    }

    DestroyMythMainWindow();

    delete gContext;

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
