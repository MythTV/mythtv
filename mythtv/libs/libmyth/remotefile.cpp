#include <qapplication.h>
#include <qsocket.h>
#include <qurl.h>

#include <unistd.h>

#include <iostream>
using namespace std;

#include "remotefile.h"
#include "util.h"

RemoteFile::RemoteFile(const QString &url, bool needevents, int lrecordernum)
{
    type = 0;
    path = url;
    readposition = 0;
    filesize = -1;
    pthread_mutex_init(&lock, NULL);

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

    QSocket *sock = new QSocket();

    sock->connectToHost(host, port);

    int num = 0;
    while (sock->state() == QSocket::HostLookup ||
           sock->state() == QSocket::Connecting)
    {
        if (events)
            qApp->processEvents();

        usleep(50);
        num++;
        if (num > 100)
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

    char localhostname[256];
    if (gethostname(localhostname, 256))
    {
        cerr << "Error getting local hostname\n";
        exit(0);
    }

    QStringList strlist;

    if (control)
    {
        strlist = QString("ANN Playback %1 %2").arg(localhostname).arg(false);
        WriteStringList(sock, strlist);
        ReadStringList(sock, strlist);
    }
    else
    {
        if (type == 0)
        {
            strlist = QString("ANN FileTransfer %1").arg(localhostname);
            strlist << dir;

            WriteStringList(sock, strlist);
            ReadStringList(sock, strlist);

            recordernum = strlist[1].toInt();
            filesize = decodeLongLong(strlist, 2);
        }
        else
        {
            strlist = QString("ANN RingBuffer %1 %2").arg(localhostname)
                             .arg(recordernum);
            WriteStringList(sock, strlist);
            ReadStringList(sock, strlist);
        }
    }
    
    return sock;
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

    pthread_mutex_lock(&lock);
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    pthread_mutex_unlock(&lock);

    if (sock)
        sock->close();
    if (controlSock)
        controlSock->close();
}

void RemoteFile::Reset(void)
{
    usleep(10000);

    pthread_mutex_lock(&lock);
    int avail = sock->bytesAvailable();
    char *trash = new char[avail + 1];
    sock->readBlock(trash, avail);
    delete [] trash;
    pthread_mutex_unlock(&lock);
}
    
bool RemoteFile::RequestBlock(int size)
{
    QStringList strlist = QString(query).arg(recordernum);
    strlist << "REQUEST_BLOCK" + append;
    strlist << QString::number(size);

    pthread_mutex_lock(&lock);
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    pthread_mutex_unlock(&lock);

    return strlist[0].toInt();
}

long long RemoteFile::Seek(long long pos, int whence, long long curpos)
{
    QStringList strlist = QString(query).arg(recordernum);
    strlist << "PAUSE" + append;

    pthread_mutex_lock(&lock);
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    pthread_mutex_unlock(&lock);

    strlist.clear();

    usleep(50000);
   
    int avail = sock->bytesAvailable();
    char *trash = new char[avail + 1];
    sock->readBlock(trash, avail);
    delete [] trash;

    strlist = QString(query).arg(recordernum);
    strlist << "SEEK" + append;
    encodeLongLong(strlist, pos);
    strlist << QString::number(whence);
    if (curpos > 0)
        encodeLongLong(strlist, curpos);
    else
        encodeLongLong(strlist, readposition);

    pthread_mutex_lock(&lock);
    WriteStringList(controlSock, strlist);
    ReadStringList(controlSock, strlist);
    pthread_mutex_unlock(&lock);

    long long retval = decodeLongLong(strlist, 0);
    readposition = retval;

    avail = sock->bytesAvailable();
    trash = new char[avail + 1];
    sock->readBlock(trash, avail);
    delete [] trash;

    return retval;
}

int RemoteFile::Read(void *data, int size, bool singlefile)
{
    int ret;
    unsigned tot = 0;
    unsigned zerocnt = 0;

    while (sock->bytesAvailable() < (unsigned)size)
    {
        int reqsize = 128000;
        if (singlefile && size - sock->bytesAvailable() < 128000)
            reqsize = size - sock->bytesAvailable();

        RequestBlock(reqsize);

        zerocnt++;
        if (zerocnt == 100)
        {
            printf("EOF %u\n", size);
            break;
        }
        usleep(50);
    }

    if (sock->bytesAvailable() >= (unsigned)size)
    {
        ret = sock->readBlock(((char *)data) + tot, size - tot);
        tot += ret;
    }

    readposition += tot;
    return tot;
}

bool RemoteFile::SaveAs(QByteArray &data, bool events)
{
    Start(true);

    if (filesize < 0)
        return false;

    data.resize(filesize);
    Read(data.data(), filesize, events);

    return true;
} 
