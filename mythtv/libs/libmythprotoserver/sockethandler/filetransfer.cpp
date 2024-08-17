#include <QFileInfo>
#include <QMutexLocker>
#include <utility>

#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/programinfo.h"
#include "libmythtv/io/mythmediabuffer.h"

#include "filetransfer.h"

FileTransfer::FileTransfer(QString &filename, MythSocket *remote,
                           MythSocketManager *parent,
                           bool usereadahead, std::chrono::milliseconds timeout) :
    SocketHandler(remote, parent, ""),
    m_pginfo(new ProgramInfo(filename)),
    m_rbuffer(MythMediaBuffer::Create(filename, false, usereadahead, timeout))
{
    m_pginfo->MarkAsInUse(true, kFileTransferInUseID);
}

FileTransfer::FileTransfer(QString &filename, MythSocket *remote,
                           MythSocketManager *parent, bool write) :
    SocketHandler(remote, parent, ""),
    m_pginfo(new ProgramInfo(filename)),
    m_rbuffer(MythMediaBuffer::Create(filename, write)),
    m_writemode(write)
{
    m_pginfo->MarkAsInUse(true, kFileTransferInUseID);

    if (write)
    {
        remote->SetReadyReadCallbackEnabled(false);
        m_rbuffer->WriterSetBlocking(true);
    }
}

FileTransfer::~FileTransfer()
{
    Stop();

    if (m_rbuffer)
    {
        delete m_rbuffer;
        m_rbuffer = nullptr;
    }

    if (m_pginfo)
    {
        m_pginfo->MarkAsInUse(false, kFileTransferInUseID);
        delete m_pginfo;
    }
}

bool FileTransfer::isOpen(void)
{
    return m_rbuffer && m_rbuffer->IsOpen();
}

bool FileTransfer::ReOpen(const QString& newFilename)
{
    if (!m_writemode)
        return false;

    if (m_rbuffer)
        return m_rbuffer->ReOpen(newFilename);

    return false;
}

void FileTransfer::Stop(void)
{
    if (m_readthreadlive)
    {
        m_readthreadlive = false;
        LOG(VB_FILE, LOG_INFO, "calling StopReads()");
        m_rbuffer->StopReads();
        QMutexLocker locker(&m_lock);
        m_readsLocked = true;
    }

    if (m_writemode)
        m_rbuffer->WriterFlush();

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

void FileTransfer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StopReads()");
    m_rbuffer->StopReads();
    QMutexLocker locker(&m_lock);
    m_readsLocked = true;

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

void FileTransfer::Unpause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StartReads()");
    m_rbuffer->StartReads();
    {
        QMutexLocker locker(&m_lock);
        m_readsLocked = false;
    }
    m_readsUnlockedCond.wakeAll();

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

int FileTransfer::RequestBlock(int size)
{
    if (!m_readthreadlive || !m_rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&m_lock);
    while (m_readsLocked)
        m_readsUnlockedCond.wait(&m_lock, 100 /*ms*/);

    m_requestBuffer.resize(std::max((size_t)std::max(size,0) + 128, m_requestBuffer.size()));
    char *buf = (m_requestBuffer).data();
    while (tot < size && !m_rbuffer->GetStopReads() && m_readthreadlive)
    {
        int request = size - tot;

        ret = m_rbuffer->Read(buf, request);

        if (m_rbuffer->GetStopReads() || ret <= 0)
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

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

int FileTransfer::WriteBlock(int size)
{
    if (!m_writemode || !m_rbuffer)
        return -1;

    int tot = 0;
    int ret = 0;

    QMutexLocker locker(&m_lock);

    m_requestBuffer.resize(std::max((size_t)std::max(size,0) + 128, m_requestBuffer.size()));
    char *buf = (m_requestBuffer).data();
    int attempts = 0;

    while (tot < size)
    {
        int request = size - tot;
        int received = GetSocket()->Read(buf, (uint)request, 200ms);

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
        ret = m_rbuffer->Write(buf, received);
        if (ret <= 0)
        {
            LOG(VB_FILE, LOG_DEBUG,
                QString("WriteBlock(): Write failed. Requested %1 got %2")
                .arg(received).arg(ret));
            break;
        }

        tot += received;
    }

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    return (ret < 0) ? -1 : tot;
}

long long FileTransfer::Seek(long long curpos, long long pos, int whence)
{
    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    if (!m_rbuffer)
        return -1;
    if (!m_readthreadlive)
        return -1;

    m_ateof = false;

    Pause();

    if (whence == SEEK_CUR)
    {
        long long desired = curpos + pos;
        long long realpos = m_rbuffer->GetReadPosition();

        pos = desired - realpos;
    }

    long long ret = m_rbuffer->Seek(pos, whence);

    Unpause();

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    return ret;
}

uint64_t FileTransfer::GetFileSize(void)
{
    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    return m_rbuffer->GetRealFileSize();
}

QString FileTransfer::GetFileName(void)
{
    if (!m_rbuffer)
        return {};

    return m_rbuffer->GetFilename();
}

void FileTransfer::SetTimeout(bool fast)
{
    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    m_rbuffer->SetOldFile(fast);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
