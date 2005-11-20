#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>

#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <iostream>

#include "libmyth/mythcontext.h"
#include "libmyth/mythdbcon.h"
#include "libmyth/langsettings.h"
#include "libmyth/dialogbox.h"
#include "libmyth/exitcodes.h"
#include "libmyth/themedmenu.h"
#include "libmyth/util.h"

#include "libmythtv/dbcheck.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "backendsettings.h"
#include "checksetup.h"

using namespace std;
bool is_backend_running(void);

void SetupMenuCallback(void* data, QString& selection) {
    (void)data;

    QString sel = selection.lower();

    if (sel == "general") {
        BackendSettings be;
        be.exec();
    } else if (sel == "capture cards") {
        CaptureCardEditor cce;
        cce.exec();
    } else if (sel == "video sources") {
        VideoSourceEditor vse;
        vse.exec();
    } else if (sel == "card inputs") {
        CardInputEditor cie;
        cie.exec();
    } else if (sel == "channel editor") {
        ChannelEditor ce;
        ce.exec();
    }
}

void SetupMenu(void) 
{
    QString theme = gContext->GetSetting("Theme", "blue");

    ThemedMenu* menu = new ThemedMenu(gContext->FindThemeDir(theme),
                                      "setup.xml", gContext->GetMainWindow(),
                                      false);

    menu->setCallback(SetupMenuCallback, gContext);
    menu->setKillable();

    if (menu->foundTheme()) {
            menu->Show();
            menu->exec();
    } else {
        cerr << "Couldn't find theme " << theme << endl;
    }
}

int main(int argc, char *argv[])
{
    QString geometry = "";
    QString verboseString = QString(" important general");

#ifdef Q_WS_X11
    // Remember any -geometry argument which QApplication init will remove
    for(int argpos = 1; argpos + 1 < argc; ++argpos)
        if (!strcmp(argv[argpos],"-geometry"))
            geometry = argv[argpos+1];
#endif

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(FALSE);
#endif
    QApplication a(argc, argv);

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-geometry") ||
            !strcmp(a.argv()[argpos],"--geometry"))      
        {
            if (a.argc()-1 > argpos)
            {
                geometry = a.argv()[argpos+1];
                if (geometry.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to --geometry option\n";                    return -1;
                }                      
                else
                    ++argpos;
            }            
            else
            {            
                cerr << "Missing argument to --geometry option\n"; 
                return -1;
            }            
        }
        else if (!strcmp(a.argv()[argpos],"-v") ||
                  !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return BACKEND_EXIT_INVALID_CMDLINE;                        
                ++argpos;
            } 
            else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        } 
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help") ||
                !strcmp(a.argv()[argpos],"--usage")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;

            cerr << "Valid options are: "<<endl
#ifdef Q_WS_X11 
                 <<"-display X-server              Create GUI on X-server, not localhost"<<endl
#endif          
                 <<"--geometry WxH                 Override window size settings"<<endl
                 <<"-geometry WxH+X+Y              Override window size and position"<<endl
                 <<"-v or --verbose debug-level    Use '-v help' for level info"<<endl 
                 <<endl;
            return -1;
        }
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(true))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }

    if (geometry != "")
        if (!gContext->ParseGeometryOverride(geometry))
            cerr << "Illegal -geometry argument '"
                 << geometry << "' (ignored)\n";

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open database.\n";
        //     << "Driver error was:" << endl
        //     << db->lastError().driverText() << endl
        //     << "Database error was:" << endl
        //     << db->lastError().databaseText() << endl;

        return -1;
    }

    UpgradeTVDatabaseSchema();

    gContext->SetSetting("Theme", "blue");
    gContext->LoadQtConfig();

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    gContext->SetMainWindow(mainWindow);

    LanguageSettings::prompt();
    LanguageSettings::load("mythfrontend");

    QString warn = 
        QObject::tr("WARNING") + ": " +
        QObject::tr("MythTV has detected that the backend is running.")+"\n\n"+
        QObject::tr("Changing existing card inputs, deleting anything, "
                    "or scanning for channels may not work.");

    if (is_backend_running())
    {
        int val = MythPopupBox::show2ButtonPopup(
            gContext->GetMainWindow(), QObject::tr("WARNING"),
            warn,
            QObject::tr("Continue"),
            QObject::tr("Exit"), 1);
        if (1 == val)
            return 0;
    }

    REG_KEY("qt", "DELETE", "Delete", "D");
    REG_KEY("qt", "EDIT", "Edit", "E");

    DialogBox *dia = NULL;
    bool haveProblems = false;
    do
    {
        // Let the user select buttons, type values, scan for channels, et c.
        SetupMenu();

        // Look for common problems
        QString *problems = new QString("");
        haveProblems = CheckSetup(problems);

        if (haveProblems)
        {
            QString prompt;

            if (problems->contains("\n") > 1)
                prompt = QObject::tr("Do you want to fix these problems?");
            else
                prompt = QObject::tr("Do you want to fix this problem?");

            dia = new DialogBox(mainWindow, problems->append("\n" + prompt));
            dia->AddButton(QObject::tr("Yes please"));
            dia->AddButton(QObject::tr("No, I know what I am doing"));

            if (dia->exec() == 2)
                haveProblems = false;
            delete dia;
        }

        delete problems;

    // Execute UI again until there are no more problems:
    }
    while (haveProblems);

    dia = new DialogBox(mainWindow,
                        QObject::tr("If this is the master backend server, "
                                    "please run 'mythfilldatabase' "
                                    "to populate the database "
                                    "with channel information."));
    dia->AddButton(QObject::tr("OK"));
    dia->exec();
    delete dia;

    return 0;
}

bool is_backend_running(void)
{
    QString tmp_file = "/tmp/backendrunning";
    myth_system("ps -ae | grep mythbackend > " + tmp_file,
                MYTH_SYSTEM_DONT_BLOCK_LIRC |
                MYTH_SYSTEM_DONT_BLOCK_JOYSTICK_MENU);

    FILE *fptr = fopen(tmp_file, "r");
    if (!fptr)
        return false;
    char buf[1024];
    int read_bytes = fread(buf, 1, 1024, fptr);
    fclose(fptr);
    unlink(tmp_file);

    return read_bytes != 0;
}
