#include <qapplication.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qfile.h>
#include <qurl.h>
#include <qthread.h>
#include <qwaitcondition.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <sys/ioctl.h>

#include <list>
#include <iostream>
using namespace std;

#include <sys/stat.h>
#include <sys/vfs.h>

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"

#include "mainserver.h"
#include "scheduler.h"
#include "httpstatus.h"
#include "programinfo.h"

class ProcessRequestThread : public QThread
{
  public:
    ProcessRequestThread(MainServer *ms) { parent = ms; dostuff = false; }
   
    void setup(QStringList list, QStringList tok, PlaybackSock *pb)
    {
        listline = list;
        tokens = tok;
        pbs = pb;
        dostuff = true;
        waitCond.wakeOne();
    }

    void killit(void)
    {
        threadlives = false;
        waitCond.wakeOne();
    }

    virtual void run()
    {
        threadlives = true;

        while (threadlives)
        {
            if (!dostuff)
                waitCond.wait();

            if (dostuff)
            {
                parent->ProcessRequest(listline, tokens, pbs);
                dostuff = false;
                parent->MarkUnused(this);
            }
        }
    }

  private:
    MainServer *parent;

    QStringList listline;
    QStringList tokens;
    PlaybackSock *pbs;   

    QWaitCondition waitCond;
    bool dostuff;
    bool threadlives;
};

MainServer::MainServer(bool master, int port, int statusport, 
                       QMap<int, EncoderLink *> *tvList)
{
    ismaster = master;
    masterServer = NULL;

    encoderList = tvList;

    for (int i = 0; i < 5; i++)
    {
        ProcessRequestThread *prt = new ProcessRequestThread(this);
        prt->start();
        threadPool.push_back(prt);
    }

    recordfileprefix = gContext->GetFilePrefix();

    masterBackendOverride = gContext->GetSetting("MasterBackendOverride", 0);

    mythserver = new MythServer(port);
    connect(mythserver, SIGNAL(newConnect(QSocket *)), 
            SLOT(newConnection(QSocket *)));
    connect(mythserver, SIGNAL(endConnect(QSocket *)), 
            SLOT(endConnection(QSocket *)));

    statusserver = new HttpStatus(this, statusport);    

    gContext->addListener(this);

    if (!ismaster)
    {
        masterServerReconnect = new QTimer(this);
        connect(masterServerReconnect, SIGNAL(timeout()), this, 
                SLOT(reconnectTimeout()));
        masterServerReconnect->start(1000, true);
    }
}

MainServer::~MainServer()
{
    delete mythserver;

    if (statusserver)
        delete statusserver;
    if (masterServerReconnect)
        delete masterServerReconnect;
}

void MainServer::newConnection(QSocket *socket)
{
    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
}

void MainServer::readSocket(void)
{
    QSocket *socket = (QSocket *)sender();

    PlaybackSock *testsock = getPlaybackBySock(socket);
    if (testsock && testsock->isExpectingReply())
        return;

    if (socket->bytesAvailable() > 0)
    {
        QStringList listline;
        ReadStringList(socket, listline);
        QString line = listline[0];

        line = line.simplifyWhiteSpace();
        QStringList tokens = QStringList::split(" ", line);
        QString command = tokens[0];
        if (command == "ANN")
        {
            if (tokens.size() < 3 || tokens.size() > 4)
                cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                     << " Bad ANN query" << endl;
            else
                HandleAnnounce(listline, tokens, socket);
            return;
        }
        else if (command == "DONE")
        {
            HandleDone(socket);
            return;
        }

        PlaybackSock *pbs = getPlaybackBySock(socket);
        if (pbs)
        {
            ProcessRequestThread *prt = NULL;
            while (!prt)
            {
                threadPoolLock.lock();
                if (!threadPool.empty())
                {
                    prt = threadPool.back();
                    threadPool.pop_back();
                }
                threadPoolLock.unlock();

                if (!prt)
                {
                    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         << " waiting for a thread.." << endl;
                    usleep(50);
                }
            }

            prt->setup(listline, tokens, pbs);
        }
        else
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Unknown socket" << endl;    
    }
}

void MainServer::ProcessRequest(QStringList &listline, QStringList &tokens,
                                PlaybackSock *pbs)
{
    QString command = tokens[0];

    if (command == "QUERY_RECORDINGS")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad QUERY_RECORDINGS query" << endl;
        else
            HandleQueryRecordings(tokens[1], pbs);
    }
    else if (command == "QUERY_FREESPACE")
    {
        HandleQueryFreeSpace(pbs);
    }
    else if (command == "QUERY_CHECKFILE")
    {
        HandleQueryCheckFile(listline, pbs);
    }
    else if (command == "QUEUE_TRANSCODE")
    {
        HandleQueueTranscode(listline, pbs);
    }
    else if (command == "DELETE_RECORDING")
    {
        HandleDeleteRecording(listline, pbs);
    }
    else if (command == "QUERY_GETALLPENDING")
    {
        HandleGetPendingRecordings(pbs);
    }
    else if (command == "QUERY_GETCONFLICTING")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad QUERY_GETCONFLICTING" << endl;
        else
            HandleGetConflictingRecordings(listline, tokens[1], pbs);
    }
    else if (command == "GET_FREE_RECORDER")
    {
        HandleGetFreeRecorder(pbs);
    }
    else if (command == "QUERY_RECORDER")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad QUERY_RECORDER" << endl;
        else
            HandleRecorderQuery(listline, tokens, pbs);
    }
    else if (command == "QUERY_REMOTEENCODER")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad QUERY_REMOTEENCODER" << endl;
        else
            HandleRemoteEncoder(listline, tokens, pbs);
    }
    else if (command == "GET_RECORDER_NUM")
    {
        HandleGetRecorderNum(listline, pbs);
    }
    else if (command == "QUERY_FILETRANSFER")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad QUERY_FILETRANSFER" << endl;
        else
            HandleFileTransferQuery(listline, tokens, pbs);
    }
    else if (command == "QUERY_GENPIXMAP")
    {
        HandleGenPreviewPixmap(listline, pbs);
    }
    else if (command == "MESSAGE")
    {
        HandleMessage(listline, pbs);
    } 
    else if (command == "FILL_PROGRAM_INFO")
    {
        HandleFillProgramInfo(listline, pbs);
    }
    else if (command == "BACKEND_MESSAGE")
    {
        QString message = listline[1];
        QString extra = listline[2];
        MythEvent me(message, extra);
        gContext->dispatch(me);
    }
    else
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " unknown command: " << command << endl;
    }
}

void MainServer::MarkUnused(ProcessRequestThread *prt)
{
    threadPoolLock.lock();
    threadPool.push_back(prt);
    threadPoolLock.unlock();
}

void MainServer::customEvent(QCustomEvent *e)
{
    QStringList broadcast;
    bool sendstuff = false;

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;

        if (me->Message().left(6) == "LOCAL_")
            return;

        broadcast = "BACKEND_MESSAGE";
        broadcast << me->Message();
        broadcast << me->ExtraData();
        sendstuff = true;
    }

    if (sendstuff)
    {
        vector<PlaybackSock *>::iterator iter = playbackList.begin();
        for (; iter != playbackList.end(); iter++)
        {
            PlaybackSock *pbs = (*iter);
            if (pbs->wantsEvents())
            {
                WriteStringList(pbs->getSocket(), broadcast);
            }
        }
    }
}

void MainServer::HandleAnnounce(QStringList &slist, QStringList commands, 
                                QSocket *socket)
{
    QStringList retlist = "OK";

    if (commands[1] == "Playback")
    {
        bool wantevents = commands[3].toInt();
        VERBOSE("MainServer::HandleAnnounce Playback");
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
             << " adding: " << commands[2] << " as a player " << wantevents << endl;
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2], wantevents);
        playbackList.push_back(pbs);
    }
    else if (commands[1] == "SlaveBackend")
    {
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
             << " adding: " << commands[2] << " as a slave backend server" << endl;
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2], false);
        pbs->setAsSlaveBackend();
        pbs->setIP(commands[3]);

        QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
        for (; iter != encoderList->end(); ++iter)
        {
            EncoderLink *elink = iter.data();
            if (elink->getHostname() == commands[2])
                elink->setSocket(pbs);
        }

        dblock.lock();
        ScheduledRecording::signalChange(QSqlDatabase::database());
        dblock.unlock();

        playbackList.push_back(pbs);
    }
    else if (commands[1] == "RingBuffer")
    {
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " adding: " << commands[2] << " as a remote ringbuffer" << endl;
        ringBufList.push_back(socket);
        int recnum = commands[3].toInt();

        QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
        if (iter == encoderList->end())
        {
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Unknown encoder." << endl;
            exit(0);
        }

        EncoderLink *enc = iter.data();

        enc->SpawnReadThread(socket);

        if (enc->isLocal())
        {
            int dsp_status, soundcardcaps;
            QSqlQuery query;
            QString audiodevice, audiooutputdevice, querytext;

            querytext = QString("SELECT audiodevice FROM capturecard "
                                "WHERE cardid=%1;").arg(recnum);
            query.exec(querytext);
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiodevice = query.value(0).toString();
            }

            query.exec("SELECT data FROM settings WHERE "
                       "value ='AudioOutputDevice';");
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiooutputdevice = query.value(0).toString();
            }

            if (audiodevice.right(4) == audiooutputdevice.right(4)) //they match
            {
                int dsp_fd = open(audiodevice, O_RDWR | O_NONBLOCK);
                if (dsp_fd != -1)
                {
                    dsp_status = ioctl(dsp_fd, SNDCTL_DSP_GETCAPS,
                                       &soundcardcaps);
                    if (dsp_status != -1)
                    {
                        if (!(soundcardcaps & DSP_CAP_DUPLEX))
                            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                                 << " WARNING:  Capture device " << audiodevice 
                                 << " is not reporting full duplex "
                                 << "capability.\nSee docs/mythtv-HOWTO, "
                                 << "section 18 for more information." << endl;
                    }
                    else 
                        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                             << " Could not get capabilities for "
                             << "audio device: " << audiodevice << endl;
                    close(dsp_fd);
                }
                else 
                {
                    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         << " Could not open audio device: " 
                         << audiodevice << endl;
                    perror("open");
                }
            }
        }
    }
    else if (commands[1] == "FileTransfer")
    {
        VERBOSE("MainServer::HandleAnnounce FileTransfer");
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " adding: " << commands[2] << " as a remote file transfer"
             << endl;
        QUrl qurl = slist[1];
        QString filename = LocalFilePath(qurl);

        FileTransfer *ft = new FileTransfer(filename, socket);

        fileTransferList.push_back(ft);

        retlist << QString::number(socket->socket());
        encodeLongLong(retlist, ft->GetFileSize());
    }

    WriteStringList(socket, retlist);
}

void MainServer::HandleDone(QSocket *socket)
{
    socket->close();
}

void MainServer::HandleQueryRecordings(QString type, PlaybackSock *pbs)
{
    QSqlQuery query;
    QString thequery;

    QString ip = gContext->GetSetting("BackendServerIP");
    QString port = gContext->GetSetting("BackendServerPort");

    dblock.lock();
    MythContext::KickDatabase(QSqlDatabase::database());

    thequery = "SELECT recorded.chanid,starttime,endtime,title,subtitle,"
               "description,hostname,channum,name,callsign FROM recorded "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "ORDER BY starttime";

    if (type == "Delete")
        thequery += " DESC";
    thequery += ";";

    query.exec(thequery);

    dblock.unlock();

    QStringList outputlist;

    QString fileprefix = gContext->GetFilePrefix();

    if (query.isActive() && query.numRowsAffected() > 0)
    {
        outputlist << QString::number(query.numRowsAffected());

        while (query.next())
        {
            ProgramInfo *proginfo = new ProgramInfo;

            proginfo->chanid = query.value(0).toString();
            proginfo->startts = QDateTime::fromString(query.value(1).toString(),
                                                      Qt::ISODate);
            proginfo->endts = QDateTime::fromString(query.value(2).toString(),
                                                    Qt::ISODate);
            proginfo->title = QString::fromUtf8(query.value(3).toString());
            proginfo->subtitle = QString::fromUtf8(query.value(4).toString());
            proginfo->description = QString::fromUtf8(query.value(5).toString());
            proginfo->hostname = query.value(6).toString();

            if (proginfo->hostname.isEmpty() || proginfo->hostname.isNull())
                proginfo->hostname = gContext->GetHostName();

            proginfo->conflicting = false;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            if (!query.value(7).toString().isNull())
            {
                proginfo->chanstr = query.value(7).toString();
                proginfo->channame = query.value(8).toString();
                proginfo->chansign = query.value(9).toString();
            }
            else
            {
                proginfo->chanstr = "#" + proginfo->chanid;
                proginfo->channame = "#" + proginfo->chanid;
                proginfo->chansign = "#" + proginfo->chanid;
            }

            QString lpath = proginfo->GetRecordFilename(fileprefix);
            PlaybackSock *slave = NULL;
            QFile checkFile(lpath);

            if (proginfo->hostname != gContext->GetHostName())
            {
                slave = getSlaveByHostname(proginfo->hostname);
            }

            if ((masterBackendOverride && checkFile.exists()) || 
                (proginfo->hostname == gContext->GetHostName()) ||
                (!slave && checkFile.exists()))
            {
                if (pbs->isLocal())
                    proginfo->pathname = lpath;
                else
                    proginfo->pathname = QString("myth://") + ip + ":" + port
                                         + "/" + proginfo->GetRecordBasename();

                struct stat st;

                long long size = 0;
                if (stat(lpath.ascii(), &st) == 0)
                    size = st.st_size;

                proginfo->filesize = size;
            }
            else
            {
                if (!slave)
                {
                    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         << " Couldn't find backend for: " << proginfo->title
                         << " : " << proginfo->subtitle << endl;
                    proginfo->filesize = 0;
                    proginfo->pathname = "file not found";
                }
                else
                {
                    QString playbackhost = pbs->getHostname();
                    slave->FillProgramInfo(proginfo, playbackhost);
                }
            }

            proginfo->ToStringList(outputlist);

            delete proginfo;
        }
    }
    else
        outputlist << "0";

    WriteStringList(pbs->getSocket(), outputlist);
}

void MainServer::HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs)
{
    QString playbackhost = slist[1];

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 2);

    QString fileprefix = gContext->GetFilePrefix();
    QString lpath = pginfo->GetRecordFilename(fileprefix);
    QString ip = gContext->GetSetting("BackendServerIP");
    QString port = gContext->GetSetting("BackendServerPort");

    if (playbackhost == gContext->GetHostName())
        pginfo->pathname = lpath;
    else
        pginfo->pathname = QString("myth://") + ip + ":" + port
                           + "/" + pginfo->GetRecordBasename();

    struct stat st;

    long long size = 0;
    if (stat(lpath.ascii(), &st) == 0)
        size = st.st_size;

    pginfo->filesize = size;

    QStringList strlist;

    pginfo->ToStringList(strlist);

    delete pginfo;

    WriteStringList(pbs->getSocket(), strlist);
}

void MainServer::HandleQueueTranscode(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    QString message = QString("LOCAL_READY_TO_TRANSCODE %1 %2")
                            .arg(pginfo->chanid)
                            .arg(pginfo->startts.toString(Qt::ISODate));
    MythEvent me(message);
    gContext->dispatch(me);

    QStringList outputlist = "0";
    WriteStringList(pbs->getSocket(), outputlist);
    delete pginfo;
    return;
}

struct DeleteStruct
{
    QString filename;
};

static void *SpawnDelete(void *param)
{
    DeleteStruct *ds = (DeleteStruct *)param;
    QString filename = ds->filename;

    unlink(filename.ascii());
    
    filename += ".png";
    unlink(filename.ascii());
    
    filename = ds->filename;
    filename += ".bookmark";
    unlink(filename.ascii());
    
    filename = ds->filename;
    filename += ".cutlist";
    unlink(filename.ascii());

    sleep(2);

    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);

    delete ds;

    return NULL;
}

void MainServer::HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        int num = -1;

        if (slave) 
        {
           num = slave->DeleteRecording(pginfo);

           if (num > 0)
              (*encoderList)[num]->StopRecording();

           QStringList outputlist = "0";
           WriteStringList(pbs->getSocket(), outputlist);
           delete pginfo;
           return;
        }
    }

    int recnum = -1;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isLocal() && elink->MatchesRecording(pginfo))
        {
            recnum = iter.key();

            elink->StopRecording();

            while (elink->isBusy())
                usleep(100);
        }
    }

    QString fileprefix = gContext->GetFilePrefix();
    QString filename = pginfo->GetRecordFilename(fileprefix);
    QFile checkFile(filename);

    if (!checkFile.exists())
    {
       cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            << " Strange.  File: " << filename << " doesn't exist." << endl;

       QStringList outputlist;
       outputlist << "BAD: Tried to delete a file that was in "
                     "the database but wasn't on the disk.";
       WriteStringList(pbs->getSocket(), outputlist);
       delete pginfo;
       return;
    }

    QSqlQuery query;
    QString thequery;

    QString startts = pginfo->startts.toString("yyyyMMddhhmm");
    startts += "00";
    QString endts = pginfo->endts.toString("yyyyMMddhhmm");
    endts += "00";

    dblock.lock();

    MythContext::KickDatabase(QSqlDatabase::database());

    thequery = QString("DELETE FROM recorded WHERE chanid = %1 AND title "
                       "= \"%2\" AND starttime = %3 AND endtime = %4;")
                       .arg(pginfo->chanid).arg(pginfo->title.utf8())
                       .arg(startts).arg(endts);

    query.exec(thequery);

    if (!query.isActive())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " DB Error: recorded program deletion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    // now delete any markups for this program
    thequery = QString("DELETE FROM recordedmarkup WHERE chanid = %1 "
                       "AND starttime = %2;")
                       .arg(pginfo->chanid).arg(startts);

    query.exec(thequery);
    if (!query.isActive())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " DB Error: recorded program deletion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    dblock.unlock();

    DeleteStruct *ds = new DeleteStruct;
    ds->filename = filename;

    pthread_t deletethread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&deletethread, &attr, SpawnDelete, ds);

    QStringList outputlist = QString::number(recnum);
    WriteStringList(pbs->getSocket(), outputlist);

    delete pginfo;
}

void MainServer::HandleQueryFreeSpace(PlaybackSock *pbs)
{
    struct statfs statbuf;
    int totalspace = -1, usedspace = -1;
    if (statfs(recordfileprefix.ascii(), &statbuf) == 0) {
        totalspace = statbuf.f_blocks / (1024*1024/statbuf.f_bsize);
        usedspace = (statbuf.f_blocks - statbuf.f_bfree) / 
                    (1024*1024/statbuf.f_bsize);
    }

    if (ismaster)
    {
        vector<PlaybackSock *>::iterator iter = playbackList.begin();
        for (; iter != playbackList.end(); iter++)
        {
            PlaybackSock *pbs = (*iter);
            if (pbs->isSlaveBackend())
            {
                int remtotal = -1, remused = -1;
                pbs->GetFreeSpace(remtotal, remused);
                totalspace += remtotal;
                usedspace += remused;
            }
        }
    }

    QStringList strlist;
    QString filename = gContext->GetSetting("RecordFilePrefix") + "/nfslockfile.lock";
    QFile checkFile(filename);

    if (!ismaster)
       if (checkFile.exists())  //found the lockfile, so don't count the space
       {
         totalspace = 0;
         usedspace = 0;
       }

    strlist << QString::number(totalspace) << QString::number(usedspace);
    WriteStringList(pbs->getSocket(), strlist);
}

void MainServer::HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    int exists = 0;

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        if (slave) 
        {
             exists = slave->CheckFile(pginfo);

             QStringList outputlist = QString::number(exists);
             WriteStringList(pbs->getSocket(), outputlist);
             delete pginfo;
             return;
        }
    }

    QUrl qurl(pginfo->pathname);
    QString cpath = LocalFilePath(qurl);
    QFile checkFile(cpath);

    if (checkFile.exists() == true)
        exists = 1;

    QStringList strlist = QString::number(exists);
    WriteStringList(pbs->getSocket(), strlist);

    delete pginfo;
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs)
{
    dblock.lock();

    MythContext::KickDatabase(QSqlDatabase::database());

    Scheduler *sched = new Scheduler(false, encoderList, 
                                     QSqlDatabase::database());

    bool conflicts = sched->FillRecordLists(false);
    list<ProgramInfo *> *recordinglist = sched->getAllPending();

    dblock.unlock();

    QStringList strlist;

    strlist << QString::number(conflicts);
    strlist << QString::number(recordinglist->size());

    list<ProgramInfo *>::iterator iter = recordinglist->begin();
    for (; iter != recordinglist->end(); iter++)
        (*iter)->ToStringList(strlist);

    WriteStringList(pbs->getSocket(), strlist);

    delete sched;
}

void MainServer::HandleGetConflictingRecordings(QStringList &slist,
                                                QString purge,
                                                PlaybackSock *pbs)
{
    dblock.lock();

    MythContext::KickDatabase(QSqlDatabase::database());

    Scheduler *sched = new Scheduler(false, encoderList, 
                                     QSqlDatabase::database());

    bool removenonplaying = purge.toInt();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    sched->FillRecordLists(false);

    list<ProgramInfo *> *conflictlist = sched->getConflicting(pginfo, 
                                                              removenonplaying);

    dblock.unlock();

    QStringList strlist = QString::number(conflictlist->size());

    list<ProgramInfo *>::iterator iter = conflictlist->begin();
    for (; iter != conflictlist->end(); iter++)
        (*iter)->ToStringList(strlist); 

    WriteStringList(pbs->getSocket(), strlist);

    delete sched;
    delete pginfo;
}

void MainServer::HandleGetFreeRecorder(PlaybackSock *pbs)
{
    QStringList strlist;
    int retval = -1;

    QString pbshost = pbs->getHostname();

    EncoderLink *encoder = NULL;
    QString enchost;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isLocal())
            enchost = gContext->GetHostName();
        else
            enchost = elink->getHostname();

        if (enchost == pbshost && elink->isConnected() && !elink->isBusy())
        {
            encoder = elink;
            retval = iter.key();
            break;
        }
        if (retval == -1 && elink->isConnected() && !elink->isBusy())
        {
            encoder = elink;
            retval = iter.key();
        }
    }

    strlist << QString::number(retval);
        
    if (encoder)
    {
        if (encoder->isLocal())
        {
            strlist << gContext->GetSetting("BackendServerIP");
            strlist << gContext->GetSetting("BackendServerPort");
        }
        else
        {
            strlist << gContext->GetSettingOnHost("BackendServerIP", 
                                                  encoder->getHostname(),
                                                  "nohostname");
            strlist << gContext->GetSettingOnHost("BackendServerPort", 
                                                  encoder->getHostname(), "-1");
        }
    }
    else
    {
        strlist << "nohost";
        strlist << "-1";
    }

    WriteStringList(pbs->getSocket(), strlist);
}

void MainServer::HandleRecorderQuery(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    int recnum = commands[1].toInt();

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown encoder." << endl;
        exit(0);
    }

    EncoderLink *enc = iter.data();  

    QString command = slist[1];

    QStringList retlist;

    if (command == "IS_RECORDING")
    {
        retlist << QString::number((int)enc->IsReallyRecording());
    }
    else if (command == "GET_FRAMERATE")
    {
        retlist << QString::number(enc->GetFramerate());
    }
    else if (command == "GET_FRAMES_WRITTEN")
    {
        long long value = enc->GetFramesWritten();
        encodeLongLong(retlist, value);
    }
    else if (command == "GET_FILE_POSITION")
    {
        long long value = enc->GetFilePosition();
        encodeLongLong(retlist, value);
    }
    else if (command == "GET_FREE_SPACE")
    {
        long long pos = decodeLongLong(slist, 2);

        long long value = enc->GetFreeSpace(pos);
        encodeLongLong(retlist, value);
    }
    else if (command == "GET_KEYFRAME_POS")
    {
        long long desired = decodeLongLong(slist, 2);

        long long value = enc->GetKeyframePosition(desired);
        encodeLongLong(retlist, value);
    }
    else if (command == "FILL_POSITION_MAP")
    {
        int start = slist[2].toInt();
        int end   = slist[3].toInt();

        for (int keynum = start; keynum <= end; keynum++)
        {
            long long value = enc->GetKeyframePosition(keynum);
            encodeLongLong(retlist, value);
        }
    }
    else if (command == "SETUP_RING_BUFFER")
    {
        bool pip = slist[2].toInt();

        QString path = "";
        long long filesize = 0;
        long long fillamount = 0;

        enc->SetupRingBuffer(path, filesize, fillamount, pip);

        QString ip = gContext->GetSetting("BackendServerIP");
        QString port = gContext->GetSetting("BackendServerPort");
        QString url = QString("rbuf://") + ip + ":" + port + path;

        retlist << url;
        encodeLongLong(retlist, filesize);
        encodeLongLong(retlist, fillamount);
    }
    else if (command == "TRIGGER_RECORDING_TRANSITION")
    {
        enc->TriggerRecordingTransition();
        retlist << "ok";
    }
    else if (command == "STOP_PLAYING")
    {
        enc->StopPlaying();
        retlist << "ok";
    }
    else if (command == "SPAWN_LIVETV")
    {
        enc->SpawnLiveTV();
        retlist << "ok";
    }
    else if (command == "STOP_LIVETV")
    {
        enc->StopLiveTV();
        retlist << "ok";
    }
    else if (command == "PAUSE")
    {
        enc->PauseRecorder();
        retlist << "ok";
    }
    else if (command == "FINISH_RECORDING")
    {
        enc->FinishRecording();
        retlist << "ok";
    }
    else if (command == "TOGGLE_INPUTS")
    {
        enc->ToggleInputs();
        retlist << "ok";
    }
    else if (command == "TOGGLE_CHANNEL_FAVORITE")
    {
        enc->ToggleChannelFavorite();
        retlist << "ok";
    }
    else if (command == "CHANGE_CHANNEL")
    {
        int direction = slist[2].toInt(); 
        enc->ChangeChannel(direction);
        retlist << "ok";
    }
    else if (command == "SET_CHANNEL")
    {
        QString name = slist[2];
        enc->SetChannel(name);
        retlist << "ok";
    }
    else if (command == "CHANGE_COLOUR")
    {
        bool up = slist[2].toInt(); 
        retlist << QString::number(enc->ChangeColour(up));
    }
    else if (command == "CHANGE_CONTRAST")
    {
        bool up = slist[2].toInt(); 
        retlist << QString::number(enc->ChangeContrast(up));
    }
    else if (command == "CHANGE_BRIGHTNESS")
    {
        bool up = slist[2].toInt(); 
        retlist << QString::number(enc->ChangeBrightness(up));
    }
    else if (command == "CHANGE_DEINTERLACER")
    {
        int deinterlacer_mode = slist[2].toInt();
        enc->ChangeDeinterlacer(deinterlacer_mode);
        retlist << "OK";
    }
    else if (command == "CHECK_CHANNEL")
    {
        QString name = slist[2];
        retlist << QString::number((int)(enc->CheckChannel(name)));
    }
    else if (command == "GET_NEXT_PROGRAM_INFO")
    {
        QString channelname = slist[2];
        QString chanid = slist[3];
        int direction = slist[4].toInt();
        QString starttime = slist[5];

        QString title = "", subtitle = "", desc = "", category = "";
        QString endtime = "", callsign = "", iconpath = "";

        enc->GetNextProgram(direction,
                            title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname, chanid);

        if (title == "")
            title = " ";
        if (subtitle == "")
            subtitle = " ";
        if (desc == "")
            desc = " ";
        if (category == "")
            category = " ";
        if (starttime == "")
            starttime = " ";
        if (endtime == "")
            endtime = " ";
        if (callsign == "")
            callsign = " ";
        if (iconpath == "")
            iconpath = " ";
        if (channelname == "")
            channelname = " ";
        if (chanid == "")
            chanid = " ";

        retlist << title;
        retlist << subtitle;
        retlist << desc;
        retlist << category;
        retlist << starttime;
        retlist << endtime;
        retlist << callsign;
        retlist << iconpath;
        retlist << channelname;
        retlist << chanid;
    }
    else if (command == "GET_PROGRAM_INFO")
    {
        QString title = "", subtitle = "", desc = "", category = "";
        QString starttime = "", endtime = "", callsign = "", iconpath = "";
        QString channelname = "", chanid = "";

        enc->GetChannelInfo(title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname, chanid);

        if (title == "")
            title = " ";
        if (subtitle == "")
            subtitle = " ";
        if (desc == "")
            desc = " ";
        if (category == "")
            category = " ";
        if (starttime == "")
            starttime = " ";
        if (endtime == "")
            endtime = " ";
        if (callsign == "")
            callsign = " ";
        if (iconpath == "")
            iconpath = " ";
        if (channelname == "")
            channelname = " ";
        if (chanid == "")
            chanid = " ";

        retlist << title;
        retlist << subtitle;
        retlist << desc;
        retlist << category;
        retlist << starttime;
        retlist << endtime;
        retlist << callsign;
        retlist << iconpath;
        retlist << channelname;
        retlist << chanid;
    }
    else if (command == "GET_INPUT_NAME")
    {
        QString name = slist[2];

        QString input = "";
        enc->GetInputName(input);

        retlist << input;
    }
    else if (command == "PAUSE_RINGBUF")
    {
        enc->PauseRingBuffer();

        retlist << "OK";
    }
    else if (command == "UNPAUSE_RINGBUF")
    {
        enc->UnpauseRingBuffer();
       
        retlist << "OK";
    }
    else if (command == "PAUSECLEAR_RINGBUF")
    {
        enc->PauseClearRingBuffer();

        retlist << "OK";
    }
    else if (command == "REQUEST_BLOCK_RINGBUF")
    {
        int size = slist[2].toInt();

        enc->RequestRingBufferBlock(size);
        retlist << QString::number(false);
    }
    else if (command == "SEEK_RINGBUF")
    {
        long long pos = decodeLongLong(slist, 2);
        int whence = slist[4].toInt();
        long long curpos = decodeLongLong(slist, 5);

        long long ret = enc->SeekRingBuffer(curpos, pos, whence);
        encodeLongLong(retlist, ret);
    }
    else if (command == "DONE_RINGBUF")
    {
        enc->KillReadThread();

        retlist << "OK";
    }
    else
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " unknown command: " << command << endl;
        retlist << "ok";
    }

    WriteStringList(pbs->getSocket(), retlist);    
}

void MainServer::HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    int recnum = commands[1].toInt();

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown encoder." << endl;
        exit(0);
    }

    EncoderLink *enc = iter.data();

    QString command = slist[1];

    QStringList retlist;

    if (command == "GET_STATE")
    {
        retlist << QString::number((int)enc->GetState());
    }
    else if (command == "MATCHES_RECORDING")
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(slist, 2);

        retlist << QString::number((int)enc->MatchesRecording(pginfo));

        delete pginfo;
    }
    else if (command == "START_RECORDING")
    {
        ProgramInfo *pginfo = new ProgramInfo();
        pginfo->FromStringList(slist, 2);
  
        enc->StartRecording(pginfo);

        retlist << "OK";
        delete pginfo;
    }

    WriteStringList(pbs->getSocket(), retlist);
}

void MainServer::HandleFileTransferQuery(QStringList &slist, 
                                         QStringList &commands,
                                         PlaybackSock *pbs)
{
    int recnum = commands[1].toInt();

    FileTransfer *ft = getFileTransferByID(recnum);
    if (!ft)
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown file transfer socket" << endl;
        return;
    }

    QString command = slist[1];

    QStringList retlist;

    if (command == "IS_OPEN")
    {
        bool isopen = ft->isOpen();

        retlist << QString::number(isopen);
    }
    else if (command == "DONE")
    {
        ft->Stop();
        retlist << "ok";
    }
    else if (command == "PAUSE")
    {
        ft->Pause();
        retlist << "ok";
    }
    else if (command == "UNPAUSE")
    {
        ft->Unpause();
        retlist << "ok";
    }
    else if (command == "REQUEST_BLOCK")
    {
        int size = slist[2].toInt();

        retlist << QString::number(ft->RequestBlock(size));
    }
    else if (command == "SEEK")
    {
        long long pos = decodeLongLong(slist, 2);
        int whence = slist[4].toInt();
        long long curpos = decodeLongLong(slist, 5);

        long long ret = ft->Seek(curpos, pos, whence);
        encodeLongLong(retlist, ret);
    }
    else 
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown command: " << command << endl;
        retlist << "ok";
    }

    WriteStringList(pbs->getSocket(), retlist);
}

void MainServer::HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs)
{
    int retval = -1;

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    EncoderLink *encoder = NULL;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isConnected() && elink->MatchesRecording(pginfo))
        {
            retval = iter.key();
            encoder = elink;
        }
    }
    
    QStringList strlist = QString::number(retval);

    if (encoder)
    {
        if (encoder->isLocal())
        {
            strlist << gContext->GetSetting("BackendServerIP");
            strlist << gContext->GetSetting("BackendServerPort");
        }
        else
        {
            strlist << gContext->GetSettingOnHost("BackendServerIP",
                                                  encoder->getHostname(),
                                                  "nohostname");
            strlist << gContext->GetSettingOnHost("BackendServerPort",
                                                  encoder->getHostname(), "-1");
        }
    }
    else
    {
        strlist << "nohost";
        strlist << "-1";
    }

    WriteStringList(pbs->getSocket(), strlist);    
    delete pginfo;
}

void MainServer::HandleMessage(QStringList &slist, PlaybackSock *pbs)
{
    QString message = slist[1];

    MythEvent me(message);
    gContext->dispatch(me);

    QStringList retlist = "OK";

    WriteStringList(pbs->getSocket(), retlist);
}

void MainServer::HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        if (!slave)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Couldn't find backend for: " << pginfo->title
                 << " : " << pginfo->subtitle << endl;
        else
            slave->GenPreviewPixmap(pginfo);

        QStringList outputlist = "OK";
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    if (pginfo->hostname != gContext->GetHostName())
    {
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Somehow we got a request to generate a preview pixmap for: "
             << pginfo->hostname << endl;

        QStringList outputlist = "BAD";
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    QUrl qurl = pginfo->pathname;
    QString filename = qurl.path();

    if (qurl.host() != "")
        filename = LocalFilePath(qurl);

    int len = 0;
    int width = 0, height = 0;

    EncoderLink *elink = encoderList->begin().data();

    unsigned char *data = (unsigned char *)elink->GetScreenGrab(pginfo, 
                                                                filename, 64,
                                                                len, width,
                                                                height);

    if (data)
    {
        QImage img(data, width, height, 32, NULL, 65536 * 65536,
                   QImage::LittleEndian);
        img = img.smoothScale(160, 120);

        filename += ".png";
        img.save(filename.ascii(), "PNG");

        delete [] data;
    }

    QStringList retlist = "OK";
    WriteStringList(pbs->getSocket(), retlist);    

    delete pginfo;
}

void MainServer::endConnection(QSocket *socket)
{
    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        QSocket *sock = (*it)->getSocket();
        if (sock == socket)
        {
            if (ismaster && (*it)->isSlaveBackend())
            {
                cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                     << " Slave backend: " << (*it)->getHostname() 
                     << " has left the building\n";

                QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
                for (; iter != encoderList->end(); ++iter)
                {
                    EncoderLink *elink = iter.data();
                    if (elink->getSocket() == (*it))
                        elink->setSocket(NULL);
                }
                ScheduledRecording::signalChange(QSqlDatabase::database());
            }
            delete (*it);
            playbackList.erase(it);
            return;
        }
    }

    vector<FileTransfer *>::iterator ft = fileTransferList.begin();
    for (; ft != fileTransferList.end(); ++ft)
    {
        QSocket *sock = (*ft)->getSocket();
        if (sock == socket)
        {
            delete (*ft);
            fileTransferList.erase(ft);
            return;
        }
    }

    vector<QSocket *>::iterator rt = ringBufList.begin();
    for (; rt != ringBufList.end(); ++rt)
    {
        QSocket *sock = *rt;
        if (sock == socket)
        {
            QMap<int, EncoderLink *>::iterator i = encoderList->begin();
            for (; i != encoderList->end(); ++i)
            {
                EncoderLink *enc = i.data();
                if (enc->isLocal() && enc->isBusy() && 
                    enc->GetReadThreadSocket() == socket)
                {
                    if (enc->GetState() == kState_WatchingLiveTV)
                    {
                        enc->StopLiveTV();
                    }
                    else if (enc->GetState() == kState_WatchingRecording)
                    {
                        enc->StopPlaying();
                    }
                }
            }

            delete (*rt);
            ringBufList.erase(rt);
            return;
        }
    }

    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
         << " unknown socket" << endl;
}

PlaybackSock *MainServer::getSlaveByHostname(QString &hostname)
{
    if (!ismaster)
        return NULL;

    vector<PlaybackSock *>::iterator iter = playbackList.begin();
    for (; iter != playbackList.end(); iter++)
    {
        PlaybackSock *pbs = (*iter);
        if (pbs->isSlaveBackend() && 
            ((pbs->getHostname() == hostname) || (pbs->getIP() == hostname)))
        {
            return pbs;
        }
    }

    return NULL;
}

PlaybackSock *MainServer::getPlaybackBySock(QSocket *sock)
{
    PlaybackSock *retval = NULL;

    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        if (sock == (*it)->getSocket())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

FileTransfer *MainServer::getFileTransferByID(int id)
{
    FileTransfer *retval = NULL;

    vector<FileTransfer *>::iterator it = fileTransferList.begin();
    for (; it != fileTransferList.end(); ++it)
    {
        if (id == (*it)->getSocket()->socket())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

FileTransfer *MainServer::getFileTransferBySock(QSocket *sock)
{
    FileTransfer *retval = NULL;

    vector<FileTransfer *>::iterator it = fileTransferList.begin();
    for (; it != fileTransferList.end(); ++it)
    {
        if (sock == (*it)->getSocket())
        {
            retval = (*it);
            break;
        }
    }

    return retval;
}

QString MainServer::LocalFilePath(QUrl &url)
{
    QString lpath = url.path();

    if (lpath.section('/', -2, -2) == "channels")
    {
        // This must be an icon request. Check channel.icon to be safe.

        QString querytext;
        QSqlQuery query;

        QString file = lpath.section('/', -1);
        lpath = "";

        querytext = QString("SELECT icon FROM channel "
                            "WHERE icon LIKE '%%1';").arg(file);
        query.exec(querytext);
        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            lpath = query.value(0).toString();
        }
        else
        {
            MythContext::DBError("Icon path", query);
        }
    }
    else
    {
        lpath = lpath.section('/', -1);
        lpath = gContext->GetFilePrefix() + url.path();
    }
    return lpath;
}

bool MainServer::isRingBufSock(QSocket *sock)
{
    vector<QSocket *>::iterator it = ringBufList.begin();
    for (; it != ringBufList.end(); it++)
    {
        if (*it == sock)
            return true;
    }

    return false;
}

void MainServer::masterServerDied(void)
{
    delete masterServer;
    masterServer = NULL;
    masterServerReconnect->start(1000, true);
}

void MainServer::reconnectTimeout(void)
{
    QSocket *masterServerSock = new QSocket();

    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);

    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
         << " - Connecting to master server: " << server << ":" << 
            port << endl;

    masterServerSock->connectToHost(server, port);

    int num = 0;
    while (masterServerSock->state() == QSocket::HostLookup ||
           masterServerSock->state() == QSocket::Connecting)
    {
        qApp->processEvents();
        usleep(50);
        num++;
        if (num > 100)
        {
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " - Connection to master server timed out." << endl;
            masterServerReconnect->start(1000, true);
            delete masterServerSock;
            return;
        }
    }

    if (masterServerSock->state() != QSocket::Connected)
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " - Could not connect to master server." << endl;
        masterServerReconnect->start(1000, true);
        delete masterServerSock;
        return;
    }

    cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
         << " - Connected." << endl;

    QString str = QString("ANN SlaveBackend %1 %2")
                          .arg(gContext->GetHostName())
                          .arg(gContext->GetSetting("BackendServerIP"));

    QStringList strlist = str;
    WriteStringList(masterServerSock, strlist);
    ReadStringList(masterServerSock, strlist);

    connect(masterServerSock, SIGNAL(readyRead()), this, SLOT(readSocket()));
    connect(masterServerSock, SIGNAL(connectionClosed()), this, 
            SLOT(masterServerDied()));

    masterServer = new PlaybackSock(masterServerSock, server, true);
    playbackList.push_back(masterServer);
}

void MainServer::PrintStatus(QSocket *socket)
{
    QTextStream os(socket);
    os.setEncoding(QTextStream::UnicodeUTF8);

    os << "HTTP/1.0 200 Ok\r\n"
       << "Content-Type: text/html; charset=\"utf-8\"\r\n"
       << "\r\n";

    os << "<HTTP>\r\n"
       << "<HEAD>\r\n"
       << "  <TITLE>MythTV Status</TITLE>\r\n"
       << "</HEAD>\r\n"
       << "<BODY>\r\n";

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isLocal())
        {
            os << "Encoder: " << elink->getCardId() << " is local.\r\n";
        }
        else
        {
            os << "Encoder: " << elink->getCardId() << " is remote and ";
            if (elink->isConnected())
                os << "is connected\r\n";
            else
                os << "is not connected\r\n";
        }

        os << "<br>\r\n";
    }

    os << "</BODY>\r\n"
       << "</HTTP>\r\n";
}
