#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <unistd.h>

#include "guidegrid.h"
#include "tv.h"
#include "settings.h"
#include "themedmenu.h"
#include "scheduler.h"
#include "infostructs.h"
#include "programinfo.h"
#include "playbackbox.h"
#include "deletebox.h"
#include "viewscheduled.h"
#include "menubox.h"

char installprefix[] = "/usr/local";

Settings *globalsettings;

QString startGuide(const QString &startchannel)
{
    GuideGrid gg(startchannel);

    gg.exec();

    return gg.getLastChannel();
}

int startManaged(TV *tv, QString prefix)
{
    QSqlDatabase *db = QSqlDatabase::database();
    ViewScheduled vsb(prefix, tv, db);

    vsb.Show();
    vsb.exec();

    return 0;
}

int startPlayback(TV *tv, QString prefix)
{
    QSqlDatabase *db = QSqlDatabase::database();  
    PlaybackBox pbb(prefix, tv, db);

    pbb.Show();

    pbb.exec();

    return 0;
}

int startDelete(TV *tv, QString prefix)
{
    QSqlDatabase *db = QSqlDatabase::database();
    DeleteBox delbox(prefix, tv, db);
   
    delbox.Show();
    
    delbox.exec();

    return 0;
}

void startTV(TV *tv)
{
    TVState nextstate = tv->LiveTV();

    if (nextstate == kState_WatchingLiveTV ||
           nextstate == kState_WatchingRecording)
    {
        while (tv->ChangingState())
            usleep(50);

        while (nextstate == kState_WatchingLiveTV ||
               nextstate == kState_WatchingRecording)
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
    TV *tv = (TV *)dummy;
    QSqlDatabase *db = QSqlDatabase::database("SUBDB");

    Scheduler *sched = new Scheduler(db);

    sched->FillRecordLists();

    int secsleft = -1;
    int asksecs = -1;
    bool asked = false;

    ProgramInfo *nextRecording = sched->GetNextRecording();
    QDateTime nextrectime;
    if (nextRecording)
    {
        nextrectime = nextRecording->startts;
        asked = false;
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
            }
        }

        curtime = QDateTime::currentDateTime();
        if (nextRecording)
        {
            secsleft = curtime.secsTo(nextrectime);
            asksecs = secsleft - 30;

            if (tv->GetState() == kState_WatchingLiveTV && asksecs <= 0)
            {
                if (!asked)
                {
                    asked = true;
                    int result = askRecording(tv, nextRecording, secsleft);

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
                    }
                }
            }
            if (secsleft <= -2)
            {
                // don't record stuff that's already started..
                if (secsleft > -30)
                    startRecording(tv, nextRecording);

                sched->RemoveFirstRecording();
                nextRecording = sched->GetNextRecording();

                if (nextRecording)
                {
                   nextrectime = nextRecording->startts;
                   curtime = QDateTime::currentDateTime();
                   secsleft = curtime.secsTo(nextrectime);
                }
            }
         //   else 
         //       cout << secsleft << " secs left until " << nextRecording->title << endl;
        }
    }
    
    return NULL;
}

bool RunTVMenu(bool usetheme, QString themedir, TV *tv, QString prefix)
{
    if (usetheme)
    {
        ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "tv.menu");

        if (diag->foundTheme())
        {
            diag->Show();
            diag->exec();

            QString sel = diag->getSelection().lower();

            bool retval = true;

            if (sel == "tv_watch_live")
                startTV(tv);
            else if (sel == "tv_watch_recording")
                startPlayback(tv, prefix);
            else if (sel == "tv_schedule")
                startGuide("3");
            else if (sel == "tv_delete")
                startDelete(tv, prefix);
            else if (sel == "tv_fix_conflicts")
                startManaged(tv, prefix);
            else if (sel == "tv_setup")
                ;
            else 
                retval = false;

            delete diag;

            return retval;
        }
    }

    MenuBox *diag = new MenuBox("MythTV");

    diag->AddButton("Watch TV");
    diag->AddButton("Schedule a Recording");
    diag->AddButton("Fix Recording Conflicts");
    diag->AddButton("Watch a Previously Recorded Program");
    diag->AddButton("Delete Recordings");
    diag->AddButton("Setup");

    diag->Show();
    int ret = diag->exec();

    bool handled = true;
    switch (ret)
    {
        case 1: startTV(tv); break;
        case 2: startGuide("3"); break;
        case 3: startManaged(tv, prefix); break;
        case 4: startPlayback(tv, prefix); break;
        case 5: startDelete(tv, prefix); break;
        case 6: break;
        default: handled = false; break;
    }

    delete diag;

    return handled;
}

QString RunMainMenu(bool usetheme, QString themedir)
{
    if (usetheme)
    {
        ThemedMenu *diag = new ThemedMenu(themedir.ascii(), "main.menu");

        if (diag->foundTheme())
        {
            diag->Show();
            diag->exec();

            QString sel = diag->getSelection().lower();

            delete diag;

            return sel;
        }
    }

    MenuBox *diag = new MenuBox("MythTV");

    diag->AddButton("TV");
    diag->AddButton("Music");

    diag->Show();
    int ret = diag->exec();

    QString returnval = "";
    if (ret == 1)
        returnval = "mode_tv";
    else if (ret == 2)
        returnval = "mode_music";

    delete diag;

    return returnval;
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

    TV *tv = new TV("3");

    QString prefix = tv->GetFilePrefix();
    QString theprefix = tv->GetInstallPrefix();

    QString themename = globalsettings->GetSetting("Theme");

    QString themedir = findThemeDir(themename, theprefix);
    bool usetheme = true;
    if (themedir == "")
        usetheme = false;
 
    pthread_t scthread;
    pthread_create(&scthread, NULL, runScheduler, tv);

    bool displaymain = false;
    QString musiclocation = theprefix + "/bin/mythmusic";
    QFile testfile(musiclocation);
    if (testfile.exists())
        displaymain = true;

    while (1)
    {
        if (displaymain)
        {
            QString sel = RunMainMenu(usetheme, themedir);
            if (sel == "mode_tv")
            {
                while (RunTVMenu(usetheme, themedir, tv, prefix))
                    ;
            }
            else if (sel == "mode_music")
                system(musiclocation);
        }
        else
            RunTVMenu(usetheme, themedir, tv, prefix);
    }

    delete tv;
    delete globalsettings;

    return 0;
}
