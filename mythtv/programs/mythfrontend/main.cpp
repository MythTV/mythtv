#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "scheduler.h"
#include "playbackbox.h"
#include "deletebox.h"
#include "viewscheduled.h"

#include "libmyth/themedmenu.h"
#include "libmyth/programinfo.h"
#include "libmyth/mythcontext.h"

QMap<int, TV *> tvList;

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
    ViewScheduled vsb(context, tvList.begin().data(), db);

    vsb.Show();
    vsb.exec();

    return 0;
}

int startPlayback(MythContext *context)
{
    QSqlDatabase *db = QSqlDatabase::database();  
    PlaybackBox pbb(context, tvList.begin().data(), db);

    pbb.Show();

    pbb.exec();

    return 0;
}

int startDelete(MythContext *context)
{
    QSqlDatabase *db = QSqlDatabase::database();
    DeleteBox delbox(context, tvList.begin().data(), db);
   
    delbox.Show();
    
    delbox.exec();

    return 0;
}

void startTV(void)
{
    TV *tv = tvList.begin().data();
    TVState nextstate = tv->LiveTV();

    if (nextstate == kState_WatchingLiveTV ||
           nextstate == kState_WatchingRecording)
    {
        while (tv->ChangingState())
            usleep(50);

        while (nextstate == kState_WatchingLiveTV ||
               nextstate == kState_WatchingRecording ||
               nextstate == kState_WatchingPreRecorded ||
               nextstate == kState_WatchingOtherRecording)
        {
            usleep(2000);
            nextstate = tv->GetState();
        }
    }
}

void *runScheduler(void *dummy)
{
    dummy = dummy;
    //MythContext *context = (MythContext *)dummy;

    QSqlDatabase *db = QSqlDatabase::database("SUBDB");

    Scheduler *sched = new Scheduler(db);

    bool asked = false;
    int secsleft;
    TV *nexttv = NULL;

    ProgramInfo *nextRecording = NULL;
    QDateTime nextrectime;
    QDateTime curtime;
    QDateTime lastupdate = QDateTime::currentDateTime().addDays(-1);

    while (1)
    {
        curtime = QDateTime::currentDateTime();

        if (sched->CheckForChanges() || 
            (lastupdate.date().day() != curtime.date().day()))
        {
            sched->FillRecordLists();
            //cout << "Found changes in the todo list.\n"; 
            nextRecording = NULL;
        }

        if (!nextRecording)
        {
            lastupdate = curtime;
            nextRecording = sched->GetNextRecording();
            if (nextRecording)
            {
                nextrectime = nextRecording->startts;
                //cout << "Will record " << nextRecording->title
                //     << " in " << curtime.secsTo(nextrectime) << "secs.\n";
                asked = false;
                if (tvList.find(nextRecording->cardid) == tvList.end())
                {
                    cerr << "invalid cardid " << nextRecording->cardid << endl;
                    exit(0);
                }
                nexttv = tvList[nextRecording->cardid];
            }
        }

        if (nextRecording)
        {
            secsleft = curtime.secsTo(nextrectime);

            //cout << secsleft << " seconds until " << nextRecording->title 
            //     << endl;

            if (nexttv->GetState() == kState_WatchingLiveTV && 
                secsleft <= 30 && !asked)
            {
                asked = true;
                int result = nexttv->AllowRecording(nextRecording, secsleft);

                if (result == 3)
                {
                    //cout << "Skipping " << nextRecording->title << endl;
                    sched->RemoveFirstRecording();
                    nextRecording = NULL;
                    continue;
                }
            }

            if (secsleft <= -2)
            {
                nexttv->StartRecording(nextRecording);
                //cout << "Started recording " << nextRecording->title << endl;
                sched->RemoveFirstRecording();
                nextRecording = NULL;
                continue;
            }
        }
       
        sleep(1);
    }
    
    return NULL;
}

void TVMenuCallback(void *data, QString &selection)
{
    MythContext *context = (MythContext *)data;

    QString sel = selection.lower();

    if (sel == "tv_watch_live")
        startTV();
    else if (sel == "tv_watch_recording")
        startPlayback(context);
    else if (sel == "tv_schedule")
        startGuide(context);
    else if (sel == "tv_delete")
        startDelete(context);
    else if (sel == "tv_fix_conflicts")
        startManaged(context);
    else if (sel == "tv_setup")
        ;
}

bool RunMenu(QString themedir, MythContext *context)
{
    ThemedMenu *diag = new ThemedMenu(context, themedir.ascii(), 
                                      "mainmenu.xml");

    diag->setCallback(TVMenuCallback, context);
    
    if (diag->foundTheme())
    {
        diag->Show();
        diag->exec();
    }
    else
    {
        cerr << "Couldn't find theme " << themedir << endl;
    }

    return true;
}   

void setupTVs(MythContext *context)
{
    QString startchannel = context->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    QSqlQuery query;

    query.exec("SELECT cardid FROM capturecard ORDER BY cardid;");

    if (query.isActive() && query.numRowsAffected())
    {
        while (query.next())
        {
            int cardid = query.value(0).toInt();

            // not correct, but it'll do for now
            TV *tv = new TV(context, startchannel, cardid, cardid + 1);
            tvList[cardid] = tv;
        }
    }
    else
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        exit(0);
    }
}
    
int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    MythContext *context = new MythContext;

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    QSqlDatabase *subthread = QSqlDatabase::addDatabase("QMYSQL3", "SUBDB");
    if (!subthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }

    if (!context->OpenDatabase(db) || !context->OpenDatabase(subthread))
    {
        printf("couldn't open db\n");
        return -1;
    }

    setupTVs(context);

    QString themename = context->GetSetting("Theme");
    QString themedir = context->FindThemeDir(themename);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    context->LoadQtConfig();

    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, context);

    RunMenu(themedir, context);

    delete context;

    return 0;
}
