#include <qsqldatabase.h>
#include <unistd.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

#include <sys/stat.h>


#include "../../libs/libmyth/mythcontext.h"
#include "../../libs/libmyth/util.h"

#include "mainserver.h"
#include "scheduler.h"

MainServer::MainServer(MythContext *context, int port, 
                       QMap<int, EncoderLink *> *tvList)
{
    encoderList = tvList;
    m_context = context;

    recordfileprefix = context->GetFilePrefix();

    mythserver = new MythServer(port);
    connect(mythserver, SIGNAL(newConnect(QSocket *)), 
            SLOT(newConnection(QSocket *)));
    connect(mythserver, SIGNAL(endConnect(QSocket *)), 
            SLOT(endConnection(QSocket *)));
}

MainServer::~MainServer()
{
    delete mythserver;
}

void MainServer::newConnection(QSocket *socket)
{
    connect(socket, SIGNAL(readyRead()), this, SLOT(readSocket()));
}

void MainServer::readSocket(void)
{
    QSocket *socket = (QSocket *)sender();
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
            cout << "Command: " << command << endl;

            if (tokens.size() < 3 || tokens.size() > 4)
                cerr << "Bad ANN query\n";
            else
                HandleAnnounce(tokens, socket);
            return;
        }
        else if (command == "DONE")
        {
            HandleDone(socket);
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
            else if (command == "DELETE_RECORDING")
            {
                HandleDeleteRecording(pbs);
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
                    HandleGetConflictingRecordings(tokens[1], pbs);
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
        }
        else
        {
            cout << "unknown connection\n";
        }
    }
}

void MainServer::HandleAnnounce(QStringList commands, QSocket *socket)
{
    if (commands[1] == "Playback")
    {
        cout << "adding: " << commands[2] << " as a player\n";
        PlaybackSock *pbs = new PlaybackSock(socket, commands[2]);
        PlaybackSock *existing = getPlaybackByName(commands[2]);
        if (existing)
        {
            QSocket *sock = existing->getSocket();
            sock->close();
            delete existing;
        }
        playbackList[commands[2]] = pbs;
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
    }
}

void MainServer::HandleDone(QSocket *socket)
{
    socket->close();
    if (isRingBufSock(socket))
    {

    }
}

void MainServer::HandleQueryRecordings(QString type, PlaybackSock *pbs)
{
    QSqlQuery query;
    QString thequery;

    thequery = "SELECT chanid,starttime,endtime,title,subtitle,description "
               "FROM recorded ORDER BY starttime";

    if (type == "Delete")
        thequery += " DESC";
    thequery += ";";

    query.exec(thequery);

    QStringList outputlist;

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
            proginfo->title = query.value(3).toString();
            proginfo->subtitle = query.value(4).toString();
            proginfo->description = query.value(5).toString();
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
                                  "WHERE chanid = %1").arg(proginfo->chanid);
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

            proginfo->pathname = proginfo->GetRecordFilename("/mnt/store");
        
            struct stat64 st;
    
            long long size = 0;
            if (stat64(proginfo->pathname.ascii(), &st) == 0)
                size = st.st_size;

            proginfo->filesize = size;

            proginfo->ToStringList(outputlist);
        }
    }
    else
        outputlist << "0";

    WriteStringList(pbs->getSocket(), outputlist);
}

static void *SpawnDelete(void *param)
{
    QString *filenameptr = (QString *)param;
    QString filename = *filenameptr;
    
    unlink(filename.ascii());
    
    filename += ".png";
    unlink(filename.ascii());
    
    filename = *filenameptr;
    filename += ".bookmark";
    unlink(filename.ascii());
    
    filename = *filenameptr;
    filename += ".cutlist";
    unlink(filename.ascii());

    delete filenameptr;

    return NULL;
}

void MainServer::HandleDeleteRecording(PlaybackSock *pbs)
{
    QStringList strlist;
    ReadStringList(pbs->getSocket(), strlist);

    ProgramInfo pginfo;

    pginfo.FromStringList(strlist, 0);

    cout << pginfo.title << endl;
/*
        QString filename = rec->GetRecordFilename(fileprefix);

        QSqlQuery query;
        QString thequery;

        QString startts = rec->startts.toString("yyyyMMddhhmm");
        startts += "00";
        QString endts = rec->endts.toString("yyyyMMddhhmm");
        endts += "00";

        thequery = QString("DELETE FROM recorded WHERE chanid = %1 AND title "
                           "= \"%2\" AND starttime = %3 AND endtime = %4;")
                           .arg(rec->chanid).arg(rec->title).arg(startts)
                           .arg(endts);

        query = db->exec(thequery);
        if (!query.isActive())
        {
            cerr << "DB Error: recorded program deletion failed, SQL query "
                 << "was:" << endl;
            cerr << thequery << endl;
        }

        QString *fileptr = new QString(filename);

        pthread_t deletethread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        pthread_create(&deletethread, &attr, SpawnDelete, fileptr);
*/
}

void MainServer::HandleQueryFreeSpace(PlaybackSock *pbs)
{
    QString command;
    command.sprintf("df -k -P %s", recordfileprefix.ascii());

    FILE *file = popen(command.ascii(), "r");

    int totalspace = -1, usedspace = -1;

    if (file)
    {
        char buffer[1024];
        fgets(buffer, 1024, file);
        fgets(buffer, 1024, file);

        char dummy[1024];
        int dummyi;
        sscanf(buffer, "%s %d %d %d %s %s\n", dummy, &totalspace, &usedspace,
               &dummyi, dummy, dummy);

        totalspace /= 1000;
        usedspace /= 1000;
        pclose(file);
    }

    QStringList strlist;

    strlist << QString::number(totalspace) << QString::number(usedspace);

    WriteStringList(pbs->getSocket(), strlist);
}

void MainServer::HandleGetPendingRecordings(PlaybackSock *pbs)
{
    Scheduler *sched = new Scheduler(QSqlDatabase::database());

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

void MainServer::HandleGetConflictingRecordings(QString purge,
                                                PlaybackSock *pbs)
{
    Scheduler *sched = new Scheduler(QSqlDatabase::database());

    bool removenonplaying = purge.toInt();

    QStringList strlist;
    ReadStringList(pbs->getSocket(), strlist);

    ProgramInfo *pginfo = new ProgramInfo();
    pginfo->FromStringList(strlist, 0);

    sched->FillRecordLists(false);

    list<ProgramInfo *> *conflictlist = sched->getConflicting(pginfo, 
                                                              removenonplaying);

    strlist.clear();

    strlist << QString::number(conflictlist->size());

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

    strlist << QString::number(1);

    WriteStringList(pbs->getSocket(), strlist);
}

void MainServer::HandleRecorderQuery(QStringList list, QStringList commands,
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

    QString command = list[1];

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
        long long pos = decodeLongLong(list, 2);

        long long value = enc->GetFreeSpace(pos);
        encodeLongLong(retlist, value);
    }
    else if (command == "GET_KEYFRAME_POS")
    {
        long long desired = decodeLongLong(list, 2);

        long long value = enc->GetKeyframePosition(desired);
        encodeLongLong(retlist, value);
    }
    else if (command == "SETUP_RING_BUFFER")
    {
        bool pip = list[2].toInt();

        QString path = "";
        long long filesize = 0;
        long long fillamount = 0;

        enc->SetupRingBuffer(path, filesize, fillamount, pip);

        QString ip = m_context->GetSetting("ServerIP");
        QString port = m_context->GetSetting("ServerPort");

        QString url = QString("rbuf://") + ip + ":" + port + path;

        retlist << url;
        encodeLongLong(retlist, filesize);
        encodeLongLong(retlist, fillamount);
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
    else if (command == "CHANGE_CHANNEL")
    {
        bool up = list[2].toInt(); 
        enc->ChangeChannel(up);
        retlist << "ok";
    }
    else if (command == "SET_CHANNEL")
    {
        QString name = list[2];
        enc->SetChannel(name);
        retlist << "ok";
    }
    else if (command == "CHECK_CHANNEL")
    {
        QString name = list[2];
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
        QString name = list[2];

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
    else if (command == "SEEK_RINGBUF")
    {
        long long pos = decodeLongLong(list, 2);
        int whence = list[4].toInt();
        long long curpos = decodeLongLong(list, 5);

        long long ret = enc->SeekRingBuffer(curpos, pos, whence);
        encodeLongLong(retlist, ret);
    }
    else if (command == "DONE_RINGBUF")
    {
        enc->KillReadThread();

        retlist << "OK";
    }

    WriteStringList(pbs->getSocket(), retlist);    
}

void MainServer::endConnection(QSocket *socket)
{
    QMap<QString, PlaybackSock *>::Iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        QSocket *sock = it.data()->getSocket();
        if (sock == socket)
        {
            cout << "deleting " << it.data()->getHostname() 
                 << " from player list\n";
            playbackList.erase(it);
            return;
        }
    }

    vector<QSocket *>::iterator ft = fileTransferList.begin();
    for (; ft != fileTransferList.end(); ++ft)
    {
        QSocket *sock = *ft;
        if (sock == socket)
        {
            cout << "deleting from ft list\n";
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
            cout << "deleting from ringBuf list\n";
            ringBufList.erase(rt);
            return;
        }
    }

    cout << "unknown socket\n";
}

PlaybackSock *MainServer::getPlaybackByName(QString name)
{
    PlaybackSock *retval = NULL;

    if (playbackList.find(name) != playbackList.end())
        retval = playbackList[name];

    return retval;
}

PlaybackSock *MainServer::getPlaybackBySock(QSocket *sock)
{
    PlaybackSock *retval = NULL;

    QMap<QString, PlaybackSock *>::Iterator it = playbackList.begin();
    for (; it != playbackList.end(); ++it)
    {
        if (sock == it.data()->getSocket())
        {
            retval = it.data();
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
