#include <qapplication.h>
#include <qsqldatabase.h>
#include <qfile.h>
#include <qurl.h>
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

MainServer::MainServer(bool master, int port, int statusport, 
                       QMap<int, EncoderLink *> *tvList)
{
    ismaster = master;
    masterServerSock = NULL;

    encoderList = tvList;

    recordfileprefix = gContext->GetFilePrefix();

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
        //masterServerReconnect->start(1000, true);
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
                cerr << "Bad ANN query\n";
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
            if (command == "QUERY_RECORDINGS")
            {
                if (tokens.size() != 2)
                    cerr << "Bad QUERY_RECORDINGS query\n";
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
                    cerr << "Bad QUERY_GETCONFLICTING\n";
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
                    cerr << "Bad QUERY_RECORDER\n";
                else
                    HandleRecorderQuery(listline, tokens, pbs);
            }
            else if (command == "QUERY_REMOTEENCODER")
            {
                if (tokens.size() != 2)
                    cerr << "Bad QUERY_REMOTEENCODER\n";
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
                    cerr << "Bad QUERY_FILETRANSFER\n";
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
               cout << "unknown command: " << command << endl;
            }
        }
        else
        {
            cout << "unknown socket\n";
        }
    }
}

void MainServer::customEvent(QCustomEvent *e)
{
    QStringList broadcast;
    bool sendstuff = false;

    if ((MythEvent::Type)(e->type()) == MythEvent::MythEventMessage)
    {
        MythEvent *me = (MythEvent *)e;

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

        if (masterServerSock)
        {
            WriteStringList(masterServerSock, broadcast);
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

        cout << "adding: " << commands[2] << " as a player " << wantevents
             << "\n";
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2], wantevents);
        playbackList.push_back(pbs);
    }
    else if (commands[1] == "SlaveBackend")
    {
        cout << "adding: " << commands[2] << " as a slave backend server\n";
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

        ScheduledRecording::signalChange(QSqlDatabase::database());

        playbackList.push_back(pbs);
    }
    else if (commands[1] == "RingBuffer")
    {
        cout << "adding: " << commands[2] << " as a remote ringbuffer\n";
        ringBufList.push_back(socket);
        int recnum = commands[3].toInt();

        QMap<int, EncoderLink *>::Iterator iter = encoderList->find(recnum);
        if (iter == encoderList->end())
        {
            cerr << "Unknown encoder.\n";
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
                int dsp_fd = open(audiodevice, O_RDWR);
                if (dsp_fd != -1)
                {
                    dsp_status = ioctl(dsp_fd, SNDCTL_DSP_GETCAPS,
                                       &soundcardcaps);
                    if (dsp_status != -1)
                    {
                        if (!(soundcardcaps & DSP_CAP_DUPLEX))
                        cerr << "WARNING:  Capture device " << audiodevice 
                             << "is not reporting full duplex "
                             << " capability.\nSee docs/mythtv-HOWTO, "
                             << "section 17 for more information.\n" << endl;
                    }
                    else 
                        cerr << "Could not get capabilities for "
                             << "audio device: " << audiodevice << endl;
                    close(dsp_fd);
                }
                else 
                    cerr << "Could not open audio device: " 
                         << audiodevice << endl;
            }
        }
    }
    else if (commands[1] == "FileTransfer")
    {
        cout << "adding: " << commands[2] << " as a remote file transfer\n";
        QString filename = slist[1];    

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

    MythContext::KickDatabase(QSqlDatabase::database());

    thequery = "SELECT chanid,starttime,endtime,title,subtitle,description, "
               "hostname FROM recorded ORDER BY starttime";

    if (type == "Delete")
        thequery += " DESC";
    thequery += ";";

    query.exec(thequery);

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
            QString hostname = query.value(6).toString();

            if (hostname.isEmpty() || hostname.isNull())
                hostname = gContext->GetHostName();

            proginfo->conflicting = false;

            if (proginfo->title == QString::null)
                proginfo->title = "";
            if (proginfo->subtitle == QString::null)
                proginfo->subtitle = "";
            if (proginfo->description == QString::null)
                proginfo->description = "";

            QSqlQuery subquery;
            QString subquerystr;

            subquerystr = QString("SELECT channum,name,callsign FROM channel "
                                  "WHERE chanid = %1;").arg(proginfo->chanid);
            subquery.exec(subquerystr);

            if (subquery.isActive() && subquery.numRowsAffected() > 0)
            {
                subquery.next();

                proginfo->chanstr = subquery.value(0).toString();
                proginfo->channame = subquery.value(1).toString();
                proginfo->chansign = subquery.value(2).toString();
            }
            else
            {
                proginfo->chanstr = "#" + proginfo->chanid;
                proginfo->channame = "#" + proginfo->chanid;
                proginfo->chansign = "#" + proginfo->chanid;
            }

            QString lpath = proginfo->GetRecordFilename(fileprefix);
            QString ip = gContext->GetSetting("BackendServerIP");
            QString port = gContext->GetSetting("BackendServerPort");

            if (hostname == gContext->GetHostName())
            {
                if (pbs->isLocal())
                    proginfo->pathname = lpath;
                else
                    proginfo->pathname = QString("myth://") + ip + ":" + port + 
                                         lpath;

                struct stat64 st;

                long long size = 0;
                if (stat64(lpath.ascii(), &st) == 0)
                    size = st.st_size;

                proginfo->filesize = size;
            }
            else
            {
                PlaybackSock *slave = getSlaveByHostname(hostname);
 
                if (!slave)
                {
                    cerr << "Couldn't find backend for: " << proginfo->title
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
        pginfo->pathname = QString("myth://") + ip + ":" + port + lpath;

    struct stat64 st;

    long long size = 0;
    if (stat64(lpath.ascii(), &st) == 0)
        size = st.st_size;

    pginfo->filesize = size;

    QStringList strlist;

    pginfo->ToStringList(strlist);

    delete pginfo;

    WriteStringList(pbs->getSocket(), strlist);
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

        if (!slave)
            cerr << "Couldn't find backend for: " << pginfo->title
                 << " : " << pginfo->subtitle << endl;
        else
            slave->DeleteRecording(pginfo);

        QStringList outputlist = "OK";
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    if (pginfo->hostname != gContext->GetHostName())
    {
        cout << "Somehow we got a request to delete a program for: " 
             << pginfo->hostname << endl;

        QStringList outputlist = "BAD";
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isConnected() && elink->MatchesRecording(pginfo))
        {
            elink->StopRecording();

            while (elink->isBusy())
                usleep(100);
        }
    }

    QString fileprefix = gContext->GetFilePrefix();
    QString filename = pginfo->GetRecordFilename(fileprefix);

    QSqlQuery query;
    QString thequery;

    QString startts = pginfo->startts.toString("yyyyMMddhhmm");
    startts += "00";
    QString endts = pginfo->endts.toString("yyyyMMddhhmm");
    endts += "00";

    MythContext::KickDatabase(QSqlDatabase::database());

    thequery = QString("DELETE FROM recorded WHERE chanid = %1 AND title "
                       "= \"%2\" AND starttime = %3 AND endtime = %4;")
                       .arg(pginfo->chanid).arg(pginfo->title.utf8())
                       .arg(startts).arg(endts);

    query.exec(thequery);
    if (!query.isActive())
    {
        cerr << "DB Error: recorded program deletion failed, SQL query "
             << "was:" << endl;
        cerr << thequery << endl;
    }

    DeleteStruct *ds = new DeleteStruct;
    ds->filename = filename;

    pthread_t deletethread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&deletethread, &attr, SpawnDelete, ds);

    QStringList outputlist = "OK";
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

    QStringList strlist;
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

        if (!slave)
            cerr << "Couldn't find backend for: " << pginfo->title
                 << " : " << pginfo->subtitle << endl;
        else
            exists = slave->CheckFile(pginfo);

        QStringList outputlist = QString::number(exists);
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    if (pginfo->hostname != gContext->GetHostName())
    {
        cout << "Somehow we got a request to check a file for: "
             << pginfo->hostname << endl;

        QStringList outputlist = QString::number(exists);
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    QUrl qurl(pginfo->pathname);
    QFile checkFile(qurl.path());

    if (checkFile.exists() == true)
        exists = 1;

    QStringList strlist = QString::number(exists);
    WriteStringList(pbs->getSocket(), strlist);

    delete pginfo;
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs)
{
    MythContext::KickDatabase(QSqlDatabase::database());

    Scheduler *sched = new Scheduler(NULL, QSqlDatabase::database());

    bool conflicts = sched->FillRecordLists(false);
    list<ProgramInfo *> *recordinglist = sched->getAllPending();

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
    MythContext::KickDatabase(QSqlDatabase::database());

    Scheduler *sched = new Scheduler(NULL, QSqlDatabase::database());

    bool removenonplaying = purge.toInt();

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(slist, 1);

    sched->FillRecordLists(false);

    list<ProgramInfo *> *conflictlist = sched->getConflicting(pginfo, 
                                                              removenonplaying);

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

    EncoderLink *encoder = NULL;

    QMap<int, EncoderLink *>::Iterator iter = encoderList->begin();
    for (; iter != encoderList->end(); ++iter)
    {
        EncoderLink *elink = iter.data();

        if (elink->isConnected() && !elink->isBusy())
        {
            encoder = elink;
            retval = iter.key();
            break;
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
        cerr << "Unknown encoder.\n";
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
        enc->ChangeColour(up);
        retlist << "ok";
    }
    else if (command == "CHANGE_CONTRAST")
    {
        bool up = slist[2].toInt(); 
        enc->ChangeContrast(up);
        retlist << "ok";
    }
    else if (command == "CHANGE_BRIGHTNESS")
    {
        bool up = slist[2].toInt(); 
        enc->ChangeBrightness(up);
        retlist << "ok";
    }
    else if (command == "CHECK_CHANNEL")
    {
        QString name = slist[2];
        retlist << QString::number((int)(enc->CheckChannel(name)));
    }
    else if (command == "GET_PROGRAM_INFO")
    {
        QString title = "", subtitle = "", desc = "", category = "";
        QString starttime = "", endtime = "", callsign = "", iconpath = "";
        QString channelname = "";

        enc->GetChannelInfo(title, subtitle, desc, category, starttime,
                            endtime, callsign, iconpath, channelname);

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

        retlist << title;
        retlist << subtitle;
        retlist << desc;
        retlist << category;
        retlist << starttime;
        retlist << endtime;
        retlist << callsign;
        retlist << iconpath;
        retlist << channelname;
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
        cout << "unknown command: " << command << endl;
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
        cerr << "Unknown encoder.\n";
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
        cerr << "Unknown file transfer socket\n";
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
        cout << "Unknown command: " << command << endl;
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
            cerr << "Couldn't find backend for: " << pginfo->title
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
        cout << "Somehow we got a request to generate a preview pixmap for: "
             << pginfo->hostname << endl;

        QStringList outputlist = "BAD";
        WriteStringList(pbs->getSocket(), outputlist);
        delete pginfo;
        return;
    }

    QUrl qurl = pginfo->pathname;
    QString filename = qurl.path();

    int len = 0;
    int width = 0, height = 0;

    EncoderLink *elink = encoderList->begin().data();

    unsigned char *data = (unsigned char *)elink->GetScreenGrab(filename, 64,
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
            if ((*it)->isSlaveBackend())
            {
                cout << "Slave backend: " << (*it)->getHostname() 
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

    cout << "unknown socket\n";
}

PlaybackSock *MainServer::getSlaveByHostname(QString &hostname)
{
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
    delete masterServerSock;
    masterServerSock = NULL;
    masterServerReconnect->start(1000);
}

void MainServer::reconnectTimeout(void)
{
    masterServerSock = new QSocket();

    QString server = gContext->GetSetting("MasterServerIP", "localhost");
    int port = gContext->GetNumSetting("MasterServerPort", 6543);

    cerr << "Attempting to connect to master server\n";

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
            cerr << "Connection to master server timed out.\n";
            masterServerReconnect->start(1000);
            delete masterServerSock;
            masterServerSock = NULL;
            return;
        }
    }

    if (masterServerSock->state() != QSocket::Connected)
    {
        cerr << "Could not connect to master server\n";
        masterServerReconnect->start(1000);
        delete masterServerSock;
        masterServerSock = NULL;
    }

    cerr << "done\n";

    QString str = QString("ANN SlaveBackend %1 %2")
                          .arg(gContext->GetHostName())
                          .arg(gContext->GetSetting("BackendServerIP"));

    QStringList strlist = str;
    WriteStringList(masterServerSock, strlist);
    ReadStringList(masterServerSock, strlist);

    connect(masterServerSock, SIGNAL(readyRead()), this, SLOT(readSocket()));
    connect(masterServerSock, SIGNAL(connectionClosed()), this, 
            SLOT(masterServerDied()));
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
