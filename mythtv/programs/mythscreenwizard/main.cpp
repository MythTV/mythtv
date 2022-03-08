#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QtGlobal>
#include <QKeyEvent>
#include <QEvent>
#include <QWidget>
#include <QApplication>
#include <QString>
#include <QFileInfo>
#include <QDir>

#include "exitcodes.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "mythversion.h"
#include "commandlineparser.h"
#include "compat.h"
#include "mythsystemevent.h"
#include "mythlogging.h"
#include "signalhandling.h"
#include "screenwizard.h"
#include "langsettings.h"
#include "mythtranslation.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythcorecontext.h"
#include "cleanupguard.h"
#include "mythdisplay.h"

#define LOC      QString("MythScreenWizard: ")
#define LOC_WARN QString("MythScreenWizard, Warning: ")
#define LOC_ERR  QString("MythScreenWizard, Error: ")

namespace
{
    void cleanup()
    {
        DestroyMythMainWindow();

        delete gContext;
        gContext = nullptr;

        ReferenceCounter::PrintDebug();

        SignalHandler::Done();
    }
}

// If the theme specified in the DB is somehow broken, try a standard one:
//
/*
static bool resetTheme(QString themedir, const QString badtheme)
{
    QString themename = DEFAULT_UI_THEME;

    if (badtheme == DEFAULT_UI_THEME)
        themename = FALLBACK_UI_THEME;

    LOG(VB_GENERAL, LOG_ERR,
        QString("Overriding broken theme '%1' with '%2'")
                .arg(badtheme).arg(themename));

    gCoreContext->OverrideSettingForSession("Theme", themename);
    themedir = GetMythUI()->FindThemeDir(themename);

    MythTranslation::reload();
    GetMythUI()->LoadQtConfig();
    GetMythMainWindow()->Init();

    return RunMenu(themedir, themename);
}
*/
static void startAppearWiz(int _x, int _y, int _w, int _h)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    auto *screenwizard = new ScreenWizard(mainStack, "screenwizard");
    screenwizard->SetInitialSettings(_x, _y, _w, _h);

    if (screenwizard->Create())
        mainStack->AddScreen(screenwizard);
    else
        delete screenwizard;
}

int main(int argc, char **argv)
{
    MythScreenWizardCommandLineParser cmdline;
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
        MythScreenWizardCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    MythDisplay::ConfigureQtGUI(1, cmdline);
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHSCREENWIZARD);

    QString mask("general");
    int retval = cmdline.ConfigureLogging(mask, false);
    if (retval != GENERIC_EXIT_OK)
        return retval;

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    SignalHandler::Init();
    signal(SIGHUP, SIG_IGN);
#endif


    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(true, false, true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (gCoreContext->GetBoolSetting("RunFrontendInWindow"))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
                    "Refusing to run screen setup wizard in windowed mode.");
        return GENERIC_EXIT_NOT_OK;
    }

    int GuiOffsetX = gCoreContext->GetNumSetting("GuiOffsetX", 0);
    int GuiOffsetY = gCoreContext->GetNumSetting("GuiOffsetY", 0);
    int GuiWidth   = gCoreContext->GetNumSetting("GuiWidth", 0);
    int GuiHeight  = gCoreContext->GetNumSetting("GuiHeight", 0);

    gCoreContext->OverrideSettingForSession("GuiOffsetX", "0");
    gCoreContext->OverrideSettingForSession("GuiOffsetY", "0");
    gCoreContext->OverrideSettingForSession("GuiWidth",   "0");
    gCoreContext->OverrideSettingForSession("GuiHeight",  "0");

    cmdline.ApplySettingsOverride();

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    mainWindow->setWindowTitle(QObject::tr("MythTV Screen Setup Wizard"));

    // We must reload the translation after a language change and this
    // also means clearing the cached/loaded theme strings, so reload the
    // theme which also triggers a translation reload
/*    if (LanguageSelection::prompt())
    {
        if (!reloadTheme())
            return GENERIC_EXIT_NO_THEME;
    }
*/

/* I don't think we need to connect to the backend
    if (!gCoreContext->ConnectToMasterServer())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to connect to master server");
        return GENERIC_EXIT_CONNECT_ERROR;
    }

    MythSystemEventHandler *sysEventHandler = new MythSystemEventHandler();
*/

    startAppearWiz(GuiOffsetX, GuiOffsetY, GuiWidth, GuiHeight);
    int exitCode = QCoreApplication::exec();

/*
    if (sysEventHandler)
        delete sysEventHandler;
*/

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
