#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qmap.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
using namespace std;

#include "tv.h"
#include "scheduler.h"
#include "mainserver.h"
#include "encoderlink.h"

#include "libmythtv/programinfo.h"
#include "libmyth/mythcontext.h"

QMap<int, EncoderLink *> tvList;

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
    QApplication a(argc, argv, false);

    QString logfile = "";
    QString pidfile = "";
    bool daemonize = false;
    for(int argpos = 1; argpos < a.argc(); ++argpos)
        if (!strcmp(a.argv()[argpos],"-l") ||
            !strcmp(a.argv()[argpos],"--logfile")) {
            if (a.argc() > argpos) {
                logfile = a.argv()[argpos+1];
                ++argpos;
            } else {
                cerr << "Missing argument to -l/--logfile option\n";
                return -1;
            }
        } else if (!strcmp(a.argv()[argpos],"-p") ||
                   !strcmp(a.argv()[argpos],"--pidfile")) {
            if (a.argc() > argpos) {
                pidfile = a.argv()[argpos+1];
                ++argpos;
            } else {
                cerr << "Missing argument to -p/--pidfile option\n";
                return -1;
            }
        } else if (!strcmp(a.argv()[argpos],"-f")) {
            daemonize = false;
        } else {
            cerr << "Invalid argument: " << a.argv()[argpos] << endl;
            return -1;
        }

    int logfd = -1;

    if (logfile != "") {
        logfd = open(logfile.ascii(), O_WRONLY|O_CREAT|O_APPEND);
         
        if (logfd < 0) {
            perror("open(logfile)");
            return -1;
        }
    }
    
    ofstream pidfs;
    if (pidfile != "") {
        pidfs.open(pidfile.ascii());
        if (!pidfs) {
            perror("open(pidfile)");
            return -1;
        }
    }

    if (pidfs) {
        pidfs << getpid() << endl;
        pidfs.close();
    }

    close(0);

    if (daemonize)
        if (daemon(0, 1) < 0) {
            perror("daemon");
            return -1;
        }

    if (logfd != -1) {
        // Send stdout and stderr to the logfile
        dup2(logfd, 1);
        dup2(logfd, 2);
    }

    MythContext *context = new MythContext(false);
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

    QSqlDatabase *scdb = QSqlDatabase::database("SUBDB");
    Scheduler *sched = new Scheduler(context, &tvList, scdb);

    int port = context->GetNumSetting("ServerPort", 6543);
    int statusport = context->GetNumSetting("StatusPort", 6544);

    MainServer *ms = new MainServer(context, port, statusport, &tvList);
    a.exec();

    delete context;
    delete sched;

    unlink(pidfile.ascii());

    return 0;
}
