#include <unistd.h>

#include <qsqldatabase.h>
#include <qsqlquery.h>
#include <qdatetime.h>
#include <qfileinfo.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <iostream>
using namespace std;

#include "commercialflag.h"
#include "libmythtv/programinfo.h"
#include "libmyth/mythcontext.h"
#include "libmythtv/NuppelVideoPlayer.h"
#include "mythdbcon.h"

CommercialFlagger::CommercialFlagger(bool master, QSqlDatabase *ldb)
{
    isMaster = master;
    db = ldb;

    if ( !db)
        cerr << "db == NULL" << endl;

    gContext->addListener(this);

    pthread_t restartThread;
    pthread_create(&restartThread, NULL, RestartUnfinishedJobs, this);
}

CommercialFlagger::~CommercialFlagger(void)
{
    gContext->removeListener(this);
}

void CommercialFlagger::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(14) == "LOCAL_COMMFLAG")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            QString action = tokens[1];
            QString chanid = tokens[2];
            QDateTime startts = QDateTime::fromString(tokens[3], Qt::ISODate);
            QString detectionHost = tokens[4];
            QString key = QString("%1_%2").arg(tokens[2]).arg(tokens[3]);

            ProgramInfo *pginfo = ProgramInfo::GetProgramFromRecorded(db,
                                      chanid, startts);

            if ((action == "START") &&
                ((detectionHost == gContext->GetHostName()) ||
                ((detectionHost == "master") && (isMaster))))
            {
                eventlock.lock();
                flaggingList[key] = FLAG_STARTING;
                FlagCommercials(pginfo);
                eventlock.unlock();
            }
            else if (action == "STOP")
            {
                eventlock.lock();
                if (flaggingSems.contains(key))
                    *(flaggingSems[key]) = true;
                eventlock.unlock();
            }
        }
    }
}

void CommercialFlagger::FlagCommercials(ProgramInfo *tmpInfo)
{
    flagThreadStarted = false;

    flagInfo = tmpInfo;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&commercials, &attr, FlagCommercialsThread, this);

    while (!flagThreadStarted)
        usleep(50);
}

void *CommercialFlagger::FlagCommercialsThread(void *param)
{
    CommercialFlagger *theFlagger = (CommercialFlagger *)param;
    theFlagger->DoFlagCommercialsThread();

    return NULL;
}

void CommercialFlagger::DoFlagCommercialsThread(void)
{
    ProgramInfo *program_info = new ProgramInfo(*flagInfo);
    bool abortFlagging = false;
    QString key = QString("%1_%2")
                          .arg(program_info->chanid)
                          .arg(program_info->startts.toString(Qt::ISODate));

    flaggingList[key] = FLAG_RUNNING;
    flaggingSems[key] = &abortFlagging;

    flagThreadStarted = true;

    QString name = QString("commercial%1%2").arg(getpid()).arg(rand());

    MythSqlDatabase *commthread_db = new MythSqlDatabase(name);

    if (!commthread_db || !commthread_db->isOpen())
        return;

    int CommercialSkipCPU = gContext->GetNumSetting("CommercialSkipCPU", 0);
    bool dontSleep = false;

    if (CommercialSkipCPU < 2)
        nice(19);

    if (CommercialSkipCPU)
        dontSleep = true;

    QString filename = program_info->GetRecordFilename(
                                gContext->GetSetting("RecordFilePrefix"));

    RingBuffer *tmprbuf = new RingBuffer(filename, false);

    NuppelVideoPlayer *nvp = new NuppelVideoPlayer(commthread_db, program_info);
    nvp->SetRingBuffer(tmprbuf);

    QString msg = QString( "Started Commercial Flagging for '%1' recorded from "
                           "channel %2 at %3" )
                           .arg(program_info->title.utf8())
                           .arg(program_info->chanid)
                           .arg(program_info->startts.toString());
    VERBOSE( VB_GENERAL, msg );

    int breaksFound = nvp->FlagCommercials(false, dontSleep, &abortFlagging);

    if ( *(flaggingSems[key]) )
        msg = QString( "ABORTED Commercial Flagging for '%1' recorded from "
                       "channel %2 at %3" )
                       .arg(program_info->title.utf8())
                       .arg(program_info->chanid)
                       .arg(program_info->startts.toString());
    else
        msg = QString( "Finished Commercial Flagging for '%1' recorded from "
                       "channel %2 at %3.  Found %4 commercial break(s)." )
                       .arg(program_info->title.utf8())
                       .arg(program_info->chanid)
                       .arg(program_info->startts.toString())
                       .arg(breaksFound);
    VERBOSE( VB_GENERAL, msg );

    flaggingSems.erase(key);

    delete nvp;
    delete tmprbuf;
    delete program_info;
    delete commthread_db;
}

void CommercialFlagger::DoRestartUnfinishedJobs()
{
    sleep(10);

    if ( isMaster )
        ProcessUnflaggedRecordings();
}

void *CommercialFlagger::RestartUnfinishedJobs(void *param)
{
    CommercialFlagger *flagger = (CommercialFlagger *)param;
    flagger->DoRestartUnfinishedJobs();

    return NULL;
}

void CommercialFlagger::ProcessUnflaggedRecordings(void)
{
    QString querystr = QString("SELECT DISTINCT chanid, starttime "
                       "FROM recorded "
                       "WHERE commflagged = %1;").arg(COMM_FLAG_PROCESSING);
    QSqlQuery query = db->exec(querystr);
    QString detectionHost = gContext->GetSetting("CommercialSkipHost");

    if (detectionHost == "Default")
        detectionHost = "master";

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        while (query.next())
        {
            QString message = QString("GLOBAL_COMMFLAG START %1 %2 %3")
                                        .arg(query.value(0).toString())
                                        .arg(query.value(1).toString())
                                        .arg(detectionHost);
            MythEvent me(message);
            gContext->dispatch(me);
        }
    }
}
