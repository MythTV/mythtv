#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include "RingBuffer.h"

#define TFW_BUF_SIZE (2*1024*1024)

static unsigned safe_write(int fd, const void *data, unsigned sz)
{
    int ret;
    unsigned tot=0;
    unsigned errcnt=0;
    while(tot < sz) {
	ret=write(fd, (char *)data+tot, sz-tot);
	if(ret<0)
	{
	    perror("ERROR: file I/O problem in 'safe_write()' (RingBuffer.cpp)\n");
	    errcnt++;
	    if(errcnt==3) break;
	}
	else
	{
	    tot += ret;
	}
	if(tot < sz) usleep(1000);
    }
    return tot;
}

static unsigned safe_read(int fd, void *data, unsigned sz)
{
    int ret;
    unsigned tot=0;
    unsigned errcnt=0;
    while(tot < sz) {
	ret = read(fd, (char *)data+tot, sz-tot);
	if(ret<0)
	{
	    perror("ERROR: file I/O problem in 'safe_read()' (RingBuffer.cpp)\n");
	    errcnt++;
	    if(errcnt==3) break;
	}
	else
	{
	    tot+=ret;
	}
        if (ret == 0) // EOF returns 0
            break;
	if(tot < sz) 
           usleep(1000);
    }
    return tot;
}

void *ThreadedFileWriter::boot_writer(void *wotsit)
{
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->DiskLoop();
    return NULL;
}

ThreadedFileWriter::ThreadedFileWriter(const char *filename,
				       int flags, mode_t mode)
{
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;

    buflock = init;
    buf = NULL;
    rpos = wpos = 0;
    in_dtor = 0;

    fd = open(filename, flags, mode);
    child_live = 0;

    if(fd<=0)
    {
	/* oops! */
	printf("ERROR opening file '%s' in ThreadedFileWriter.\n", filename);
    }
    else
    {
	buf = (char *)malloc(TFW_BUF_SIZE);

	child_live = 1;
	pthread_create(&writer, NULL, boot_writer, this);
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    in_dtor = 1; /* tells child thread to exit */

    while(child_live)
    {
	usleep(10000);
    }

    close(fd);
    fd = -1;

    if(buf)
    {
	free(buf); 
	buf = NULL;
    }
}

unsigned ThreadedFileWriter::Write(const void *data, unsigned count)
{
    int first = 1;

    while(count > BufFree())
    {
	if(first)
	    printf("IOBOUND - blocking in ThreadedFileWriter::Write()\n");
	first = 0;
	usleep(5000);
    }
    
    if((wpos + count) > TFW_BUF_SIZE)
    {
	int first_chunk_size = TFW_BUF_SIZE - wpos;
	int second_chunk_size = count - first_chunk_size;
	memcpy(buf + wpos, data, first_chunk_size );
	memcpy(buf,        (char *)data+first_chunk_size, second_chunk_size );
    }
    else
    {
	memcpy(buf + wpos, data, count);
    }
    pthread_mutex_lock(&buflock);
    wpos = (wpos + count) % TFW_BUF_SIZE;
    pthread_mutex_unlock(&buflock);

    return count;
}

long long ThreadedFileWriter::Seek(long long pos, int whence)
{
    /* Assumes that we don't seek very often. This is not a high
       performance approach... we just block until the write thread
       empties the buffer. */
    while(BufUsed()>0)
	usleep(5000);

    return lseek(fd, pos, whence);
}

void ThreadedFileWriter::DiskLoop()
{
    int size;

    while( !in_dtor || BufUsed()>0 )
    {
	size = BufUsed();
	
	if(size == 0)
	{
	    usleep(5000);
	    continue;
	}

	/* cap the max. write size. Prevents the situation where 90% of the
	   buffer is valid, and we try to write all of it at once which
	   takes a long time. During this time, the other thread fills up
	   the 10% that was free... */
	if(size > (TFW_BUF_SIZE/4))
	    size = TFW_BUF_SIZE/4;

	if((rpos + size) > TFW_BUF_SIZE)
	{
	    int first_chunk_size  = TFW_BUF_SIZE - rpos;
	    int second_chunk_size = size - first_chunk_size;
	    safe_write(fd, buf+rpos, first_chunk_size);
	    safe_write(fd, buf,      second_chunk_size);
	}
	else
	{
	    safe_write(fd, buf+rpos, size);
	}
	pthread_mutex_lock(&buflock);
	rpos = (rpos + size) % TFW_BUF_SIZE;
	pthread_mutex_unlock(&buflock);
    }
    child_live=0;
}

unsigned ThreadedFileWriter::BufUsed()
{
    unsigned ret;
    pthread_mutex_lock(&buflock);

    if(wpos >= rpos)
	ret = wpos - rpos;
    else
	ret = TFW_BUF_SIZE - rpos + wpos;

    pthread_mutex_unlock(&buflock);
    return ret;
}

unsigned ThreadedFileWriter::BufFree()
{
    unsigned ret;
    pthread_mutex_lock(&buflock);

    if(wpos >= rpos)
	ret = rpos + TFW_BUF_SIZE - wpos - 1;
    else
	ret = rpos - wpos - 1;

    pthread_mutex_unlock(&buflock);
    return ret;
}

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write)
{
    normalfile = true;
    filename = (QString)lfilename;
    tfw = NULL;
    fd2 = -1;

    if (write)
    {
        tfw = new ThreadedFileWriter(filename.ascii(),
				     O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 
				     0644);
        writemode = true;
    }
    else
    {
        fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);
        writemode = false;
    }

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;
    smudgeamount = 0;

    stopreads = false;
    dumpfw = NULL;
    dumpwritepos = 0;

    pthread_rwlock_init(&rwlock, NULL);
}

RingBuffer::RingBuffer(const QString &lfilename, long long size, 
                       long long smudge)
{
    normalfile = false;
    filename = (QString)lfilename;
    filesize = size;
   
    tfw = NULL;
    fd2 = -1;

    tfw = new ThreadedFileWriter(filename.ascii(), 
				 O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
    fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;

    wrapcount = 0;
    smudgeamount = smudge;

    stopreads = false;
    dumpfw = NULL;
    dumpwritepos = 0;

    pthread_rwlock_init(&rwlock, NULL);
}

RingBuffer::~RingBuffer(void)
{
    pthread_rwlock_wrlock(&rwlock);
    if (tfw)
    {
	delete tfw;
	tfw = NULL;
    }
    if (fd2 > 0)
    {
        close(fd2);
    }
    if (dumpfw)
    {
	delete dumpfw; 
	dumpfw = NULL;
    }
}

void RingBuffer::TransitionToFile(const QString &lfilename)
{
    pthread_rwlock_wrlock(&rwlock);

    dumpfw = new ThreadedFileWriter(lfilename.ascii(), 
                  O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
    dumpwritepos = 0;

    pthread_rwlock_unlock(&rwlock);
}

void RingBuffer::TransitionToRing(void)
{
    pthread_rwlock_wrlock(&rwlock);
 
    delete dumpfw;
    dumpfw = NULL;
    dumpwritepos = 0;

    pthread_rwlock_unlock(&rwlock);
}

void RingBuffer::Reset(void)
{
    pthread_rwlock_rdlock(&rwlock);

    if (!normalfile)
    {
	delete tfw;
        close(fd2);

        tfw = new ThreadedFileWriter(filename.ascii(),
				     O_WRONLY|O_CREAT|O_LARGEFILE, 0644);
        fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);
 
        totalwritepos = writepos = 0;
        totalreadpos = readpos = 0;

        wrapcount = 0;
    }
    
    pthread_rwlock_unlock(&rwlock);
}

int RingBuffer::Read(void *buf, int count)
{
    pthread_rwlock_rdlock(&rwlock);

    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            ret = safe_read(fd2, buf, count);
	    totalreadpos += ret;
            readpos += ret;
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
        while (totalreadpos + count >= 
	       totalwritepos - (dumpfw ? dumpfw->BufUsed() : tfw->BufUsed() ) )
        {
            usleep(1000);
            if (stopreads)
            {
                pthread_rwlock_unlock(&rwlock);
                return 0;
            }
	}

	if (readpos + count > filesize)
        {
	    int toread = filesize - readpos;

	    ret = safe_read(fd2, buf, toread);
	    
	    int left = count - toread;
	    lseek(fd2, 0, SEEK_SET);

	    ret = safe_read(fd2, (char *)buf + toread, left);
	    ret += toread;

	    totalreadpos += ret;
	    readpos = left;
	}
	else
        {
            ret = safe_read(fd2, buf, count);
            readpos += ret;
	    totalreadpos += ret;
        }
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

int RingBuffer::WriteToDumpFile(const void *buf, int count)
{
    int ret = dumpfw->Write(buf, count);
    dumpwritepos += ret;
    return ret;
}

bool RingBuffer::IsIOBound(void)
{
    bool ret;
    int used, free;
    ThreadedFileWriter *fw;
    pthread_rwlock_rdlock(&rwlock);

    fw = dumpfw ? dumpfw : tfw;

    used = tfw->BufUsed();
    free = tfw->BufFree();

    ret = (used * 5 > free);

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

int RingBuffer::Write(const void *buf, int count)
{
    int ret = -1;

    pthread_rwlock_rdlock(&rwlock);

    if (normalfile)
    {
        if (writemode)
        {
	    ret = tfw->Write(buf, count);
	    totalwritepos += ret;
            writepos += ret;
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
	    ret = tfw->Write(buf, towrite);

	    int left = count - towrite;
	    tfw->Seek(0, SEEK_SET);

	    ret = tfw->Write((char *)buf + towrite, left);
	    writepos = left;

	    ret += towrite;

	    totalwritepos += ret;
	    wrapcount++;
        }
	else
	{
	    ret = tfw->Write(buf, count);
            writepos += ret;
	    totalwritepos += ret;
	}

        if (dumpfw)
        {
	    int ret2 = dumpfw->Write(buf, count);
            dumpwritepos += ret2;
        }
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

long long RingBuffer::GetFileWritePosition(void)
{
    long long ret = -1;
    if (dumpfw)
        ret = dumpwritepos;
    else
        ret = totalwritepos;

    return ret;
}

long long RingBuffer::Seek(long long pos, int whence)
{
    pthread_rwlock_rdlock(&rwlock);

    long long ret = -1;
    if (normalfile)
    {
        ret = lseek(fd2, pos, whence);
	if (whence == SEEK_SET)
            readpos = ret;
	else if (whence == SEEK_CUR)
            readpos += pos;
        totalreadpos = readpos;
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
    pthread_rwlock_unlock(&rwlock);

    return ret;
}

long long RingBuffer::WriterSeek(long long pos, int whence)
{
    long long ret = -1;

    if(dumpfw)
	ret = dumpfw->Seek(pos, whence);
    else
	ret = tfw->Seek(pos, whence);

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

