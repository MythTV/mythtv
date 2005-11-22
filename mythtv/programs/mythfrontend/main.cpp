#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qmap.h>
#include <unistd.h>
#include <qdir.h>
#include <qtextcodec.h>
#include <qwidget.h>
#include <fcntl.h>
#include <signal.h>

#include <iostream>
using namespace std;

#include "mythconfig.h"
#include "tv.h"
#include "proglist.h"
#include "progfind.h"
#include "manualbox.h"
#include "manualschedule.h"
#include "playbackbox.h"
#include "previouslist.h"
#include "customrecord.h"
#include "viewscheduled.h"
#include "programrecpriority.h"
#include "channelrecpriority.h"
#include "globalsettings.h"
#include "profilegroup.h"
#include "playgroup.h"

#include "exitcodes.h"
#include "themedmenu.h"
#include "programinfo.h"
#include "mythcontext.h"
#include "mythdbcon.h"
#include "dialogbox.h"
#include "guidegrid.h"
#include "mythplugin.h"
#include "remoteutil.h"
#include "xbox.h"
#include "dbcheck.h"
#include "mythmediamonitor.h"
#include "statusbox.h"
#include "lcddevice.h"
#include "langsettings.h"
#include "livetvchain.h"

#define NO_EXIT  0
#define QUIT     1
#define HALT     2

ThemedMenu *menu;
XBox *xbox = NULL;

void startGuide(void)
{
    uint chanid = 0;
    QString channum = gContext->GetSetting("DefaultTVChannel");
    channum = (channum.isEmpty()) ? "3" : channum;
    RunProgramGuide(chanid, channum);
}

void startFinder(void)
{
    RunProgramFind();
}

void startSearchTitle(void)
{
    ProgLister searchTitle(plTitleSearch, "", "",
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchTitle.exec();
    qApp->lock();
}

void startSearchKeyword(void)
{
    ProgLister searchKeyword(plKeywordSearch, "", "",
                        gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchKeyword.exec();
    qApp->lock();
}

void startSearchPeople(void)
{
    ProgLister searchPeople(plPeopleSearch, "", "",
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchPeople.exec();
    qApp->lock();
}

void startSearchPower(void)
{
    ProgLister searchPower(plPowerSearch, "", "",
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchPower.exec();
    qApp->lock();
}

void startSearchChannel(void)
{
    ProgLister searchChannel(plChannel, "", "",
                             gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchChannel.exec();
    qApp->lock();
}

void startSearchCategory(void)
{
    ProgLister searchCategory(plCategory, "", "",
                            gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchCategory.exec();
    qApp->lock();
}

void startSearchMovie(void)
{
    ProgLister searchMovie(plMovies, "", "",
                           gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchMovie.exec();
    qApp->lock();
}

void startSearchNew(void)
{
    ProgLister searchNew(plNewListings, "", "",
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchNew.exec();
    qApp->lock();
}

void startSearchTime(void)
{
    ProgLister searchTime(plTime, "", "",
                         gContext->GetMainWindow(), "proglist");

    qApp->unlock();
    searchTime.exec();
    qApp->lock();
}

void startManaged(void)
{
    ViewScheduled vsb(gContext->GetMainWindow(), "view scheduled");

    qApp->unlock();
    vsb.exec();
    qApp->lock();
}

void startProgramRecPriorities(void)
{
    ProgramRecPriority rsb(gContext->GetMainWindow(), "recpri scheduled");

    qApp->unlock();
    rsb.exec();
    qApp->lock();
}

void startChannelRecPriorities(void)
{
    ChannelRecPriority rch(gContext->GetMainWindow(), "recpri channels");

    qApp->unlock();
    rch.exec();
    qApp->lock();
}

void startPlayback(void)
{
    PlaybackBox pbb(PlaybackBox::Play, gContext->GetMainWindow(), 
                    "tvplayselect");

    qApp->unlock();
    pbb.exec();
    qApp->lock();
}

void startDelete(void)
{
    PlaybackBox delbox(PlaybackBox::Delete, gContext->GetMainWindow(), 
                       "tvplayselect");
   
    qApp->unlock();
    delbox.exec();
    qApp->lock();
}

void startPrevious(void)
{
    PreviousList previous(gContext->GetMainWindow(), "previous list");

    qApp->unlock();
    previous.exec();
    qApp->lock();
}

void startCustomRecord(void)
{
    CustomRecord custom(gContext->GetMainWindow(), "custom record");

    qApp->unlock();
    custom.exec();
    qApp->lock();
}

void startManual(void)
{
    ManualBox manbox(gContext->GetMainWindow(), "manual box");

    qApp->unlock();
    manbox.exec();
    qApp->lock();
}

void startManualSchedule(void)
{
    ManualSchedule mansched(gContext->GetMainWindow(), "manual schedule");

    qApp->unlock();
    mansched.exec();
    qApp->lock();
}

void startTV(void)
{
    TV *tv = new TV();

    MythTimer timer;
    timer.start();

    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Experienced fatal error "
                "initializing TV class in startTV().");
        delete tv;
        return;
    }

    bool tryTV = false;
    bool tryRecorder = false;
    bool quitAll = false;
    bool showDialogs = true;

    LiveTVChain *tvchain = new LiveTVChain();
    QString chainid = tvchain->InitializeNewChain(gContext->GetHostName());

    if (!tv->LiveTV(tvchain, showDialogs))
    {
        tv->StopLiveTV();
        quitAll = true;
    }

    while (!quitAll)
    {
        timer.restart();
        while (tryRecorder)
        { //try to play recording, if filenotfound retry until timeout

            if (tv->PlayFromRecorder(tv->GetLastRecorderNum()) == 1)
                tryRecorder = false;
            else if (timer.elapsed() > 2000)
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

        timer.restart();
        while (tryTV)
        {//try to start livetv
            bool showDialogs = false;
            if (tv->LiveTV(tvchain, showDialogs) == 1) //1 == livetv ok
                tryTV = false;
            else if (timer.elapsed() > 2000)
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
            usleep(10000);
            qApp->lock();
        }

        if (tv->WantsToQuit() && !tv->IsSwitchingCards())
            quitAll = true;
        else
        {
            if (tv->IsSwitchingCards())
            {
                tryRecorder = false;
                tryTV = true;
            }
            else
            {
                tryRecorder = true;
                tryTV = false;
            }
        }
    }

    while (tv->IsMenuRunning()) 
    {
        qApp->unlock();
        qApp->processEvents();
        usleep(10000);
        qApp->lock();
    }

    delete tv;

    tvchain->DestroyChain();
    delete tvchain;
}

void showStatus(void)
{
    StatusBox statusbox(gContext->GetMainWindow(), "status box");
    if (statusbox.IsErrored())
    {
        MythPopupBox::showOkPopup(
            gContext->GetMainWindow(), QObject::tr("Theme Error"),
            QString(QObject::tr(
                        "Your theme does not contain elements required "
                        "to display the status screen.")));
    }
    else
    {
        qApp->unlock();
        statusbox.exec();
        qApp->lock();
    }
}

void TVMenuCallback(void *data, QString &selection)
{
    (void)data;
    QString sel = selection.lower();

    if (sel.left(9) == "settings ")
        gContext->ActivateSettingsCache(false);

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
    else if (sel == "tv_custom_record")
        startCustomRecord();
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "tv_set_recpriorities")
        startProgramRecPriorities();
    else if (sel == "tv_progfind")
        startFinder();
    else if (sel == "tv_search_title")
        startSearchTitle();
    else if (sel == "tv_search_keyword")
        startSearchKeyword();
    else if (sel == "tv_search_people")
        startSearchPeople();
    else if (sel == "tv_search_power")
        startSearchPower();
    else if (sel == "tv_search_channel")
        startSearchChannel();
    else if (sel == "tv_search_category")
        startSearchCategory();
    else if (sel == "tv_search_movie")
        startSearchMovie();
    else if (sel == "tv_search_new")
        startSearchNew();
    else if (sel == "tv_search_time")
        startSearchTime();
    else if (sel == "tv_previous")
        startPrevious();
    else if (sel == "settings appearance") 
    {
        AppearanceSettings settings;
        settings.exec();

        LanguageSettings::reload();

        gContext->LoadQtConfig();
        gContext->GetMainWindow()->Init();
        gContext->UpdateImageCache();
        menu->ReloadTheme();
        if (!menu->foundTheme())
            exit(FRONTEND_BUGGY_EXIT_NO_THEME);

        LCD::SetupLCD ();
        if (class LCD * lcd = LCD::Get()) {
            lcd->setupLEDs(RemoteGetRecordingMask);
            lcd->resetServer();
        }
    } 
    else if (sel == "settings recording") 
    {
        ProfileGroupEditor editor;
        editor.exec();
    } 
    else if (sel == "settings playgroup") 
    {
        PlayGroupEditor editor;
        editor.exec();
    } 
    else if (sel == "settings general") 
    {
        GeneralSettings settings;
        settings.exec();
    } 
    else if (sel == "settings maingeneral") 
    {
        MainGeneralSettings mainsettings;
        mainsettings.exec();
        menu->ReloadExitKey();
        QStringList strlist = QString("REFRESH_BACKEND");
        gContext->SendReceiveStringList(strlist);
    } 
    else if (sel == "settings playback") 
    {
        PlaybackSettings settings;
        settings.exec();
    } 
    else if (sel == "settings epg") 
    {
        EPGSettings settings;
        settings.exec();
    } 
    else if (sel == "settings generalrecpriorities") 
    {
        GeneralRecPrioritiesSettings settings;
        settings.exec();
        ScheduledRecording::signalChange(0);
    } 
    else if (sel == "settings channelrecpriorities") 
    {
        startChannelRecPriorities();
    }
    else if (xbox && sel == "settings xboxsettings")
    {
        XboxSettings settings;
        settings.exec();

        xbox->GetSettings();
    }
    else if (sel == "tv_status")
        showStatus();

    if (sel.left(9) == "settings ")
    {
        gContext->ActivateSettingsCache(true);
        RemoteSendMessage("CLEAR_SETTINGS_CACHE");
    }
}

int handleExit(void)
{
    if (gContext->GetNumSetting("NoPromptOnExit", 1) == 0)
        return QUIT;

    // first of all find out, if a backend runs on this host...
    bool backendOnLocalhost = false;

    QStringList strlist = "QUERY_IS_ACTIVE_BACKEND";
    strlist << gContext->GetHostName();

    gContext->SendReceiveStringList(strlist);

    if (QString(strlist[0]) == "FALSE")
        backendOnLocalhost = false;
    else //if (QString(strlist[0]) == "TRUE")
        backendOnLocalhost = true;

    QString title = QObject::tr("Do you really want to exit MythTV?");

    DialogBox diag(gContext->GetMainWindow(), title);
    diag.AddButton(QObject::tr("No"));
    diag.AddButton(QObject::tr("Yes, Exit now"));
    if (!backendOnLocalhost)
        diag.AddButton(QObject::tr("Yes, Exit and Shutdown"));

    int result = diag.exec();
    switch (result)
    {
        case 1: return NO_EXIT;
        case 2: return QUIT;
        case 3: return HALT;
        default: return NO_EXIT;
    }
}

void haltnow()
{
    QString halt_cmd = gContext->GetSetting("HaltCommand", 
                                            "sudo /sbin/halt -p");
    if (!halt_cmd.isEmpty())
        system(halt_cmd.ascii());
}

int RunMenu(QString themedir)
{
    menu = new ThemedMenu(themedir.ascii(), "mainmenu.xml", 
                          gContext->GetMainWindow(), "mainmenu");
    menu->setCallback(TVMenuCallback, gContext);
   
    int exitstatus = NO_EXIT;
 
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

    delete menu;
    menu = NULL;
    return exitstatus;
}   

// If any settings are missing from the database, this will write
// the default values
void WriteDefaults() 
{
    PlaybackSettings ps;
    ps.load();
    ps.save();
    GeneralSettings gs;
    gs.load();
    gs.save();
    EPGSettings es;
    es.load();
    es.save();
    AppearanceSettings as;
    as.load();
    as.save();
    MainGeneralSettings mgs;
    mgs.load();
    mgs.save();
    GeneralRecPrioritiesSettings grs;
    grs.load();
    grs.save();
}

QString RandTheme(QString &themename)
{
    QDir themes(gContext->GetThemesParentDir());
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

    if (themelist.size()) 
        themename = themelist[rand() % themelist.size()];

    ThemeSelector Theme;
    Theme.setValue(themename);
    Theme.save();

    return themename;
}

int internal_play_media(const char *mrl, const char* plot, const char* title, 
                        const char* director, int lenMins, const char* year) 
{
    int res = -1; 
   
    TV *tv = new TV();

    if (!tv->Init())
    {
        VERBOSE(VB_IMPORTANT, "Experienced fatal error initializing "
                "TV class in internal_play_media().");
        delete tv;
        return res;
    }

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->recstartts = QDateTime::currentDateTime().addSecs((0 - (lenMins + 1)) * 60 );
    pginfo->recendts = QDateTime::currentDateTime().addSecs(-60);
    pginfo->recstartts.setDate(QDate::fromString(year));
    pginfo->lenMins = lenMins;
    pginfo->isVideo = true;
    pginfo->pathname = mrl;
    
    if (pginfo->pathname.find(".iso", 0, false) != -1)
    {
        pginfo->pathname = QString("dvd:%1").arg(mrl);
    }
    
    pginfo->description = plot;
    
    
    if (strlen(director))
        pginfo->subtitle = QString( "%1: %2" ).arg(QObject::tr("Directed By")).arg(director);
    
    pginfo->title = title;

        
    if (tv->Playback(pginfo))
    {
        
        while (tv->GetState() != kState_None)
        {
            qApp->unlock();
            qApp->processEvents();
            usleep(10000);
            qApp->lock();
            
        }
    }
    

    res = 0;
    
    sleep(1);
    delete tv;
    delete pginfo;
    
    return res;
}

void InitJumpPoints(void)
{
    REG_JUMP("Program Guide", "", "", startGuide);
    REG_JUMP("Program Finder", "", "", startFinder);
    //REG_JUMP("Search Listings", "", "", startSearch);
    REG_JUMP("Manage Recordings / Fix Conflicts", "", "", startManaged);
    REG_JUMP("Program Recording Priorities", "", "", startProgramRecPriorities);
    REG_JUMP("Channel Recording Priorities", "", "", startChannelRecPriorities);
    REG_JUMP("TV Recording Playback", "", "", startPlayback);
    REG_JUMP("TV Recording Deletion", "", "", startDelete);
    REG_JUMP("Live TV", "", "", startTV);
    REG_JUMP("Manual Record Scheduling", "", "", startManual);

    TV::InitKeys();
}

int internal_media_init() 
{
    REG_MEDIAPLAYER("Internal", "MythTV's native media player.", 
                    internal_play_media);
    return 0;
}

static void *run_priv_thread(void *data)
{
    (void)data;
    while (true) 
    {
        gContext->waitPrivRequest();
        
        for (MythPrivRequest req = gContext->popPrivRequest(); 
             true; req = gContext->popPrivRequest()) 
        {
            bool done = false;
            switch (req.getType()) 
            {
            case MythPrivRequest::MythRealtime:
                if (gContext->GetNumSetting("RealtimePriority", 1))
                {
                    pthread_t *target_thread = (pthread_t *)(req.getData());
                    // Raise the given thread to realtime priority
                    struct sched_param sp = {1};
                    if (target_thread)
                    {
                        int status = pthread_setschedparam(
                            *target_thread, SCHED_FIFO, &sp);
                        if (status) 
                        {
                            // perror("pthread_setschedparam");
                            VERBOSE(VB_GENERAL, "Realtime priority would require SUID as root.");
                        }
                        else
                            VERBOSE(VB_GENERAL, "Using realtime priority.");
                    }
                    else
                    {
                        VERBOSE(VB_IMPORTANT, "Unexpected NULL thread ptr "
                                "for MythPrivRequest::MythRealtime");
                    }
                }
                else
                    VERBOSE(VB_GENERAL, "The realtime priority setting is not enabled.");
                break;
            case MythPrivRequest::MythExit:
                pthread_exit(NULL);
                break;
            case MythPrivRequest::PrivEnd:
                done = true; // queue is empty
                break;
            }
            if (done)
                break; // from processing the current queue
        }
    }
    return NULL; // will never happen
}

void CleanupMyOldInUsePrograms(void)
{
    MSqlQuery query(MSqlQuery::InitCon());

    query.prepare("DELETE FROM inuseprograms "
                  "WHERE hostname = :HOSTNAME and recusage = 'player' ;");
    query.bindValue(":HOSTNAME", gContext->GetHostName());
    query.exec();
}

int main(int argc, char **argv)
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

    QString logfile = "";

    QString pluginname = "";

    QFileInfo finfo(a.argv()[0]);

    QString binname = finfo.baseName();

    bool ResetSettings = false;

    if (binname != "mythfrontend")
        pluginname = binname;

    for(int argpos = 1; argpos < a.argc(); ++argpos)
    {
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile"))
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
        } else if (!strcmp(a.argv()[argpos],"-v") ||
            !strcmp(a.argv()[argpos],"--verbose"))
        {
            if (a.argc()-1 > argpos)
            {
                if (parse_verbose_arg(a.argv()[argpos+1]) ==
                        GENERIC_EXIT_INVALID_CMDLINE)
                    return FRONTEND_EXIT_INVALID_CMDLINE;

                ++argpos;
            } else
            {
                cerr << "Missing argument to -v/--verbose option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if (!strcmp(a.argv()[argpos],"--version"))
        {
            cout << MYTH_BINARY_VERSION << endl;
#ifdef MYTH_BUILD_CONFIG
            cout << "Options compiled in:" <<endl;
            cout << MYTH_BUILD_CONFIG << endl;
#endif
            return FRONTEND_EXIT_OK;
        }
        else if (!strcmp(a.argv()[argpos],"-r") ||
                 !strcmp(a.argv()[argpos],"--reset"))
        {
            ResetSettings = true;
        }
        else if (!strcmp(a.argv()[argpos],"-geometry") ||
                 !strcmp(a.argv()[argpos],"--geometry"))
        {
            if (a.argc()-1 > argpos)
            {
                geometry = a.argv()[argpos+1];
                if (geometry.startsWith("-"))
                {
                    cerr << "Invalid or missing argument to -geometry option\n";
                    return FRONTEND_EXIT_INVALID_CMDLINE;
                }
                else
                    ++argpos;
            }
            else
            {
                cerr << "Missing argument to -geometry option\n";
                return FRONTEND_EXIT_INVALID_CMDLINE;
            }
        }
        else if ((argpos + 1 == a.argc()) &&
                    !QString(a.argv()[argpos]).startsWith("-"))
        {
            pluginname = a.argv()[argpos];
        }
        else
        {
            if (!(!strcmp(a.argv()[argpos],"-h") ||
                !strcmp(a.argv()[argpos],"--help") ||
                !strcmp(a.argv()[argpos],"--usage")))
                cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            cerr << "Valid options are: " << endl <<
#ifdef Q_WS_X11
                    "-display X-server              Create GUI on X-server, not localhost\n" <<
#endif
                    "-geometry or --geometry WxH    Override window size settings\n" <<
                    "--geometry WxH+X+Y             Override window size and position\n" <<
                    "-l or --logfile filename       Writes STDERR and STDOUT messages to filename" << endl <<
                    "-r or --reset                  Resets frontend appearance settings and language" << endl <<
                    "-v or --verbose debug-level    Use '-v help' for level info" << endl <<

                    "--version                      Version information" << endl <<
                    "<plugin>                       Initialize and run this plugin" << endl <<
                    endl <<
                    "Environment Variables:" << endl <<
                    "$MYTHTVDIR                     Set the installation prefix" << endl <<
                    "$MYTHCONFDIR                   Set the config dir (instead of ~/.mythtv)" << endl;
            return FRONTEND_EXIT_INVALID_CMDLINE;
        }
    }

    int logfd = -1;

    if (logfile != "")
    {
        logfd = open(logfile.ascii(), O_WRONLY|O_CREAT|O_APPEND, 0664);
 
        if (logfd < 0)
        {
            perror("open(logfile)");
            return FRONTEND_EXIT_OPENING_LOGFILE_ERROR;
        }
    }

    if (logfd != -1)
    {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);

        // Close the unduplicated logfd
        if (logfd != 1 && logfd != 2)
            close(logfd);
    }

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        cerr << "Unable to ignore SIGPIPE\n";

    QString fileprefix = MythContext::GetConfDir();

    QDir dir(fileprefix);
    if (!dir.exists())
        dir.mkdir(fileprefix);

    gContext = NULL;
    gContext = new MythContext(MYTH_BINARY_VERSION);
    if (!gContext->Init())
    {
        VERBOSE(VB_IMPORTANT, "Failed to init MythContext, exiting.");
        return FRONTEND_EXIT_NO_MYTHCONTEXT;
    }

    if (ResetSettings)
    {
       AppearanceSettings as;
       as.save();

       MSqlQuery query(MSqlQuery::InitCon());
       query.prepare("update settings set data='EN' "
                     "WHERE hostname = :HOSTNAME and value='Language' ;");
       query.bindValue(":HOSTNAME", gContext->GetHostName());
       query.exec();

       return FRONTEND_EXIT_OK;
    }

    if (geometry != "" && !gContext->ParseGeometryOverride(geometry))
    {
        VERBOSE(VB_IMPORTANT,
                QString("Illegal -geometry argument '%1' (ignored)")
                .arg(geometry));
    }

    // Create priveleged thread, then drop privs
    pthread_t priv_thread;

    int status = pthread_create(&priv_thread, NULL, run_priv_thread, NULL);
    if (status) 
    {
        perror("pthread_create");
        priv_thread = 0;
    }
    setuid(getuid());

    if (!UpgradeTVDatabaseSchema())
    {
        VERBOSE(VB_IMPORTANT,
                "Couldn't upgrade database to new schema, exiting.");
        return FRONTEND_EXIT_DB_OUTOFDATE;
    }

    VERBOSE(VB_ALL, QString("%1 version: %2 www.mythtv.org")
                            .arg(binname).arg(MYTH_BINARY_VERSION));

    VERBOSE(VB_ALL, QString("Enabled verbose msgs :%1").arg(verboseString));

    LCD::SetupLCD ();
    if (class LCD *lcd = LCD::Get ()) {
            lcd->setupLEDs(RemoteGetRecordingMask);
    }

    LanguageSettings::load("mythfrontend");

    WriteDefaults();

    QString themename = gContext->GetSetting("Theme", "blue");
    bool randomtheme = gContext->GetNumSetting("RandomTheme", 0);

    if (randomtheme)
        themename = RandTheme(themename);

    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        return FRONTEND_EXIT_NO_THEME;
    }

    gContext->LoadQtConfig();

    MythMainWindow *mainWindow = new MythMainWindow();
    gContext->SetMainWindow(mainWindow);
    LanguageSettings::prompt();

    InitJumpPoints();

    internal_media_init();

    CleanupMyOldInUsePrograms();

    MythPluginManager *pmanager = new MythPluginManager();
    gContext->SetPluginManager(pmanager);

    gContext->UpdateImageCache();

    if (gContext->GetNumSetting("EnableXbox") == 1)
    {
        xbox = new XBox();
        xbox->GetSettings();
    }
    else
        xbox = NULL;

    // this direct connect is only necessary, if the user wants to use the
    // auto shutdown/wakeup feature
    if (gContext->GetNumSetting("idleTimeoutSecs",0) > 0)
        gContext->ConnectToMasterServer();

    qApp->lock();

    if (pluginname != "")
    {
        if (pmanager->run_plugin(pluginname))
        {
            qApp->unlock();
            return FRONTEND_EXIT_OK;
        }
        else 
        {
            pluginname = "myth" + pluginname;
            if (pmanager->run_plugin(pluginname))
            {
                qApp->unlock();
                return FRONTEND_EXIT_OK;
            }
        }
    }

    qApp->unlock();

#ifndef _WIN32
    MediaMonitor *mon = NULL;
    mon = MediaMonitor::getMediaMonitor();
    if (mon)
    {
        VERBOSE(VB_ALL, QString("Starting media monitor."));
        mon->startMonitoring();
    }
#endif

    int exitstatus = RunMenu(themedir);

    if (exitstatus == HALT)
        haltnow();

    pmanager->DestroyAllPlugins();

#ifndef _WIN32
    if (mon)
    {
        mon->stopMonitoring();
        delete mon;
    }
#endif

    if (priv_thread != 0)
    {
        void *value;
        gContext->addPrivRequest(MythPrivRequest::MythExit, NULL);
        pthread_join(priv_thread, &value);
    }
    
    delete mainWindow;
    delete gContext;
    return FRONTEND_EXIT_OK;
}
