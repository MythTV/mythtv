#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "RingBuffer.h"

RingBuffer::RingBuffer(const char *lfilename, bool actasnormalfile, bool write)
{
    normalfile = true;
    filename = lfilename;
    fd = -1;

    if (write)
    {
        fd = open(lfilename, O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
        writemode = true;
    }
    else
    {
        fd = open(lfilename, O_RDONLY|O_LARGEFILE);
        writemode = false;
    }
}

RingBuffer::RingBuffer(const char *lfilename, long long size)
{
    normalfile = false;
    filename = lfilename;

    fd = -1; fd2 = -1;

    fd = open(filename.c_str(), O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
    fd2 = open(filename.c_str(), O_RDONLY|O_LARGEFILE);

    writestart = writepos = 0;
    readstart = readpos = 0;
}

RingBuffer::~RingBuffer(void)
{
    if (fd > 0)
        close(fd);
    if (fd2 > 0)
        close(fd2);
}

int RingBuffer::Read(void *buf, int count)
{
    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            ret = read(fd, buf, count);
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
        while (readpos + count >= writepos)
            usleep(5);
        ret = read(fd2, buf, count);
        readpos += ret;
    }
    return ret;
}

int RingBuffer::Write(const void *buf, int count)
{
    int ret = -1;

    if (normalfile)
    {
        if (writemode)
        {
            ret = write(fd, buf, count);
        }
        else
        {
            fprintf(stderr, "Attempt to write to a read only file\n");
        }
    }
    else
    {
        ret = write(fd, buf, count);
        writepos += ret;
    }
    return ret;
}

long long RingBuffer::Seek(long long pos, int whence)
{
    long long ret = -1;
    if (normalfile)
    {
        ret = lseek(fd, pos, whence);
    }
    else
    {
        ret = lseek(fd2, pos, whence);
    }
    return ret;
}
