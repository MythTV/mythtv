#include <qapplication.h>
#include <qsqldatabase.h>
#include <unistd.h>

#include "guidegrid.h"
#include "tv.h"
#include "settings.h"
#include "menubox.h"
#include "scheduler.h"
#include "infostructs.h"
#include "recordinginfo.h"

int startGuide(int startchannel)
{
    GuideGrid gg(startchannel);

    gg.exec();

    int chan = gg.getLastChannel();
    return chan;
}

void startTV(TV *tv)
{
    tv->LiveTV();

    while (tv->GetState() == kState_None)
        usleep(1000);
    while (tv->GetState() != kState_None)
        usleep(1000);
}

void startRecording(TV *tv, ProgramInfo *rec)
{
    char startt[128];
    char endt[128];

    QDateTime starttime = QDateTime::fromString(rec->starttime, Qt::ISODate);
    QDateTime endtime = QDateTime::fromString(rec->endtime, Qt::ISODate);

    sprintf(startt, "%4d%02d%02d%02d%02d00", starttime.date().year(),
            starttime.date().month(), starttime.date().day(), 
            starttime.time().hour(), starttime.time().minute());
    sprintf(endt, "%4d%02d%02d%02d%02d00", endtime.date().year(),
            endtime.date().month(), endtime.date().day(), 
            endtime.time().hour(), endtime.time().minute());

    RecordingInfo *tvrec = new RecordingInfo(rec->channum.ascii(),
                                             startt, endt, rec->title.ascii(),
                                             rec->subtitle.ascii(), 
                                             rec->description.ascii());

    tv->StartRecording(tvrec);
}

void *runScheduler(void *dummy)
{
    TV *tv = (TV *)dummy;
    Scheduler *sched = new Scheduler(tv);

    if (sched->FillRecordLists())
    {
        // conflict exists
    }

    ProgramInfo *nextRecording = sched->GetNextRecording();
    QDateTime nextrectime;
    if (nextRecording)
        nextrectime = QDateTime::fromString(nextRecording->starttime, 
                                            Qt::ISODate);
    QDateTime curtime = QDateTime::currentDateTime();
    int secsleft = -1;

    while (1)
    {
        sleep(1);

        if (sched->CheckForChanges())
        {
            if (sched->FillRecordLists())
            {
                // conflict
            }
            nextRecording = sched->GetNextRecording();
            if (nextRecording)
            {   
                nextrectime = QDateTime::fromString(nextRecording->starttime,
                                                    Qt::ISODate);
            }   
        }

        if (nextRecording)
        {
            curtime = QDateTime::currentDateTime();
            secsleft = curtime.secsTo(nextrectime);

            if (secsleft <= 0)
            {
                // don't record stuff that's already started..
                if (secsleft > -30)
                    startRecording(tv, nextRecording);

                sched->RemoveFirstRecording();
                nextRecording = sched->GetNextRecording();

                if (nextRecording)
                {
                   nextrectime = QDateTime::fromString(nextRecording->starttime,
                                                       Qt::ISODate);
                   curtime = QDateTime::currentDateTime();
                   secsleft = curtime.secsTo(nextrectime);
                }
            }
            else
                cout << secsleft << " secs left until " << nextRecording->title << endl;
        }
    }
    
    return NULL;
}

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    db->setDatabaseName("mythconverg");
    db->setUserName("mythtv");
    db->setPassword("mythtv");
    db->setHostName("localhost");

    QSqlDatabase *subthread = QSqlDatabase::addDatabase("QMYSQL3", "SUBDB");
    if (!subthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    subthread->setDatabaseName("mythconverg");
    subthread->setUserName("mythtv");
    subthread->setPassword("mythtv");
    subthread->setHostName("localhost");    

    if (!db->open() || !subthread->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    TV *tv = new TV("3");
 
    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, tv);

    while (1)
    {
        MenuBox *diag = new MenuBox("MythTV");

        diag->AddButton("Watch TV");
        diag->AddButton("Schedule a Recording");
        diag->AddButton("Manage Scheduled Recordings");
        diag->AddButton("Watch a Previously Recorded Program");  
 
        diag->Show();
        int result = diag->exec();

        switch (result)
        {
            case 0: startTV(tv); break;
            case 1: startGuide(3); break;
            default: break;
        }
    }

    return 0;
}
