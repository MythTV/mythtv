#include <qapplication.h>
#include <qsocket.h>
#include <qurl.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "remotefile.h"
#include "util.h"
#include "mythcontext.h"

RemoteFile::RemoteFile(const QString &url, bool needevents, int lrecordernum)
{
    type = 0;
    path = url;
    readposition = 0;
    filesize = -1;

    if (lrecordernum > 0)
    {
        type = 1;
        query = "QUERY_RECORDER %1";
        append = "_RINGBUF";
        recordernum = lrecordernum;
    }
    else
    {
        type = 0;
        query = "QUERY_FILETRANSFER %1";
        append = "";
        recordernum = -1;
    }

    if (needevents)
    {
        controlSock = NULL;
        sock = NULL;
    }
    else
    {
        controlSock = openSocket(true);
        sock = openSocket(false);
    }
}

RemoteFile::~RemoteFile()
{
    Close();
    if (controlSock)
        delete controlSock;
    if (sock)
        delete sock;
}

void RemoteFile::Start(bool events)
{
    if (!controlSock)
    {
        controlSock = openSocket(true, events);
        sock = openSocket(false, events);
    }
}

QSocket *RemoteFile::openSocket(bool control, bool events)
{
    QUrl qurl(path);

    QString host = qurl.host();
    int port = qurl.port();
    QString dir = qurl.path();

    qApp->lock();
    QSocket *sock = new QSocket();
    sock->connectToHost(host, port);
    qApp->unlock();

    int num = 0;
    while (sock->state() == QSocket::HostLookup ||
           sock->state() == QSocket::Connecting)
    {
        if (events)
            qApp->processEvents();

        usleep(50);
        num++;
        if (num > 500)
        {
            cerr << "Connection timed out.\n";
            exit(0);
        }
    }

    if (sock->state() != QSocket::Connected)
    {
        cout << "Could not connect to server\n";
        return NULL;
    }

    QString hostname = gContext->GetHostName();

    QStringList strlist;

    if (control)
    {
        strlist = QString("ANN Playback %1 %2").arg(hostname).arg(false);
        WriteStringList(sock, strlist);
        ReadStringList(sock, strlist);
    }
    else
    {
        if (type == 0)
        {
            strlist = QString("ANN FileTransfer %1").arg(hostname);
            strlist << dir;

            WriteStringList(sock, strlist);
            ReadStringList(sock, strlist);

            recordernum = strlist[1].toInt();
            filesize = decodeLongLong(strlist, 2);
        }
        else
        {
            strlist = QString("ANN RingBuffer %1 %2").arg(hostname)
                             .arg(recordernum);
            WriteStringList(sock, strlist);
            ReadStringList(sock, strlist);
        }
    }
    
    return sock;
}    

long long RemoteFile::GetFileSize(void)
{
    return filesize;
}

bool RemoteFile::isOpen(void)
{
    return (sock != NULL && controlSock != NULL);
}

QSocket *RemoteFile::getSocket(void)
{
    return sock;
}

void RemoteFile::Close(void)
{
    if (!controlSock)
        return;

    QStringList strlist = QString(query).arg(recordernum);
    strlist << "DONE" + append;

    lock.lock();
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    lock.unlock();

    qApp->lock();
    if (sock)
        sock->close();
    if (controlSock)
        controlSock->close();
    qApp->unlock();
}

void RemoteFile::Reset(void)
{
    int avail;
    char *trash;

    usleep(10000);

    while (sock->bytesAvailable() > 0)
    {
        lock.lock();
        qApp->lock();
        avail = sock->bytesAvailable();
        trash = new char[avail + 1];
        sock->readBlock(trash, avail);
        delete [] trash;
        qApp->unlock();
        lock.unlock();

        // cerr << avail << " bytes available during reset.\n";
        usleep(30000);
    }
}
    
bool RemoteFile::RequestBlock(int size)
{
    QStringList strlist = QString(query).arg(recordernum);
    strlist << "REQUEST_BLOCK" + append;
    strlist << QString::number(size);

    lock.lock();
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    lock.unlock();

    return strlist[0].toInt();
}

long long RemoteFile::Seek(long long pos, int whence, long long curpos)
{
    QStringList strlist = QString(query).arg(recordernum);
    strlist << "SEEK" + append;
    encodeLongLong(strlist, pos);
    strlist << QString::number(whence);
    if (curpos > 0)
        encodeLongLong(strlist, curpos);
    else
        encodeLongLong(strlist, readposition);

    lock.lock();
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    lock.unlock();

    long long retval = decodeLongLong(strlist, 0);
    readposition = retval;

    Reset();

    return retval;
}

int RemoteFile::Read(void *data, int size, bool singlefile)
{
    int ret;
    unsigned tot = 0;
    unsigned zerocnt = 0;

    qApp->lock();
    while (sock->bytesAvailable() < (unsigned)size)
    {
        int reqsize = 128000;
        if (singlefile && size - sock->bytesAvailable() < 128000)
            reqsize = size - sock->bytesAvailable();
        qApp->unlock();

        RequestBlock(reqsize);

        zerocnt++;
        if (zerocnt == 100)
        {
            printf("EOF %u\n", size);
            break;
        }
        usleep(50);
        qApp->lock();
    }

    if (sock->bytesAvailable() >= (unsigned)size)
    {
        ret = sock->readBlock(((char *)data) + tot, size - tot);
        tot += ret;
    }

    qApp->unlock();

    readposition += tot;
    return tot;
}

bool RemoteFile::SaveAs(QByteArray &data, bool events)
{
    Start(events);

    if (filesize < 0)
        return false;

    data.resize(filesize);
    Read(data.data(), filesize, events);

    return true;
} 
