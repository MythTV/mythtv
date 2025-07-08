#include <QtGlobal>

// MythTV
#include "libmythbase/compat.h"
#include "libmythbase/mythlogging.h"

#ifdef Q_OS_DARWIN
#include <sys/aio.h>
#endif
#include "io/mythfifowriter.h"

// Std
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>
#include <iostream>

MythFIFOThread::MythFIFOThread()
  : MThread("FIFOThread")
{
}

MythFIFOThread::~MythFIFOThread()
{
    wait();
    m_parent = nullptr;
    m_id = -1;
}

void MythFIFOThread::SetId(int Id)
{
    m_id = Id;
}

void MythFIFOThread::SetParent(MythFIFOWriter *Parent)
{
    m_parent = Parent;
}

void MythFIFOThread::run(void)
{
    RunProlog();
    if (m_parent && m_id != -1)
        m_parent->FIFOWriteThread(m_id);
    RunEpilog();
}

MythFIFOWriter::MythFIFOWriter(uint Count, bool Sync)
  : m_numFifos(Count),
    m_useSync(Sync)
{
    if (Count < 1)
        return;

    m_fifoBuf    = new MythFifoBuffer*[Count];
    m_fbInptr    = new MythFifoBuffer*[Count];
    m_fbOutptr   = new MythFifoBuffer*[Count];
    m_fifoThrds  = new MythFIFOThread[Count];
    m_fifoLock   = new QMutex[Count];
    m_fullCond   = new QWaitCondition[Count];
    m_emptyCond  = new QWaitCondition[Count];
    m_filename   = new QString [Count];
    m_fbDesc     = new QString [Count];
    m_maxBlkSize = new long[Count];
    m_killWr     = new int[Count];
    m_fbCount    = new int[Count];
    m_fbMaxCount = new int[Count];
}

MythFIFOWriter::~MythFIFOWriter()
{
    if (m_numFifos < 1)
        return;

    for (uint i = 0; i < m_numFifos; i++)
    {
        QMutexLocker flock(&m_fifoLock[i]);
        m_killWr[i] = 1;
        m_emptyCond[i].wakeAll();
    }

    for (uint i = 0; i < m_numFifos; i++)
        m_fifoThrds[i].wait();

    m_numFifos = 0;

    delete [] m_maxBlkSize;
    delete [] m_fifoBuf;
    delete [] m_fbInptr;
    delete [] m_fbOutptr;
    delete [] m_fifoThrds;
    delete [] m_fullCond;
    delete [] m_emptyCond;
    delete [] m_fifoLock;
    delete [] m_filename;
    delete [] m_fbDesc;
    delete [] m_killWr;
    delete [] m_fbCount;
    delete [] m_fbMaxCount;
}

bool MythFIFOWriter::FIFOInit(uint Id, const QString& Desc, const QString& Name,
                          long Size, int NumBufs)
{
    if (Id >= m_numFifos)
        return false;

    QByteArray  fname = Name.toLatin1();
    const char *aname = fname.constData();
    if (mkfifo(aname, S_IREAD | S_IWRITE | S_IRGRP | S_IROTH) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't create fifo for file: '%1'").arg(Name) + ENO);
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, QString("Created %1 fifo: %2").arg(Desc, Name));

    m_maxBlkSize[Id] = Size;
    m_filename[Id]   = Name;
    m_fbDesc[Id]     = Desc;
    m_killWr[Id]     = 0;
    m_fbCount[Id]    = (m_useSync) ? 2 : NumBufs;
    m_fbMaxCount[Id] = 512;
    m_fifoBuf[Id]    = new MythFifoBuffer;
    struct MythFifoBuffer *fifoptr = m_fifoBuf[Id];
    for (int i = 0; i < m_fbCount[Id]; i++)
    {
        fifoptr->m_data = new unsigned char[static_cast<unsigned long>(m_maxBlkSize[Id])];
        if (i == m_fbCount[Id] - 1)
        {
            fifoptr->m_next = m_fifoBuf[Id];
        }
        else
        {
            fifoptr->m_next = new struct MythFifoBuffer;
        }
        fifoptr = fifoptr->m_next;
    }
    m_fbInptr[Id]  = m_fifoBuf[Id];
    m_fbOutptr[Id] = m_fifoBuf[Id];

    m_fifoThrds[Id].SetParent(this);
    m_fifoThrds[Id].SetId(static_cast<int>(Id));
    m_fifoThrds[Id].start();

    while (0 == m_killWr[Id] && !m_fifoThrds[Id].isRunning())
        usleep(1000);

    return m_fifoThrds[Id].isRunning();
}

void MythFIFOWriter::FIFOWriteThread(int Id)
{
    int fd = -1;

    QMutexLocker flock(&m_fifoLock[Id]);
    while (true)
    {
        if ((m_fbInptr[Id] == m_fbOutptr[Id]) && (0 == m_killWr[Id]))
            m_emptyCond[Id].wait(flock.mutex());
        flock.unlock();
        if (m_killWr[Id])
            break;
        if (fd < 0)
        {
            QByteArray fname = m_filename[Id].toLatin1();
            fd = open(fname.constData(), O_WRONLY| O_SYNC);
        }
        if (fd >= 0)
        {
            int written = 0;
            while (written < m_fbOutptr[Id]->m_blockSize)
            {
                int ret = static_cast<int>(write(fd, m_fbOutptr[Id]->m_data + written,
                                                 static_cast<size_t>(m_fbOutptr[Id]->m_blockSize-written)));
                if (ret < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR, QString("FIFOW: write failed with %1")
                        .arg(strerror(errno)));
                    ///FIXME: proper error propagation
                    break;
                }
                written += ret;
            }
        }
        flock.relock();
        m_fbOutptr[Id] = m_fbOutptr[Id]->m_next;
        m_fullCond[Id].wakeAll();
    }

    if (fd != -1)
        close(fd);

    unlink(m_filename[Id].toLocal8Bit().constData());

    while (m_fifoBuf[Id]->m_next != m_fifoBuf[Id])
    {
        struct MythFifoBuffer *tmpfifo = m_fifoBuf[Id]->m_next->m_next;
        delete [] m_fifoBuf[Id]->m_next->m_data;
        delete m_fifoBuf[Id]->m_next;
        m_fifoBuf[Id]->m_next = tmpfifo;
    }
    delete [] m_fifoBuf[Id]->m_data;
    delete m_fifoBuf[Id];
}

void MythFIFOWriter::FIFOWrite(uint Id, void *Buffer, long Size)
{
    QMutexLocker flock(&m_fifoLock[Id]);
    while (m_fbInptr[Id]->m_next == m_fbOutptr[Id])
    {
        bool blocking = false;
        if (!m_useSync)
        {
            for (uint i = 0; i < m_numFifos; i++)
            {
                if (i == Id)
                    continue;
                if (m_fbInptr[i] == m_fbOutptr[i])
                    blocking = true;
            }
        }

        if (blocking && m_fbCount[Id] < m_fbMaxCount[Id])
        {
            struct MythFifoBuffer *tmpfifo = m_fbInptr[Id]->m_next;
            m_fbInptr[Id]->m_next = new struct MythFifoBuffer;
            m_fbInptr[Id]->m_next->m_data = new unsigned char[static_cast<unsigned long>(m_maxBlkSize[Id])];
            m_fbInptr[Id]->m_next->m_next = tmpfifo;
            QString msg = QString("allocating additonal buffer for : %1(%2)")
                          .arg(m_fbDesc[Id]).arg(++m_fbCount[Id]);
            LOG(VB_FILE, LOG_INFO, msg);
        }
        else
        {
            m_fullCond[Id].wait(flock.mutex(), 1000);
        }
    }

    if (Size > m_maxBlkSize[Id])
    {
        delete [] m_fbInptr[Id]->m_data;
        m_fbInptr[Id]->m_data = new unsigned char[static_cast<unsigned long>(Size)];
    }

    memcpy(m_fbInptr[Id]->m_data,Buffer, static_cast<size_t>(Size));
    m_fbInptr[Id]->m_blockSize = Size;
    m_fbInptr[Id] = m_fbInptr[Id]->m_next;
    m_emptyCond[Id].wakeAll();
}

void MythFIFOWriter::FIFODrain(void)
{
    uint count = 0;
    while (count < m_numFifos)
    {
        count = 0;
        for (uint i = 0; i < m_numFifos; i++)
        {
            QMutexLocker flock(&m_fifoLock[i]);
            if (m_fbInptr[i] == m_fbOutptr[i])
            {
                m_killWr[i] = 1;
                m_emptyCond[i].wakeAll();
                count++;
            }
        }
        usleep(1000);
    }
}
