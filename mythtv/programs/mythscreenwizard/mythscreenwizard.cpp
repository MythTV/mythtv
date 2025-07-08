// C/C++
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// Qt
#include <QtGlobal>
#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QString>
#include <QWidget>

// MythTV
#include "libmythui/langsettings.h"
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/mythappname.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythdbcon.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythtv/mythsystemevent.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"

// MythScreenWizard
#include "mythscreenwizard_commandlineparser.h"
#include "screenwizard.h"

#define LOC      QString("MythScreenWizard: ")
#define LOC_WARN QString("MythScreenWizard, Warning: ")
#define LOC_ERR  QString("MythScreenWizard, Error: ")

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

    retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    MythContext context {MYTH_BINARY_VERSION};
    if (!context.Init(true, false, true))
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
