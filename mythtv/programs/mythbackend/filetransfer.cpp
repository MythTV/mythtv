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

bool FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive)
        return true;

    if (size > 256000)
        size = 256000;

    char buffer[256001];

    readthreadLock.lock();

    while (size > 0 && readthreadlive && !rbuffer->GetStopReads() && !ateof)
    {
        int ret = rbuffer->Read(buffer, size);
        if (!rbuffer->GetStopReads())
        {
            if (ret)
                WriteBlock(sock, buffer, ret);
            else
            {
                ateof = true;
                size = 0;
            }
        }

        size -= ret;
    }

    readthreadLock.unlock();

    return ateof;
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
