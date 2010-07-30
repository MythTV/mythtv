#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// Qt
#include <QApplication>

// MythTV
#include "mythcontext.h"
#include "mythverbose.h"
#include "mythversion.h"
#include "settings.h"
#include "mythtranslation.h"
#include "mythdbcon.h"
#include "exitcodes.h"
#include "compat.h"
#include "mythuihelper.h"
#include "lcddevice.h"
#include "mythcommandlineparser.h"
#include "tv.h"

// mythwelcome
#include "welcomedialog.h"
#include "welcomesettings.h"


QString logfile = "";

static bool log_rotate(bool report_error);
static void log_rotate_handler(int);


void initKeys(void)
{
    REG_KEY("Welcome", "STARTXTERM", QT_TRANSLATE_NOOP("MythControls",
        "Open an Xterm window"),       "F12");
    REG_KEY("Welcome", "SHOWSETTINGS", QT_TRANSLATE_NOOP("MythControls",
        "Show Mythshutdown settings"), "F11");
    REG_KEY("Welcome", "STARTSETUP", QT_TRANSLATE_NOOP("MythControls",
        "Start Mythtv-Setup"),            "");
}

void showUsage(const MythCommandLineParser &cmdlineparser)
{
    QString    help  = cmdlineparser.GetHelpString(false);
    QByteArray ahelp = help.toLocal8Bit();

    QString binname = "mythwelcome";

    extern const char *myth_source_version;
    extern const char *myth_source_path;

    VERBOSE(VB_IMPORTANT, QString("%1 version: %2 [%3] www.mythtv.org")
                            .arg(binname)
                            .arg(myth_source_path)
                            .arg(myth_source_version));

    cerr << "Valid options are: " << endl <<
            "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
            "-s or --setup                  Run setup for the mythshutdown program" << endl <<
            "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
            ahelp.constData() <<
            endl;

}

int main(int argc, char **argv)
{
    bool bShowSettings = false;

    bool cmdline_err;

    MythCommandLineParser cmdline(
        kCLPOverrideSettingsFile |
        kCLPOverrideSettings     |
        kCLPQueryVersion);

    for (int argpos = 0; argpos < argc; ++argpos)
    {
        if (cmdline.PreParse(argc, argv, argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
    }

    QApplication a(argc, argv);

    QFileInfo finfo(a.argv()[0]);
    QString binname = finfo.baseName();

    // Check command line arguments
    for (int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return FRONTEND_EXIT_INVALID_CMDLINE;

                ++argpos;
            }
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--setup"))
        {
            bShowSettings = true;
        }
        else if (!strcmp(a.argv()[argpos], "-l") ||
            !strcmp(a.argv()[argpos], "--logfile"))
        {
            if (a.argc()-1 > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
            else
            {
                cerr << "Missing argument to -l/--logfile option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (cmdline.Parse(a.argc(), a.argv(), argpos, cmdline_err))
        {
            if (cmdline_err)
                return BACKEND_EXIT_INVALID_CMDLINE;

            if (cmdline.WantsToExit())
                return BACKEND_EXIT_OK;
        }
        else
        {
            showUsage(cmdline);
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }

    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not initialize myth context. "
                        "Exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    gCoreContext->SetAppName(binname);

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "mythwelcome: Could not open the database. "
                        "Exiting.");
        return -1;
    }

    if (!logfile.isEmpty())
    {
        if (!log_rotate(true))
            cerr << "cannot open logfile; using stdout/stderr" << endl;
        else
            signal(SIGHUP, &log_rotate_handler);
    }

    LCD::SetupLCD();

    if (class LCD *lcd = LCD::Get())
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
    int new_logfd = open(logfile.toLocal8Bit().constData(),
                         O_WRONLY|O_CREAT|O_APPEND, 0664);

    if (new_logfd < 0)
    {
        /* If we can't open the new logfile, send data to /dev/null */
        if (report_error)
        {
            cerr << "cannot open logfile " << logfile.toAscii().constData() << endl;
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
    // simply assign the file descriptors to the logfile
    *stdout = *(_fdopen(new_logfd, "w"));
    *stderr = *stdout;
    setvbuf(stdout, NULL, _IOLBF, 256);
#else
    while (dup2(new_logfd, 1) < 0 && errno == EINTR);
    while (dup2(new_logfd, 2) < 0 && errno == EINTR);
    while (close(new_logfd) < 0   && errno == EINTR);
#endif

    return true;
}


static void log_rotate_handler(int)
{
    log_rotate(false);
}
