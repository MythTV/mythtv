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
#include "themesetup.h"
#include "globalsettings.h"
#include "recordingprofile.h"

#include "libmyth/themedmenu.h"
#include "libmyth/programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmyth/dialogbox.h"
#include "libmythtv/videosource.h"

#define QUIT     1
#define HALTWKUP 2
#define HALT     3

ThemedMenu *menu;

QString startGuide(MythContext *context)
{
    QString startchannel = context->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    return context->RunProgramGuide(startchannel);
}

int startManaged(MythContext *context)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ViewScheduled vsb(context, db);

    vsb.Show();
    qApp->unlock();
    vsb.exec();
    qApp->lock();

    return 0;
}

int startPlayback(MythContext *context)
{
    PlaybackBox pbb(context, PlaybackBox::Play);

    pbb.Show();
    qApp->unlock();
    pbb.exec();
    qApp->lock();

    return 0;
}

int startDelete(MythContext *context)
{
    PlaybackBox delbox(context, PlaybackBox::Delete);
   
    delbox.Show();
    qApp->unlock();
    delbox.exec();
    qApp->lock();

    return 0;
}

void startTV(MythContext *context)
{
    TV *tv = new TV(context);
    tv->Init();
    TVState nextstate = tv->LiveTV();

    if (nextstate == kState_WatchingLiveTV ||
           nextstate == kState_WatchingRecording)
    {
        while (tv->ChangingState())
        {
            usleep(50);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
        }

        while (nextstate == kState_WatchingLiveTV ||
               nextstate == kState_WatchingRecording ||
               nextstate == kState_WatchingPreRecorded) 
        {
            usleep(500);
            qApp->unlock();
            qApp->processEvents();
            qApp->lock();
            nextstate = tv->GetState();
        }
    }
}

void themesSettings(MythContext *context)
{ 
    QSqlDatabase *db = QSqlDatabase::database();
    ThemeSetup ts(context, db);
    ts.Show();

    qApp->unlock();
    ts.exec();
    qApp->lock();

    menu->ReloadTheme();
}

void TVMenuCallback(void *data, QString &selection)
{
    MythContext *context = (MythContext *)data;

    QString sel = selection.lower();

    if (sel == "tv_watch_live")
        startTV(context);
    else if (sel == "tv_watch_recording")
        startPlayback(context);
    else if (sel == "tv_schedule")
        startGuide(context);
    else if (sel == "tv_delete")
        startDelete(context);
    else if (sel == "tv_fix_conflicts")
        startManaged(context);
    else if (sel == "settings themes")
        themesSettings(context);
    else if (sel == "settings recording") {
        RecordingProfileEditor editor(context, QSqlDatabase::database());
        editor.exec(QSqlDatabase::database());
    } else if (sel == "settings general") {
        GeneralSettings settings(context);
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings playback") {
        PlaybackSettings settings(context);
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings epg") {
        EPGSettings settings(context);
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings tuner capturecard") {
        CaptureCardEditor cce(context, QSqlDatabase::database());
        cce.exec(QSqlDatabase::database());
    } else if (sel == "settings tuner videosource") {
        VideoSourceEditor vse(context, QSqlDatabase::database());
        vse.exec(QSqlDatabase::database());
    } else if (sel == "settings tuner cardinput") {
        CardInputEditor cie(context, QSqlDatabase::database());
        cie.exec(QSqlDatabase::database());
    }
}

int handleExit(MythContext *context)
{
    QString title = "Do you really want to exit MythTV?";

    DialogBox diag(context, title);
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

void haltnow(MythContext *context, int how)
{
    QString halt_cmd = context->GetSetting("HaltCommand", "halt");
    QString haltwkup_cmd = context->GetSetting("HaltCommandWhenWakeup",
                                               "echo 'would halt now, and set "
                                               "wakeup time to %1 (secs since "
                                               "1970) if command was set'");

    if (how == HALTWKUP)
    {
    }
    else if (how == HALT)
        system(halt_cmd.ascii());
}

int RunMenu(QString themedir, MythContext *context)
{
    menu = new ThemedMenu(context, themedir.ascii(), "mainmenu.xml");
    menu->setCallback(TVMenuCallback, context);
   
    int exitstatus = 0;
 
    if (menu->foundTheme())
    {
        do {
            menu->Show();
            menu->exec();
        } while (!(exitstatus = handleExit(context)));
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    return exitstatus;
}   

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext;
    //context->ConnectServer("localhost", 6543);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!context->OpenDatabase(db))
    {
        printf("couldn't open db\n");
        return -1;
    }

    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    context->LoadQtConfig();

    qApp->unlock();

    int exitstatus = RunMenu(themedir, context);

    haltnow(context, exitstatus);

    delete context;

    return exitstatus;
}
