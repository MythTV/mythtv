#include <qapplication.h>
#include <qsqldatabase.h>
#include <qdatetime.h>
#include <qfile.h>
#include <qdir.h>
#include <qurl.h>
#include <qthread.h>
#include <qwaitcondition.h>
#include <qregexp.h>

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
   
    void setup(QSocket *sock)
    {
        socket = sock;
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
                parent->ProcessRequest(socket);
                dostuff = false;
                parent->MarkUnused(this);
            }
        }
    }

  private:
    MainServer *parent;

    QSocket *socket;

    QWaitCondition waitCond;
    bool dostuff;
    bool threadlives;
};

MainServer::MainServer(bool master, int port, int statusport, 
                       QMap<int, EncoderLink *> *tvList,
                       QSqlDatabase *db, Scheduler *sched)
{
    m_db = db;
    m_sched = sched;

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

    readReadyLock.lock();

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

    prt->setup(socket);

    readReadyLock.unlock();
}

void MainServer::ProcessRequest(QSocket *sock)
{
    if (sock->bytesAvailable() <= 0)
        return;

    QStringList listline;
    if (!ReadStringList(sock, listline))
        return;

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
            HandleAnnounce(listline, tokens, sock);
        return;
    }
    else if (command == "DONE")
    {
        HandleDone(sock);
        return;
    }

    PlaybackSock *pbs = getPlaybackBySock(sock);
    if (!pbs)
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown socket" << endl;
        return;
    }

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
        HandleQueueTranscode(listline, pbs, TRANSCODE_QUEUED);
    }
    else if (command == "QUEUE_TRANSCODE_CUTLIST")
    {
        HandleQueueTranscode(listline, pbs, TRANSCODE_QUEUED |
                                            TRANSCODE_USE_CUTLIST);
    }
    else if (command == "QUEUE_TRANSCODE_STOP")
    {
        HandleQueueTranscode(listline, pbs, TRANSCODE_STOP);
    }
    else if (command == "STOP_RECORDING")
    {
        HandleStopRecording(listline, pbs);
    }
    else if (command == "CHECK_RECORDING")
    {
        HandleCheckRecordingActive(listline, pbs);
    }
    else if (command == "DELETE_RECORDING")
    {
        HandleDeleteRecording(listline, pbs);
    }
    else if (command == "QUERY_GETALLPENDING")
    {
        HandleGetPendingRecordings(pbs);
    }
    else if (command == "QUERY_GETALLSCHEDULED")
    {
        HandleGetScheduledRecordings(pbs);
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
    else if (command == "GET_RECORDER_FROM_NUM")
    {
        HandleGetRecorderFromNum(listline, pbs);
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
    else if (command == "QUERY_ISRECORDING") 
    {
        HandleIsRecording(listline, pbs);
    }
    else if (command == "MESSAGE")
    {
        HandleMessage(listline, pbs);
    } 
    else if (command == "FILL_PROGRAM_INFO")
    {
        HandleFillProgramInfo(listline, pbs);
    }
    else if (command == "LOCK_TUNER")
    {
        HandleLockTuner(pbs);
    }
    else if (command == "FREE_TUNER")
    {
        if (tokens.size() != 2)
            cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                 << " Bad FREE_TUNER query" << endl;
        else
            HandleFreeTuner(tokens[1].toInt(), pbs);
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

        if (me->Message().left(11) == "AUTO_EXPIRE")
        {
            QStringList tokens = QStringList::split(" ", me->Message());
            QDateTime startts = QDateTime::fromString(tokens[2], Qt::ISODate);

            dblock.lock();
            ProgramInfo *pinfo = ProgramInfo::GetProgramFromRecorded(m_db,
                                                                     tokens[1],
                                                                     startts);
            dblock.unlock();
            if (pinfo)
            {
                DoHandleDeleteRecording(pinfo, NULL);
            }
            else
            {
                cerr << "Cannot find program info for '" << me->Message()
                     << "', while attempting to Auto-Expire." << endl;
            }

            return;
        }

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
        VERBOSE(VB_GENERAL, "MainServer::HandleAnnounce Playback");
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
             << " adding: " << commands[2] << " as a player " 
             << wantevents << endl;
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2], wantevents);
        playbackList.push_back(pbs);
    }
    else if (commands[1] == "SlaveBackend")
    {
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") 
             << " adding: " << commands[2] << " as a slave backend server" 
             << endl;
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
        ScheduledRecording::signalChange(m_db);
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

        enc->SetReadThreadSock(socket);

        if (enc->isLocal())
        {
            dblock.lock();

            int dsp_status, soundcardcaps;
            QString audiodevice, audiooutputdevice, querytext;

            querytext = QString("SELECT audiodevice FROM capturecard "
                                "WHERE cardid=%1;").arg(recnum);
            QSqlQuery query = m_db->exec(querytext);
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiodevice = query.value(0).toString();
            }

            query = m_db->exec("SELECT data FROM settings WHERE "
                               "value ='AudioOutputDevice';");
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiooutputdevice = query.value(0).toString();
            }

            dblock.unlock();

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
                                 << "section 20.13 for more information." << endl;
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
        VERBOSE(VB_GENERAL, "MainServer::HandleAnnounce FileTransfer");
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

void MainServer::SendResponse(QSocket *socket, QStringList &commands)
{
    if (getPlaybackBySock(socket) || getFileTransferBySock(socket))
    {
        WriteStringList(socket, commands);
    }
    else
    {
        cerr << "Unable to write to client socket, as it's no longer there\n";
    }
}

void MainServer::HandleQueryRecordings(QString type, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();
    bool islocal = pbs->isLocal();
    QString playbackhost = pbs->getHostname();

    QString thequery;

    QString ip = gContext->GetSetting("BackendServerIP");
    QString port = gContext->GetSetting("BackendServerPort");

    dblock.lock();
    MythContext::KickDatabase(m_db);

    thequery = "SELECT recorded.chanid,starttime,endtime,title,subtitle,"
               "description,hostname,channum,name,callsign FROM recorded "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "ORDER BY starttime";

    if (type == "Delete")
        thequery += " DESC";
    thequery += ";";

    QSqlQuery query = m_db->exec(thequery);

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
            proginfo->recstartts = proginfo->startts;
            proginfo->recendts = proginfo->endts;
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
                if (islocal)
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
                    slave->FillProgramInfo(proginfo, playbackhost);
                }
            }

            proginfo->ToStringList(outputlist);

            delete proginfo;
        }
    }
    else
        outputlist << "0";

    dblock.unlock();

    SendResponse(pbssock, outputlist);
}

void MainServer::HandleFillProgramInfo(QStringList &slist, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

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

    SendResponse(pbssock, strlist);
}

void MainServer::HandleQueueTranscode(QStringList &slist, PlaybackSock *pbs,
                                      int state)
{
    QSocket *pbssock = pbs->getSocket();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    QString cmd;
    if (state & TRANSCODE_STOP)
       cmd = QString("LOCAL_TRANSCODE_STOP");
    else if (state & TRANSCODE_USE_CUTLIST)
       cmd = QString("LOCAL_TRANSCODE_CUTLIST");
    else
       cmd = QString("LOCAL_TRANSCODE");

    QString message = QString("%1 %2 %3")
                            .arg(cmd)
                            .arg(pginfo->chanid)
                            .arg(pginfo->recstartts.toString(Qt::ISODate));
    MythEvent me(message);
    gContext->dispatch(me);

    QStringList outputlist = "0";
    SendResponse(pbssock, outputlist);
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

void MainServer::HandleCheckRecordingActive(QStringList &slist, 
                                            PlaybackSock *pbs)
{
    QSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    int result = 0;

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        if (slave)
            result = slave->CheckRecordingActive(pginfo);
    }
    else
    {
        QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
        for (; iter != encoderList->end(); ++iter)
        {
            EncoderLink *elink = iter.data();

            if (elink->isLocal() && elink->MatchesRecording(pginfo))
                result = iter.key();
        }
    }

    QStringList outputlist = QString::number(result);
    if (pbssock)
        SendResponse(pbssock, outputlist);

    delete pginfo;
    return;
}

void MainServer::HandleStopRecording(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    DoHandleStopRecording(pginfo, pbs);
}

void MainServer::DoHandleStopRecording(ProgramInfo *pginfo, PlaybackSock *pbs)
{
    QSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        int num = -1;

        if (slave)
        {
            num = slave->StopRecording(pginfo);

            if (num > 0)
                (*encoderList)[num]->StopRecording();

            if (pbssock)
            {
                QStringList outputlist = "0";
                SendResponse(pbssock, outputlist);
            }
 
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

            while (elink->IsBusyRecording() ||
                   elink->GetState() == kState_ChangingState)
            {
                usleep(100);
            }
        }
    }

    if (QDateTime::currentDateTime() > pginfo->recendts)
    { //don't update filename if in overrecord
        if (pbssock)
        {
            QStringList outputlist = QString::number(recnum);
            SendResponse(pbssock, outputlist);
        }
        delete pginfo;
        return;
    }

    QString thequery;
    QSqlQuery query;

    dblock.lock();

    MythContext::KickDatabase(m_db);

    QString startts = pginfo->recstartts.toString("yyyyMMddhhmm");
    startts += "00";
    QString endts = pginfo->endts.toString("yyyyMMddhhmm");
    endts += "00";
    QDateTime now(QDateTime::currentDateTime());
    QString newendts = now.toString("yyyyMMddhhmm");
    newendts += "00";

    // Set the recorded end time to the current time
    // (we're stopping the recording so it'll never get to its originally 
    // intended end time)
    thequery = QString("UPDATE recorded SET starttime = %1, endtime = %2 "
                       "WHERE chanid = %3 AND title = \"%4\" AND "
                       "starttime = %5 AND endtime = %6;")
                       .arg(startts).arg(newendts).arg(pginfo->chanid)
                       .arg(pginfo->title.utf8()).arg(startts).arg(endts);

    query = m_db->exec(thequery);

    if (!query.isActive())
    {
        MythContext::DBError("Stop recording program update", query);
    }

    dblock.unlock();

    // If we change the recording times we must also adjust the .nuv file's name
    QString fileprefix = gContext->GetFilePrefix();
    QString oldfilename = pginfo->GetRecordFilename(fileprefix);
    pginfo->recendts = now;
    QString newfilename = pginfo->GetRecordFilename(fileprefix);
    QFile checkFile(oldfilename);

    if (checkFile.exists())
    {
        if (!QDir::root().rename(oldfilename, newfilename, TRUE))
        {
            VERBOSE(VB_IMPORTANT, QString("Could not rename: %1 to %2")
                                         .arg(oldfilename).arg(newfilename));
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, QString("File: %1 does not exist.")
                                     .arg(oldfilename));

        if (pbssock)
        {
            QStringList outputlist;
            outputlist << "BAD: Tried to rename a file that was in "
                          "the database but wasn't on the disk.";
            SendResponse(pbssock, outputlist);

            delete pginfo;
            return;
        }
    }

    if (pbssock)
    {
        QStringList outputlist = QString::number(recnum);
        SendResponse(pbssock, outputlist);
    }

    delete pginfo;
}

void MainServer::HandleDeleteRecording(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    DoHandleDeleteRecording(pginfo, pbs);
}

void MainServer::DoHandleDeleteRecording(ProgramInfo *pginfo, PlaybackSock *pbs)
{
    QSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    if (ismaster && pginfo->hostname != gContext->GetHostName())
    {
        PlaybackSock *slave = getSlaveByHostname(pginfo->hostname);

        int num = -1;

        if (slave) 
        {
           num = slave->DeleteRecording(pginfo);

           if (num > 0)
              (*encoderList)[num]->StopRecording();

           if (pbssock)
           {
               QStringList outputlist = "0";
               SendResponse(pbssock, outputlist);
           }

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

            while (elink->IsBusyRecording() || 
                   elink->GetState() == kState_ChangingState)
            {
                usleep(100);
            }
        }
    }

    QString thequery;

    QString startts = pginfo->recstartts.toString("yyyyMMddhhmm");
    startts += "00";
    QString endts = pginfo->recendts.toString("yyyyMMddhhmm");
    endts += "00";

    dblock.lock();

    MythContext::KickDatabase(m_db);

    thequery = QString("DELETE FROM recorded WHERE chanid = %1 AND title "
                       "= \"%2\" AND starttime = %3 AND endtime = %4;")
                       .arg(pginfo->chanid).arg(pginfo->title.utf8())
                       .arg(startts).arg(endts);

    QSqlQuery query = m_db->exec(thequery);

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

    query = m_db->exec(thequery);
    if (!query.isActive())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " DB Error: recorded program deletion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    dblock.unlock();

    QString fileprefix = gContext->GetFilePrefix();
    QString filename = pginfo->GetRecordFilename(fileprefix);
    QFile checkFile(filename);

    if (checkFile.exists())
    {
        DeleteStruct *ds = new DeleteStruct;
        ds->filename = filename;

        pthread_t deletethread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&deletethread, &attr, SpawnDelete, ds);
    }
    else
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Strange.  File: " << filename << " doesn't exist." << endl;

        if (pbssock)
        {
            QStringList outputlist;
            outputlist << "BAD: Tried to delete a file that was in "
                          "the database but wasn't on the disk.";
            SendResponse(pbssock, outputlist);

            delete pginfo;
            return;
        }
    }

    if (pbssock)
    {
        QStringList outputlist = QString::number(recnum);
        SendResponse(pbssock, outputlist);
    }

    delete pginfo;
}

void MainServer::getFreeSpace(int &totalspace, int &usedspace)
{
    struct statfs statbuf;
    if (statfs(recordfileprefix.ascii(), &statbuf) == 0) 
    {
        // total space available to user is total blocks - reserved blocks
        totalspace = (statbuf.f_blocks - (statbuf.f_bfree - statbuf.f_bavail)) /
                      (1024*1024/statbuf.f_bsize);
        usedspace = (statbuf.f_blocks - statbuf.f_bavail) /
                     (1024*1024/statbuf.f_bsize);
    }

    if (ismaster)
    {
        vector<PlaybackSock *>::iterator iter = playbackList.begin();
        for (; iter != playbackList.end(); iter++)
        {
            PlaybackSock *lsock = (*iter);
            if (lsock->isSlaveBackend())
            {
                int remtotal = -1, remused = -1;
                lsock->GetFreeSpace(remtotal, remused);
                totalspace += remtotal;
                usedspace += remused;
            }
        }
    }

    QString filename = gContext->GetSetting("RecordFilePrefix") + 
                                             "/nfslockfile.lock";
    QFile checkFile(filename);

    if (!ismaster)
    {
        if (checkFile.exists())  //found the lockfile, so don't count the space
        {
            totalspace = 0;
            usedspace = 0;
        }
    }
}

void MainServer::HandleQueryFreeSpace(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    int totalspace = -1, usedspace = -1;

    getFreeSpace(totalspace, usedspace);

    QStringList strlist;
    strlist << QString::number(totalspace) << QString::number(usedspace);
    SendResponse(pbssock, strlist);
}

void MainServer::HandleQueryCheckFile(QStringList &slist, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

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
             SendResponse(pbssock, outputlist);
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
    SendResponse(pbssock, strlist);

    delete pginfo;
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strList;

    m_sched->getAllPending(&strList);

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetScheduledRecordings(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strList;

    m_sched->getAllScheduled(&strList);

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetConflictingRecordings(QStringList &slist,
                                                QString purge,
                                                PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    bool removenonplaying = purge.toInt();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    QStringList strlist;

    m_sched->getConflicting(pginfo, removenonplaying, &strlist);

    SendResponse(pbssock, strlist);

    delete pginfo;
}

void MainServer::HandleLockTuner(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();
    QString pbshost = pbs->getHostname();

    QStringList strlist;
    int retval;
    
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

        if ((enchost == pbshost) &&
            (elink->isConnected()) &&
            (!elink->IsBusy()) &&
            (!elink->isTunerLocked()))
        {
            encoder = elink;
            break;
        }
    }
    
    if (encoder)
    {
        retval = encoder->LockTuner();

        if (retval != -1)
        {
            QString msg = QString("Cardid %1 LOCKed for external use on %2.")
                                  .arg(retval).arg(pbshost);
            VERBOSE(VB_GENERAL, msg);

            QString querystr = QString("SELECT videodevice, audiodevice, "
                                            "vbidevice "
                                          "FROM capturecard "
                                          "WHERE cardid = %1;")
                                          .arg(retval);

            QSqlQuery query = m_db->exec(querystr);

            if (query.isActive() && query.numRowsAffected())
            {
                // Success
                query.next();
                strlist << QString::number(retval)
                        << query.value(0).toString()
                        << query.value(1).toString()
                        << query.value(2).toString();

                dblock.lock();
                ScheduledRecording::signalChange(m_db);
                dblock.unlock();

                SendResponse(pbssock, strlist);
                return;
            }
            else
            {
                cerr << "mainserver.o: Failed querying the db for a videodevice"
                     << endl;
            }
        }
        else
        {
            // Tuner already locked
            strlist << "-2" << "" << "" << "";
            SendResponse(pbssock, strlist);
            return;
        }
    }

    strlist << "-1" << "" << "" << "";
    SendResponse(pbssock, strlist);
}

void MainServer::HandleFreeTuner(int cardid, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();
    QStringList strlist;
    EncoderLink *encoder = NULL;
    
    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(cardid);
    if (iter == encoderList->end())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
        << " Unknown encoder " << cardid << endl;
        strlist << "FAILED";
    }
    else
    {
        encoder = iter.data();
        encoder->FreeTuner();
        
        QString msg = QString("Cardid %1 FREED from external use on %2.")
                              .arg(cardid).arg(pbs->getHostname());
        VERBOSE(VB_GENERAL, msg);

        dblock.lock();
        ScheduledRecording::signalChange(m_db);
        dblock.unlock();
        
        strlist << "OK";
    }
    
    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetFreeRecorder(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();
    QString pbshost = pbs->getHostname();

    QStringList strlist;
    int retval = -1;

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

        if ((enchost == pbshost) &&
            (elink->isConnected()) &&
            (!elink->IsBusy()) &&
            (!elink->isTunerLocked()))
        {
            encoder = elink;
            retval = iter.key();
            break;
        }
        if ((retval == -1) &&
            (elink->isConnected()) &&
            (!elink->IsBusy()) &&
            (!elink->isTunerLocked()))
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

    SendResponse(pbssock, strlist);
}

void MainServer::HandleRecorderQuery(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    int recnum = commands[1].toInt();

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
    if (iter == encoderList->end())
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown encoder." << endl;
        QStringList retlist = "bad";
        SendResponse(pbssock, retlist);
        return;
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
    else if (command == "GET_RECORDING")
    {
        ProgramInfo *pginfo = enc->GetRecording();
        if (pginfo)
        {
            pginfo->ToStringList(retlist);
            delete pginfo;
        }
        else
        {
            ProgramInfo dummy;
            dummy.ToStringList(retlist);
        }
    }
    else if (command == "STOP_PLAYING")
    {
        enc->StopPlaying();
        retlist << "ok";
    }
    else if (command == "FRONTEND_READY")
    {
        enc->FrontendReady();
        retlist << "ok";
    }
    else if (command == "CANCEL_NEXT_RECORDING")
    {
        enc->CancelNextRecording();
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
    else if (command == "CHANGE_HUE")
    {
        bool up = slist[2].toInt(); 
        retlist << QString::number(enc->ChangeHue(up));
    }
    else if (command == "CHECK_CHANNEL")
    {
        QString name = slist[2];
        retlist << QString::number((int)(enc->CheckChannel(name)));
    }
    else if (command == "CHECK_CHANNEL_PREFIX")
    {
        bool unique;
        QString name = slist[2];
        retlist << QString::number((int)(enc->CheckChannelPrefix(name, 
                                                                 unique)));
        retlist << QString::number((int)unique);
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
        enc->SetReadThreadSock(NULL);

        retlist << "OK";
    }
    else
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " unknown command: " << command << endl;
        retlist << "ok";
    }

    SendResponse(pbssock, retlist);    
}

void MainServer::HandleRemoteEncoder(QStringList &slist, QStringList &commands,
                                     PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

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
    if (command == "IS_BUSY")
    {
        retlist << QString::number((int)enc->IsBusy());
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
 
        retlist << QString::number((int)enc->StartRecording(pginfo));

        delete pginfo;
    }
    else if (command == "RECORD_PENDING")
    {
        ProgramInfo *pginfo = new ProgramInfo();
        int secsleft = slist[2].toInt();
        pginfo->FromStringList(slist, 3);

        enc->RecordPending(pginfo, secsleft);
 
        retlist << "OK";
        delete pginfo;
    }

    SendResponse(pbssock, retlist);
}

void MainServer::HandleFileTransferQuery(QStringList &slist, 
                                         QStringList &commands,
                                         PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    int recnum = commands[1].toInt();

    FileTransfer *ft = getFileTransferByID(recnum);
    if (!ft)
    {
        cerr << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Unknown file transfer socket" << endl;
        exit(0);
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

    SendResponse(pbssock, retlist);
}

void MainServer::HandleGetRecorderNum(QStringList &slist, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

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

    SendResponse(pbssock, strlist);    
    delete pginfo;
}

void MainServer::HandleGetRecorderFromNum(QStringList &slist, 
                                          PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    int recordernum = slist[1].toInt();
    EncoderLink *encoder = NULL;
    QStringList strlist;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recordernum);

    if(iter != encoderList->end())
        encoder =  iter.data();

    if (encoder && encoder->isConnected())
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

    SendResponse(pbssock, strlist);
}

void MainServer::HandleMessage(QStringList &slist, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QString message = slist[1];

    MythEvent me(message);
    gContext->dispatch(me);

    QStringList retlist = "OK";

    SendResponse(pbssock, retlist);
}

void MainServer::HandleIsRecording(QStringList &slist, PlaybackSock *pbs)
{
    (void)slist;

    QSocket *pbssock = pbs->getSocket();
    int RecordingsInProgress = 0;
    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();
        if (elink->IsBusyRecording())
            RecordingsInProgress++;
    }

    QStringList retlist = QString::number(RecordingsInProgress);

    SendResponse(pbssock, retlist);
}

void MainServer::HandleGenPreviewPixmap(QStringList &slist, PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

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
        SendResponse(pbssock, outputlist);
        delete pginfo;
        return;
    }

    if (pginfo->hostname != gContext->GetHostName())
    {
        cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
             << " Somehow we got a request to generate a preview pixmap for: "
             << pginfo->hostname << endl;

        QStringList outputlist = "BAD";
        SendResponse(pbssock, outputlist);
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
    SendResponse(pbssock, retlist);    

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
                dblock.lock();
                ScheduledRecording::signalChange(m_db);
                dblock.unlock();
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
                if (enc->isLocal())
                {
                    while (enc->GetState() == kState_ChangingState)
                        usleep(500);

                    if(enc->IsBusy() && enc->GetReadThreadSocket() == socket)
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
        dblock.lock();

        QString querytext;

        QString file = lpath.section('/', -1);
        lpath = "";

        querytext = QString("SELECT icon FROM channel "
                            "WHERE icon LIKE '%%1';").arg(file);
        QSqlQuery query = m_db->exec(querytext);
        if (query.isActive() && query.numRowsAffected())
        {
            query.next();
            lpath = query.value(0).toString();
        }
        else
        {
            MythContext::DBError("Icon path", query);
        }

        dblock.unlock();
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
    QString shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    os.setEncoding(QTextStream::UnicodeUTF8);

    QDateTime qdtNow = QDateTime::currentDateTime();

    os << "HTTP/1.0 200 Ok\r\n"
       << "Content-Type: text/html; charset=\"utf-8\"\r\n"
       << "\r\n";

    os << "<HTTP>\r\n"
       << "<HEAD>\r\n"
       << "  <TITLE>MythTV Status - " 
       << qdtNow.toString(shortdateformat) 
       << " " << qdtNow.toString(timeformat) << "</TITLE>\r\n"
       << "</HEAD>\r\n"
       << "<BODY>\r\n";

    // encoder information ---------------------
    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        bool bIsLocal = elink->isLocal();

        if (bIsLocal)
        {
            os << "Encoder: " << elink->getCardId() << " is local ";
        }
        else
        {
            os << "Encoder: " << elink->getCardId() << " is remote ";
            if (elink->isConnected())
                os << "(currently connected) ";
            else
                os << "(currently not connected) ";
        }

        TVState encstate = elink->GetState();

        if (encstate == kState_WatchingLiveTV)
        {
            os << "and is watching Live TV.\r\n";
        }
        else if (encstate == kState_RecordingOnly || 
                 encstate == kState_WatchingRecording)
        {
            os << "and is recording";
            if (bIsLocal)
            {
                ProgramInfo *pi = elink->GetRecording();
                if (pi)
                {
                    os << " '" << pi->title << "'.  This recording will end "
                       << "at " << (pi->recendts).toString(timeformat);
                    delete pi;
                }
            }
            os << ".\r\n";
        }
        else
            os << "and is not recording.\r\n";

        os << "<br>\r\n";
    }

    // upcoming shows ---------------------
    list<ProgramInfo *> recordingList;
    m_sched->getAllPending(&recordingList);

    unsigned int iNum = 5;
    if (recordingList.size() < iNum) 
    {
        iNum = recordingList.size();
    }

    if (iNum == 0)
    {
        os << "<P>There are no shows scheduled for recording.\r\n";
    }
    else
    {
       os << "<P>The next " << iNum << " show" << (iNum == 1 ? "" : "s" )
          << " that " << (iNum == 1 ? "is" : "are") 
          << " scheduled for recording:<BR>\r\n";

       os << "<Script language=JavaScript>";
       os << "function dispDesc(i) {\r\n";
       os << "wnd=window.open(\"\", \"\",\"width=250 height=180\");";
       os << "wnd.document.write(\"<font face=arial size=+1><b>\");";
       os << "wnd.document.write("
          << "document.forms['Desc'].elements['title_' + i].value);";
       os << "wnd.document.write(\"</b></font><br><br>\");";
       os << "wnd.document.write(\"<font face=helvetica size=-1>\");";
       os << "wnd.document.write("
          << "document.forms['Desc'].elements['desc_' + i].value);";
       os << "wnd.document.write(\"</font>\");";
       os << "}";
       os << "</script>";

       os << "<form name=\"Desc\">\r\n";

       os << "<TABLE BORDER WIDTH=80%>\r\n"; 
       os << "<TR><TH>Start Time</TH><TH>Show</TH><TH>Encoder</TH></TR>\r\n";
       list<ProgramInfo *>::iterator iter = recordingList.begin();
       for (unsigned int i = 0; (iter != recordingList.end()) && i < iNum; 
            iter++, i++)
       {
           if (!(*iter)->recording ||     // bad entry, don't show as upcoming
               ((*iter)->recstartts) < QDateTime::currentDateTime()) // recording
           {                       
               i--;
           }
           else
           {
               QString qstrTitle = ((*iter)->title).replace(QRegExp("\""), 
                                                            "&quot;");
               QString qstrDescription = ((*iter)->description).replace(
                                                     QRegExp("\""), "&quot;");
               os << "<TR " << ((i % 2 == 0) ? "BGCOLOR=EEEEEE" : "") << ">" 
                  << "<TD>" << ((*iter)->recstartts).toString(shortdateformat) 
                  << " " << ((*iter)->recstartts).toString(timeformat) << "</TD>" 
                  << "<TD><input type=\"hidden\" name=\"desc_" << i << "\""
                  << "value=\"" << qstrDescription << "\">"
                  << "<input type=\"hidden\" name=\"title_" << i << "\""
                  << "value=\"" << qstrTitle << "\">"
                  << "<a href=\"javascript:dispDesc('" << i << "')\">"
                  << (*iter)->title << "</a></TD>"
                  << "<TD>" << (*iter)->cardid << "</TD></TR>\r\n";
           }
       }
       os << "</TABLE>";
       os << "</form>";
   }

    os << "<P>Machine Information:\r\n";
    os << "<TABLE WIDTH =100% BGCOLOR=EEEEEE>";
    os << "<TR><TD>";

    // drive space   ---------------------
    int iTotal = -1, iUsed = -1;
    QString rep;

    getFreeSpace(iTotal, iUsed);

    os << "Disk Usage:\r\n";

    os << "<UL><LI>Total Space: ";
    rep.sprintf(tr("%d,%03d MB "), (iTotal) / 1000, (iTotal) % 1000);
    os << rep << "\r\n";

    os << "<LI>Space Used: ";
    rep.sprintf(tr("%d,%03d MB "), (iUsed) / 1000, (iUsed) % 1000);
    os << rep << "\r\n";

    os << "<LI>Space Free: ";
    rep.sprintf(tr("%d,%03d MB "), (iTotal - iUsed) / 1000,
                (iTotal - iUsed) % 1000);
    os << rep << "</UL>\r\n";

    os << "</TD><TD>";
    
    // load average ---------------------
    double rgdAverages[3];
    if (getloadavg(rgdAverages, 3) != -1)
    {
        os << "This machine's load average:";
        os << "<UL><LI>1 Minute: " << rgdAverages[0] << "\r\n";
        os << "<LI>5 Minute: " << rgdAverages[1] << "\r\n";
        os << "<LI>15 Minute: " << rgdAverages[2] << "\r\n</UL>";
    }

    os << "</TD></TR></TABLE>";    // end the machine information table

    dblock.lock();

    QString querytext;
    int DaysOfData;
    QDateTime GuideDataThrough;

    querytext = QString("SELECT max(endtime) FROM program;");

    QSqlQuery query = m_db->exec(querytext);

    if (query.isActive() && query.numRowsAffected())
    {
        query.next();
        GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                 Qt::ISODate);
    }
    dblock.unlock();

    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus;

    mfdLastRunStart = gContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunEnd = gContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunStatus = gContext->GetNumSetting("mythfilldatabaseLastRunStatus");

    os << "Last mythfilldatabase run started on: "
       << mfdLastRunStart
       << "\r\n";
    os << "and ended on: "
       << mfdLastRunEnd
       << ".\r\n";

    if(!mfdLastRunStatus)
    {
       os << "<P><strong>WARNING</strong>: There was an error during the last run."
          << "\r\n";
    }
    else
    {
       os << "Last run: successful."
          << "\r\n";
    }

    os << "<P>Guide data until "
       << QDateTime(GuideDataThrough).toString("yyyy-MM-dd hh:mm")
       << "\r\n";

    DaysOfData = qdtNow.daysTo(GuideDataThrough);

    os << "<p>There " << (DaysOfData == 1 ? "is " : "are ")
       << DaysOfData
       << " day" << (DaysOfData == 1 ? "" : "s" ) << " of guide data.\r\n";

    if (DaysOfData <= 3)
    {
        os << "<p><strong>WARNING</strong>: is "
           << "<tt>mythfilldatabase</tt> running?\r\n";
    }

    os << "</BODY>\r\n"
       << "</HTTP>\r\n";

    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }
}

