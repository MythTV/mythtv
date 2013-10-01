
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cmath>

#include <QKeyEvent>
#include <QEvent>
#include <QTextCodec>
#include <QWidget>
#include <QApplication>
#include <QString>
#include <QRegExp>
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

#define LOC      QString("MythScreenWizard: ")
#define LOC_WARN QString("MythScreenWizard, Warning: ")
#define LOC_ERR  QString("MythScreenWizard, Error: ")

using namespace std;

namespace
{
    void cleanup()
    {
        DestroyMythMainWindow();

        delete gContext;
        gContext = NULL;

        ReferenceCounter::PrintDebug();

        delete qApp;

        SignalHandler::Done();
    }

    class CleanupGuard
    {
      public:
        typedef void (*CleanupFunc)();

      public:
        CleanupGuard(CleanupFunc cleanFunction) :
            m_cleanFunction(cleanFunction) {}

        ~CleanupGuard()
        {
            m_cleanFunction();
        }

      private:
        CleanupFunc m_cleanFunction;
    };
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
#if CONFIG_DARWIN
    GetMythMainWindow()->Init(QT_PAINTER);
#else
    GetMythMainWindow()->Init();
#endif
    GetMythMainWindow()->ReinitDone();

    return RunMenu(themedir, themename);
}
*/
static void startAppearWiz(int _x, int _y, int _w, int _h)
{
    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    ScreenWizard *screenwizard = new ScreenWizard(mainStack,
                                                        "screenwizard");
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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }


#ifdef Q_OS_MAC
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(false);
#endif
    new QApplication(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHSCREENWIZARD);

    int retval;
    QString mask("general");
    if ((retval = cmdline.ConfigureLogging(mask, false)) != GENERIC_EXIT_OK)
        return retval;

    CleanupGuard callCleanup(cleanup);

#ifndef _WIN32
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    signal(SIGHUP, SIG_IGN);
#endif


    if ((retval = cmdline.ConfigureLogging()) != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toString("display").isEmpty())
    {
        MythUIHelper::SetX11Display(cmdline.toString("display"));
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init(true, false, true))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to init MythContext, exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (gCoreContext->GetNumSetting("RunFrontendInWindow"))
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

    GetMythUI()->LoadQtConfig();

    QString themename = gCoreContext->GetSetting("Theme", DEFAULT_UI_THEME);
    QString themedir = GetMythUI()->FindThemeDir(themename);
    if (themedir.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't find theme '%1'")
                .arg(themename));
        return GENERIC_EXIT_NO_THEME;
    }

    MythMainWindow *mainWindow = GetMythMainWindow();
#if CONFIG_DARWIN
    mainWindow->Init(OPENGL2_PAINTER);
#else
    mainWindow->Init();
#endif
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
    int exitCode = qApp->exec();

/*
    if (sysEventHandler)
        delete sysEventHandler;
*/

    return exitCode ? exitCode : GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
