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
#include <cstdlib>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>

#include <list>
#include <iostream>
using namespace std;

#include <sys/stat.h>
#ifdef linux
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif

#include "libmyth/mythcontext.h"
#include "libmyth/util.h"
#include "libmyth/mythdbcon.h"

#include "mainserver.h"
#include "scheduler.h"
#include "httpstatus.h"
#include "programinfo.h"
#include "jobqueue.h"

class ProcessRequestThread : public QThread
{
  public:
    ProcessRequestThread(MainServer *ms) { parent = ms; }
   
    void setup(RefSocket *sock)
    {
        lock.lock();
        socket = sock;
        socket->UpRef();
        lock.unlock();
        waitCond.wakeOne();
    }

    void killit(void)
    {
        lock.lock();
        threadlives = false;
        lock.unlock();
        waitCond.wakeOne();
    }

    virtual void run()
    {
        threadlives = true;

        lock.lock();

        while (1)
        {
            waitCond.wait(&lock);

            if (!threadlives)
                break;

            parent->ProcessRequest(socket);
            parent->MarkUnused(this);
            socket->DownRef();
            socket = NULL;
        }

        lock.unlock();
    }

  private:
    MainServer *parent;

    RefSocket *socket;

    QMutex lock;
    QWaitCondition waitCond;
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
    connect(mythserver, SIGNAL(newConnect(RefSocket *)), 
            SLOT(newConnection(RefSocket *)));
    connect(mythserver, SIGNAL(endConnect(RefSocket *)), 
            SLOT(endConnection(RefSocket *)));

    statusserver = new HttpStatus(this, statusport);    

    gContext->addListener(this);

    if (!ismaster)
    {
        masterServerReconnect = new QTimer(this);
        connect(masterServerReconnect, SIGNAL(timeout()), this, 
                SLOT(reconnectTimeout()));
        masterServerReconnect->start(1000, true);
    }

    if (sched)
        sched->SetMainServer(this);
}

MainServer::~MainServer()
{
    delete mythserver;

    if (statusserver)
        delete statusserver;
    if (masterServerReconnect)
        delete masterServerReconnect;
}

void MainServer::newConnection(RefSocket *socket)
{
    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
}

void MainServer::readSocket(void)
{
    RefSocket *socket = (RefSocket *)sender();

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
            VERBOSE(VB_ALL, "waiting for a thread..");
            usleep(50);
        }
    }

    prt->setup(socket);

    readReadyLock.unlock();
}

void MainServer::ProcessRequest(RefSocket *sock)
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
    //cerr << "command='" << command << "'\n";
    if (command == "MYTH_PROTO_VERSION")
    {
        if (tokens.size() < 2)
            VERBOSE(VB_ALL, "Bad MYTH_PROTO_VERSION command");
        else
            HandleVersion(sock,tokens[1]);
        return;
    }
    else if (command == "ANN")
    {
        if (tokens.size() < 3 || tokens.size() > 4)
            VERBOSE(VB_ALL, "Bad ANN query");
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
        VERBOSE(VB_ALL, "unknown socket");
        return;
    }

    if (command == "QUERY_RECORDINGS")
    {
        if (tokens.size() != 2)
            VERBOSE(VB_ALL, "Bad QUERY_RECORDINGS query");
        else
            HandleQueryRecordings(tokens[1], pbs);
    }
    else if (command == "QUERY_FREESPACE")
    {
        HandleQueryFreeSpace(pbs);
    }
    else if (command == "QUERY_LOAD")
    {
        HandleQueryLoad(pbs);
    }
    else if (command == "QUERY_UPTIME")
    {
        HandleQueryUptime(pbs);
    }
    else if (command == "QUERY_MEMSTATS")
    {
        HandleQueryMemStats(pbs);
    }
    else if (command == "QUERY_CHECKFILE")
    {
        HandleQueryCheckFile(listline, pbs);
    }
    else if (command == "QUERY_GUIDEDATATHROUGH")
    {
        HandleQueryGuideDataThrough(pbs);
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
    else if (command == "REACTIVATE_RECORDING")
    {
        HandleReactivateRecording(listline, pbs);
    }
    else if (command == "RESCHEDULE_RECORDINGS")
    {
        if (tokens.size() != 2)
            VERBOSE(VB_ALL, "Bad RESCHEDULE_RECORDINGS request");
        else
            HandleRescheduleRecordings(tokens[1].toInt(), pbs);
    }
    else if (command == "FORGET_RECORDING")
    {
        HandleForgetRecording(listline, pbs);
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
        HandleGetConflictingRecordings(listline, pbs);
    }
    else if (command == "GET_FREE_RECORDER")
    {
        HandleGetFreeRecorder(pbs);
    }
    else if (command == "GET_FREE_RECORDER_COUNT")
    {
        HandleGetFreeRecorderCount(pbs);
    }
    else if (command == "GET_NEXT_FREE_RECORDER")
    {
        HandleGetNextFreeRecorder(listline, pbs);
    }
    else if (command == "QUERY_RECORDER")
    {
        if (tokens.size() != 2)
            VERBOSE(VB_ALL, "Bad QUERY_RECORDER");
        else
            HandleRecorderQuery(listline, tokens, pbs);
    }
    else if (command == "QUERY_REMOTEENCODER")
    {
        if (tokens.size() != 2)
            VERBOSE(VB_ALL, "Bad QUERY_REMOTEENCODER");
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
            VERBOSE(VB_ALL, "Bad QUERY_FILETRANSFER");
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
            VERBOSE(VB_ALL, "Bad FREE_TUNER query");
        else
            HandleFreeTuner(tokens[1].toInt(), pbs);
    }
    else if (command == "QUERY_IS_ACTIVE_BACKEND")
    {
        if (tokens.size() != 1)
            VERBOSE(VB_ALL, "Bad QUERY_IS_ACTIVE_BACKEND");
        else
            HandleIsActiveBackendQuery(listline, pbs);
        
    }
    else if (command == "SHUTDOWN_NOW")
    {
        if (tokens.size() != 1)
            VERBOSE(VB_ALL, "Bad SHUTDOWN_NOW query");
        else if (!ismaster)
        {
            QString halt_cmd = listline[1];
            if (!halt_cmd.isEmpty())
            {
                VERBOSE(VB_ALL, "Going down now as of Mainserver request!");
                system(halt_cmd.ascii());
            }
            else
                VERBOSE(VB_ALL,
                        "WARNING: Recieved an empty SHUTDOWN_NOW query!");
        }
    }
    else if (command == "BACKEND_MESSAGE")
    {
        QString message = listline[1];
        QString extra = listline[2];
        MythEvent me(message, extra);
        gContext->dispatch(me);
    }
    else if (command == "REFRESH_BACKEND")
    {
        VERBOSE(VB_ALL,"Reloading backend settings");
        HandleBackendRefresh(sock);
    }
    else
    {
        VERBOSE(VB_ALL, "Unknown command: " + command);

        QSocket *pbssock = pbs->getSocket();

        QStringList strlist;
        strlist << "UNKNOWN_COMMAND";
        
        SendResponse(pbssock, strlist);
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
                if (gContext->GetSetting("RerecordAutoExpired", 0))
                    pinfo->DeleteHistory(m_db);
                DoHandleDeleteRecording(pinfo, NULL);
            }
            else
            {
                cerr << "Cannot find program info for '" << me->Message()
                     << "', while attempting to Auto-Expire." << endl;
            }

            return;
        }

        if (me->Message().left(21) == "RESCHEDULE_RECORDINGS")
        {
            QStringList tokens = QStringList::split(" ", me->Message());
            int recordid = tokens[1].toInt();
            if (m_sched)
            {
                m_sched->Reschedule(recordid);
                return;
            }
        }

        if (me->Message().left(6) == "LOCAL_")
            return;

        broadcast = "BACKEND_MESSAGE";
        broadcast << me->Message();
        broadcast += me->ExtraDataList();
        sendstuff = true;
    }

    if (sendstuff)
    {
        bool sendGlobal = false;
        if (ismaster && broadcast[1].left(7) == "GLOBAL_")
        {
            broadcast[1].replace(QRegExp("GLOBAL_"), "LOCAL_");
            MythEvent me(broadcast[1], broadcast[2]);
            gContext->dispatch(me);

            sendGlobal = true;
        }

        QPtrList<PlaybackSock> sentSet;

        vector<PlaybackSock *>::iterator iter = playbackList.begin();
        for (; iter != playbackList.end(); iter++)
        {
            PlaybackSock *pbs = (*iter);

            if (sentSet.containsRef(pbs))
                continue;

            sentSet.append(pbs);

            RefSocket *sock = pbs->getSocket();
            sock->UpRef();

            if (sendGlobal)
            {
                if (pbs->isSlaveBackend())
                    WriteStringList(sock, broadcast);
            }
            else if (pbs->wantsEvents())
            {
                WriteStringList(sock, broadcast);
            }

            if (sock->DownRef())
            {
                // was deleted elsewhere, so the iterator's invalid.
                iter = playbackList.begin();
            }
        }
    }
}

void MainServer::HandleVersion(QSocket *socket,QString version)
{
    QStringList retlist;
    if (version != MYTH_PROTO_VERSION)
    {
        VERBOSE(VB_GENERAL,
                "MainServer::HandleVersion - Client speaks protocol version "
                + version + " but we speak " + MYTH_PROTO_VERSION + "!");
        retlist << "REJECT" << MYTH_PROTO_VERSION;
        WriteStringList(socket, retlist);
        HandleDone(socket);
        return;
    }

    retlist << "ACCEPT" << MYTH_PROTO_VERSION;
    WriteStringList(socket, retlist);
}

void MainServer::HandleAnnounce(QStringList &slist, QStringList commands, 
                                RefSocket *socket)
{
    QStringList retlist = "OK";

    if (commands[1] == "Playback")
    {
        bool wantevents = commands[3].toInt();
        VERBOSE(VB_GENERAL, "MainServer::HandleAnnounce Playback");
        VERBOSE(VB_ALL, QString("adding: %1 as a client (events: %2)")
                               .arg(commands[2]).arg(wantevents));
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2], wantevents);
        playbackList.push_back(pbs);
    }
    else if (commands[1] == "SlaveBackend")
    {
        VERBOSE(VB_ALL, QString("adding: %1 as a slave backend server")
                               .arg(commands[2]));
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

        if (m_sched)
            m_sched->Reschedule(0);

        QString message = QString("LOCAL_SLAVE_BACKEND_ONLINE %2")
                                  .arg(commands[2]);
        MythEvent me(message);
        gContext->dispatch(me);

        playbackList.push_back(pbs);
    }
    else if (commands[1] == "RingBuffer")
    {
        VERBOSE(VB_ALL, QString("adding: %1 as a remote ringbuffer")
                               .arg(commands[2]));
        ringBufList.push_back(socket);
        int recnum = commands[3].toInt();

        QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
        if (iter == encoderList->end())
        {
            VERBOSE(VB_ALL, "Unknown encoder.");
            exit(4);
        }

        EncoderLink *enc = iter.data();

        enc->SetReadThreadSock(socket);

        if (enc->isLocal())
        {
            dblock.lock();

            int dsp_status, soundcardcaps;
            QString audiodevice, audiooutputdevice, querytext;
            QString cardtype;

            querytext = QString("SELECT audiodevice,cardtype FROM capturecard "
                                "WHERE cardid=%1;").arg(recnum);
            QSqlQuery query = m_db->exec(querytext);
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiodevice = query.value(0).toString();
                cardtype = query.value(1).toString();
            }

            query = m_db->exec("SELECT data FROM settings WHERE "
                               "value ='AudioOutputDevice';");
            if (query.isActive() && query.numRowsAffected())
            {
                query.next();
                audiooutputdevice = query.value(0).toString();
            }

            dblock.unlock();

            if (audiodevice.right(4) == audiooutputdevice.right(4) &&
                (cardtype == "V4L" || cardtype == "MJPEG")) //they match
            {
                int dsp_fd = open(audiodevice, O_RDWR | O_NONBLOCK);
                if (dsp_fd != -1)
                {
                    dsp_status = ioctl(dsp_fd, SNDCTL_DSP_GETCAPS,
                                       &soundcardcaps);
                    if (dsp_status != -1)
                    {
                        if (!(soundcardcaps & DSP_CAP_DUPLEX))
                            VERBOSE(VB_ALL, QString(" WARNING:  Capture device"
                                                    " %1 is not reporting full "
                                                    "duplex capability.\nSee "
                                                    "docs/mythtv-HOWTO, section"
                                                    " 20.13 for more "
                                                    "information.")
                                                    .arg(audiodevice));
                    }
                    else 
                        VERBOSE(VB_ALL, QString("Could not get capabilities for"
                                                " audio device: %1")
                                               .arg(audiodevice));
                    close(dsp_fd);
                }
                else 
                {
                    perror("open");
                    VERBOSE(VB_ALL, QString("Could not open audio device: %1")
                                           .arg(audiodevice));
                }
            }
        }
    }
    else if (commands[1] == "FileTransfer")
    {
        VERBOSE(VB_GENERAL, "MainServer::HandleAnnounce FileTransfer");
        VERBOSE(VB_ALL, QString("adding: %1 as a remote file transfer")
                               .arg(commands[2]));
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

    QString fs_db_name = "";
    MythSqlDatabase *updatefs_mdb = NULL;
    QSqlDatabase *updatefs_db = NULL;

    QString thequery;

    QString ip = gContext->GetSetting("BackendServerIP");
    QString port = gContext->GetSetting("BackendServerPort");

    dblock.lock();
    MythContext::KickDatabase(m_db);

    thequery = "SELECT recorded.chanid,recorded.starttime,recorded.endtime,"
               "recorded.title,recorded.subtitle,recorded.description,"
               "recorded.hostname,channum,name,callsign,commflagged,cutlist,"
               "recorded.autoexpire,editing,bookmark,recorded.category,"
               "recorded.recgroup,record.dupin,record.dupmethod,"
               "record.recordid,outputfilters,"
               "recorded.seriesid,recorded.programid,recorded.filesize, "
               "recorded.lastmodified, recorded.findid "
               "FROM recorded "
               "LEFT JOIN record ON recorded.recordid = record.recordid "
               "LEFT JOIN channel ON recorded.chanid = channel.chanid "
               "WHERE recorded.deletepending = 0 "
               "ORDER BY recorded.starttime";

    if (type == "Delete")
        thequery += " DESC";
    thequery += ";";

    QSqlQuery query = m_db->exec(thequery);

    QStringList outputlist;
    QString fileprefix = gContext->GetFilePrefix();
    QMap<QString, QString> backendIpMap;
    QMap<QString, QString> backendPortMap;

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

            proginfo->dupin = RecordingDupInType(query.value(17).toInt());
            proginfo->dupmethod = RecordingDupMethodType(query.value(18).toInt());
            proginfo->recordid = query.value(19).toInt();
            proginfo->chanOutputFilters = query.value(20).toString();
            proginfo->seriesid = query.value(21).toString();
            proginfo->programid = query.value(22).toString();
            proginfo->filesize = stringToLongLong(query.value(23).toString());
            proginfo->lastmodified =
                      QDateTime::fromString(query.value(24).toString(),
                                            Qt::ISODate);
            proginfo->findid = query.value(25).toInt();

            if (proginfo->hostname.isEmpty() || proginfo->hostname.isNull())
                proginfo->hostname = gContext->GetHostName();

            if (!query.value(7).toString().isEmpty())
            {
                proginfo->chanstr = query.value(7).toString();
                proginfo->channame = QString::fromUtf8(query.value(8).toString());
                proginfo->chansign = QString::fromUtf8(query.value(9).toString());
            }
            else
            {
                proginfo->chanstr = "#" + proginfo->chanid;
                proginfo->channame = "#" + proginfo->chanid;
                proginfo->chansign = "#" + proginfo->chanid;
            }

            // Taken out of programinfo.cpp just to reduce the number of queries
            int flags = 0;
            flags |= (query.value(10).toInt() == 1) ? FL_COMMFLAG : 0;
            flags |= query.value(11).toString().length() > 1 ? FL_CUTLIST : 0;
            flags |= query.value(12).toInt() ? FL_AUTOEXP : 0;
            if (query.value(13).toInt() || (query.value(10).toInt() == 2))
                flags |= FL_EDITING;
            flags |= query.value(14).toString().length() > 1 ? FL_BOOKMARK : 0;

            proginfo->programflags = flags;

            proginfo->category = QString::fromUtf8(query.value(15).toString());

            proginfo->recgroup = query.value(16).toString();

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

                if (proginfo->filesize == 0)
                {
                    struct stat st;

                    long long size = 0;
                    if (stat(lpath.ascii(), &st) == 0)
                        size = st.st_size;

                    proginfo->filesize = size;

                    if (!updatefs_db)
                    {
                        fs_db_name = QString("updatefilesize%1%2")
                                             .arg(getpid()).arg(rand());
                        updatefs_mdb = new MythSqlDatabase(fs_db_name);
                        updatefs_db = updatefs_mdb->db();
                    }
                    proginfo->SetFilesize(size, updatefs_db);
                }
            }
            else
            {
                if (!slave)
                {
                    VERBOSE(VB_ALL, QString("Couldn't find backend for: %1 : "
                                            "%2").arg(proginfo->title)
                                                 .arg(proginfo->subtitle));
                    proginfo->filesize = 0;
                    proginfo->pathname = "file not found";
                }
                else
                {
                    if (proginfo->filesize == 0)
                    {
                        slave->FillProgramInfo(proginfo, playbackhost);

                        if (!updatefs_db)
                        {
                            fs_db_name = QString("updatefilesize%1%2")
                                                 .arg(getpid()).arg(rand());
                            updatefs_mdb = new MythSqlDatabase(fs_db_name);
                            updatefs_db = updatefs_mdb->db();
                        }
                        proginfo->SetFilesize(proginfo->filesize, updatefs_db);
                    }
                    else
                    {
                        ProgramInfo *p = proginfo;
                        if (!backendIpMap.contains(p->hostname))
                            backendIpMap[p->hostname] =
                                gContext->GetSettingOnHost("BackendServerIp",
                                                           p->hostname);
                        if (!backendPortMap.contains(p->hostname))
                            backendPortMap[p->hostname] =
                                gContext->GetSettingOnHost("BackendServerPort",
                                                           p->hostname);
                        p->pathname = QString("myth://") +
                                      backendIpMap[p->hostname] + ":" +
                                      backendPortMap[p->hostname] + "/" +
                                      p->GetRecordBasename();
                    }
                }
            }

            proginfo->ToStringList(outputlist);

            delete proginfo;
        }
    }
    else
        outputlist << "0";

    dblock.unlock();

    if (updatefs_mdb)
        delete updatefs_mdb;

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

void *MainServer::SpawnDeleteThread(void *param)
{
    DeleteStruct *ds = (DeleteStruct *)param;

    MainServer *ms = ds->ms;
    ms->DoDeleteThread(ds);

    delete ds;

    return NULL;
}

void MainServer::DoDeleteThread(DeleteStruct *ds)
{
    deletelock.lock();

    QString logInfo = QString("chanid %1 at %2")
                              .arg(ds->chanid).arg(ds->recstartts.toString());
                             
    QString name = QString("deleteThread%1%2").arg(getpid()).arg(rand());
    QFile checkFile(ds->filename);

    if (!checkFile.exists())
    {
        VERBOSE(VB_ALL, QString("ERROR when trying to delete file: %1. File "
                                "doesn't exist.  Database metadata"
                                "will not be removed.")
                                .arg(ds->filename));
        gContext->LogEntry("mythbackend", LP_WARNING, "Delete Recording",
                           QString("File %1 does not exist for %2 when trying "
                                   "to delete recording.")
                                   .arg(ds->filename).arg(logInfo));
        deletelock.unlock();
        return;
    }

    MythSqlDatabase *delete_db = new MythSqlDatabase(name);

    if (!delete_db || !delete_db->isOpen() || !delete_db->db())
    {
        QString msg = QString("ERROR opening database connection for Delete "
                              "Thread for chanid %1 recorded at %2.  Program "
                              "will NOT be deleted.")
                              .arg(ds->chanid).arg(ds->recstartts.toString());
        VERBOSE(VB_GENERAL, msg);
        gContext->LogEntry("mythbackend", LP_ERROR, "Delete Recording",
                           QString("Unable to open database connection for %1. "
                                   "Program will NOT be deleted.")
                                   .arg(logInfo));
        if (delete_db)
            delete delete_db;

        deletelock.unlock();
        return;
    }

    ProgramInfo *pginfo;
    pginfo = ProgramInfo::GetProgramFromRecorded(delete_db->db(),
                                                 ds->chanid,
                                                 ds->recstartts);
    JobQueue::DeleteAllJobs(delete_db->db(), ds->chanid, ds->recstartts);

    QString filename;
    bool followLinks = gContext->GetNumSetting("DeletesFollowLinks", 0);

    filename = ds->filename;
    if (followLinks)
    {
        QFileInfo finfo(filename);
        if (finfo.isSymLink())
            unlink(finfo.readLink().ascii());
    }
    unlink(filename.ascii());
    
    sleep(2);

    if (checkFile.exists())
    {
        VERBOSE(VB_ALL,
            QString("Error deleting file: %1. Keeping metadata in database.")
                    .arg(ds->filename));
        gContext->LogEntry("mythbackend", LP_WARNING, "Delete Recording",
                           QString("File %1 for %2 could not be deleted.")
                                   .arg(ds->filename).arg(logInfo));
        if (pginfo)
        {
            pginfo->SetDeleteFlag(false, delete_db->db());
            delete pginfo;
        }

        if (delete_db)
            delete delete_db;

        deletelock.unlock();
        return;
    }

    filename = ds->filename + ".png";
    if (followLinks)
    {
        QFileInfo finfo(filename);
        if (finfo.isSymLink())
            unlink(finfo.readLink().ascii());
    }
    unlink(filename.ascii());

    QSqlQuery query(QString::null, delete_db->db());
    query.prepare("DELETE FROM recorded WHERE chanid = :CHANID AND "
                  "title = :TITLE AND starttime = :STARTTIME AND "
                  "endtime = :ENDTIME;");
    query.bindValue(":CHANID", ds->chanid);
    query.bindValue(":TITLE", ds->title.utf8());
    query.bindValue(":STARTTIME", ds->recstartts.toString("yyyyMMddhhmm00"));
    query.bindValue(":ENDTIME", ds->recendts.toString("yyyyMMddhhmm00"));
    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Recorded program deletion", query);
        gContext->LogEntry("mythbackend", LP_ERROR, "Delete Recording",
                           QString("Error deleting recorded table for %1.")
                                   .arg(logInfo));
    }

    sleep(1);

    // Notify the frontend so it can requery for Free Space
    MythEvent me("RECORDING_LIST_CHANGE");
    gContext->dispatch(me);

    query.prepare("DELETE FROM recordedmarkup "
                  "WHERE chanid = :CHANID AND starttime = :STARTTIME;");
    query.bindValue(":CHANID", ds->chanid);
    query.bindValue(":STARTTIME", ds->recstartts.toString("yyyyMMddhhmm00"));
    query.exec();

    if (!query.isActive())
    {
        MythContext::DBError("Recorded program delete recordedmarkup",
                             query);
        gContext->LogEntry("mythbackend", LP_ERROR, "Delete Recording",
                           QString("Error deleting recordedmarkup for %1.")
                                   .arg(logInfo));
    }

    ScheduledRecording::signalChange(0);

    if (pginfo)
        delete pginfo;

    if (delete_db)
        delete delete_db;

    deletelock.unlock();
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
            {
                (*encoderList)[num]->StopRecording();
                pginfo->recstatus = rsStopped;
                if (m_sched)
                    m_sched->UpdateRecStatus(pginfo);
            }

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

            if (ismaster)
            {
                pginfo->recstatus = rsStopped;
                if (m_sched)
                    m_sched->UpdateRecStatus(pginfo);
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

    dblock.lock();

    MythContext::KickDatabase(m_db);

    QString startts = pginfo->recstartts.toString("yyyyMMddhhmm");
    startts += "00";
    QString recendts = pginfo->recendts.toString("yyyyMMddhhmm");
    recendts += "00";
    QDateTime now(QDateTime::currentDateTime());
    QString newendts = now.toString("yyyyMMddhhmm");
    newendts += "00";

    // Set the recorded end time to the current time
    // (we're stopping the recording so it'll never get to its originally 
    // intended end time)

    VERBOSE(VB_RECORD, QString("Host %1 updating endtime to %2")
                               .arg(gContext->GetHostName()).arg(newendts));
    
    QSqlQuery query(QString::null, m_db);
    query.prepare("UPDATE recorded SET starttime = :NEWSTARTTIME, "
                  "endtime = :NEWENDTIME WHERE chanid = :CHANID AND "
                  "title = :TITLE AND starttime = :STARTTIME AND "
                  "endtime = :ENDTIME;");
    query.bindValue(":NEWSTARTTIME", startts);
    query.bindValue(":NEWENDTIME", newendts);
    query.bindValue(":CHANID", pginfo->chanid);
    query.bindValue(":TITLE", pginfo->title.utf8());
    query.bindValue(":STARTTIME", startts);
    query.bindValue(":ENDTIME", recendts);

    query.exec();

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

    VERBOSE(VB_RECORD, QString("Host %1 renaming %2 to %3")
                               .arg(gContext->GetHostName())
                               .arg(oldfilename).arg(newfilename));
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

    int jobTypes;

    dblock.lock();
    jobTypes = pginfo->GetAutoRunJobs(m_db);

    if (pginfo->chancommfree)
        jobTypes = jobTypes & (~JOB_COMMFLAG);

//    if (autoTranscode)
//        jobTypes |= JOB_TRANSCODE;

    if (jobTypes)
    {
        QString jobHost = "";

        if (gContext->GetNumSetting("JobsRunOnRecordHost", 0))
            jobHost = pginfo->hostname;

        JobQueue::QueueJobs(m_db, jobTypes, pginfo->chanid,
                            pginfo->recstartts, "", "", jobHost);
    }
    dblock.unlock();

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
           {
               (*encoderList)[num]->StopRecording();
               pginfo->recstatus = rsDeleted;
               if (m_sched)
                   m_sched->UpdateRecStatus(pginfo);
           }

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

            if (ismaster)
            {
                pginfo->recstatus = rsDeleted;
                if (m_sched)
                    m_sched->UpdateRecStatus(pginfo);
            }
        }
    }

    QString fileprefix = gContext->GetFilePrefix();
    QString filename = pginfo->GetRecordFilename(fileprefix);
    QFile checkFile(filename);
    bool fileExists = checkFile.exists();

    if (fileExists)
    {
        DeleteStruct *ds = new DeleteStruct;
        ds->ms = this;
        ds->filename = filename;
        ds->title = pginfo->title;
        ds->chanid = pginfo->chanid;
        ds->recstartts = pginfo->recstartts;
        ds->recendts = pginfo->recendts;

        dblock.lock();
        MythContext::KickDatabase(m_db);
        pginfo->SetDeleteFlag(true, m_db);
        dblock.unlock();

        pthread_t deleteThread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&deleteThread, &attr, SpawnDeleteThread, ds);
    }
    else
    {
        QString logInfo = QString("chanid %1 at %2")
                              .arg(pginfo->chanid)
                              .arg(pginfo->recstartts.toString());
        VERBOSE(VB_ALL,
                QString("ERROR when trying to delete file: %1. File doesn't "
                        "exist.  Database metadata will not be removed.")
                        .arg(filename));
        gContext->LogEntry("mythbackend", LP_WARNING, "Delete Recording",
                           QString("File %1 does not exist for %2 when trying "
                                   "to delete recording.")
                                   .arg(filename).arg(logInfo));
    }

    if (pbssock)
    {
        QStringList outputlist;

        if (fileExists)
            outputlist = QString::number(recnum);
        else
            outputlist << "BAD: Tried to delete a file that was in "
                          "the database but wasn't on the disk.";

        SendResponse(pbssock, outputlist);
    }

    delete pginfo;
}

void MainServer::HandleReactivateRecording(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo pginfo;
    pginfo.FromStringList(slist, 1);

    QStringList result;
    if (m_sched)
        result = QString::number(m_sched->ReactivateRecording(&pginfo));
    else
        result = QString::number(0);

    if (pbs)
    {
        QSocket *pbssock = pbs->getSocket();
        if (pbssock)
            SendResponse(pbssock, result);
    }
}

void MainServer::HandleRescheduleRecordings(int recordid, PlaybackSock *pbs)
{
    QStringList result;
    if (m_sched)
    {
        m_sched->Reschedule(recordid);
        result = QString::number(1);
    }
    else
        result = QString::number(0);

    if (pbs)
    {
        QSocket *pbssock = pbs->getSocket();
        if (pbssock)
            SendResponse(pbssock, result);
    }
}

void MainServer::HandleForgetRecording(QStringList &slist, PlaybackSock *pbs)
{
    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    QSocket *pbssock = NULL;
    if (pbs)
        pbssock = pbs->getSocket();

    pginfo->DeleteHistory(m_db);

    if (pbssock)
    {
        QStringList outputlist = QString::number(0);
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

void MainServer::HandleQueryLoad(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strlist;

    double loads[3];
    if (getloadavg(loads,3) == -1)
        strlist << "getloadavg() failed";
    else
        strlist << QString::number(loads[0])
                << QString::number(loads[1])
                << QString::number(loads[2]);

    SendResponse(pbssock, strlist);
}

void MainServer::HandleQueryUptime(PlaybackSock *pbs)
{
    QSocket    *pbssock = pbs->getSocket();
    QStringList strlist;
    time_t      uptime;

    if (getUptime(uptime))
        strlist << QString::number(uptime);
    else
        strlist << "Could not determine uptime.";

    SendResponse(pbssock, strlist);
}

void MainServer::HandleQueryMemStats(PlaybackSock *pbs)
{
    QSocket    *pbssock = pbs->getSocket();
    QStringList strlist;
    int         totalMB, freeMB, totalVM, freeVM;

    if (getMemStats(totalMB, freeMB, totalVM, freeVM))
        strlist << QString::number(totalMB) << QString::number(freeMB)
                << QString::number(totalVM) << QString::number(freeVM);
    else
        strlist << "Could not determine memory stats.";

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

void MainServer::getGuideDataThrough(QDateTime &GuideDataThrough)
{
    QString querytext;

    querytext = QString("SELECT max(endtime) FROM program;");

    dblock.lock();
    QSqlQuery query = m_db->exec(querytext);

    if (query.isActive() && query.numRowsAffected())
    {
        query.next();
        if (query.isValid())
            GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                     Qt::ISODate);
    }
    dblock.unlock();
}

void MainServer::HandleQueryGuideDataThrough(PlaybackSock *pbs)
{
    QDateTime GuideDataThrough;
    QSocket *pbssock = pbs->getSocket();
    QStringList strlist;

    getGuideDataThrough(GuideDataThrough);

    if (GuideDataThrough.isNull())
        strlist << QString("0000-00-00 00:00");
    else
        strlist << QDateTime(GuideDataThrough).toString("yyyy-MM-dd hh:mm");

    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strList;

    if (m_sched)
        m_sched->getAllPending(strList);
    else
    {
        strList << QString::number(0);
        strList << QString::number(0);
    }

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetScheduledRecordings(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strList;

    if (m_sched)
        m_sched->getAllScheduled(strList);
    else
        strList << QString::number(0);

    SendResponse(pbssock, strList);
}

void MainServer::HandleGetConflictingRecordings(QStringList &slist,
                                                PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    QStringList strlist;

    if (m_sched)
        m_sched->getConflicting(pginfo, strlist);
    else
        strlist << QString::number(0);

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

                if (m_sched)
                    m_sched->Reschedule(0);

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
        VERBOSE(VB_ALL, QString("Unknown encoder: %1").arg(cardid));
        strlist << "FAILED";
    }
    else
    {
        encoder = iter.data();
        encoder->FreeTuner();
        
        QString msg = QString("Cardid %1 FREED from external use on %2.")
                              .arg(cardid).arg(pbs->getHostname());
        VERBOSE(VB_GENERAL, msg);

        if (m_sched)
            m_sched->Reschedule(0);
        
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

    bool lastcard = false;

    if (gContext->GetSetting("LastFreeCard", "0") == "1")
        lastcard = true;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (!lastcard)
        {
            if (elink->isLocal())
                enchost = gContext->GetHostName();
            else
                enchost = elink->getHostname();

            if (enchost == pbshost && elink->isConnected() &&
                !elink->IsBusy() && !elink->isTunerLocked())
            {
                encoder = elink;
                retval = iter.key();
                VERBOSE(VB_RECORD, QString("Card %1 is local.")
                        .arg(iter.key()));
                break;
            }
        }

        if ((retval == -1 || lastcard) && elink->isConnected() &&
            !elink->IsBusy() && !elink->isTunerLocked())
        {
            encoder = elink;
            retval = iter.key();
        }
        VERBOSE(VB_RECORD, QString("Checking card %1. Best card so far %2")
                .arg(iter.key()).arg(retval));
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

void MainServer::HandleGetFreeRecorderCount(PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();

    QStringList strlist;
    int count = 0;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if ((elink->isConnected()) &&
            (!elink->IsBusy()) &&
            (!elink->isTunerLocked()))
        {
            count++;
        }
    }

    strlist << QString::number(count);
        
    SendResponse(pbssock, strlist);
}

void MainServer::HandleGetNextFreeRecorder(QStringList &slist, 
                                           PlaybackSock *pbs)
{
    QSocket *pbssock = pbs->getSocket();
    QString pbshost = pbs->getHostname();

    QStringList strlist;
    int retval = -1;
    int currrec = slist[1].toInt();

    EncoderLink *encoder = NULL;
    QString enchost;

    VERBOSE(VB_RECORD, QString("Getting next free recorder after : %1")
            .arg(currrec));

    // find current recorder
    QMap<int, EncoderLink *>::Iterator iter, curr = encoderList->find(currrec);

    if (currrec > 0 && curr != encoderList->end())
    {
        // cycle through all recorders
        for (iter = curr;;)
        {
            EncoderLink *elink;

            // last item? go back
            if (++iter == encoderList->end())
            {
                iter = encoderList->begin();
            }

            elink = iter.data();

            if ((retval == -1) &&
                (elink->isConnected()) &&
                (!elink->IsBusy()) &&
                (!elink->isTunerLocked()))
            {
                encoder = elink;
                retval = iter.key();
            }

            // cycled right through? no more available recorders
            if (iter == curr) 
                break;
        }
    } 
    else 
    {
        HandleGetFreeRecorder(pbs);
        return;
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
        VERBOSE(VB_ALL, QString("Unknown encoder: %1").arg(recnum));
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
            if (value != -1) 
            {
                encodeLongLong(retlist, keynum);
                encodeLongLong(retlist, value);
            }
        }

        if (!retlist.size())
            retlist << "ok";
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
        VERBOSE(VB_IMPORTANT, "Received: CANCEL_NEXT_RECORDING");
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
        QString seriesid = "", programid = "";

        enc->GetNextProgram(direction,
                            title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname, chanid,
                            seriesid, programid);

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
        if (seriesid == "")
            seriesid = " ";
        if (programid == "")
            programid = " ";

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
        retlist << seriesid;
        retlist << programid;
    }
    else if (command == "GET_PROGRAM_INFO")
    {
        QString title = "", subtitle = "", desc = "", category = "";
        QString starttime = "", endtime = "", callsign = "", iconpath = "";
        QString channelname = "", chanid = "", seriesid = "", programid = "";
        QString chanOutputFilters = "";
        QString repeat = "", airdate = "", stars = "";

        enc->GetChannelInfo(title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname, chanid,
                            seriesid, programid, chanOutputFilters,
                            repeat, airdate, stars);

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
        if (seriesid == "")
            seriesid = " ";
        if (programid == "")
            programid = " ";
        if (chanOutputFilters == "")
            chanOutputFilters = " ";    
        if (repeat == "" )
            repeat = "0";
        if (airdate == "")
            airdate = starttime;
        if (stars == "")
            stars = " ";

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
        retlist << seriesid;
        retlist << programid;
        retlist << chanOutputFilters;
        retlist << repeat;
        retlist << airdate;
        retlist << stars;
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

        int ret = enc->RequestRingBufferBlock(size);
        retlist << QString::number(ret);
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
        VERBOSE(VB_ALL, QString("Unknown command: %1").arg(command));
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
        VERBOSE(VB_ALL, QString("Unknown encoder: %1").arg(recnum));
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

void MainServer::HandleIsActiveBackendQuery(QStringList &slist,
                                            PlaybackSock *pbs)
{
    QStringList retlist;
    QString queryhostname = slist[1];
    
    if (gContext->GetHostName() != queryhostname)
    {
        if (getSlaveByHostname(queryhostname) != NULL)
            retlist << "TRUE";
        else
            retlist << "FALSE";
    }
    else
        retlist << "TRUE";
    
    SendResponse(pbs->getSocket(), retlist);
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
        VERBOSE(VB_ALL, QString("Unknown file transfer socket: %1")
                               .arg(recnum));
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
        VERBOSE(VB_ALL, QString("Unknown command: %1").arg(command));
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
            VERBOSE(VB_ALL, QString("Couldn't find backend for: %1 : %2")
                                   .arg(pginfo->title).arg(pginfo->subtitle));
        else
            slave->GenPreviewPixmap(pginfo);

        QStringList outputlist = "OK";
        SendResponse(pbssock, outputlist);
        delete pginfo;
        return;
    }

    if (pginfo->hostname != gContext->GetHostName())
    {
        VERBOSE(VB_ALL, QString("Got requested to generate preview pixmap "
                                "for %1").arg(pginfo->hostname));

        QStringList outputlist = "BAD";
        SendResponse(pbssock, outputlist);
        delete pginfo;
        return;
    }

    QUrl qurl = pginfo->pathname;
    QString filename = qurl.path();

    if (qurl.host() != "")
        filename = LocalFilePath(qurl);

    EncoderLink *elink = NULL;
    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();

    for (; iter != encoderList->end(); ++iter)
    {
        if (iter.data()->isLocal())
            elink = iter.data();
    }

    if (elink == NULL)
    {
        VERBOSE(VB_ALL, QString("Couldn't find backend for: %1 : %2")
                               .arg(pginfo->title).arg(pginfo->subtitle));
        QStringList outputlist = "BAD";
        SendResponse(pbssock, outputlist);
        delete pginfo;
        return;
    }

    int len = 0;
    int width = 0, height = 0;
    int secondsin = gContext->GetNumSetting("PreviewPixmapOffset", 64);

    unsigned char *data = (unsigned char *)elink->GetScreenGrab(pginfo, 
                                                                filename, 
                                                                secondsin,
                                                                len, width,
                                                                height);

    if (data)
    {
        QImage img(data, width, height, 32, NULL, 65536 * 65536,
                   QImage::LittleEndian);
        img = img.smoothScale(160, 120);

        filename += ".png";
        img.save(filename.ascii(), "PNG");

        QStringList retlist = "OK";
        SendResponse(pbssock, retlist);    
    }
    else
    {
        VERBOSE(VB_ALL, QString("Not enough video to make thumbnail"));

        QStringList outputlist = "BAD";
        SendResponse(pbssock, outputlist);
    } 

    delete [] data;
    delete pginfo;
}

void MainServer::HandleBackendRefresh(QSocket *socket)
{
    gContext->RefreshBackendConfig();

    QStringList retlist = "OK";
    SendResponse(socket, retlist);    
}

void MainServer::endConnection(RefSocket *socket)
{
    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        QSocket *sock = (*it)->getSocket();
        if (sock == socket)
        {
            if (ismaster && (*it)->isSlaveBackend())
            {
                VERBOSE(VB_ALL, QString("Slave backend: %1 has left the "
                                        "building").arg((*it)->getHostname()));

                QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
                for (; iter != encoderList->end(); ++iter)
                {
                    EncoderLink *elink = iter.data();
                    if (elink->getSocket() == (*it))
                        elink->setSocket(NULL);
                }
                if (m_sched)
                    m_sched->Reschedule(0);

                QString message = QString("LOCAL_SLAVE_BACKEND_OFFLINE %2")
                                          .arg((*it)->getHostname());
                MythEvent me(message);
                gContext->dispatch(me);
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
            socket->DownRef();
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

            socket->DownRef();
            ringBufList.erase(rt);
            return;
        }
    }

    VERBOSE(VB_ALL, "unknown socket");
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
        lpath = gContext->GetFilePrefix() + "/" + lpath;
    }
    VERBOSE(VB_FILE, QString("Local file path: %1").arg(lpath));

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
    vector<PlaybackSock *>::iterator it = playbackList.begin();
    for (; it != playbackList.end(); it++)
    {
        PlaybackSock *pbs = (*it);
        if (pbs == masterServer)
        {
            playbackList.erase(it);
            break;
        }
    }

    delete masterServer;
    masterServer = NULL;
    masterServerReconnect->start(1000, true);
}

void MainServer::reconnectTimeout(void)
{
    RefSocket *masterServerSock = new RefSocket();

    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);

    VERBOSE(VB_ALL, QString("Connecting to master server: %1:%2")
                           .arg(server).arg(port));

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
            VERBOSE(VB_ALL, "Connection to master server timed out.");
            masterServerReconnect->start(1000, true);
            delete masterServerSock;
            return;
        }
    }

    if (masterServerSock->state() != QSocket::Connected)
    {
        VERBOSE(VB_ALL, "Could not connect to master server.");
        masterServerReconnect->start(1000, true);
        delete masterServerSock;
        return;
    }

    VERBOSE(VB_ALL, "Connected successfully");

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

// returns true, if a client (slavebackends are not counted!)
// is connected by checking the lists.
bool MainServer::isClientConnected()
{
    bool foundClient = false;

    foundClient |= (ringBufList.size() > 0);
    foundClient |= (fileTransferList.size() > 0);

    if ((playbackList.size() > 0) && !foundClient)
    {
        vector<PlaybackSock *>::iterator it = playbackList.begin();
        for (; !foundClient && (it != playbackList.end()); ++it)
        {
            // we simply ignore slaveBackends!
            if (!(*it)->isSlaveBackend())
                foundClient = true;
        }
    }
    
    return (foundClient);
}

// sends the Slavebackends the request to shut down using haltcmd
void MainServer::ShutSlaveBackendsDown(QString &haltcmd)
{
    QStringList bcast = "SHUTDOWN_NOW";
    bcast << haltcmd;
    
    if (playbackList.size() > 0)
    {
        vector<PlaybackSock *>::iterator it = playbackList.begin();
        for (; it != playbackList.end(); ++it)
        {
            if ((*it)->isSlaveBackend())
                WriteStringList((*it)->getSocket(),bcast); 
        }
    }
}

#if USING_DVB
void MainServer::PrintDVBStatus(QTextStream& os)
{
    dblock.lock();

    QString querytext;

    bool doneAnything = false;
    
    os << "\r\n  <div class=\"content\">\r\n" <<
        "    <h2>DVB Signal Information</h2>\r\n" <<
        "    Details of DVB error statistics for last 48 hours:<br />\r\n";

    QString outerqry =
        "SELECT starttime,endtime FROM recorded "
        "WHERE starttime >= DATE_SUB(NOW(), INTERVAL 48 HOUR) "
        "ORDER BY starttime;";

    QSqlQuery oquery = m_db->exec(outerqry);

    if (oquery.isActive() && oquery.numRowsAffected())
    {
        querytext = QString("SELECT cardid,"
                            "max(fe_ss),min(fe_ss),avg(fe_ss),"
                            "max(fe_snr),min(fe_snr),avg(fe_snr),"
                            "max(fe_ber),min(fe_ber),avg(fe_ber),"
                            "max(fe_unc),min(fe_unc),avg(fe_unc),"
                            "max(myth_cont),max(myth_over),max(myth_pkts) "
                            "FROM dvb_signal_quality "
                            "WHERE sampletime BETWEEN ? AND ? "
                            "GROUP BY cardid");

        QSqlQuery query = m_db->exec(NULL);
        query.prepare(querytext);
        
        while (oquery.next())
        {
            QDateTime t_start = oquery.value(0).toDateTime();
            QDateTime t_end = oquery.value(1).toDateTime();

            query.bindValue(0, t_start);
            query.bindValue(1, t_end);

            if (!query.exec())
                cout << query.lastError().databaseText() << "\r\n" 
                     << query.lastError().driverText() << "\r\n";
            
            if (query.isActive() && query.numRowsAffected())
            {
                os << "    <br />Recording period from " 
                   << t_start.toString() << 
                    " to " << t_end.toString() <<
                    "<br />\n";
                
                while (query.next())
                {
                    os << "    Encoder " << query.value(0).toInt() <<
                        " Min SNR: " << query.value(5).toInt() <<
                        " Avg SNR: " << query.value(6).toInt() <<
                        " Min BER: " << query.value(8).toInt() <<
                        " Avg BER: " << query.value(9).toInt() <<
                        " Cont Errs: " << query.value(13).toInt() <<
                        " Overflows: " << query.value(14).toInt() <<
                        "<br />\r\n";

                    doneAnything = true;
                }
            }
        }
    }

    if (!doneAnything)
    {
        os << "    <br />There is no DVB signal quality data available to "
            "display.<br />\r\n";
    }

    dblock.unlock();

    os << "  </div>\r\n";
}
#endif

void MainServer::PrintStatus(QSocket *socket)
{
    QTextStream os(socket);
    QString shortdateformat = gContext->GetSetting("ShortDateFormat", "M/d");
    QString timeformat = gContext->GetSetting("TimeFormat", "h:mm AP");

    os.setEncoding(QTextStream::UnicodeUTF8);

    QDateTime qdtNow = QDateTime::currentDateTime();

    os << "HTTP/1.0 200 Ok\r\n"
       << "Content-Type: text/html; charset=\"UTF-8\"\r\n"
       << "\r\n";

    os << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" "
       << "\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\r\n"
       << "<html xmlns=\"http://www.w3.org/1999/xhtml\""
       << " xml:lang=\"en\" lang=\"en\">\r\n"
       << "<head>\r\n"
       << "  <meta http-equiv=\"Content-Type\""
       << "content=\"text/html; charset=UTF-8\" />\r\n"
       << "  <style type=\"text/css\" title=\"Default\" media=\"all\">\r\n"
       << "  <!--\r\n"
       << "  body {\r\n"
       << "    background-color:#fff;\r\n"
       << "    font:11px verdana, arial, helvetica, sans-serif;\r\n"
       << "    margin:20px;\r\n"
       << "  }\r\n"
       << "  h1 {\r\n"
       << "    font-size:28px;\r\n"
       << "    font-weight:900;\r\n"
       << "    color:#ccc;\r\n"
       << "    letter-spacing:0.5em;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    width:650px;\r\n"
       << "    text-align:center;\r\n"
       << "  }\r\n"
       << "  h2 {\r\n"
       << "    font-size:18px;\r\n"
       << "    font-weight:800;\r\n"
       << "    color:#360;\r\n"
       << "    border:none;\r\n"
       << "    letter-spacing:0.3em;\r\n"
       << "    padding:0px;\r\n"
       << "    margin-bottom:10px;\r\n"
       << "    margin-top:0px;\r\n"
       << "  }\r\n"
       << "  hr {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div.content {\r\n"
       << "    width:650px;\r\n"
       << "    border-top:1px solid #000;\r\n"
       << "    border-right:1px solid #000;\r\n"
       << "    border-bottom:1px solid #000;\r\n"
       << "    border-left:10px solid #000;\r\n"
       << "    padding:10px;\r\n"
       << "    margin-bottom:30px;\r\n"
       << "    -moz-border-radius:8px 0px 0px 8px;\r\n"
       << "  }\r\n"
       << "  div#schedule a {\r\n"
       << "    display:block;\r\n"
       << "    color:#000;\r\n"
       << "    text-decoration:none;\r\n"
       << "    padding:.2em .8em;\r\n"
       << "    border:thin solid #fff;\r\n"
       << "    width:350px;\r\n"
       << "  }\r\n"
       << "  div#schedule a span {\r\n"
       << "    display:none;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover {\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    border-top:thin solid #000;\r\n"
       << "    border-bottom:thin solid #000;\r\n"
       << "    border-left:thin solid #000;\r\n"
       << "    cursor:default;\r\n"
       << "  }\r\n"
       << "  div#schedule a:hover span {\r\n"
       << "    display:block;\r\n"
       << "    position:absolute;\r\n"
       << "    background-color:#F4F4F4;\r\n"
       << "    color:#000;\r\n"
       << "    left:400px;\r\n"
       << "    margin-top:-20px;\r\n"
       << "    width:280px;\r\n"
       << "    padding:5px;\r\n"
       << "    border:thin dashed #000;\r\n"
       << "  }\r\n"
       << "  div.diskstatus {\r\n"
       << "    width:325px;\r\n"
       << "    height:7em;\r\n"
       << "    float:left;\r\n"
       << "  }\r\n"
       << "  div.loadstatus {\r\n"
       << "    width:325px;\r\n"
       << "    height:7em;\r\n"
       << "    float:right;\r\n"
       << "  }\r\n"
       << "  .jobfinished { color: #0000ff; }\r\n"
       << "  .jobaborted { color: #7f0000; }\r\n"
       << "  .joberrored { color: #ff0000; }\r\n"
       << "  .jobrunning { color: #005f00; }\r\n"
       << "  -->\r\n"
       << "  </style>\r\n"
       << "  <title>MythTV Status - " 
       << qdtNow.toString(shortdateformat) 
       << " " << qdtNow.toString(timeformat) << " - "
       << MYTH_BINARY_VERSION << "</title>\r\n"
       << "</head>\r\n"
       << "<body>\r\n\r\n"
       << "  <h1>MythTV Status</h1>\r\n"
       << "  <div class=\"content\">\r\n"
       << "    <h2>Encoder status</h2>\r\n";

    // encoder information ---------------------
    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    int numencoders = 0;
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        bool bIsLocal = elink->isLocal();

        if (bIsLocal)
        {
            os << "    Encoder " << elink->getCardId() << " is local on "
               << gContext->GetHostName();
            numencoders++;
        }
        else
        {
            os << "    Encoder " << elink->getCardId() << " is remote on "
               << elink->getHostname();
            if (!elink->isConnected())
            {
                os << " (currently not connected).<br />";
                continue;
            }
            else
                numencoders++;
        }

        TVState encstate = elink->GetState();

        if (encstate == kState_WatchingLiveTV)
        {
            os << " and is watching Live TV";
            if (bIsLocal)
            {
                QString title, callsign, dummy;
                elink->GetChannelInfo(title, dummy, dummy, dummy, dummy,
                                      dummy, callsign, dummy, dummy,
                                      dummy, dummy, dummy, dummy,
                                      dummy, dummy, dummy);
                os << ": '" << title << "' on "  << callsign;
            }
            os << ".";
        }
        else if (encstate == kState_RecordingOnly || 
                 encstate == kState_WatchingRecording)
        {
            os << " and is recording";
            if (bIsLocal)
            {
                ProgramInfo *pi = elink->GetRecording();
                if (pi)
                {
                    os << " '" << pi->title << "'. This recording will end "
                       << "at " << (pi->recendts).toString(timeformat);
                    delete pi;
                }
            }
            os << ".";
        }
        else
            os << " and is not recording.";

        if (elink->isLowOnFreeSpace())
            os << " <strong>WARNING</strong>:"
               << " This backend is low on free disk space!";

        os << "<br />\r\n";
    }

    os << "  </div>\r\n\r\n"
       << "  <div class=\"content\">\r\n"
       << "    <h2>Schedule</h2>\r\n";

    // upcoming shows ---------------------
    list<ProgramInfo *> recordingList;
    if (m_sched)
        m_sched->getAllPending(&recordingList);

    list<ProgramInfo *>::iterator iterCnt = recordingList.begin();

    unsigned int iNum = 10;

    // count the number of upcoming recordings
    unsigned int iNumRecordings = 0;
    while (iterCnt != recordingList.end())
    {
        if (!((*iterCnt)->recstatus > rsWillRecord ||     
               ((*iterCnt)->recstartts) < QDateTime::currentDateTime())) 
        {                       
               iNumRecordings++;
        }

        if (iNumRecordings > iNum) break;
        iterCnt++;
    }

    if (iNumRecordings < iNum) 
        iNum = iNumRecordings;

    if (iNum == 0)
        os << "    There are no shows scheduled for recording.\r\n";
    else
    {
       os << "    The next " << iNum << " show" << (iNum == 1 ? "" : "s" )
          << " that " << (iNum == 1 ? "is" : "are") 
          << " scheduled for recording:\r\n";

       os << "    <div id=\"schedule\">\r\n";

       list<ProgramInfo *>::iterator iter = recordingList.begin();

       for (unsigned int i = 0; (iter != recordingList.end()) && i < iNum; 
            iter++, i++)
       {
           if ((*iter)->recstatus > rsWillRecord ||     // bad entry, don't show as upcoming
               ((*iter)->recstartts) < QDateTime::currentDateTime()) // rec
           {                       
               i--;
           }
           else
           {
               QString qstrTitle = ((*iter)->title).replace(QRegExp("\""), 
                                                            "&quot;");
               QString qstrDescription = ((*iter)->description).replace(
                                                     QRegExp("\""), "&quot;");
               QString qstrSubtitle = ((*iter)->subtitle).replace(
                                                     QRegExp("\""), "&quot;");
               QString timeToRecstart = "in";
               int totalSecsToRecstart = qdtNow.secsTo((*iter)->recstartts);
               int preRollSeconds = gContext->GetNumSetting("RecordPreRoll", 0);
               totalSecsToRecstart -= preRollSeconds;
               totalSecsToRecstart -= 60; //since we're not displaying seconds
               int totalDaysToRecstart = totalSecsToRecstart / 86400;
               int hoursToRecstart = (totalSecsToRecstart / 3600)
                                     - (totalDaysToRecstart * 24);
               int minutesToRecstart = (totalSecsToRecstart / 60) % 60;

               if (totalDaysToRecstart > 1)
                   timeToRecstart += QString(" %1 days,")
                                             .arg(totalDaysToRecstart);
               else if (totalDaysToRecstart == 1)
                   timeToRecstart += (" 1 day,");

               if (hoursToRecstart != 1)
                   timeToRecstart += QString(" %1 hours and")
                                             .arg(hoursToRecstart);
               else if (hoursToRecstart == 1)
                   timeToRecstart += " 1 hour and";
 
               if (minutesToRecstart != 1)
                   timeToRecstart += QString(" %1 minutes")
                                             .arg(minutesToRecstart);
               else
                   timeToRecstart += " 1 minute";

               if (hoursToRecstart == 0 && minutesToRecstart == 0)
                   timeToRecstart = "within one minute";

               if (totalSecsToRecstart < 0)
                   timeToRecstart = "soon";

               os << "      <a href=\"#\">"
                  << ((*iter)->recstartts).addSecs(-preRollSeconds)
                                          .toString("ddd") << " "
                  << ((*iter)->recstartts).addSecs(-preRollSeconds)
                                          .toString(shortdateformat) << " "
                  << ((*iter)->recstartts).addSecs(-preRollSeconds)
                                          .toString(timeformat) << " - ";
               if (numencoders > 1)
                   os << "Encoder " << (*iter)->cardid << " - ";
               os << (*iter)->channame << " - " << qstrTitle << "<br />"
                  << "<span><strong>" << qstrTitle << "</strong> ("
                  << (*iter)->startts.toString(timeformat) << "-"
                  << (*iter)->endts.toString(timeformat) << ")<br />"
                  << (qstrSubtitle == "" ? QString("") : "<em>" + qstrSubtitle
                                                          + "</em><br /><br />")
                  << qstrDescription << "<br /><br />"
                  << "This recording will start "  << timeToRecstart
                  << " using encoder " << (*iter)->cardid << " with the '";
               dblock.lock();
               os << (*iter)->GetProgramRecordingProfile(m_db);
               dblock.unlock();
               os << "' profile.</span></a><hr />\r\n";
           }
       }
       os  << "    </div>\r\n";
   }

    os << "  </div>\r\n\r\n"
       << "  <div class=\"content\">\r\n"
       << "    <h2>Job Queue</h2>\r\n";

    // Job Queue Entries -----------------
    QMap<int, JobQueueEntry> jobs;
    QMap<int, JobQueueEntry>::Iterator it;
    dblock.lock();
    JobQueue::GetJobsInQueue(m_db, jobs,
                             JOB_LIST_NOT_DONE | JOB_LIST_ERROR |
                             JOB_LIST_RECENT);
    dblock.unlock();

    if (jobs.size())
    {
        QString statusColor;
        QString jobColor;
        QString timeDateFormat;
        timeDateFormat = gContext->GetSetting("DateFormat", "ddd MMMM d") +
                         " " + gContext->GetSetting("TimeFormat", "h:mm AP");


        os << "    Jobs currently in Queue or recently ended:\r\n<br />"
           << "    <div id=\"schedule\">\r\n";

        for (it = jobs.begin(); it != jobs.end(); ++it)
        {
            ProgramInfo *pginfo;

            dblock.lock();
            pginfo = ProgramInfo::GetProgramFromRecorded(m_db, it.data().chanid,
                                                         it.data().starttime);
            dblock.unlock();

            if (!pginfo)
                continue;

            if (it.data().status == JOB_ABORTED)
            {
                statusColor = " class=\"jobaborted\"";
                jobColor = "";
            }
            else if (it.data().status == JOB_ERRORED)
            {
                statusColor = " class=\"joberrored\"";
                jobColor = " class=\"joberrored\"";
            }
            else if (it.data().status == JOB_FINISHED)
            {
                statusColor = " class=\"jobfinished\"";
                jobColor = " class=\"jobfinished\"";
            }
            else if (it.data().status == JOB_RUNNING)
            {
                statusColor = " class=\"jobrunning\"";
                jobColor = " class=\"jobrunning\"";
            }
            else
            {
                statusColor = "";
                jobColor = "";
            }

            QString qstrTitle = pginfo->title.replace(QRegExp("\""), 
                                                        "&quot;");
            QString qstrSubtitle = pginfo->subtitle.replace(
                                                 QRegExp("\""), "&quot;");

            os << "<a href=\"#\">"
               << pginfo->recstartts.toString("ddd") << " "
               << pginfo->recstartts.toString(shortdateformat) << " "
               << pginfo->recstartts.toString(timeformat) << " - "
               << qstrTitle << " - <font" << jobColor << ">"
               << JobQueue::JobText(it.data().type) << "</font><br />"
               << "<span><strong>" << qstrTitle << "</strong> ("
               << pginfo->startts.toString(timeformat) << "-"
               << pginfo->endts.toString(timeformat) << ")<br />"
               << (qstrSubtitle == "" ? QString("") : "<em>" + qstrSubtitle
                                                          + "</em><br /><br />")
               << "Job: " << JobQueue::JobText(it.data().type) << "<br />"
               << "Status: <font" << statusColor << ">"
               << JobQueue::StatusText(it.data().status)
               << "</font><br />"
               << "Status Time: "
               << it.data().statustime.toString(timeDateFormat)
               << "<br />";

            if (it.data().status != JOB_QUEUED)
            {
                os << "Host: ";
                if (it.data().hostname == "")
                    os << QObject::tr("master");
                else
                    os << it.data().hostname;
                os << "<br />";
            }

            if (it.data().comment != "")
            {
                os << "<br />Comments:<br />" << it.data().comment << "<br />";
            }

            os << "</span></a><hr />\r\n";

            delete pginfo;
        }
        os << "      </div>\r\n";
    }
    else
    {
        os << "    Job Queue is currently empty.\r\n\r\n";
    }

    os << "  </div>\r\n\r\n  <div class=\"content\">\r\n"
       << "    <h2>Machine information</h2>\r\n"
       << "    <div class=\"diskstatus\">\r\n";

    // drive space   ---------------------
    int iTotal = -1, iUsed = -1;
    QString rep;

    getFreeSpace(iTotal, iUsed);

    os << "      Disk Usage:\r\n      <ul>\r\n        <li>Total Space: ";
    rep.sprintf(tr("%d,%03d MB "), (iTotal) / 1000, (iTotal) % 1000);
    os << rep << "</li>\r\n";

    os << "        <li>Space Used: ";
    rep.sprintf(tr("%d,%03d MB "), (iUsed) / 1000, (iUsed) % 1000);
    os << rep << "</li>\r\n";

    os << "        <li>Space Free: ";
    rep.sprintf(tr("%d,%03d MB "), (iTotal - iUsed) / 1000,
                (iTotal - iUsed) % 1000);
    os << rep << "</li>\r\n      </ul>\r\n    </div>\r\n\r\n";

    // load average ---------------------
    os << "    <div class=\"loadstatus\">\r\n";
    double rgdAverages[3];
    if (getloadavg(rgdAverages, 3) != -1)
    {
        os << "      This machine's load average:"
           << "\r\n      <ul>\r\n        <li>"
           << "1 Minute: " << rgdAverages[0] << "</li>\r\n"
           << "        <li>5 Minutes: " << rgdAverages[1] << "</li>\r\n"
           << "        <li>15 Minutes: " << rgdAverages[2]
           << "</li>\r\n      </ul>\r\n";
    }

    os << "    </div>\r\n";    // end of disk status and load average

    dblock.lock();

    QString querytext;
    int DaysOfData;
    QDateTime GuideDataThrough;

    querytext = QString("SELECT max(endtime) FROM program;");

    QSqlQuery query = m_db->exec(querytext);

    if (query.isActive() && query.numRowsAffected())
    {
        query.next();
        if (query.isValid())
            GuideDataThrough = QDateTime::fromString(query.value(0).toString(),
                                                     Qt::ISODate);
    }
    dblock.unlock();

    QString mfdLastRunStart, mfdLastRunEnd, mfdLastRunStatus;
    QString DataDirectMessage;

    mfdLastRunStart = gContext->GetSetting("mythfilldatabaseLastRunStart");
    mfdLastRunEnd = gContext->GetSetting("mythfilldatabaseLastRunEnd");
    mfdLastRunStatus = gContext->GetSetting("mythfilldatabaseLastRunStatus");
    DataDirectMessage = gContext->GetSetting("DataDirectMessage");

    os << "    Last mythfilldatabase run started on " << mfdLastRunStart
       << " and ";

    if (mfdLastRunEnd < mfdLastRunStart) 
        os << "is ";
    else 
        os << "ended on " << mfdLastRunEnd << ". ";

    os << mfdLastRunStatus << "<br />\r\n";    

    if (!GuideDataThrough.isNull())
    {
        os << "    There's guide data until "
           << QDateTime(GuideDataThrough).toString("yyyy-MM-dd hh:mm");

        DaysOfData = qdtNow.daysTo(GuideDataThrough);

        if (DaysOfData > 0)
            os << " (" << DaysOfData << " day"
               << (DaysOfData == 1 ? "" : "s" ) << ").";
        else
           os << ".";

        if (DaysOfData <= 3)
            os << " <strong>WARNING</strong>: is mythfilldatabase running?";
    }
    else
        os << "    There's <strong>no guide data</strong> available! "
           << "Have you run mythfilldatabase?";

    if (!DataDirectMessage.isNull())
        os << "<br />\r\nDataDirect Status: " << DataDirectMessage;

    os << "\r\n  </div>\r\n";

#if USING_DVB
    PrintDVBStatus(os);
#endif

    os << "\r\n</body>\r\n</html>\r\n";

    while (recordingList.size() > 0)
    {
        ProgramInfo *pginfo = recordingList.back();
        delete pginfo;
        recordingList.pop_back();
    }
}

