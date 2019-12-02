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

#include "fifowriter.h"
#include "compat.h"
#include "mythlogging.h"

#include "mythconfig.h"
#if CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

#include <iostream>
using namespace std;

FIFOWriter::FIFOWriter(int count, bool sync) :
    m_num_fifos(count),
    m_usesync(sync)
{
    if (count <= 0)
        return;

    m_fifo_buf   = new fifo_buf *[count];
    m_fb_inptr   = new fifo_buf *[count];
    m_fb_outptr  = new fifo_buf *[count];
    m_fifothrds  = new FIFOThread[count];
    m_fifo_lock  = new QMutex[count];
    m_full_cond  = new QWaitCondition[count];
    m_empty_cond = new QWaitCondition[count];
    m_filename   = new QString [count];
    m_fbdesc     = new QString [count];
    m_maxblksize = new long[count];
    m_killwr     = new int[count];
    m_fbcount    = new int[count];
    m_fbmaxcount = new int[count];
}

FIFOWriter::~FIFOWriter()
{
    if (m_num_fifos <= 0)
        return;

    for (int i = 0; i < m_num_fifos; i++)
    {
        QMutexLocker flock(&m_fifo_lock[i]);
        m_killwr[i] = 1;
        m_empty_cond[i].wakeAll();
    }

    for (int i = 0; i < m_num_fifos; i++)
    {
        m_fifothrds[i].wait();
    }

    m_num_fifos = 0;

    delete [] m_maxblksize;
    delete [] m_fifo_buf;
    delete [] m_fb_inptr;
    delete [] m_fb_outptr;
    delete [] m_fifothrds;
    delete [] m_full_cond;
    delete [] m_empty_cond;
    delete [] m_fifo_lock;
    delete [] m_filename;
    delete [] m_fbdesc;
    delete [] m_killwr;
    delete [] m_fbcount;
    delete [] m_fbmaxcount;
}

bool FIFOWriter::FIFOInit(int id, const QString& desc, const QString& name, long size,
                         int num_bufs)
{
    if (id < 0 || id >= m_num_fifos)
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
    m_maxblksize[id] = size;
    m_filename[id]   = name;
    m_fbdesc[id]     = desc;
    m_killwr[id]     = 0;
    m_fbcount[id]    = (m_usesync) ? 2 : num_bufs;
    m_fbmaxcount[id] = 512;
    m_fifo_buf[id]   = new fifo_buf;
    struct fifo_buf *fifoptr = m_fifo_buf[id];
    for (int i = 0; i < m_fbcount[id]; i++)
    {
        fifoptr->data = new unsigned char[m_maxblksize[id]];
        if (i == m_fbcount[id] - 1)
            fifoptr->next = m_fifo_buf[id];
        else
            fifoptr->next = new struct fifo_buf;
        fifoptr = fifoptr->next;
    }
    m_fb_inptr[id]  = m_fifo_buf[id];
    m_fb_outptr[id] = m_fifo_buf[id];

    m_fifothrds[id].SetParent(this);
    m_fifothrds[id].SetId(id);
    m_fifothrds[id].start();

    while (0 == m_killwr[id] && !m_fifothrds[id].isRunning())
        usleep(1000);

    return m_fifothrds[id].isRunning();
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

    QMutexLocker flock(&m_fifo_lock[id]);
    while (true)
    {
        if ((m_fb_inptr[id] == m_fb_outptr[id]) && (0 == m_killwr[id]))
            m_empty_cond[id].wait(flock.mutex());
        flock.unlock();
        if (m_killwr[id])
            break;
        if (fd < 0)
        {
            QByteArray fname = m_filename[id].toLatin1();
            fd = open(fname.constData(), O_WRONLY| O_SYNC);
        }
        if (fd >= 0)
        {
            int written = 0;
            while (written < m_fb_outptr[id]->blksize)
            {
                int ret = write(fd, m_fb_outptr[id]->data+written,
                                m_fb_outptr[id]->blksize-written);
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
        m_fb_outptr[id] = m_fb_outptr[id]->next;
        m_full_cond[id].wakeAll();
    }

    if (fd != -1)
        close(fd);

    unlink(m_filename[id].toLocal8Bit().constData());

    while (m_fifo_buf[id]->next != m_fifo_buf[id])
    {
        struct fifo_buf *tmpfifo = m_fifo_buf[id]->next->next;
        delete [] m_fifo_buf[id]->next->data;
        delete m_fifo_buf[id]->next;
        m_fifo_buf[id]->next = tmpfifo;
    }
    delete [] m_fifo_buf[id]->data;
    delete m_fifo_buf[id];
}

void FIFOWriter::FIFOWrite(int id, void *buffer, long blksize)
{
    QMutexLocker flock(&m_fifo_lock[id]);
    while (m_fb_inptr[id]->next == m_fb_outptr[id])
    {
        bool blocking = false;
        if (!m_usesync)
        {
            for(int i = 0; i < m_num_fifos; i++)
            {
                if (i == id)
                    continue;
                if (m_fb_inptr[i] == m_fb_outptr[i])
                    blocking = true;
            }
        }

        if (blocking && m_fbcount[id] < m_fbmaxcount[id])
        {
            struct fifo_buf *tmpfifo;
            tmpfifo = m_fb_inptr[id]->next;
            m_fb_inptr[id]->next = new struct fifo_buf;
            m_fb_inptr[id]->next->data = new unsigned char[m_maxblksize[id]];
            m_fb_inptr[id]->next->next = tmpfifo;
            QString msg = QString("allocating additonal buffer for : %1(%2)")
                          .arg(m_fbdesc[id]).arg(++m_fbcount[id]);
            LOG(VB_FILE, LOG_INFO, msg);
        }
        else
        {
            m_full_cond[id].wait(flock.mutex(), 1000);
        }
    }
    if (blksize > m_maxblksize[id])
    {
        delete [] m_fb_inptr[id]->data;
        m_fb_inptr[id]->data = new unsigned char[blksize];
    }
    memcpy(m_fb_inptr[id]->data,buffer,blksize);
    m_fb_inptr[id]->blksize = blksize;
    m_fb_inptr[id] = m_fb_inptr[id]->next;
    m_empty_cond[id].wakeAll();
}

void FIFOWriter::FIFODrain(void)
{
    int count = 0;
    while (count < m_num_fifos)
    {
        count = 0;
        for (int i = 0; i < m_num_fifos; i++)
        {
            QMutexLocker flock(&m_fifo_lock[i]);
            if (m_fb_inptr[i] == m_fb_outptr[i])
            {
                m_killwr[i] = 1;
                m_empty_cond[i].wakeAll();
                count++;
            }
        }
        usleep(1000);
    }
}
