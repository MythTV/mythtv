// MythTV
#include "compat.h"
#include "mythlogging.h"
#include "mythconfig.h"
#if CONFIG_DARWIN
#include <sys/aio.h>
#endif
#include "io/fifowriter.h"

// Std
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include <cmath>
#include <iostream>
using namespace std;

FIFOWriter::FIFOWriter(int count, bool sync) :
    m_numFifos(count),
    m_useSync(sync)
{
    if (count <= 0)
        return;

    m_fifoBuf    = new fifo_buf *[count];
    m_fbInptr    = new fifo_buf *[count];
    m_fbOutptr   = new fifo_buf *[count];
    m_fifoThrds  = new FIFOThread[count];
    m_fifoLock   = new QMutex[count];
    m_fullCond   = new QWaitCondition[count];
    m_emptyCond  = new QWaitCondition[count];
    m_filename   = new QString [count];
    m_fbDesc     = new QString [count];
    m_maxBlkSize = new long[count];
    m_killWr     = new int[count];
    m_fbCount    = new int[count];
    m_fbMaxCount = new int[count];
}

FIFOWriter::~FIFOWriter()
{
    if (m_numFifos <= 0)
        return;

    for (int i = 0; i < m_numFifos; i++)
    {
        QMutexLocker flock(&m_fifoLock[i]);
        m_killWr[i] = 1;
        m_emptyCond[i].wakeAll();
    }

    for (int i = 0; i < m_numFifos; i++)
    {
        m_fifoThrds[i].wait();
    }

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

bool FIFOWriter::FIFOInit(int id, const QString& desc, const QString& name, long size,
                         int num_bufs)
{
    if (id < 0 || id >= m_numFifos)
        return false;

    QByteArray  fname = name.toLatin1();
    const char *aname = fname.constData();
    if (mkfifo(aname, S_IREAD | S_IWRITE | S_IRGRP | S_IROTH) == -1)
    {
        LOG(VB_GENERAL, LOG_ERR, QString("Couldn't create fifo for file: '%1'")
                .arg(name) + ENO);
        return false;
    }
    LOG(VB_GENERAL, LOG_INFO, QString("Created %1 fifo: %2")
            .arg(desc).arg(name));
    m_maxBlkSize[id] = size;
    m_filename[id]   = name;
    m_fbDesc[id]     = desc;
    m_killWr[id]     = 0;
    m_fbCount[id]    = (m_useSync) ? 2 : num_bufs;
    m_fbMaxCount[id] = 512;
    m_fifoBuf[id]   = new fifo_buf;
    struct fifo_buf *fifoptr = m_fifoBuf[id];
    for (int i = 0; i < m_fbCount[id]; i++)
    {
        fifoptr->data = new unsigned char[m_maxBlkSize[id]];
        if (i == m_fbCount[id] - 1)
            fifoptr->next = m_fifoBuf[id];
        else
            fifoptr->next = new struct fifo_buf;
        fifoptr = fifoptr->next;
    }
    m_fbInptr[id]  = m_fifoBuf[id];
    m_fbOutptr[id] = m_fifoBuf[id];

    m_fifoThrds[id].SetParent(this);
    m_fifoThrds[id].SetId(id);
    m_fifoThrds[id].start();

    while (0 == m_killWr[id] && !m_fifoThrds[id].isRunning())
        usleep(1000);

    return m_fifoThrds[id].isRunning();
}

void FIFOThread::run(void)
{
    RunProlog();
    if (m_parent && m_id != -1)
        m_parent->FIFOWriteThread(m_id);
    RunEpilog();
}

void FIFOWriter::FIFOWriteThread(int id)
{
    int fd = -1;

    QMutexLocker flock(&m_fifoLock[id]);
    while (true)
    {
        if ((m_fbInptr[id] == m_fbOutptr[id]) && (0 == m_killWr[id]))
            m_emptyCond[id].wait(flock.mutex());
        flock.unlock();
        if (m_killWr[id])
            break;
        if (fd < 0)
        {
            QByteArray fname = m_filename[id].toLatin1();
            fd = open(fname.constData(), O_WRONLY| O_SYNC);
        }
        if (fd >= 0)
        {
            int written = 0;
            while (written < m_fbOutptr[id]->blksize)
            {
                int ret = write(fd, m_fbOutptr[id]->data+written,
                                m_fbOutptr[id]->blksize-written);
                if (ret < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("FIFOW: write failed with %1")
                            .arg(strerror(errno)));
                    ///FIXME: proper error propagation
                    break;
                }
                written += ret;
            }
        }
        flock.relock();
        m_fbOutptr[id] = m_fbOutptr[id]->next;
        m_fullCond[id].wakeAll();
    }

    if (fd != -1)
        close(fd);

    unlink(m_filename[id].toLocal8Bit().constData());

    while (m_fifoBuf[id]->next != m_fifoBuf[id])
    {
        struct fifo_buf *tmpfifo = m_fifoBuf[id]->next->next;
        delete [] m_fifoBuf[id]->next->data;
        delete m_fifoBuf[id]->next;
        m_fifoBuf[id]->next = tmpfifo;
    }
    delete [] m_fifoBuf[id]->data;
    delete m_fifoBuf[id];
}

void FIFOWriter::FIFOWrite(int id, void *buffer, long blksize)
{
    QMutexLocker flock(&m_fifoLock[id]);
    while (m_fbInptr[id]->next == m_fbOutptr[id])
    {
        bool blocking = false;
        if (!m_useSync)
        {
            for(int i = 0; i < m_numFifos; i++)
            {
                if (i == id)
                    continue;
                if (m_fbInptr[i] == m_fbOutptr[i])
                    blocking = true;
            }
        }

        if (blocking && m_fbCount[id] < m_fbMaxCount[id])
        {
            struct fifo_buf *tmpfifo = m_fbInptr[id]->next;
            m_fbInptr[id]->next = new struct fifo_buf;
            m_fbInptr[id]->next->data = new unsigned char[m_maxBlkSize[id]];
            m_fbInptr[id]->next->next = tmpfifo;
            QString msg = QString("allocating additonal buffer for : %1(%2)")
                          .arg(m_fbDesc[id]).arg(++m_fbCount[id]);
            LOG(VB_FILE, LOG_INFO, msg);
        }
        else
        {
            m_fullCond[id].wait(flock.mutex(), 1000);
        }
    }
    if (blksize > m_maxBlkSize[id])
    {
        delete [] m_fbInptr[id]->data;
        m_fbInptr[id]->data = new unsigned char[blksize];
    }
    memcpy(m_fbInptr[id]->data,buffer,blksize);
    m_fbInptr[id]->blksize = blksize;
    m_fbInptr[id] = m_fbInptr[id]->next;
    m_emptyCond[id].wakeAll();
}

void FIFOWriter::FIFODrain(void)
{
    int count = 0;
    while (count < m_numFifos)
    {
        count = 0;
        for (int i = 0; i < m_numFifos; i++)
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
