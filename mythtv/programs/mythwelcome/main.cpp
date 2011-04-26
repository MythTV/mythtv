#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Qt
#include <QApplication>
#include <QFileInfo>
#include <QDir>

// MythTV
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "mythtranslation.h"
#include "mythdbcon.h"
#include "exitcodes.h"
#include "compat.h"
#include "lcddevice.h"
#include "mythcommandlineparser.h"
#include "tv.h"
#include "mythlogging.h"

// libmythui
#include "mythmainwindow.h"
#include "mythuihelper.h"

// mythwelcome
#include "welcomedialog.h"
#include "welcomesettings.h"


QString logfile = "";

static bool log_rotate(bool report_error);
static void log_rotate_handler(int);


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
        cmdline.PrintVersion();
        return GENERIC_EXIT_OK;
    }
    
    QApplication a(argc, argv);

    QCoreApplication::setApplicationName(MYTH_APPNAME_MYTHWELCOME);

    if (parse_verbose_arg(cmdline.toString("verbose")) ==
            GENERIC_EXIT_INVALID_CMDLINE)
        return GENERIC_EXIT_INVALID_CMDLINE;

    if (cmdline.toBool("setup"))
        bShowSettings = true;
    if (!cmdline.toString("logfile").isEmpty())
        logfile = cmdline.toString("logfile");

    logStart("");

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not initialize MythContext. "
                        "Exiting.");
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not open the database. "
                        "Exiting.");
        return -1;
    }

    if (!logfile.isEmpty())
    {
        if (!log_rotate(true))
            cerr << "cannot open log file; using stdout/stderr" << endl;
        else
            signal(SIGHUP, &log_rotate_handler);
    }

    LCD::SetupLCD();

    if (LCD *lcd = LCD::Get())
        lcd->switchToTime();

    MythTranslation::load("mythfrontend");

    GetMythUI()->LoadQtConfig();

#ifdef Q_WS_MACX
    // Mac OS 10.4 and Qt 4.4 have window-focus problems
    gCoreContext->SetSetting("RunFrontendInWindow", "1");
#endif

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();

    initKeys();

    if (bShowSettings)
    {
        MythShutdownSettings settings;
        settings.exec();
    }
    else
    {
        MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

        WelcomeDialog *welcome = new WelcomeDialog(mainStack, "mythwelcome");

        if (welcome->Create())
            mainStack->AddScreen(welcome, false);
        else
            return -1;

        do
        {
            qApp->processEvents();
            usleep(5000);
        } while (mainStack->TotalScreens() > 0);
    }

    DestroyMythMainWindow();

    delete gContext;

    return 0;
}


static bool log_rotate(bool report_error)
{
#if 0
    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND, 0664);

    if (new_logfd < 0)
    {
        /* If we can't open the new log file, send data to /dev/null */
        if (report_error)
        {
            cerr << "cannot open log file " << logfile.toAscii().constData() << endl;
            return false;
        }

        new_logfd = open("/dev/null", O_WRONLY);

        if (new_logfd < 0)
        {
            /* There's not much we can do, so punt. */
            return false;
        }
    }

#ifdef WINDOWS_CLOSE_CONSOLE
    // pure Win32 GUI app does not have standard IO streams
    // simply assign the file descriptors to the log file
    *stdout = *(_fdopen(new_logfd, "w"));
    *stderr = *stdout;
    setvbuf(stdout, NULL, _IOLBF, 256);
#else
    while (dup2(new_logfd, 1) < 0 && errno == EINTR);
    while (dup2(new_logfd, 2) < 0 && errno == EINTR);
    while (close(new_logfd) < 0   && errno == EINTR);
#endif
#endif

    logStart(logfile);
    return true;
}


static void log_rotate_handler(int)
{
    log_rotate(false);
}
