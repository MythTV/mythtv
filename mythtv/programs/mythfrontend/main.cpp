#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <qdir.h>
#include <qtextcodec.h>
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

#define QUIT     1
#define HALTWKUP 2
#define HALT     3

ThemedMenu *menu;
MythContext *gContext;

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
        gContext->GetLCDDevice()->connectToHost(lcd_host, lcd_port);

    if (a.argc() == 2)
    {
        QString pluginname = a.argv()[1];

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
