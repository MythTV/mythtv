#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <unistd.h>

#include <vector>

using namespace std;

#include "tv.h"
#include "scheduler.h"
#include "playbackbox.h"
#include "deletebox.h"
#include "viewscheduled.h"

#include "libmyth/infostructs.h"
#include "libmyth/guidegrid.h"
#include "libmyth/settings.h"
#include "libmyth/themedmenu.h"
#include "libmyth/programinfo.h"

char installprefix[] = "/usr/local";

QString prefix;
vector<TV *> tvList;
Settings *globalsettings;

QString startGuide(void)
{
    QString startchannel = globalsettings->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    GuideGrid gg(startchannel);

    gg.exec();

    return gg.getLastChannel();
}

int startManaged(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ViewScheduled vsb(prefix, tvList.front(), db);

    vsb.Show();
    vsb.exec();

    return 0;
}

int startPlayback(void)
{
    QSqlDatabase *db = QSqlDatabase::database();  
    PlaybackBox pbb(prefix, tvList.front(), db);

    pbb.Show();

    pbb.exec();

    return 0;
}

int startDelete(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    DeleteBox delbox(prefix, tvList.front(), db);
   
    delbox.Show();
    
    delbox.exec();

    return 0;
}

void startTV(void)
{
    TV *tv = tvList.front();
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

void startRecording(TV *tv, ProgramInfo *rec)
{
    ProgramInfo *tvrec = new ProgramInfo(*rec);
    tv->StartRecording(tvrec);
}

int askRecording(TV *tv, ProgramInfo *rec, int timeuntil)
{
    ProgramInfo *tvrec = new ProgramInfo(*rec);
    int retval = tv->AllowRecording(tvrec, timeuntil);

    delete tvrec;
    return retval;
}

void *runScheduler(void *dummy)
{
    dummy = dummy;

    QSqlDatabase *db = QSqlDatabase::database("SUBDB");

    Scheduler *sched = new Scheduler(db);

    sched->FillRecordLists();

    int secsleft = -1;
    int asksecs = -1;
    bool asked = false;

    TV *nexttv = NULL;

    ProgramInfo *nextRecording = sched->GetNextRecording();
    QDateTime nextrectime;
    if (nextRecording)
    {
        nextrectime = nextRecording->startts;
        asked = false;
        if (nextRecording->cardid > tvList.size() || nextRecording->cardid < 1)
        {
            cerr << "cardid is higher than number of cards.\n";
            exit(0);
        }
        nexttv = tvList[nextRecording->cardid - 1];
    }
    QDateTime curtime = QDateTime::currentDateTime();

    QDateTime lastupdate = curtime;

    while (1)
    {
        sleep(1);

        if (sched->CheckForChanges() ||
            (lastupdate.date().day() != curtime.date().day()))
        {
            lastupdate = curtime;
            sched->FillRecordLists();
            nextRecording = sched->GetNextRecording();
            if (nextRecording)
            {
                nextrectime = nextRecording->startts;
                asked = false;
                if (nextRecording->cardid > tvList.size() || 
                    nextRecording->cardid < 1)
                {
                    cerr << "invalid cardid " << nextRecording->cardid << endl;
                    exit(0);
                }
                nexttv = tvList[nextRecording->cardid - 1];
            }
        }

        curtime = QDateTime::currentDateTime();
        if (nextRecording)
        {
            secsleft = curtime.secsTo(nextrectime);
            asksecs = secsleft - 30;

            if (nexttv->GetState() == kState_WatchingLiveTV && asksecs <= 0)
            {
                if (!asked)
                {
                    asked = true;
                    int result = askRecording(nexttv, nextRecording, secsleft);

                    if (result == 3)
                    {
                        sched->RemoveFirstRecording();
                        nextRecording = sched->GetNextRecording();
                    }
 
                    if (nextRecording)
                    {
                        nextrectime = nextRecording->startts;
                        curtime = QDateTime::currentDateTime();
                        secsleft = curtime.secsTo(nextrectime);
                        if (nextRecording->cardid > tvList.size() ||
                            nextRecording->cardid < 1)
                        {
                            cerr << "invalid cardid " << nextRecording->cardid 
                                 << endl;
                            exit(0);
                        }
                        nexttv = tvList[nextRecording->cardid - 1];
                    }
                }
            }
            if (secsleft <= -2)
            {
                // don't record stuff that's already started..
                if (secsleft > -30)
                    startRecording(nexttv, nextRecording);

                sched->RemoveFirstRecording();
                nextRecording = sched->GetNextRecording();

                if (nextRecording)
                {
                    nextrectime = nextRecording->startts;
                    curtime = QDateTime::currentDateTime();
                    secsleft = curtime.secsTo(nextrectime);
                    if (nextRecording->cardid > tvList.size() ||
                        nextRecording->cardid < 1)
                    {
                        cerr << "invalid cardid " << nextRecording->cardid 
                             << endl;
                        exit(0);
                    }
                    nexttv = tvList[nextRecording->cardid - 1];
                }
            }
//            else 
//                cout << secsleft << " secs left until " << nextRecording->title << endl;
        }
    }
    
    return NULL;
}

void TVMenuCallback(void *data, QString &selection)
{
    data = data;

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
    else if (sel == "tv_setup")
        ;
}

bool RunMenu(QString themedir)
{
    ThemedMenu *diag = new ThemedMenu(themedir.ascii(), installprefix, 
                                      "mainmenu.xml");

    diag->setCallback(TVMenuCallback, NULL);
    
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

void setupTVs(void)
{
    QSqlQuery query;

    int numcards = -1;
    query.exec("SELECT NULL FROM capturecard;");

    if (query.isActive())
        numcards = query.numRowsAffected();

    if (numcards < 0)
    {
        cerr << "ERROR: no capture cards are defined in the database.\n";
        exit(0);
    }

    QString startchannel = globalsettings->GetSetting("DefaultTVChannel");
    if (startchannel == "")
        startchannel = "3";

    for (int i = 0; i < numcards; i++)
    {
        TV *tv = new TV(startchannel, i+1, i+2);
        tvList.push_back(tv);
    }
}
    
int main(int argc, char **argv)
{
    QApplication a(argc, argv);

    globalsettings = new Settings;

    globalsettings->LoadSettingsFiles("settings.txt", installprefix);
    globalsettings->LoadSettingsFiles("theme.txt", installprefix);
    globalsettings->LoadSettingsFiles("mysql.txt", installprefix);

    QSqlDatabase *db = QSqlDatabase::addDatabase("QMYSQL3");
    if (!db)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    db->setDatabaseName(globalsettings->GetSetting("DBName"));
    db->setUserName(globalsettings->GetSetting("DBUserName"));
    db->setPassword(globalsettings->GetSetting("DBPassword"));
    db->setHostName(globalsettings->GetSetting("DBHostName"));

    QSqlDatabase *subthread = QSqlDatabase::addDatabase("QMYSQL3", "SUBDB");
    if (!subthread)
    {
        printf("Couldn't connect to database\n");
        return -1;
    }
    subthread->setDatabaseName(globalsettings->GetSetting("DBName"));
    subthread->setUserName(globalsettings->GetSetting("DBUserName"));
    subthread->setPassword(globalsettings->GetSetting("DBPassword"));
    subthread->setHostName(globalsettings->GetSetting("DBHostName"));    

    if (!db->open() || !subthread->open())
    {
        printf("couldn't open db\n");
        return -1;
    }

    setupTVs();

    prefix = (tvList.front())->GetFilePrefix();
    QString theprefix = (tvList.front())->GetInstallPrefix();

    QString themename = globalsettings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    QString qttheme = themedir + "/qtlook.txt";
    globalsettings->ReadSettings(qttheme);
 
    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, NULL);

    RunMenu(themedir);

    delete globalsettings;

    return 0;
}
