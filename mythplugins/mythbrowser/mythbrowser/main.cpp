#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// qt
#include <QApplication>

// myth
#include "mythcontext.h"
#include "mythversion.h"
#include "settings.h"
#include "langsettings.h"
#include "mythdbcon.h"
#include "exitcodes.h"
#include "compat.h"
#include "lcddevice.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/myththemebase.h"

// mythbrowser
#include "mythbrowser.h"

QString logfile = "";

static bool log_rotate(bool report_error);
static void log_rotate_handler(int);

void setupKeys(void)
{
    REG_KEY("Browser", "NEXTTAB",   "Move to next browser tab",       "P");
    REG_KEY("Browser", "PREVTAB",   "Move to previous browser tab",   "");
    REG_KEY("Browser", "DELETETAB", "Delete the current browser tab", "D");
}

void printOptions(void)
{
    cerr << "Valid options are: " << endl <<
            "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
            "-z or --zoom                   Zoom factor 0.2-5 (default: 1)" << endl <<
            "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
            "+file                          URLs to display" << endl;

}

int main(int argc, char **argv)
{
    float zoom = 1.0;
    QStringList urls;

    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "mythbrowser: Could not initialize myth context. "
                              "Exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_IMPORTANT, "mythbrowser: Could not open the database. "
                              "Exiting.");
        return -1;
    }

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
        else if (!strcmp(a.argv()[argpos],"-z") ||
            !strcmp(a.argv()[argpos],"--zoom"))
        {
            if (a.argc()-1 > argpos)
            {
                QString temp = a.argv()[argpos+1];
                zoom = temp.toFloat();
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -z/--zoom option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                {
                    ++argpos;
                }
            }
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
        else if (!strcmp(a.argv()[argpos], "-h") ||
            !strcmp(a.argv()[argpos], "--help"))
        {
            cerr << endl << endl;
            printOptions();
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
        else
        {
            QString url = a.argv()[argpos];
            if (url.startsWith("-"))
            {
                cerr << endl << endl << "Invalid argument: " << qPrintable(url) << endl;
                printOptions();
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
            urls += url;
        }
    }

    if (urls.count() == 0)
        urls += "http://www.mythtv.org";

    if (zoom < 0.2 || zoom > 5.0)
        zoom = 1.5;

    if (logfile != "")
    {
        if (!log_rotate(true))
            cerr << "cannot open logfile; using stdout/stderr" << endl;
        else
            signal(SIGHUP, &log_rotate_handler);
    }

    LCD::SetupLCD();

    if (class LCD *lcd = LCD::Get())
        lcd->switchToTime();

    LanguageSettings::load("mythfrontend");

    GetMythUI()->LoadQtConfig();

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    MythThemeBase  *themeBase = new MythThemeBase();

    setupKeys();

    MythScreenStack *mainStack = GetMythMainWindow()->GetMainStack();

    MythBrowser *mythbrowser = new MythBrowser(mainStack, "mythbrowser", urls, zoom);

    if (mythbrowser->Create())
        mainStack->AddScreen(mythbrowser, false);
    else
        return -1;

    do
    {
        qApp->processEvents();
        usleep(5000);
    } while (mainStack->TotalScreens() > 0);

    DestroyMythMainWindow();

    delete gContext;
    delete themeBase;

    return 0;
}


static bool log_rotate(bool report_error)
{
    int new_logfd = open(logfile, O_WRONLY|O_CREAT|O_APPEND, 0664);

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

    while (dup2(new_logfd, 1) < 0 && errno == EINTR);
    while (dup2(new_logfd, 2) < 0 && errno == EINTR);
    while (close(new_logfd) < 0   && errno == EINTR);

    return true;
}


static void log_rotate_handler(int)
{
    log_rotate(false);
}
