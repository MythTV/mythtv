#include <qapplication.h>
#include <cstdlib>

#include "libmyth/mythcontext.h"
#include "libmyth/settings.h"
#include "libmyth/mythdbcon.h"
#include <libmyth/exitcodes.h>

#include "libmythtv/tv.h"

#include "welcomedialog.h"
#include "welcomesettings.h"

void initKeys(void)
{
    REG_KEY("Welcome", "STARTXTERM", "Open an Xterm window", "F12");
    REG_KEY("Welcome", "SHOWSETTINGS", "Show Mythshutdown settings", "F11");
}

int main(int argc, char **argv)
{
    bool bShowSettings = false;
    
    QApplication a(argc, argv);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init()) 
    {
        VERBOSE(VB_ALL, "mythwelcome: Could not initialize myth context. "
                        "Exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (!MSqlQuery::testDBConnection())
    {
        VERBOSE(VB_ALL, "mythwelcome: Could not open the database. "
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
        else if (!strcmp(a.argv()[argpos],"-s") ||
            !strcmp(a.argv()[argpos],"--setup"))
        {
            bShowSettings = true;
        }
        else
        {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl <<
                    "Valid options are: " << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<
                    "-s or --setup                  Run setup for the mythshutdown program" << endl;
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }
    
    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Init();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);
    
    initKeys();

    if (bShowSettings)        
    {        
        MythShutdownSettings settings;
        settings.exec();
    }
    else
    {
        WelcomeDialog *mythWelcome = new WelcomeDialog(gContext->GetMainWindow(),
                                                    "welcome_screen", "welcome-", 
                                                    "welcome_screen");
        mythWelcome->exec();
        
        delete mythWelcome;
    }
    
    delete gContext;

    return 0;
}
