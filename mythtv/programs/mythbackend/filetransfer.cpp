#include <qapplication.h>

#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#include "filetransfer.h"

#include "RingBuffer.h"
#include "libmyth/util.h"

FileTransfer::FileTransfer(QString &filename, QSocket *remote)
{
    rbuffer = new RingBuffer(filename, false);
    pthread_mutex_init(&readthreadLock, NULL);
    sock = remote;
    readthreadlive = false;
    readrequest = 0;
    ateof = false;

    pthread_create(&readthread, NULL, FTReadThread, this);

    while (!readthreadlive)
        usleep(50);
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (rbuffer)
        delete rbuffer;
}

bool FileTransfer::isOpen(void)
{
    if (rbuffer && rbuffer->IsOpen())
        return true;
    return false;
}

void FileTransfer::Stop(void)
{
    if (readthreadlive)
    {
        readthreadlive = false;
        rbuffer->StopReads();
        pthread_join(readthread, NULL);
    }
}

void FileTransfer::Pause(void)
{
    rbuffer->StopReads();
    pthread_mutex_lock(&readthreadLock);
}

void FileTransfer::Unpause(void)
{
    rbuffer->StartReads();
    pthread_mutex_unlock(&readthreadLock);
}

bool FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive)
        return true;

    if (size > 128000)
        size = 128000;

    pthread_mutex_lock(&readthreadLock);
    readrequest = size;
    pthread_mutex_unlock(&readthreadLock);

    while (readrequest > 0 && readthreadlive && !ateof)
    {
        qApp->unlock();
        usleep(100);
        qApp->lock();
    }

    return ateof;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (!rbuffer)
        return -1;
    if (!readthreadlive)
        return -1;

    ateof = false;

    if (whence == SEEK_CUR)
    {
        long long desired = curpos + pos;
        long long realpos = rbuffer->GetReadPosition();

        pos = desired - realpos;
    }

    long long ret = rbuffer->Seek(pos, whence);

    Unpause();
    return ret;
}

void FileTransfer::DoFTReadThread(void)
{
    char *buffer = new char[128001];

    readthreadlive = true;
    while (readthreadlive && rbuffer)
    {
        while (readrequest == 0 && readthreadlive)
            usleep(5000);

        if (!readthreadlive)
            break;

        pthread_mutex_lock(&readthreadLock);

        int ret = rbuffer->Read(buffer, readrequest);
        if (!rbuffer->GetStopReads())
        {
            WriteBlock(sock, buffer, ret);
            if (ret == 0)
                ateof = true;
        }        

        readrequest -= ret;

        pthread_mutex_unlock(&readthreadLock);
    }

    delete [] buffer;
}

void *FileTransfer::FTReadThread(void *param)
{
    FileTransfer *ft = (FileTransfer *)param;
    ft->DoFTReadThread();

    return NULL;
}

long long FileTransfer::GetFileSize(void)
{
    QString filename = rbuffer->GetFilename();

    struct stat64 st;
    long long size = 0;

    if (stat64(filename.ascii(), &st) == 0)
        size = st.st_size;

    return size;
}
