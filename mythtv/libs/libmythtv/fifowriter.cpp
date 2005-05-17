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
#include "mythcontext.h"

#include "config.h"
#ifdef CONFIG_DARWIN
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
    fifothrds = new pthread_t[count];
    fifo_lock = new pthread_mutex_t [count];
    empty_cond = new pthread_cond_t[count];
    full_cond = new pthread_cond_t[count];
    for (int i = 0; i < count; i++)
    {
      pthread_cond_init(&empty_cond[i], NULL);
      pthread_cond_init(&full_cond[i], NULL);
    }
    filename = new QString [count];
    fbdesc = new QString [count];
}

FIFOWriter::~FIFOWriter()
{
    for (int i = 0; i <num_fifos; i++)
    {
        killwr[i] = 1;
        pthread_cond_signal(&empty_cond[i]);
        pthread_join(fifothrds[i], NULL);
    }
    delete [] maxblksize;
    delete [] fifo_buf;
    delete [] fb_inptr;
    delete [] fb_outptr;
    delete [] fifothrds;
    delete [] full_cond;
    delete [] empty_cond;
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
    if (mkfifo(name.ascii(),S_IREAD | S_IWRITE | S_IRGRP | S_IROTH) == -1)
    {
        cerr << "Couldn't create fifo for file: " << name.ascii() << endl;
        perror(NULL);
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
    pthread_mutex_init(&fifo_lock[id], NULL);

    cur_id = id;

    pthread_create(&fifothrds[id], NULL, FIFOStartThread, this);
    while (cur_id >= 0)
        usleep(50);
    if (cur_id == -1)
        return true;
    else
        return false;
}

void *FIFOWriter::FIFOStartThread(void *param)
{
    FIFOWriter *fifo = (FIFOWriter *)param;
    fifo->FIFOWriteThread();

    return NULL;
}

void FIFOWriter::FIFOWriteThread(void)
{
    int id = cur_id;
    int fd = -1;
    pthread_mutex_lock(&fifo_lock[id]);
    cur_id = -1;
    while (1) 
    {
        if (fb_inptr[id] == fb_outptr[id])
            pthread_cond_wait(&empty_cond[id],&fifo_lock[id]);
        pthread_mutex_unlock(&fifo_lock[id]);
        if (killwr[id])
            break;
        if (fd == -1)
            fd = open(filename[id].ascii(), O_WRONLY| O_SYNC);
        write(fd, fb_outptr[id]->data, fb_outptr[id]->blksize);
        pthread_mutex_lock(&fifo_lock[id]);
        fb_outptr[id] = fb_outptr[id]->next;
        pthread_cond_signal(&full_cond[id]);
    }

    if (fd != -1)
        close(fd);

    unlink(filename[id]);

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
    pthread_mutex_lock(&fifo_lock[id]);
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
            struct timespec timeout;
            struct timeval now;
            gettimeofday(&now, NULL);
            timeout.tv_sec = now.tv_sec + 1;
            timeout.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&full_cond[id], &fifo_lock[id], &timeout);
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
    pthread_cond_signal(&empty_cond[id]);
    pthread_mutex_unlock(&fifo_lock[id]);
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
                pthread_cond_signal(&empty_cond[i]);
                count++;
            }
        }
        usleep(1000);
    }
}
