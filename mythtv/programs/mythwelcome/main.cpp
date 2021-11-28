// Qt
#include <QApplication>

// MythTV
#include "mythcontext.h"
#include "mythversion.h"
#include "mythtranslation.h"
#include "exitcodes.h"
#include "compat.h"
#include "lcddevice.h"
#include "commandlineparser.h"
#include "loggingserver.h"
#include "mythlogging.h"
#include "signalhandling.h"
#include "mythdisplay.h"

// libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"

// mythwelcome
#include "welcomedialog.h"
#include "welcomesettings.h"

#if CONFIG_SYSTEMD_NOTIFY
#include <systemd/sd-daemon.h>
#define mw_sd_notify(x) \
    (void)sd_notify(0, x);
#else
#define mw_sd_notify(x)
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
    QList<int> signallist;
    signallist << SIGINT << SIGTERM << SIGSEGV << SIGABRT << SIGBUS << SIGFPE
               << SIGILL;
#if ! CONFIG_DARWIN
    signallist << SIGRTMIN;
#endif
    SignalHandler::Init(signallist);
    SignalHandler::SetHandler(SIGHUP, logSigHup);
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
    mw_sd_notify("READY=1")

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

    mw_sd_notify("STOPPING=1\nSTATUS=Exiting")
    DestroyMythMainWindow();

    delete gContext;

    SignalHandler::Done();

    return ok ? 0 : -1;
}
