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

#include "transcoder.h"

#include "libmythtv/programinfo.h"
#include "encoderlink.h"
#include "libmyth/mythcontext.h"

Transcoder::Transcoder(QMap<int, EncoderLink *> *tvList,
                       QSqlDatabase *ldb)
{
    pthread_mutex_init(&transqlock, NULL);
    m_tvList = tvList;
    db_conn = ldb;

    if (gContext->GetNumSetting("MaxTranscoders", 0)) 
    {
        ClearTranscodeTable(false);
//        RestartTranscoding();
    }

    transcodePoll = false;

    pthread_create(&transpoll, NULL, TranscodePollThread, this);

    while (!transcodePoll)
        usleep(50);
    gContext->addListener(this);
}

Transcoder::~Transcoder(void)
{
    transcodePoll = false;
    pthread_join(transpoll, NULL);
}

void Transcoder::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(24) == "LOCAL_READY_TO_TRANSCODE")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(tokens[1],
                                                                     tokens[2]);
            EnqueueTranscode(pinfo);
        }
    }
}

pid_t Transcoder::Transcode(ProgramInfo *pginfo)
{
    QString fileprefix = gContext->GetFilePrefix();
    QString infile = pginfo->GetRecordFilename(fileprefix);
    QString tmpfile = infile;
    tmpfile += ".tmp";
    pid_t child = 0;

    InitTranscoder(pginfo);
    QString path = "mythtranscode";
    QString command = QString("%1 %2 %3 %4") .arg(path.ascii())
                      .arg(pginfo->chanid)
                      .arg(pginfo->startts.toString("yyyyMMddhhmmss"))
                      .arg("Transcode");
    //Transcode may use the avencoder which is not thread-safe
    child = fork();
    if (child < 0) {
        perror("fork\n");
    }
    else if (child == 0)
    {
        execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        perror("exec");
        exit(1);
    }
    if (setpriority(PRIO_PROCESS, child, 19) != 0)
        cerr << "Failed to set priority of transcoder!\n";
    return child;
}

void Transcoder::InitTranscoder(ProgramInfo *pginfo)
{
     QString query;
     query = QString("INSERT into transcoding (chanid,starttime,isdone) "
                     "VALUES ('%1','%2',0);")
                     .arg(pginfo->chanid)
                     .arg(pginfo->startts.toString("yyyyMMddhhmmss"));
     MythContext::KickDatabase(db_conn);
     db_conn->exec(query);
}

void Transcoder::ClearTranscodeTable(bool skipPartial)
{
     QString query;
     QString fileprefix = gContext->GetFilePrefix();
     MythContext::KickDatabase(db_conn);
     query = QString("SELECT transcoding.chanid,starttime,isdone FROM transcoding;");
     QSqlQuery result = db_conn->exec(query);
     if (result.isActive() && result.numRowsAffected() > 0)
     {
        while (result.next())
        {
            if (!result.value(2).toInt())
            {
                if (!skipPartial)
                {
                     // transcode didn't finish delete partial transcode
                     QDateTime dtstart = result.value(1).toDateTime();
                     ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(
                            result.value(0).toString(),
                            dtstart.toString("yyyyMMddhhmmss")
                     );

                     if (!pinfo) 
                     {
                         cerr << "Bad transcoding info\n";
                         continue;
                     }
                     QString filename = pinfo->GetRecordFilename(fileprefix);
                     filename += ".tmp";
                     cout << "Deleting " << filename << endl;
                     unlink(filename);
                     EnqueueTranscode(pinfo);
                }
            }
            else
            {
                QDateTime dtstart = result.value(1).toDateTime();
                ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(
                     result.value(0).toString(),
                     dtstart.toString("yyyyMMddhhmmss")
                );

                if (!pinfo) {
                    cerr << "Bad transcoding info\n";
                    continue;
                }
                if (!isFileInUse(pinfo))
                {
                    QString filename = pinfo->GetRecordFilename(fileprefix);
                    QString tmpfile = filename;
                    tmpfile += ".tmp";
                    // To save the original file...
                    QString oldfile = filename;
                    oldfile += ".old";
                    rename (filename, oldfile);
                    // unlink(filename);
                    rename (tmpfile, filename);
                    query = QString("DELETE FROM transcoding WHERE "
                                    "chanid = '%1' AND starttime = '%2'")
                                    .arg(result.value(0).toString())
                                    .arg(dtstart.toString("yyyyMMddhhmmss"));
                    db_conn->exec(query);
                }
            }
        }
     }
     if (!skipPartial) 
     {
         query = QString("DELETE from transcoding WHERE isdone = '0';");
         db_conn->exec(query);
     }
}

bool Transcoder::isFileInUse(ProgramInfo *pginfo)
{
    QMap<int, EncoderLink *>::Iterator enciter = m_tvList->begin();
    for (; enciter != m_tvList->end(); ++enciter)
    {
        EncoderLink *elink = enciter.data();
        if (!elink->isLocal())
            continue;
        if (elink->GetState() == kState_WatchingPreRecorded)
            return true;
        if (elink->isParsingCommercials(pginfo))
            return true;
    }
    return false;
}

void Transcoder::EnqueueTranscode(ProgramInfo *pinfo)
{
    ProgramInfo *pinfo_copy = new ProgramInfo(*pinfo);
    pthread_mutex_lock(&transqlock);
    TranscodeQueue.append(pinfo_copy);
    pthread_mutex_unlock(&transqlock);
}

void Transcoder::TranscodePoll()
{
    transcodePoll = true;
    while (transcodePoll) 
    {
        int maxTranscoders = gContext->GetNumSetting("MaxTranscoders", 0);
        if (maxTranscoders) 
        {
            ClearTranscodeTable(true);
            ProgramInfo *pinfo = NULL;
            CheckDoneTranscoders();

            if (Transcoders.count() < (unsigned int)maxTranscoders) 
            {
                pthread_mutex_lock(&transqlock);
                if (!TranscodeQueue.isEmpty()) 
                {
                    pinfo = TranscodeQueue.first();
                    TranscodeQueue.removeFirst();
                }
                pthread_mutex_unlock(&transqlock);
            }

            if (pinfo) 
            {
                pid_t pid = Transcode(pinfo);
                delete pinfo;
                if (pid > 0) 
                    Transcoders.append(pid);
            }
        }

        sleep(30);
    }
}

void Transcoder::CheckDoneTranscoders(void)
{
   QValueList<pid_t>::iterator it = Transcoders.begin();
   int status;
   while(it != Transcoders.end())
       if (waitpid(*it, &status, WNOHANG) != 0)
           it = Transcoders.remove(it);
       else
           ++it;
}

void *Transcoder::TranscodePollThread(void *param)
{
    Transcoder *thetv = (Transcoder *)param;
    thetv->TranscodePoll();

    return NULL;
}

void Transcoder::RestartTranscoding()
{
    QString fileprefix = gContext->GetFilePrefix();
    QString query = QString("SELECT recorded.chanid,starttime from recorded;");
    QSqlQuery result = db_conn->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0)
    {
        while (result.next())
        {
             QDateTime dtstart = result.value(1).toDateTime();
             // This is backwards (calling a query for something we just queried)
             // but it is a simple way to get all recordings
             ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(
                      result.value(0).toString(),
                      dtstart.toString("yyyyMMddhhmmss"));

             if (pinfo == NULL)
                 continue;
             QString path = pinfo->GetRecordFilename(fileprefix);
             struct stat st;
             if(stat(path.ascii(), &st) == 0)
             {
                 // File exists
                 EnqueueTranscode(pinfo);
             }
         }
     }
}

