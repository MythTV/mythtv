#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>

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

char installprefix[] = PREFIX;

QString prefix;
QMap<int, TV *> tvList;
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
    ViewScheduled vsb(prefix, tvList.begin().data(), db);

    vsb.Show();
    vsb.exec();

    return 0;
}

int startPlayback(void)
{
    QSqlDatabase *db = QSqlDatabase::database();  
    PlaybackBox pbb(prefix, tvList.begin().data(), db);

    pbb.Show();

    pbb.exec();

    return 0;
}

int startDelete(void)
{
    QSqlDatabase *db = QSqlDatabase::database();
    DeleteBox delbox(prefix, tvList.begin().data(), db);
   
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
        if (tvList.find(nextRecording->cardid) == tvList.end())
        {
            cerr << "cardid: " << nextRecording->cardid 
                 << " isn't in the databae of capture cards.\n";
            exit(0);
        }
        nexttv = tvList[nextRecording->cardid];
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
                if (tvList.find(nextRecording->cardid) == tvList.end())
                {
                    cerr << "invalid cardid " << nextRecording->cardid << endl;
                    exit(0);
                }
                nexttv = tvList[nextRecording->cardid];
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
                        if (tvList.find(nextRecording->cardid) == tvList.end())
                        {
                            cerr << "invalid cardid " << nextRecording->cardid 
                                 << endl;
                            exit(0);
                        }
                        nexttv = tvList[nextRecording->cardid];
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
                    if (tvList.find(nextRecording->cardid) == tvList.end())
                    {
                        cerr << "invalid cardid " << nextRecording->cardid 
                             << endl;
                        exit(0);
                    }
                    nexttv = tvList[nextRecording->cardid];
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
    QString startchannel = globalsettings->GetSetting("DefaultTVChannel");
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
            TV *tv = new TV(startchannel, cardid, cardid + 1);
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

    prefix = (tvList.begin().data())->GetFilePrefix();
    QString theprefix = (tvList.begin().data())->GetInstallPrefix();

    QString themename = globalsettings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    if (themedir == "")
    {
        cerr << "Couldn't find theme " << themename << endl;
        exit(0);
    }

    globalsettings->SetSetting("ThemePathName", themedir + "/");

    QString qttheme = themedir + "/qtlook.txt";
    globalsettings->ReadSettings(qttheme);
 
    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, NULL);

    RunMenu(themedir);

    delete globalsettings;

    return 0;
}
