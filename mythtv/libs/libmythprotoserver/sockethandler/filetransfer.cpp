#include <QFileInfo>
#include <QMutexLocker>

#include "filetransfer.h"
#include "ringbuffer.h"
#include "programinfo.h"
#include "mythsocket.h"
#include "mythlogging.h"

FileTransfer::FileTransfer(QString &filename, MythSocket *remote,
                           MythSocketManager *parent,
                           bool usereadahead, int timeout_ms) :
    SocketHandler(remote, parent, ""),
    readthreadlive(true), readsLocked(false),
    rbuffer(RingBuffer::Create(filename, false, usereadahead, timeout_ms)),
    ateof(false), lock(QMutex::NonRecursive),
    writemode(false)
{
    pginfo = new ProgramInfo(filename);
    pginfo->MarkAsInUse(true, kFileTransferInUseID);
}

FileTransfer::FileTransfer(QString &filename, MythSocket *remote,
                           MythSocketManager *parent, bool write) :
    SocketHandler(remote, parent, ""),
    readthreadlive(true), readsLocked(false),
    rbuffer(RingBuffer::Create(filename, write)),
    ateof(false), lock(QMutex::NonRecursive),
    writemode(write)
{
    pginfo = new ProgramInfo(filename);
    pginfo->MarkAsInUse(true, kFileTransferInUseID);

    if (write)
    {
        remote->SetReadyReadCallbackEnabled(false);
        rbuffer->WriterSetBlocking(true);
    }
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (rbuffer)
    {
        delete rbuffer;
        rbuffer = NULL;
    }

    if (pginfo)
    {
        pginfo->MarkAsInUse(false, kFileTransferInUseID);
        delete pginfo;
    }
}

bool FileTransfer::isOpen(void)
{
    if (rbuffer && rbuffer->IsOpen())
        return true;
    return false;
}

bool FileTransfer::ReOpen(QString newFilename)
{
    if (!writemode)
        return false;

    if (rbuffer)
        return rbuffer->ReOpen(newFilename);

    return false;
}

void FileTransfer::Stop(void)
{
    if (readthreadlive)
    {
        readthreadlive = false;
        LOG(VB_FILE, LOG_INFO, "calling StopReads()");
        rbuffer->StopReads();
        QMutexLocker locker(&lock);
        readsLocked = true;
    }

    if (writemode)
        rbuffer->WriterFlush();

    if (pginfo)
        pginfo->UpdateInUseMark();
}

void FileTransfer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StopReads()");
    rbuffer->StopReads();
    QMutexLocker locker(&lock);
    readsLocked = true;

    if (pginfo)
        pginfo->UpdateInUseMark();
}

void FileTransfer::Unpause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StartReads()");
    rbuffer->StartReads();
    {
        QMutexLocker locker(&lock);
        readsLocked = false;
    }
    readsUnlockedCond.wakeAll();

    if (pginfo)
        pginfo->UpdateInUseMark();
}

int FileTransfer::RequestBlock(int size)
{
    if (!readthreadlive || !rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&lock);
    while (readsLocked)
        readsUnlockedCond.wait(&lock, 100 /*ms*/);

    requestBuffer.resize(max((size_t)max(size,0) + 128, requestBuffer.size()));
    char *buf = &requestBuffer[0];
    while (tot < size && !rbuffer->GetStopReads() && readthreadlive)
    {
        int request = size - tot;

        ret = rbuffer->Read(buf, request);

        if (rbuffer->GetStopReads() || ret <= 0)
            break;

        if (GetSocket()->Write(buf, (uint)ret) != ret)
        {
            tot = -1;
            break;
        }

        tot += ret;
        if (ret < request)
            break; // we hit eof
    }

    if (pginfo)
        pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

int FileTransfer::WriteBlock(int size)
{
    if (!writemode || !rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&lock);

    requestBuffer.resize(max((size_t)max(size,0) + 128, requestBuffer.size()));
    char *buf = &requestBuffer[0];
    int attempts = 0;

    while (tot < size)
    {
        int request = size - tot;
        int received;

        received = GetSocket()->Read(buf, (uint)request, 200 /*ms */);

        if (received != request)
        {
            LOG(VB_FILE, LOG_DEBUG,
                QString("WriteBlock(): Read failed. Requested %1 got %2")
                .arg(request).arg(received));
            if (received < 0)
            {
                // An error occurred, abort immediately
                break;
            }
            if (received == 0)
            {
                attempts++;
                if (attempts > 3)
                {
                    LOG(VB_FILE, LOG_ERR,
                        "WriteBlock(): Read tried too many times, aborting "
                        "(client or network too slow?)");
                    break;
                }
                continue;
            }
        }
        attempts = 0;
        ret = rbuffer->Write(buf, received);
        if (ret <= 0)
        {
            LOG(VB_FILE, LOG_DEBUG,
                QString("WriteBlock(): Write failed. Requested %1 got %2")
                .arg(received).arg(ret));
            break;
        }

        tot += received;
    }

    if (pginfo)
        pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

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

    if (pginfo)
        pginfo->UpdateInUseMark();

    return ret;
}

uint64_t FileTransfer::GetFileSize(void)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

    return rbuffer->GetRealFileSize();
}

QString FileTransfer::GetFileName(void)
{
    if (!rbuffer)
        return QString();

    return rbuffer->GetFilename();
}

void FileTransfer::SetTimeout(bool fast)
{
    if (pginfo)
        pginfo->UpdateInUseMark();

    rbuffer->SetOldFile(fast);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
