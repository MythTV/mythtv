#include <qapplication.h>
#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qstring.h>
#include <qdir.h>
#include <qfile.h>
#include <qstringlist.h>
#include <qregexp.h>
#include <qmap.h>

#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <cstdlib>
#include <sys/types.h>

#include <iostream>

#include "mythconfig.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "langsettings.h"
#include "dialogbox.h"
#include "exitcodes.h"
#include "util.h"
#include "storagegroup.h"
#include "myththemedmenu.h"
#include "myththemebase.h"
#include "mythuihelper.h"
#include "mythdirs.h"

#include "libmythtv/dbcheck.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "libmythtv/remoteutil.h"
#include "backendsettings.h"
#include "checksetup.h"

using namespace std;

void SetupMenuCallback ( void* data, QString& selection )
{
    (void)data;

    QString sel = selection.lower();

    if ( sel == "general" )
    {
        BackendSettings be;
        be.exec();
    }
    else if ( sel == "capture cards" )
    {
        CaptureCardEditor cce;
        cce.exec();
    }
    else if ( sel == "video sources" )
    {
        VideoSourceEditor vse;
        vse.exec();
    }
    else if ( sel == "card inputs" )
    {
        CardInputEditor cie;
        cie.exec();
    }
    else if ( sel == "channel editor" )
    {
        ChannelEditor ce;
        ce.exec();
    }
    else if ( sel == "storage groups" )
    {
        StorageGroupListEditor sge;
        sge.exec();
    }
}

void SetupMenu(MythMainWindow *win) 
{
    QString theme = gContext->GetSetting("Theme", "blue");

    MythThemedMenu* menu = new MythThemedMenu(GetMythUI()->FindThemeDir(theme),
                                              "setup.xml", win->GetMainStack(),
                                              "mainmenu", false);

    menu->setCallback(SetupMenuCallback, gContext);
    menu->setKillable();

    if ( menu->foundTheme() )
    {
        win->GetMainStack()->AddScreen(menu);
        qApp->setMainWidget(win);
        qApp->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << (const char *)theme << endl;
    }
}

int main(int argc, char *argv[])
{
    QString geometry = QString::null;
    QString display  = QString::null;
    QString verboseString = QString(" important general");

#ifdef USING_X11
    // Remember any -display or -geometry argument
    // which QApplication init will remove.
    for(int argpos = 1; argpos + 1 < argc; ++argpos)
    {
        if (!strcmp(argv[argpos],"-geometry"))
            geometry = argv[argpos+1];
        else if (!strcmp(argv[argpos],"-display"))
            display = argv[argpos+1];
    }
#endif

#ifdef Q_WS_MACX
    // Without this, we can't set focus to any of the CheckBoxSetting, and most
    // of the MythPushButton widgets, and they don't use the themed background.
    QApplication::setDesktopSettingsAware(FALSE);
#endif
    QApplication a(argc, argv);

    QMap<QString, QString> settingsOverride;

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-display") ||
            !strcmp(a.argv()[argpos],"--display"))
        {
            if (a.argc()-1 > argpos)
            {
                display = a.argv()[argpos+1];
                if (display.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -display option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }
                else
                    ++argpos;
            }
            else
            {
                cerr << "Missing argument to -display option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"-geometry") ||
                 !strcmp(a.argv()[argpos],"--geometry"))      
        {
            if (a.argc()-1 > argpos)
            {
                geometry = a.argv()[argpos+1];
                if (geometry.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to "
                        "-geometry option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                }                      
                else
                    ++argpos;
            }            
            else
            {            
                cerr << "Missing argument to -geometry option\n"; 
                return BACKEND_EXIT_INVALID_CMDLINE;
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
        else if (!strcmp(a.argv()[argpos],"-O") ||
                 !strcmp(a.argv()[argpos],"--override-setting"))
        {
            if (a.argc()-1 > argpos)
            {
                QString tmpArg = a.argv()[argpos+1];
                if (tmpArg.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -O/--override-setting option\n";
                    return BACKEND_EXIT_INVALID_CMDLINE;
                } 
 
                QStringList pairs = QStringList::split(",", tmpArg);
                for (int index = 0; index < pairs.size(); ++index)
                {
                    QStringList tokens = QStringList::split("=", pairs[index]);
                    tokens[0].replace(QRegExp("^[\"']"), "");
                    tokens[0].replace(QRegExp("[\"']$"), "");
                    tokens[1].replace(QRegExp("^[\"']"), "");
                    tokens[1].replace(QRegExp("[\"']$"), "");
                    settingsOverride[tokens[0]] = tokens[1];
                }
            }
            else
            {
                cerr << "Invalid or missing argument to -O/--override-setting option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            }

            ++argpos;
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help") ||
                !strcmp(a.argv()[argpos],"--usage")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;

            cerr << "Valid options are: "<<endl
#ifdef USING_X11 
                 <<"-display X-server              Create GUI on X-server, not localhost"<<endl
#endif          
                 <<"-geometry or --geometry WxH    Override window size settings"<<endl
                 <<"-geometry WxH+X+Y              Override window size and position"<<endl
                 <<"-O or "<<endl
                 <<"  --override-setting KEY=VALUE Force the setting named 'KEY' to value 'VALUE'"<<endl
                 <<"-v or --verbose debug-level    Use '-v help' for level info"<<endl 
                 <<endl;
            return -1;
        }
    }

    if (!display.isEmpty())
    {
        MythUIHelper::SetX11Display(display);
    }

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);

    if (!gContext->Init(true))
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        delete gContext;
        return GENERIC_EXIT_NO_MYTHCONTEXT;
    }

    if (geometry != "")
        if (!GetMythUI()->ParseGeometryOverride(geometry))
            cerr << "Illegal -geometry argument '"
                 << (const char *)geometry << "' (ignored)\n";

    if (settingsOverride.size())
    {
        QMap<QString, QString>::iterator it;
        for (it = settingsOverride.begin(); it != settingsOverride.end(); ++it)
        {
            VERBOSE(VB_IMPORTANT, QString("Setting '%1' being forced to '%2'")
                                          .arg(it.key()).arg(it.data()));
            gContext->OverrideSettingForSession(it.key(), it.data());
        }
    }   

    if (!MSqlQuery::testDBConnection())
    {
        cerr << "Unable to open database.\n";
        //     << "Driver error was:" << endl
        //     << db->lastError().driverText() << endl
        //     << "Database error was:" << endl
        //     << db->lastError().databaseText() << endl;

        delete gContext;
        return GENERIC_EXIT_DB_ERROR;
    }

    if (!UpgradeTVDatabaseSchema(true))
    {
        VERBOSE(VB_IMPORTANT, "Couldn't upgrade database to new schema.");
        delete gContext;
        return BACKEND_EXIT_DB_OUTOFDATE;
    }

    gContext->SetSetting("Theme", "G.A.N.T");
    GetMythUI()->LoadQtConfig();

    QString fileprefix = GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    MythMainWindow *mainWindow = GetMythMainWindow();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);
    mainWindow->setWindowTitle(QObject::tr("MythTV Setup"));

    MythThemeBase *themeBase = new MythThemeBase();
    GetMythUI()->UpdateImageCache();
    (void) themeBase;

    LanguageSettings::prompt();
    LanguageSettings::load("mythfrontend");

    QString warn = 
        QObject::tr("WARNING") + ": " +
        QObject::tr("MythTV has detected that the backend is running.")+"\n\n"+
        QObject::tr("Changing existing card inputs, deleting anything, "
                    "or scanning for channels may not work.");

    bool backendIsRunning = gContext->BackendIsRunning();

    if (backendIsRunning)
    {
        DialogCode val = MythPopupBox::Show2ButtonPopup(
            mainWindow, QObject::tr("WARNING"), warn,
            QObject::tr("Continue"),
            QObject::tr("Exit"), kDialogCodeButton0);

        if (kDialogCodeButton1 == val)
        {
            delete gContext;
            return GENERIC_EXIT_OK;
        }
    }

    REG_KEY("qt", "DELETE", "Delete", "D");
    REG_KEY("qt", "EDIT", "Edit", "E");

    DialogBox *dia = NULL;
    bool haveProblems = false;
    do
    {
        // Let the user select buttons, type values, scan for channels, etc.
        SetupMenu(mainWindow);

        // Look for common problems
        QString *problems = new QString("");
        haveProblems = CheckSetup(problems);

        if (haveProblems)
        {
            QString prompt;

            if (problems->count("\n") > 1)
                prompt = QObject::tr("Do you want to fix these problems?");
            else
                prompt = QObject::tr("Do you want to fix this problem?");

            dia = new DialogBox(mainWindow, problems->append("\n" + prompt));
            dia->AddButton(QObject::tr("Yes please"));
            dia->AddButton(QObject::tr("No, I know what I am doing"));

            if (kDialogCodeButton1 == dia->exec())
                haveProblems = false;
            dia->deleteLater();
        }

        delete problems;

    // Execute UI again until there are no more problems:
    }
    while (haveProblems);

    if (gContext->IsMasterHost())
    {
        dia = new DialogBox(mainWindow,
                            QObject::tr("If this is the master backend server, "
                                        "please run 'mythfilldatabase' "
                                        "to populate the database "
                                        "with channel information."));
        dia->AddButton(QObject::tr("OK"));
        dia->exec();
        dia->deleteLater();
    }

    if (backendIsRunning)
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");

    delete gContext;

    return GENERIC_EXIT_OK;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
