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

    QSqlDatabase *scdb = QSqlDatabase::database("SUBDB");
    Scheduler *sched = new Scheduler(context, &tvList, scdb);

    int port = context->GetNumSetting("ServerPort", 6543);
    int statusport = context->GetNumSetting("StatusPort", 6544);

    MainServer *ms = new MainServer(context, port, statusport, &tvList);
    a.setMainWidget(ms);

    a.exec();

    delete context;
    delete sched;

    return 0;
}
