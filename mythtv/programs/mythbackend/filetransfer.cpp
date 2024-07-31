// C++ headers
#include <utility>

// Qt headers
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>

// MythTV
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythsocket.h"
#include "libmythbase/programinfo.h"
#include "libmythtv/io/mythmediabuffer.h"

// MythBackend
#include "filetransfer.h"

BEFileTransfer::BEFileTransfer(QString &filename, MythSocket *remote,
                           bool usereadahead, std::chrono::milliseconds timeout) :
    ReferenceCounter(QString("BEFileTransfer:%1").arg(filename)),
    m_pginfo(new ProgramInfo(filename)),
    m_rbuffer(MythMediaBuffer::Create(filename, false, usereadahead, timeout, true)),
    m_sock(remote)
{
    m_pginfo->MarkAsInUse(true, kFileTransferInUseID);
    if (m_rbuffer && m_rbuffer->IsOpen())
        m_rbuffer->Start();
}

BEFileTransfer::BEFileTransfer(QString &filename, MythSocket *remote, bool write) :
    ReferenceCounter(QString("BEFileTransfer:%1").arg(filename)),
    m_pginfo(new ProgramInfo(filename)),
    m_rbuffer(MythMediaBuffer::Create(filename, write)),
    m_sock(remote), m_writemode(write)
{
    m_pginfo->MarkAsInUse(true, kFileTransferInUseID);

    if (write)
        remote->SetReadyReadCallbackEnabled(false);
    if (m_rbuffer)
        m_rbuffer->Start();
}

BEFileTransfer::~BEFileTransfer()
{
    Stop();

    if (m_sock) // BEFileTransfer becomes responsible for deleting the socket
        m_sock->DecrRef();

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

bool BEFileTransfer::isOpen(void)
{
    return m_rbuffer && m_rbuffer->IsOpen();
}

bool BEFileTransfer::ReOpen(const QString& newFilename)
{
    if (!m_writemode)
        return false;

    if (m_rbuffer)
        return m_rbuffer->ReOpen(newFilename);

    return false;
}

void BEFileTransfer::Stop(void)
{
    if (m_readthreadlive)
    {
        m_readthreadlive = false;
        LOG(VB_FILE, LOG_INFO, "calling StopReads()");
        if (m_rbuffer)
            m_rbuffer->StopReads();
        QMutexLocker locker(&m_lock);
        m_readsLocked = true;
    }

    if (m_writemode)
        if (m_rbuffer)
            m_rbuffer->WriterFlush();

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

void BEFileTransfer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StopReads()");
    if (m_rbuffer)
        m_rbuffer->StopReads();
    QMutexLocker locker(&m_lock);
    m_readsLocked = true;

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

void BEFileTransfer::Unpause(void)
{
    LOG(VB_FILE, LOG_INFO, "calling StartReads()");
    if (m_rbuffer)
        m_rbuffer->StartReads();
    {
        QMutexLocker locker(&m_lock);
        m_readsLocked = false;
    }
    m_readsUnlockedCond.wakeAll();

    if (m_pginfo)
        m_pginfo->UpdateInUseMark();
}

int BEFileTransfer::RequestBlock(int size)
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

        if (m_sock->Write(buf, (uint)ret) != ret)
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

int BEFileTransfer::WriteBlock(int size)
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
        int received = m_sock->Read(buf, (uint)request, 200ms);

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

long long BEFileTransfer::Seek(long long curpos, long long pos, int whence)
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

uint64_t BEFileTransfer::GetFileSize(void)
{
    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    return m_rbuffer ? m_rbuffer->GetRealFileSize() : 0;
}

QString BEFileTransfer::GetFileName(void)
{
    if (!m_rbuffer)
        return {};

    return m_rbuffer->GetFilename();
}

void BEFileTransfer::SetTimeout(bool fast)
{
    if (m_pginfo)
        m_pginfo->UpdateInUseMark();

    if (m_rbuffer)
        m_rbuffer->SetOldFile(fast);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
