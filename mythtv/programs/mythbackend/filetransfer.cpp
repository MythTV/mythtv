#include <qapplication.h>
#include <qdatetime.h>

#include <unistd.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

#include "filetransfer.h"

#include "RingBuffer.h"
#include "libmyth/util.h"

FileTransfer::FileTransfer(QString &filename, QSocket *remote,
                           bool usereadahead, int retries) :
    readthreadlive(true),
    rbuffer(new RingBuffer(filename, false, usereadahead, retries)),
    sock(remote), ateof(false)
{
}

FileTransfer::FileTransfer(QString &filename, QSocket *remote)
{
    rbuffer = new RingBuffer(filename, false);
    sock = remote;
    readthreadlive = true;
    ateof = false;
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (rbuffer)
        delete rbuffer;

    readthreadLock.unlock();
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
        readthreadLock.lock();
    }
}

void FileTransfer::Pause(void)
{
    rbuffer->StopReads();
    readthreadLock.lock();
}

void FileTransfer::Unpause(void)
{
    rbuffer->StartReads();
    readthreadLock.unlock();
}

int FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive || !rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    readthreadLock.lock();
    requestBuffer.resize(max((size_t)max(size,0) + 128, requestBuffer.size()));
    char *buf = &requestBuffer[0];
    while (tot < size && !rbuffer->GetStopReads() && readthreadlive)
    {
        int request = size - tot;

        ret = rbuffer->Read(buf, request);
        
        if (rbuffer->GetStopReads() || ret <= 0)
            break;

        if (!WriteBlock(sock->socketDevice(), buf, (uint)ret))
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < request)
            break; // we hit eof
    }
    readthreadLock.unlock();

    return (ret < 0) ? -1 : tot;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (!rbuffer)
        return -1;
    if (!readthreadlive)
        return -1;

    ateof = false;

    Pause();

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

long long FileTransfer::GetFileSize(void)
{
    QString filename = rbuffer->GetFilename();

    struct stat st;
    long long size = 0;

    if (stat(filename.ascii(), &st) == 0)
        size = st.st_size;

    return size;
}

void FileTransfer::SetTimeout(bool fast)
{
    rbuffer->SetTimeout(fast);
}

