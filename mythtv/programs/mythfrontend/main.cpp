#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <qdir.h>
#include <qtextcodec.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "progfind.h"
#include "manualbox.h"
#include "manualschedule.h"
#include "playbackbox.h"
#include "viewscheduled.h"
#include "rankprograms.h"
#include "rankchannels.h"
#include "globalsettings.h"
#include "recordingprofile.h"

#include "themedmenu.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "dialogbox.h"
#include "guidegrid.h"
#include "mythplugin.h"
#include "remoteutil.h"
#include "xbox.h"
#include "dbcheck.h"

#define QUIT     1
#define HALTWKUP 2
#define HALT     3

ThemedMenu *menu;
MythContext *gContext;
XBox *xbox = NULL;

QString startGuide(void)
{
    QString startchannel = gContext->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    return RunProgramGuide(startchannel);
}

int startManaged(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ViewScheduled vsb(db, gContext->GetMainWindow(), "view scheduled");

    qApp->unlock();
    vsb.exec();
    qApp->lock();

    return 0;
}

int startProgramRankings(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    RankPrograms rsb(db, gContext->GetMainWindow(), "rank scheduled");

    qApp->unlock();
    rsb.exec();
    qApp->lock();

    return 0;
}

int startChannelRankings(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    RankChannels rch(db, gContext->GetMainWindow(), "rank channels");

    qApp->unlock();
    rch.exec();
    qApp->lock();

    return 0;
}

int startPlayback(void)
{
    PlaybackBox pbb(PlaybackBox::Play, gContext->GetMainWindow(), "play");

    qApp->unlock();
    pbb.exec();
    qApp->lock();

    return 0;
}

int startDelete(void)
{
    PlaybackBox delbox(PlaybackBox::Delete, gContext->GetMainWindow(), 
                       "delete");
   
    qApp->unlock();
    delbox.exec();
    qApp->lock();

    return 0;
}

int startManual(void)
{
    ManualBox manbox(gContext->GetMainWindow(), "manual box");

    qApp->unlock();
    manbox.exec();
    qApp->lock();

    return 0;
}

int startManualSchedule(void)
{
    ManualSchedule mansched(gContext->GetMainWindow(), "manual schedule");

    qApp->unlock();
    mansched.exec();
    qApp->lock();

    return 0;
}

void startTV(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    TV *tv = new TV(db);

    QDateTime timeout;

    tv->Init();

    bool tryTV = false;
    bool tryRecorder = false;
    bool quitAll = false;

    bool showDialogs = true;
    if (!tv->LiveTV(showDialogs))
        quitAll = true;

    while (!quitAll)
    {
        timeout = QDateTime::currentDateTime().addSecs(2);
        while (tryRecorder)
        { //try to play recording, if filenotfound retry until timeout

            if (tv->PlayFromRecorder(tv->GetLastRecorderNum()) == 1)
                tryRecorder = false;
            else if (QDateTime::currentDateTime().secsTo(timeout) <= 0)
            {
                tryRecorder = false;
                tryTV = true;
            }
            else
            {
                qApp->unlock();
                qApp->processEvents();
                usleep(100000);
                qApp->lock();
             }
        }

        timeout = QDateTime::currentDateTime().addSecs(2);
        while (tryTV)
        {//try to start livetv
            bool showDialogs = false;
            if (tv->LiveTV(showDialogs) == 1) //1 == livetv ok
                tryTV = false;
            else if (QDateTime::currentDateTime().secsTo(timeout) <= 0)
            {
                tryTV = false;
                quitAll = true;
            }
            else
            {
                qApp->unlock();
                qApp->processEvents();
                usleep(100000);
                qApp->lock();
            }
        }

        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(100);
            qApp->lock();
        }

        if (tv->WantsToQuit())
            quitAll = true;
        else
        {
            tryRecorder = true;
            tryTV = false;
        }
    }

    delete tv;
}

void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel == "tv_watch_live")
        startTV();
    else if (sel == "tv_watch_recording")
        startPlayback();
    else if (sel == "tv_schedule")
        startGuide();
    else if (sel == "tv_delete")
        startDelete();
    else if (sel == "tv_manual")
        startManual();
    else if (sel == "tv_manualschedule")
        startManualSchedule();
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "tv_set_rankings")
        startProgramRankings();
    else if (sel == "tv_progfind")
        RunProgramFind();
    else if (sel == "settings appearance") 
    {
        AppearanceSettings settings;
        settings.exec(QSqlDatabase::database());
        gContext->LoadQtConfig();
        gContext->GetMainWindow()->Init();
        gContext->UpdateImageCache();
        menu->ReloadTheme();
    } 
    else if (sel == "settings recording") 
    {
        RecordingProfileEditor editor(QSqlDatabase::database());
        editor.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings general") 
    {
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings maingeneral") 
    {
        MainGeneralSettings mainsettings;
        mainsettings.exec(QSqlDatabase::database());
        menu->ReloadExitKey();
    } 
    else if (sel == "settings playback") 
    {
        PlaybackSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings epg") 
    {
        EPGSettings settings;
        settings.exec(QSqlDatabase::database());
    } 
    else if (sel == "settings generalranking") 
    {
        GeneralRankingSettings settings;
        settings.exec(QSqlDatabase::database());
        ScheduledRecording::signalChange(QSqlDatabase::database());
    } 
    else if (sel == "settings channelranking") 
    {
        startChannelRankings();
    }
    else if (xbox && sel == "settings xboxsettings")
    {
        XboxSettings settings;
        settings.exec(QSqlDatabase::database());

        xbox->GetSettings();
    }
}

int handleExit(void)
{
    QString title = QObject::tr("Do you really want to exit MythTV?");

    DialogBox diag(gContext->GetMainWindow(), title);
    diag.AddButton(QObject::tr("No"));
    diag.AddButton(QObject::tr("Yes, Exit now"));
    diag.AddButton(QObject::tr("Yes, Exit and shutdown the computer"));
//    bool haltNwakeup = context->GetNumSetting("UseHaltWakeup");
//    if (haltNwakeup)
//        diag.AddButton("Yes, Shutdown and set wakeup time");

    int result = diag.exec();
    switch (result)
    {
        case 2: return QUIT;
        case 3: return HALT;
//        case 4: return HALTWKUP;
        default: return 0;
    }

    return 0;
}

void haltnow(int how)
{
    QString halt_cmd = gContext->GetSetting("HaltCommand", "halt");
    QString haltwkup_cmd = gContext->GetSetting("HaltCommandWhenWakeup",
                                                "echo 'would halt now, and set "
                                                "wakeup time to %1 (secs since "
                                                "1970) if command was set'");

    if (how == HALTWKUP)
    {
    }
    else if (how == HALT)
        system(halt_cmd.ascii());
}

int RunMenu(QString themedir)
{
    menu = new ThemedMenu(themedir.ascii(), "mainmenu.xml", 
                          gContext->GetMainWindow(), "mainmenu");
    menu->setCallback(TVMenuCallback, gContext);
   
    int exitstatus = 0;
 
    if (menu->foundTheme())
    {
        do {
            menu->exec();
        } while (!(exitstatus = handleExit()));
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    return exitstatus;
}   

void WriteDefaults(QSqlDatabase* db) {
    // If any settings are missing from the database, this will write
    // the default values
    PlaybackSettings ps;
    ps.load(db);
    ps.save(db);
    GeneralSettings gs;
    gs.load(db);
    gs.save(db);
    EPGSettings es;
    es.load(db);
    es.save(db);
    AppearanceSettings as;
    as.load(db);
    as.save(db);
    MainGeneralSettings mgs;
    mgs.load(db);
    mgs.save(db);
    GeneralRankingSettings grs;
    grs.load(db);
    grs.save(db);
}

QString RandTheme(QString &themename, QSqlDatabase *db)
{
    QDir themes(PREFIX"/share/mythtv/themes");
    themes.setFilter(QDir::Dirs);

    const QFileInfoList *fil = themes.entryInfoList(QDir::Dirs);

    QFileInfoListIterator it( *fil);
    QFileInfo *theme;
    QStringList themelist;

    srand(time(NULL));

    for ( ; it.current() !=0; ++it)
    {
        theme = it.current();
        if (theme->fileName() == "." || theme->fileName() =="..")
            continue;

        QFileInfo preview(theme->absFilePath() + "/preview.jpg");
        QFileInfo xml(theme->absFilePath() + "/theme.xml");

        if (!preview.exists() || !xml.exists())
            continue;

        // We don't want the same one as last time.
        if (theme->fileName() != themename)
            themelist.append(theme->fileName());
    }

    themename = themelist[rand() % themelist.size()];

    ThemeSelector Theme;
    Theme.setValue(themename);
    Theme.save(db);

    return themename;
}

int main(int argc, char **argv)
{
    QString lcd_host;
    int lcd_port;

    QApplication a(argc, argv);
    QTranslator translator( 0 );

    QString logfile = "";
    QString verboseString = QString(" important general");

    QString pluginname = "";

    QString binname = basename(a.argv()[0]);

    if (binname != "mythfrontend")
        pluginname = binname;

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
        {
            if (a.argc() > argpos)
            {
                logfile = a.argv()[argpos+1];
                if (logfile.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -l/--logfile option\n";
                    return -1;
                }
                else
                {
                    ++argpos;
                }
            }
        } else if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc() > argpos)
            {
                QString temp = a.argv()[argpos+1];
                if (temp.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -v/--verbose option\n";
                    return -1;
                } else
                {
                    QStringList verboseOpts;
                    verboseOpts = QStringList::split(',',a.argv()[argpos+1]);
                    ++argpos;
                    for (QStringList::Iterator it = verboseOpts.begin(); 
                         it != verboseOpts.end(); ++it )
                    {
                        if(!strcmp(*it,"none"))
                        {
                            print_verbose_messages = VB_NONE;
                            verboseString = "";
                        }
                        else if(!strcmp(*it,"all"))
                        {
                            print_verbose_messages = VB_ALL;
                            verboseString = "all";
                        }
                        else if(!strcmp(*it,"quiet"))
                        {
                            print_verbose_messages = VB_IMPORTANT;
                            verboseString = "important";
                        }
                        else if(!strcmp(*it,"record"))
                        {
                            print_verbose_messages |= VB_RECORD;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"playback"))
                        {
                            print_verbose_messages |= VB_PLAYBACK;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"channel"))
                        {
                            print_verbose_messages |= VB_CHANNEL;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"osd"))
                        {
                            print_verbose_messages |= VB_OSD;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"file"))
                        {
                            print_verbose_messages |= VB_FILE;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"schedule"))
                        {
                            print_verbose_messages |= VB_SCHEDULE;
                            verboseString += " " + *it;
                        }
                        else if(!strcmp(*it,"network"))
                        {
                            print_verbose_messages |= VB_NETWORK;
                            verboseString += " " + *it;
                        }
                        else
                        {
                            cerr << "Unknown argument for -v/--verbose: "
                                 << *it << endl;;
                        }
                    }
                }
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return -1;
            }
        } else if ((argpos + 1 == a.argc()) &&
                    !QString(a.argv()[argpos]).startsWith("-"))
        {
            pluginname = a.argv()[argpos];
        } 
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: " << endl <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-v or --verbose debug-level    Prints more information" << endl <<
                    "                               Accepts any combination (separated by comma)" << endl << 
                    "                               of all,none,quiet,record,playback," << endl <<
                    "                               channel,osd,file,schedule,network" << endl <<
                    "<plugin>                       Initialize and run this plugin" << endl;
            return -1;
        }
    }

    int logfd = -1;

    if (logfile != "")
    {
        logfd = open(logfile.ascii(), O_WRONLY|O_CREAT|O_APPEND, 0664);
         
        if (logfd < 0)
        {
            perror("open(logfile)");
            return -1;
        }
    }
    
    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    gContext = new MythContext(MYTH_BINARY_VERSION);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!gContext->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    UpgradeTVDatabaseSchema();

    VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

    MythPluginManager::init();

    translator.load(PREFIX + QString("/share/mythtv/i18n/mythfrontend_") + 
                    QString(gContext->GetSetting("Language").lower()) + 
                    QString(".qm"), ".");
    a.installTranslator(&translator);

    WriteDefaults(db);

    QString themename = gContext->GetSetting("Theme", "blue");
    bool randomtheme = gContext->GetNumSetting("RandomTheme", 0);

    if (randomtheme)
        themename = RandTheme(themename, db);

    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    mainWindow->Show();
    mainWindow->Init();
    gContext->SetMainWindow(mainWindow);

    gContext->UpdateImageCache();

    lcd_host = gContext->GetSetting("LCDHost");
    lcd_port = gContext->GetNumSetting("LCDPort");

    if (lcd_host.length() > 0 && lcd_port > 1024 && gContext->GetLCDDevice())
    {
        gContext->GetLCDDevice()->connectToHost(lcd_host, lcd_port);
        gContext->GetLCDDevice()->setupLEDs(RemoteGetRecordingMask);
    }

    if (gContext->GetNumSetting("EnableXbox") == 1)
    {
        xbox = new XBox();
        xbox->GetSettings();
    }
    else
        xbox = NULL;

    if (pluginname != "")
    {
        if (MythPluginManager::init_plugin(pluginname))
        {
            MythPluginManager::run_plugin(pluginname);
            return 0;
        }
        else
        {
            pluginname = "myth" + pluginname;
            if (MythPluginManager::init_plugin(pluginname))
            {
                MythPluginManager::run_plugin(pluginname);
                return 0;
            }
        }
    }            

    qApp->lock();
    qApp->unlock();

    int exitstatus = RunMenu(themedir);

    haltnow(exitstatus);

    delete gContext;
    return exitstatus;
}
