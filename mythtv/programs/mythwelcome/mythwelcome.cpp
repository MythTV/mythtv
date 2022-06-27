// Qt
#include <QtGlobal>
#include <QApplication>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/exitcodes.h"
#include "libmythbase/lcddevice.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythtranslation.h"
#include "libmythbase/mythversion.h"
#include "libmythbase/signalhandling.h"
#include "libmythui/mythdisplay.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"

// mythwelcome
#include "mythwelcome_commandlineparser.h"
#include "welcomedialog.h"
#include "welcomesettings.h"

#if CONFIG_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
static inline void mw_sd_notify(const char *str) { sd_notify(0, str); };
#else
static inline void mw_sd_notify(const char */*str*/) {};
#endif

static void initKeys(void)
{
    REG_KEY("Welcome", "STARTXTERM", QT_TRANSLATE_NOOP("MythControls",
        "Open an Xterm window"),       "F12");
    REG_KEY("Welcome", "SHOWSETTINGS", QT_TRANSLATE_NOOP("MythControls",
        "Show Mythshutdown settings"), "F11");
    REG_KEY("Welcome", "STARTSETUP", QT_TRANSLATE_NOOP("MythControls",
        "Start Mythtv-Setup"),            "");
}

int main(int argc, char **argv)
{
    bool bShowSettings = false;

    MythWelcomeCommandLineParser cmdline;
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
        MythWelcomeCommandLineParser::PrintVersion();
        return GENERIC_EXIT_OK;
    }

    MythDisplay::ConfigureQtGUI(1, cmdline);
    QApplication a(argc, argv);
    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHWELCOME);

    int retval = cmdline.ConfigureLogging();
    if (retval != GENERIC_EXIT_OK)
        return retval;

    if (!cmdline.toString("geometry").isEmpty())
        MythMainWindow::ParseGeometryOverride(cmdline.toString("geometry"));

    if (cmdline.toBool("setup"))
        bShowSettings = true;

#ifndef _WIN32
    SignalHandler::Init();
#endif

    gContext = new MythContext(MYTH_BINARY_VERSION, true);

    cmdline.ApplySettingsOverride();
    if (!gContext->Init())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "mythwelcome: Could not initialize MythContext. Exiting.");
        SignalHandler::Done();
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    cmdline.ApplySettingsOverride();

    if (!MSqlQuery::testDBConnection())
    {
        LOG(VB_GENERAL, LOG_ERR,
            "mythwelcome: Could not open the database. Exiting.");
        SignalHandler::Done();
        return -1;
    }

    LCD::SetupLCD();

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime();

    MythTranslation::load("mythfrontend");

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    mainWindow->DisableIdleTimer();

    initKeys();
    // Provide systemd ready notification (for type=notify units)
    mw_sd_notify("READY=1");

    MythScreenStack *mainStack = mainWindow->GetMainStack();

    MythScreenType *screen = nullptr;
    if (bShowSettings)
    {
        screen = new StandardSettingDialog(mainStack, "shutdown",
                                           new MythShutdownSettings());
    }
    else
    {
        screen = new WelcomeDialog(mainStack, "mythwelcome");
    }

    bool ok = screen->Create();
    if (ok)
    {
        mainStack->AddScreen(screen, false);

        // Block by running an event loop until last screen is removed
        QEventLoop block;
        QObject::connect(mainStack, &MythScreenStack::topScreenChanged,
                         &block, [&](const MythScreenType* _screen)
                             { if (!_screen) block.quit(); });
        block.exec();
    }

    mw_sd_notify("STOPPING=1\nSTATUS=Exiting");
    DestroyMythMainWindow();

    delete gContext;

    SignalHandler::Done();

    return ok ? 0 : -1;
}
