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
    m_tvList = tvList;
    db_conn = ldb;

    dblock = new QMutex(true);

    CheckTranscodeTable(false);

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

    delete dblock;
}

void Transcoder::customEvent(QCustomEvent *e)
{
    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;
        QString message = me->Message();

        if (message.left(15) == "LOCAL_TRANSCODE")
        {
            message = message.simplifyWhiteSpace();
            QStringList tokens = QStringList::split(" ", message);
            QDateTime startts = QDateTime::fromString(tokens[2], Qt::ISODate);
            dblock->lock();
            ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(db_conn,
                                                                     tokens[1], 
                                                                     startts);
            if (pinfo == NULL) 
            {
                cerr << "Could not read program from database, skipping "
                        "transcoding\n";
                return;
            }
            // cout << "DEBUG: Received transcode message\n";
            if (message.left(20) == "LOCAL_TRANSCODE_STOP")
                StopTranscoder(pinfo);
            else if (message.left(23) == "LOCAL_TRANSCODE_CUTLIST")
                EnqueueTranscode(pinfo, true);
            else
                EnqueueTranscode(pinfo, false);

            dblock->unlock();
            delete pinfo;
        }
    }
}

pid_t Transcoder::Transcode(ProgramInfo *pginfo, bool useCutlist)
{
    QString fileprefix = gContext->GetFilePrefix();
    QString infile = pginfo->GetRecordFilename(fileprefix);
    QString tmpfile = infile;
    tmpfile += ".tmp";
    pid_t child = 0;

    UpdateTranscoder(pginfo, TRANSCODE_LAUNCHED |
                             ((useCutlist) ? TRANSCODE_USE_CUTLIST : 0));
    QString path = "mythtranscode";
    QString command = QString("%1 -c %2 -s %3 -p %4 -d %5").arg(path.ascii())
                      .arg(pginfo->chanid)
                      .arg(pginfo->startts.toString(Qt::ISODate))
                      .arg("Transcode")
                      .arg(useCutlist ? "-l" : "");
    // cout << "DEBUG: " << command.ascii() << endl;
    //Transcode may use the avencoder which is not thread-safe
    child = fork();
    if (child < 0) {
        perror("fork\n");
    }
    else if (child == 0)
    {
        for(int i = 3; i < sysconf(_SC_OPEN_MAX) - 1; ++i)
            close(i);
        execl("/bin/sh", "sh", "-c", command.ascii(), NULL);
        perror("exec");
        _exit(1);
    }
    if (setpriority(PRIO_PROCESS, child, 19) != 0)
        cerr << "Failed to set priority of transcoder!\n";
    return child;
}

void Transcoder::EnqueueTranscode(ProgramInfo *pinfo, bool useCutlist)
{
    QString query = QString("SELECT status FROM transcoding "
                            "WHERE chanid = '%1' AND starttime = '%2';")
                            .arg(pinfo->chanid)
                            .arg(pinfo->startts.toString("yyyyMMddhhmmss"));

    dblock->lock();
    QSqlQuery result = db_conn->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0 && result.next())
    {
        int old_status = result.value(0).toInt();
        if (old_status & ~TRANSCODE_FLAGS < TRANSCODE_LAUNCHED)
            UpdateTranscoder(pinfo, TRANSCODE_QUEUED |
                                    ((useCutlist) ? TRANSCODE_USE_CUTLIST : 0));
        // if the transcode is already started, do nothing
    }
    else
    {
        // only transcode if this program is not already transcoding

        query = QString("INSERT into transcoding "
                        "(chanid,starttime,status,hostname) "
                        "VALUES ('%1','%2','%3','%4');")
                        .arg(pinfo->chanid)
                        .arg(pinfo->startts.toString("yyyyMMddhhmmss"))
                        .arg(TRANSCODE_QUEUED |
                              ((useCutlist) ? TRANSCODE_USE_CUTLIST : 0))
                        .arg(gContext->GetHostName());

        db_conn->exec(query);
    }
    dblock->unlock();
}

void Transcoder::StopTranscoder(ProgramInfo *pinfo)
{
    QString query = QString("SELECT status FROM transcoding "
                            "WHERE chanid = '%1' AND starttime = '%2';")
                            .arg(pinfo->chanid)
                            .arg(pinfo->startts.toString("yyyyMMddhhmmss"));
    dblock->lock();
    MythContext::KickDatabase(db_conn);
    QSqlQuery result = db_conn->exec(query);
    if (result.isActive() && result.numRowsAffected() > 0 && result.next())
    {
        int status = (result.value(0).toInt() & ~TRANSCODE_FLAGS);
        if (status == TRANSCODE_LAUNCHED || status == TRANSCODE_STARTED)
        {
            UpdateTranscoder(pinfo, result.value(0).toInt() | TRANSCODE_STOP);
        }
        else
            DeleteTranscode(pinfo);
    }
    dblock->unlock();
}

void Transcoder::UpdateTranscoder(ProgramInfo *pginfo, int status)
{
    QString query;
    query = QString("UPDATE transcoding "
                     "SET starttime = starttime, status = %1 WHERE "
                     "chanid = '%1' AND starttime = '%2' "
                     "AND hostname = '%3';")
                     .arg(status)
                     .arg(pginfo->chanid)
                     .arg(pginfo->startts.toString("yyyyMMddhhmmss"))
                     .arg(gContext->GetHostName());
     dblock->lock();
     MythContext::KickDatabase(db_conn);
     db_conn->exec(query);
     dblock->unlock();
}

void Transcoder::DeleteTranscode(ProgramInfo *pinfo)
{
     QString query = QString("DELETE FROM transcoding WHERE "
                             "chanid = '%1' AND starttime = '%2' "
                             "AND hostname = '%3';")
                             .arg(pinfo->chanid)
                             .arg(pinfo->startts.toString("yyyyMMddhhmmss"))
                             .arg(gContext->GetHostName());
     dblock->lock();
     MythContext::KickDatabase(db_conn);
     db_conn->exec(query);
     dblock->unlock();
}

struct TranscodeData *Transcoder::CheckTranscodeTable(bool skipPartial)
{
     QString query;
     struct TranscodeData *transData = NULL;
     QString fileprefix = gContext->GetFilePrefix();
     dblock->lock();
     MythContext::KickDatabase(db_conn);
     query = QString("SELECT chanid,starttime,status FROM transcoding "
                     "WHERE hostname = '%1';").arg(gContext->GetHostName());
     QSqlQuery result = db_conn->exec(query);
     if (result.isActive() && result.numRowsAffected() > 0)
     {
        while (result.next())
        {
            int status = (result.value(2).toInt() & ~TRANSCODE_FLAGS);
            int flags = (result.value(2).toInt() & TRANSCODE_FLAGS);

            QDateTime dtstart = result.value(1).toDateTime();
            ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(
                                              db_conn,
                                              result.value(0).toString(),
                                              dtstart);
            if (!pinfo)
            {
                cerr << "Bad transcoding info\n";
                query = QString("DELETE FROM transcoding WHERE "
                                "chanid = '%1' AND starttime = '%2' AND "
                                "hostname = '%3';")
                                .arg(result.value(0).toString())
                                .arg(dtstart.toString("yyyyMMddhhmmss"))
                                .arg(gContext->GetHostName());
                db_conn->exec(query);
                continue;
            }

            if ((flags & TRANSCODE_STOP) &&
                (!skipPartial ||
                 (status != TRANSCODE_LAUNCHED &&
                  status != TRANSCODE_STARTED)))
            {
                // Discard this transcoding, and do nothing
                QString filename = pinfo->GetRecordFilename(fileprefix);
                filename += ".tmp";
                cout << "Deleting " << filename << endl;
                unlink(filename);
                DeleteTranscode(pinfo);
            }
            else if ((!skipPartial && status != TRANSCODE_FINISHED) ||
                     status == TRANSCODE_RETRY)
            {
                // transcode didn't finish delete partial transcode
                QString filename = pinfo->GetRecordFilename(fileprefix);
                filename += ".tmp";
                cout << "Deleting " << filename << endl;
                unlink(filename);
                UpdateTranscoder(pinfo, TRANSCODE_QUEUED |
                                        (flags & TRANSCODE_USE_CUTLIST));
            }
            else if (status == TRANSCODE_FINISHED)
            {
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
                    DeleteTranscode(pinfo);
                    if (flags & TRANSCODE_USE_CUTLIST)
                    {
                        query = QString("DELETE FROM recordedmarkup WHERE "
                                        "chanid = '%1' AND starttime = '%2';")
                                        .arg(result.value(0).toString())
                                        .arg(dtstart.toString("yyyyMMddhhmmss"))
;
                        db_conn->exec(query);
                        query = QString("UPDATE recorded SET cutlist = NULL, "
                                        "bookmark = NULL, "
                                        "starttime = starttime WHERE "
                                        "chanid = '%1' AND starttime = '%2';")
                                        .arg(result.value(0).toString())
                                        .arg(dtstart.toString("yyyyMMddhhmmss"));
                        db_conn->exec(query);
                    } else {
                        query = QString("DELETE FROM recordedmarkup WHERE "
                                        "chanid = '%1' AND starttime = '%2' "
                                        "AND type = '%3';")
                                        .arg(result.value(0).toString())
                                        .arg(dtstart.toString("yyyyMMddhhmmss"))
                                        .arg(MARK_KEYFRAME);
                        db_conn->exec(query);
                    }
                }
            }
            else if (status == TRANSCODE_FAILED)
            {
                 // transcoding failed for an unknown reason,
                 // it is likely that this cannot be recoverable.
                 // Do nothing for now.
            }
            else if (status == TRANSCODE_QUEUED && transData == NULL)
            {
                 transData = new struct TranscodeData;
                 transData->pinfo = pinfo;
                 transData->useCutlist =
                                 (flags & TRANSCODE_USE_CUTLIST ) ? 1 : 0;
                 continue;
            }
            delete pinfo;
        }
    }
    dblock->unlock();
    return transData;
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

void Transcoder::TranscodePoll()
{
    transcodePoll = true;
    struct TranscodeData *transData = NULL;
    while (transcodePoll) 
    {
        transData = CheckTranscodeTable(true);
        CheckDoneTranscoders();

        if (transData && Transcoders.isEmpty())
        {
            if (!transData->useCutlist || !isFileInUse(transData->pinfo))
            {
                pid_t pid = Transcode(transData->pinfo, transData->useCutlist);
                delete transData->pinfo;
                delete transData;
                if (pid > 0)
                    Transcoders.append(pid);
            }
        }

        sleep(60);
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
