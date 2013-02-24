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
    fifo_buf(NULL),
    fb_inptr(NULL),
    fb_outptr(NULL),
    fifothrds(NULL),
    fifo_lock(NULL),
    full_cond(NULL),
    empty_cond(NULL),
    filename(NULL),
    fbdesc(NULL),
    maxblksize(NULL),
    killwr(NULL),
    fbcount(NULL),
    num_fifos(count),
    usesync(sync)
{
    if (count <= 0)
        return;

    fifo_buf = new struct fifo_buf *[count];
    fb_inptr = new struct fifo_buf *[count];
    fb_outptr = new struct fifo_buf *[count];
    fifothrds = new FIFOThread[count];
    fifo_lock = new QMutex[count];
    full_cond = new QWaitCondition[count];
    empty_cond = new QWaitCondition[count];
    filename = new QString [count];
    fbdesc = new QString [count];
    maxblksize = new long[count];
    killwr = new int[count];
    fbcount = new int[count];
}

FIFOWriter::~FIFOWriter()
{
    if (num_fifos <= 0)
        return;

    for (int i = 0; i < num_fifos; i++)
    {
        QMutexLocker flock(&fifo_lock[i]);
        killwr[i] = 1;
        empty_cond[i].wakeAll();
    }

    for (int i = 0; i < num_fifos; i++)
    {
        fifothrds[i].wait();
    }

    num_fifos = 0;

    delete [] maxblksize;
    delete [] fifo_buf;
    delete [] fb_inptr;
    delete [] fb_outptr;
    delete [] fifothrds;
    delete [] full_cond;
    delete [] empty_cond;
    delete [] fifo_lock;
    delete [] filename;
    delete [] fbdesc;
    delete [] killwr;
    delete [] fbcount;
}

int FIFOWriter::FIFOInit(int id, QString desc, QString name, long size,
                         int num_bufs)
{
    if (id < 0 || id >= num_fifos)
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
    maxblksize[id] = size;
    filename[id] = name;
    fbdesc[id] = desc;
    killwr[id] = 0;
    fbcount[id] = (usesync) ? 2 : num_bufs;
    fifo_buf[id] = new struct fifo_buf;
    struct fifo_buf *fifoptr = fifo_buf[id];
    for (int i = 0; i < fbcount[id]; i++)
    {
        fifoptr->data = new unsigned char[maxblksize[id]];
        if (i == fbcount[id] - 1)
            fifoptr->next = fifo_buf[id];
        else
            fifoptr->next = new struct fifo_buf;
        fifoptr = fifoptr->next;
    }
    fb_inptr[id]  = fifo_buf[id];
    fb_outptr[id] = fifo_buf[id];

    fifothrds[id].SetParent(this);
    fifothrds[id].SetId(id);
    fifothrds[id].start();

    while (0 == killwr[id] && !fifothrds[id].isRunning())
        usleep(1000);

    return fifothrds[id].isRunning();
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

    QMutexLocker flock(&fifo_lock[id]);
    while (1)
    {
        if ((fb_inptr[id] == fb_outptr[id]) && (0 == killwr[id]))
            empty_cond[id].wait(flock.mutex());
        flock.unlock();
        if (killwr[id])
            break;
        if (fd < 0)
        {
            QByteArray fname = filename[id].toLatin1();
            fd = open(fname.constData(), O_WRONLY| O_SYNC);
        }
        if (fd >= 0)
        {
            int written = 0;
            while (written < fb_outptr[id]->blksize)
            {
                int ret = write(fd, fb_outptr[id]->data+written,
                                fb_outptr[id]->blksize-written);
                if (ret < 0)
                {
                    LOG(VB_GENERAL, LOG_ERR,
                        QString("FIFOW: write failed with %1")
                            .arg(strerror(errno)));
                    ///FIXME: proper error propagation
                    break;
                }
                else
                {
                    written += ret;
                }
            }
        }
        flock.relock();
        fb_outptr[id] = fb_outptr[id]->next;
        full_cond[id].wakeAll();
    }

    if (fd != -1)
        close(fd);

    unlink(filename[id].toLocal8Bit().constData());

    while (fifo_buf[id]->next != fifo_buf[id])
    {
        struct fifo_buf *tmpfifo = fifo_buf[id]->next->next;
        delete [] fifo_buf[id]->next->data;
        delete fifo_buf[id]->next;
        fifo_buf[id]->next = tmpfifo;
    }
    delete [] fifo_buf[id]->data;
    delete fifo_buf[id];
}

void FIFOWriter::FIFOWrite(int id, void *buffer, long blksize)
{
    QMutexLocker flock(&fifo_lock[id]);
    while (fb_inptr[id]->next == fb_outptr[id])
    {
        bool blocking = false;
        if (!usesync)
        {
            for(int i = 0; i < num_fifos; i++)
            {
                if (i == id)
                    continue;
                if (fb_inptr[i] == fb_outptr[i])
                    blocking = true;
            }
        }

        if (blocking)
        {
            struct fifo_buf *tmpfifo;
            tmpfifo = fb_inptr[id]->next;
            fb_inptr[id]->next = new struct fifo_buf;
            fb_inptr[id]->next->data = new unsigned char[maxblksize[id]];
            fb_inptr[id]->next->next = tmpfifo;
            QString msg = QString("allocating additonal buffer for : %1(%2)")
                          .arg(fbdesc[id]).arg(++fbcount[id]);
            LOG(VB_FILE, LOG_INFO, msg);
        }
        else
        {
            full_cond[id].wait(flock.mutex(), 1000);
        }
    }
    if (blksize > maxblksize[id])
    {
        delete [] fb_inptr[id]->data;
        fb_inptr[id]->data = new unsigned char[blksize];
    }
    memcpy(fb_inptr[id]->data,buffer,blksize);
    fb_inptr[id]->blksize = blksize;
    fb_inptr[id] = fb_inptr[id]->next;
    empty_cond[id].wakeAll();
}

void FIFOWriter::FIFODrain(void)
{
    int count = 0;
    while (count < num_fifos)
    {
        count = 0;
        for (int i = 0; i < num_fifos; i++)
        {
            QMutexLocker flock(&fifo_lock[i]);
            if (fb_inptr[i] == fb_outptr[i])
            {
                killwr[i] = 1;
                empty_cond[i].wakeAll();
                count++;
            }
        }
        usleep(1000);
    }
}
