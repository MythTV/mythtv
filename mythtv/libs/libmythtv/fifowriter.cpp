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
#include "mythverbose.h"

#include "mythconfig.h"
#if CONFIG_DARWIN
    #include <sys/aio.h>    // O_SYNC
#endif

#include <iostream>
using namespace std;

FIFOWriter::FIFOWriter(int count, bool sync)
{
    num_fifos = count;
    usesync = sync;
    maxblksize = new long[count];
    killwr = new int[count];
    fbcount = new int[count];
    fifo_buf = new struct fifo_buf *[count];
    fb_inptr = new struct fifo_buf *[count];
    fb_outptr = new struct fifo_buf *[count];
    fifothrds = new FIFOThread[count];
    fifo_lock = new QMutex[count];
    empty_cond = new QWaitCondition[count];
    full_cond = new QWaitCondition[count];
    filename = new QString [count];
    fbdesc = new QString [count];
}

FIFOWriter::~FIFOWriter()
{
    for (int i = 0; i <num_fifos; i++)
    {
        killwr[i] = 1;

        fifo_lock[i].lock();
        empty_cond[i].wakeAll();
        fifo_lock[i].unlock();

        fifothrds[i].wait();
    }
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

    QByteArray  fname = name.toAscii();
    const char *aname = fname.constData();
    if (mkfifo(aname, S_IREAD | S_IWRITE | S_IRGRP | S_IROTH) == -1)
    {
        VERBOSE(VB_IMPORTANT, QString("Couldn't create fifo for file: '%1'")
                .arg(name) + ENO);
        return false;
    }
    VERBOSE(VB_GENERAL, QString("Created %1 fifo: %2").arg(desc).arg(name));
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

    usleep(50);

    return (fifothrds[id].isRunning());
}

void FIFOThread::run(void)
{
    if (!m_parent || m_id == -1)
        return;

    m_parent->FIFOWriteThread(m_id);
}

void FIFOWriter::FIFOWriteThread(int id)
{
    int fd = -1;

    fifo_lock[id].lock();
    while (1)
    {
        if (fb_inptr[id] == fb_outptr[id])
            empty_cond[id].wait(&fifo_lock[id]);
        fifo_lock[id].unlock();
        if (killwr[id])
            break;
        if (fd < 0)
        {
            QByteArray fname = filename[id].toAscii();
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
                    VERBOSE(VB_IMPORTANT, QString("FIFOW: write failed with %1")
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
        fifo_lock[id].lock();
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
    fifo_lock[id].lock();
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
            VERBOSE(VB_FILE, msg);
        }
        else
        {
            full_cond[id].wait(&fifo_lock[id], 1000);
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
    fifo_lock[id].unlock();
}

void FIFOWriter::FIFODrain(void)
{
    int count = 0;
    while (count < num_fifos)
    {
        count = 0;
        for (int i = 0; i < num_fifos; i++)
        {
            if (fb_inptr[i] == fb_outptr[i])
            {
                killwr[i] = 1;
                fifo_lock[i].lock();
                empty_cond[i].wakeAll();
                fifo_lock[i].unlock();
                count++;
            }
        }
        usleep(1000);
    }
}
