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
    vsb.exec();

    return 0;
}

int startPlayback(MythContext *context)
{
    PlaybackBox pbb(context, PlaybackBox::Play);

    pbb.Show();
    pbb.exec();

    return 0;
}

int startDelete(MythContext *context)
{
    PlaybackBox delbox(context, PlaybackBox::Delete);
   
    delbox.Show();
    delbox.exec();

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
               nextstate == kState_WatchingPreRecorded ||
               nextstate == kState_WatchingOtherRecording)
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

    ts.exec();

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
        RecordingProfileEditor editor(context, NULL, QSqlDatabase::database());
        editor.showFullScreen();
        editor.exec();
    } else if (sel == "settings general") {
        GeneralSettings settings(context);
        settings.exec(QSqlDatabase::database());
    } else if (sel == "settings playback") {
        PlaybackSettings settings(context);
        settings.exec(QSqlDatabase::database());
    }
}

bool RunMenu(QString themedir, MythContext *context)
{
    menu = new ThemedMenu(context, themedir.ascii(), "mainmenu.xml");
    menu->setCallback(TVMenuCallback, context);
    
    if (menu->foundTheme())
    {
        menu->Show();
        menu->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    return true;
}   

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext;
    context->ConnectServer("localhost", 6543);

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

    RunMenu(themedir, context);

    delete context;

    return 0;
}
