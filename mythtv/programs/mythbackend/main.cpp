#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>

#include <iostream>
using namespace std;

#include "tv.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"

#include "libmythtv/programinfo.h"
#include "libmyth/mythcontext.h"

QMap<int, EncoderLink *> tvList;
void *runScheduler(void *dummy)
{
    dummy = dummy;
    //MythContext *context = (MythContext *)dummy;

    QSqlDatabase *db = QSqlDatabase::database("SUBDB");

    Scheduler *sched = new Scheduler(db);

    bool asked = false;
    int secsleft;
    EncoderLink *nexttv = NULL;

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

            TVRec *tv = new TVRec(context, startchannel, cardid);
            tv->Init();
            EncoderLink *enc = new EncoderLink(tv);
            tvList[cardid] = enc;
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
    context->LoadSettingsFiles("backend_settings.txt");

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

    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, context);

    int port = context->GetNumSetting("ServerPort", 6543);
    int statusport = context->GetNumSetting("StatusPort", 6544);

    MainServer *ms = new MainServer(context, port, statusport, &tvList);
    a.setMainWidget(ms);

    a.exec();

    delete context;
    return 0;
}
