#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "RingBuffer.h"

RingBuffer::RingBuffer(const string &lfilename, bool actasnormalfile, 
                       bool write)
{
    normalfile = true;
    filename = (string)lfilename;
    fd = -1;

    if (write)
    {
        fd = open(filename.c_str(), O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
        writemode = true;
    }
    else
    {
        fd = open(filename.c_str(), O_RDONLY|O_LARGEFILE);
        writemode = false;
    }

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;
    smudgeamount = 0;
}

RingBuffer::RingBuffer(const string &lfilename, long long size, 
                       long long smudge)
{
    normalfile = false;
    filename = (string)lfilename;
    filesize = size;
   
    fd = -1; fd2 = -1;

    fd = open(filename.c_str(), O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
    fd2 = open(filename.c_str(), O_RDONLY|O_LARGEFILE);

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;

    wrapcount = 0;
    smudgeamount = smudge;
}

RingBuffer::~RingBuffer(void)
{
    if (fd > 0)
        close(fd);
    if (fd2 > 0)
        close(fd2);
}

void RingBuffer::Reset(void)
{
    if (!normalfile)
    {
        close(fd);
        close(fd2);

        fd = open(filename.c_str(), O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
        fd2 = open(filename.c_str(), O_RDONLY|O_LARGEFILE);
 
        totalwritepos = writepos = 0;
        totalreadpos = readpos = 0;

        wrapcount = 0;
    }
}

int RingBuffer::Read(void *buf, int count)
{
    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            ret = read(fd, buf, count);
	    totalreadpos += ret;
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
        while (totalreadpos + count >= totalwritepos)
	{
            usleep(5);
	}

	if (readpos + count > filesize)
        {
	    int toread = filesize - readpos;

	    ret = read(fd2, buf, toread);
	    
	    int left = count - toread;
	    lseek(fd2, 0, SEEK_SET);

	    ret = read(fd2, (char *)buf + toread, left);
	    ret += toread;

	    totalreadpos += ret;
	    readpos = left;
	}
	else
        {
            ret = read(fd2, buf, count);
            readpos += ret;
	    totalreadpos += ret;
        }
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
	    totalwritepos += ret;
        }
        else
        {
            fprintf(stderr, "Attempt to write to a read only file\n");
        }
    }
    else
    {
        if (writepos + count > filesize)
        {
	    int towrite = filesize - writepos;
	    ret = write(fd, buf, towrite);

	    int left = count - towrite;
	    lseek(fd, 0, SEEK_SET);

	    ret = write(fd, (char *)buf + towrite, left);
	    writepos = left;

	    ret += towrite;

	    totalwritepos += ret;
	    wrapcount++;
        }
	else
	{
            ret = write(fd, buf, count);
            writepos += ret;
	    totalwritepos += ret;
	}
    }

    return ret;
}

long long RingBuffer::Seek(long long pos, int whence)
{
    long long ret = -1;
    if (normalfile)
    {
        ret = lseek(fd, pos, whence);
	if (whence == SEEK_SET)
            readpos = ret;
	else if (whence == SEEK_CUR)
            readpos += pos;
    }
    else
    {
        ret = lseek(fd2, pos, whence);
	if (whence == SEEK_SET)
        {
            // FIXME: set totalreadpos too
	    readpos = ret;
        }
	else if (whence == SEEK_CUR)
        {
            readpos += pos;
	    totalreadpos += pos;
	}
    }
    return ret;
}

long long RingBuffer::GetFreeSpace(void)
{
    if (!normalfile)
        return (totalreadpos + filesize - totalwritepos - smudgeamount);
    else
        return -1;
}

long long RingBuffer::GetFreeSpaceWithReadChange(long long readchange)
{
    if (!normalfile)
    {
        if (readchange > 0)
            readchange = 0 - (filesize - readchange);

	return (totalreadpos + readchange + filesize - totalwritepos - 
                smudgeamount);
    }
    else
    {
        return readpos + readchange;
    }
}

