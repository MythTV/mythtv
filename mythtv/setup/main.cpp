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
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "libmyth/themedmenu.h"
#include "backendsettings.h"

#include "libmythtv/dbcheck.h"

using namespace std;

void clearCardDB(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("TRUNCATE TABLE capturecard;");
    query.exec("TRUNCATE TABLE cardinput;");
}

void clearAllDB(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.exec("TRUNCATE TABLE channel;");
    query.exec("TRUNCATE TABLE program;");
    query.exec("TRUNCATE TABLE videosource;");
    query.exec("TRUNCATE TABLE credits;");
    query.exec("TRUNCATE TABLE programrating;");
    query.exec("TRUNCATE TABLE programgenres;");
    query.exec("TRUNCATE TABLE dtv_multiplex;");
    query.exec("TRUNCATE TABLE cardinput;");
}

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
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||    
                !strcmp(a.argv()[argpos],"--help") ||
                !strcmp(a.argv()[argpos],"--usage")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: \n" << 
#ifdef Q_WS_X11 
                    "-display X-server     Create GUI on X-server, not localhost\n" <<
#endif          
                    "--geometry WxH        Override window size settings\n" <<
                    "-geometry WxH+X+Y     Override window size and position\n";
            return -1;
        }
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (geometry != "")
        if (!gContext->ParseGeometryOverride(geometry))
            cerr << "Illegal -geometry argument '"
                 << geometry << "' (ignored)\n";

    if (!gContext->Init(true))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return -1;
    }

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

    LanguageSettings::load("mythfrontend");

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

    DialogBox dboxCard(mainWindow, QObject::tr("Would you like to clear all "
                                   "capture card settings before starting "
                                   "configuration?"));
    dboxCard.AddButton(QObject::tr("No, leave my card settings alone"));
    dboxCard.AddButton(QObject::tr("Yes, delete my card settings"));
    if (dboxCard.exec() == 2)
        clearCardDB();
    
    // Give the user time to realize the first dialog is gone
    // before we bring up a similar-looking one
    usleep(750000);
    
    DialogBox dboxProg(mainWindow, QObject::tr("Would you like to clear all "
                                   "program data and channel settings before "
                                   "starting configuration? This will not "
                                   "affect any existing recordings."));
    dboxProg.AddButton(QObject::tr("No, leave my channel settings alone"));
    dboxProg.AddButton(QObject::tr("Yes, delete my channel settings"));
    if (dboxProg.exec() == 2)
        clearAllDB();

    REG_KEY("qt", "DELETE", "Delete", "D");
    REG_KEY("qt", "EDIT", "Edit", "E");

    SetupMenu();

    cout << "If this is the master backend server:\n";
    cout << "Now, please run 'mythfilldatabase' to populate the database\n";
    cout << "with channel information.\n";
    cout << endl;

    return 0;
}
