#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "playbackbox.h"
#include "viewscheduled.h"
#include "globalsettings.h"
#include "recordingprofile.h"

#include "libmyth/themedmenu.h"
#include "libmythtv/programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"
#include "libmythtv/guidegrid.h"

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
    ViewScheduled vsb(db);

    vsb.Show();
    qApp->unlock();
    vsb.exec();
    qApp->lock();

    return 0;
}

int startPlayback(void)
{
    PlaybackBox pbb(PlaybackBox::Play);

    pbb.Show();
    qApp->unlock();
    pbb.exec();
    qApp->lock();

    return 0;
}

int startDelete(void)
{
    PlaybackBox delbox(PlaybackBox::Delete);
   
    delbox.Show();
    qApp->unlock();
    delbox.exec();
    qApp->lock();

    return 0;
}

void startTV(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    TV *tv = new TV(db);
    tv->Init();
    TVState nextstate = tv->LiveTV();

    qApp->unlock();

    if (nextstate == kState_WatchingLiveTV ||
           nextstate == kState_WatchingRecording)
    {
        while (tv->ChangingState())
        {
            usleep(50);
            qApp->processEvents();
        }

        while (nextstate == kState_WatchingLiveTV ||
               nextstate == kState_WatchingRecording ||
               nextstate == kState_WatchingPreRecorded) 
        {
            usleep(100);
            qApp->processEvents();
            nextstate = tv->GetState();
        }
    }

    qApp->lock();
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
    else if (sel == "tv_fix_conflicts")
        startManaged();
    else if (sel == "settings appearance") {
        AppearanceSettings settings;
        settings.exec(QSqlDatabase::database());
        gContext->LoadQtConfig();
        menu->ReloadTheme();
    } else if (sel == "settings recording") {
        RecordingProfileEditor editor(QSqlDatabase::database());
        editor.exec(QSqlDatabase::database());
    } else if (sel == "settings general") {
        GeneralSettings settings;
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings playback") {
        PlaybackSettings settings;
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings epg") {
        EPGSettings settings;
        settings.exec(QSqlDatabase::database());
    }
}

int handleExit(void)
{
    QString title = "Do you really want to exit MythTV?";

    DialogBox diag(title);
    diag.AddButton("No");
    diag.AddButton("Yes, Exit now");
    diag.AddButton("Yes, Exit and shutdown the computer");
//    bool haltNwakeup = context->GetNumSetting("UseHaltWakeup");
//    if (haltNwakeup)
//        diag.AddButton("Yes, Shutdown and set wakeup time");

    diag.Show();

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
    menu = new ThemedMenu(themedir.ascii(), "mainmenu.xml");
    menu->setCallback(TVMenuCallback, gContext);
   
    int exitstatus = 0;
 
    if (menu->foundTheme())
    {
        do {
            menu->Show();
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
}

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    gContext = new MythContext;
    gContext->ConnectServer("localhost", 6543);

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

    WriteDefaults(db);

    QString themename = gContext->GetSetting("Theme", "blue");
    QString themedir = gContext->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    gContext->LoadQtConfig();

    qApp->unlock();

    int exitstatus = RunMenu(themedir);

    haltnow(exitstatus);

    delete gContext;

    return exitstatus;
}
