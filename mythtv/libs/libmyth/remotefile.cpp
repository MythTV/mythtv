#include <qurl.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "remotefile.h"
#include "util.h"
#include "mythcontext.h"
#include "mythsocket.h"

RemoteFile::RemoteFile(const QString &_path, bool useRA, int _retries) :
    path(_path),
    usereadahead(useRA),  retries(_retries),
    filesize(-1),         timeoutisfast(false),
    readposition(0),      recordernum(0),
    lock(false),
    controlSock(NULL),    sock(NULL),
    query("QUERY_FILETRANSFER %1")
{
    Open();
}

RemoteFile::~RemoteFile()
{
    Close();
    if (controlSock)
        controlSock->DownRef();
    if (sock)
        sock->DownRef();
}

MythSocket *RemoteFile::openSocket(bool control)
{
    QUrl qurl(path);

    QString host = qurl.host();
    int port = qurl.port();
    QString dir = qurl.path();

    MythSocket *lsock = new MythSocket();
    
    if (!lsock->connect(host, port))
    {
        QString stype = (control) ? "control socket" : "file data socket";
        VERBOSE(VB_IMPORTANT,
                QString("RemoteFile::openSocket(%1): \n"
                        "\t\t\tCould not connect to server \"%2\" @ port %3")
                .arg(stype).arg(host).arg(port));
        lsock->DownRef();
        return NULL;
    }
    
    QString hostname = gContext->GetHostName();

    QStringList strlist;

    if (control)
    {
        strlist = QString("ANN Playback %1 %2").arg(hostname).arg(false);
        lsock->writeStringList(strlist);
        lsock->readStringList(strlist, true);
    }
    else
    {
        strlist = QString("ANN FileTransfer %1 %2 %3")
            .arg(hostname).arg(usereadahead).arg(retries);
        strlist << dir;

        lsock->writeStringList(strlist);
        lsock->readStringList(strlist, true);

        recordernum = strlist[1].toInt();
        filesize = decodeLongLong(strlist, 2);
    }
    
    return lsock;
}    

bool RemoteFile::Open(void)
{
    controlSock = openSocket(true);
    sock = openSocket(false);
    return isOpen();
}

void RemoteFile::Close(void)
{
    if (!controlSock)
        return;

    QStringList strlist = QString(query).arg(recordernum);
    strlist << "DONE";

    lock.lock();
    controlSock->writeStringList(strlist);
    if (!controlSock->readStringList(strlist, true))
    {
        VERBOSE(VB_IMPORTANT, "Remote file timeout.");
    }
    
    if (sock)
    {
        sock->DownRef();
        sock = NULL;
    }    
    if (controlSock)
    {
        controlSock->DownRef();
        controlSock = NULL;
    } 

    lock.unlock();   
}

void RemoteFile::Reset(void)
{
    if (!sock)
    {
        VERBOSE(VB_NETWORK, "RemoteFile::Reset(): Called with no socket");
        return;
    }

    while (sock->bytesAvailable() > 0)
    {
        int avail;
        char *trash;

        lock.lock();
        avail = sock->bytesAvailable();
        trash = new char[avail + 1];
        sock->readBlock(trash, avail);
        delete [] trash;
        lock.unlock();

        VERBOSE(VB_NETWORK, QString ("%1 bytes available during reset.")
                                      .arg(avail));
        usleep(30000);
    }
}

long long RemoteFile::Seek(long long pos, int whence, long long curpos)
{
    if (!sock)
    {
        VERBOSE(VB_NETWORK, "RemoteFile::Seek(): Called with no socket");
        return 0;
    }

    if (!sock->isOpen() || sock->error())
        return 0;

    if (!controlSock->isOpen() || controlSock->error())
        return 0;

    QStringList strlist = QString(query).arg(recordernum);
    strlist << "SEEK";
    encodeLongLong(strlist, pos);
    strlist << QString::number(whence);
    if (curpos > 0)
        encodeLongLong(strlist, curpos);
    else
        encodeLongLong(strlist, readposition);

    lock.lock();
    controlSock->writeStringList(strlist);
    controlSock->readStringList(strlist);
    lock.unlock();

    long long retval = decodeLongLong(strlist, 0);
    readposition = retval;

    Reset();

    return retval;
}

int RemoteFile::Read(void *data, int size)
{
    int recv = 0;
    int sent = 0;
    unsigned zerocnt = 0;
    bool error = false;
    bool response = false;
   
    if (!sock)
    {
        VERBOSE(VB_NETWORK, "RemoteFile::Read(): Called with no socket");
        return -1;
    }

    if (!sock->isOpen() || sock->error())
        return -1;
   
    if (!controlSock->isOpen() || controlSock->error())
        return -1;
    
    lock.lock();
    
    if (sock->bytesAvailable() > 0)
    {
        VERBOSE(VB_NETWORK, 
                "RemoteFile::Read(): Read socket not empty to start!");
        while (sock->waitForMore(5) > 0)
        {
            int avail = sock->bytesAvailable();
            char *trash = new char[avail + 1];
            sock->readBlock(trash, avail);
            delete [] trash;
        }
    }
    
    if (controlSock->bytesAvailable() > 0)
    {
        VERBOSE(VB_NETWORK, 
                "RemoteFile::Read(): Control socket not empty to start!");
        QStringList tempstrlist;
        controlSock->readStringList(tempstrlist);
    }
    
    QStringList strlist = QString(query).arg(recordernum);
    strlist << "REQUEST_BLOCK";
    strlist << QString::number(size);
    controlSock->writeStringList(strlist);

    sent = size;
    
    while (recv < sent && !error && zerocnt++ < 50)
    {
        while (recv < sent && sock->waitForMore(200) > 0)
        {
            int ret = sock->readBlock(((char *)data) + recv, sent - recv);
            if (ret > 0)
            {
                recv += ret;
            }
            else if (sock->error() != MythSocket::NoError)
            {
                VERBOSE(VB_IMPORTANT, "RemoteFile::Read(): socket error");
                error = true;
                break;
            }
        }

        if (controlSock->bytesAvailable() > 0)
        {
            controlSock->readStringList(strlist, true);
            sent = strlist[0].toInt(); // -1 on backend error
            response = true;
        }
    } 
    
    if (!error && !response)
    {
        if (controlSock->readStringList(strlist, true))
        {
            sent = strlist[0].toInt(); // -1 on backend error
        }
        else
        {
            VERBOSE(VB_IMPORTANT, 
                   "RemoteFile::Read(): No response from control socket.");
            sent = -1;
        }
    }

    lock.unlock();

    VERBOSE(VB_NETWORK, QString("Read(): reqd=%1, rcvd=%2, rept=%3, error=%4")
                                .arg(size).arg(recv).arg(sent).arg(error));

    if (sent < 0)
        return sent;

    if (error || sent != recv)
        recv = -1;

    return recv;
}

bool RemoteFile::SaveAs(QByteArray &data)
{
    if (filesize < 0)
        return false;

    data.resize(filesize);
    Read(data.data(), filesize);

    return true;
}

void RemoteFile::SetTimeout(bool fast)
{
    if (timeoutisfast == fast)
        return;

    if (!sock)
    {
        VERBOSE(VB_NETWORK, "RemoteFile::Seek(): Called with no socket");
        return;
    }

    if (!sock->isOpen() || sock->error())
        return;

    if (!controlSock->isOpen() || controlSock->error())
        return;

    QStringList strlist = QString(query).arg(recordernum);
    strlist << "SET_TIMEOUT";
    strlist << QString::number((int)fast);

    lock.lock();
    controlSock->writeStringList(strlist);
    controlSock->readStringList(strlist);
    lock.unlock();

    timeoutisfast = fast;
}

