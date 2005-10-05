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

#include "libmythtv/dbcheck.h"
#include "libmythtv/videosource.h"
#include "libmythtv/channeleditor.h"
#include "backendsettings.h"
#include "checksetup.h"

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
        else if ((!strcmp(a.argv()[argpos],"-v") ||
                  !strcmp(a.argv()[argpos],"--verbose")) && (a.argc() <= argpos))
        {
            cerr << "Missing argument to -v/--verbose option\n";
            return BACKEND_EXIT_INVALID_CMDLINE;
        }
        else if ((!strcmp(a.argv()[argpos],"-v") ||
                  !strcmp(a.argv()[argpos],"--verbose")) && (a.argc() > argpos))
        {
            QString temp = a.argv()[argpos+1];
            if (temp.startsWith("-"))
            {
                cerr << "Invalid or missing argument to -v/--verbose option\n";
                return BACKEND_EXIT_INVALID_CMDLINE;
            } 
            else
            {
                QStringList verboseOpts;
                verboseOpts = QStringList::split(',',a.argv()[argpos+1]);
                ++argpos;
                for (QStringList::Iterator it = verboseOpts.begin(); 
                     it != verboseOpts.end(); ++it )
                {
                    if (!strcmp(*it,"none"))
                    {
                        print_verbose_messages = VB_NONE;
                        verboseString = "";
                    }
                    else if (!strcmp(*it,"all"))
                    {
                        print_verbose_messages = VB_ALL;
                        verboseString = "all";
                    }
                    else if (!strcmp(*it,"quiet"))
                    {
                        print_verbose_messages = VB_IMPORTANT;
                        verboseString = "important";
                    }
                    else if (!strcmp(*it,"record"))
                    {
                        print_verbose_messages |= VB_RECORD;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"playback"))
                    {
                        print_verbose_messages |= VB_PLAYBACK;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"channel"))
                    {
                        print_verbose_messages |= VB_CHANNEL;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"osd"))
                    {
                        print_verbose_messages |= VB_OSD;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"file"))
                    {
                        print_verbose_messages |= VB_FILE;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"schedule"))
                    {
                        print_verbose_messages |= VB_SCHEDULE;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"network"))
                    {
                        print_verbose_messages |= VB_NETWORK;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"commflag"))
                    {
                        print_verbose_messages |= VB_COMMFLAG;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"jobqueue"))
                    {
                        print_verbose_messages |= VB_JOBQUEUE;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"audio"))
                    {
                        print_verbose_messages |= VB_AUDIO;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"libav"))
                    {
                        print_verbose_messages |= VB_LIBAV;
                        verboseString += " " + *it;
                    }
                    else if (!strcmp(*it,"siparser"))
                    {
                        print_verbose_messages |= VB_SIPARSER;
                        verboseString += " " + *it;
                    }
                    else
                    {
                        cerr << "Unknown argument for -v/--verbose: "
                             << *it << endl;;
                    }
                }
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
                 <<"-v or --verbose debug-level    Prints more information\n"
                 <<"                               Accepts any combination (separated by comma)\n"
                 <<"                               of all,none,quiet,record,playback,channel,\n"
                 <<"                               osd,file,schedule,network,commflag,audio,\n"
                 <<"                               libav,jobqueue"<<endl;
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

    ClearDialog clr;
    if (clr.exec() == QDialog::Accepted)
    {
        if (clr.IsClearCardsSet())
            clearCardDB();
        if (clr.IsClearChannelsSet())
            clearAllDB();
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
