#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <iostream>
using namespace std;

#include "RingBuffer.h"
#include "mythcontext.h"
#include "remotefile.h"
#include "remoteencoder.h"

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
	    perror("ERROR: file I/O problem in 'safe_write()'\n");
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

    if(fd<=0)
    {
	/* oops! */
	printf("ERROR opening file '%s' in ThreadedFileWriter.\n", filename);
    }
    else
    {
	buf = (char *)malloc(TFW_BUF_SIZE);

	pthread_create(&writer, NULL, boot_writer, this);
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    in_dtor = 1; /* tells child thread to exit */

    pthread_join(writer, NULL);

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

    return lseek64(fd, pos, whence);
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

RingBuffer::RingBuffer(MythContext *context, const QString &lfilename, 
                       bool write, bool needevents)
{
    m_context = context;
    recorder_num = 0;   
 
    normalfile = true;
    filename = (QString)lfilename;
    tfw = NULL;
    remotefile = NULL;
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
        if (filename.left(7) == "myth://")
            remotefile = new RemoteFile(filename, needevents);
        else
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

RingBuffer::RingBuffer(MythContext *context, const QString &lfilename, 
                       long long size, long long smudge, RemoteEncoder *enc)
{
    m_context = context;
    recorder_num = 0;

    remoteencoder = NULL;

    if (enc)
    {
        remoteencoder = enc;
        recorder_num = enc->GetRecorderNumber();
    }

    normalfile = false;
    filename = (QString)lfilename;
    filesize = size;
   
    remotefile = NULL;
    tfw = NULL;
    fd2 = -1;

    if (recorder_num == 0)
    {
        tfw = new ThreadedFileWriter(filename.ascii(), 
                                     O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 
                                     0644);
        fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);
    }
    else
    {
        remotefile = new RemoteFile(filename, false, recorder_num);
    }

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
    if (remotefile)
    {
        delete remotefile;
    }
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

void RingBuffer::Start(void)
{
    if (remotefile)
        remotefile->Start();
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
    pthread_rwlock_wrlock(&rwlock);

    if (!normalfile)
    {
        if (remotefile)
        {
            remotefile->Reset();
        }
        else
        {
	    delete tfw;
            close(fd2);

            tfw = new ThreadedFileWriter(filename.ascii(),
                                         O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 
                                         0644);
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);
        }

        totalwritepos = writepos = 0;
        totalreadpos = readpos = 0;

        wrapcount = 0;
    }
    
    pthread_rwlock_unlock(&rwlock);
}

int RingBuffer::safe_read(int fd, void *data, unsigned sz)
{
    int ret;
    unsigned tot = 0;
    unsigned errcnt = 0;
    unsigned zerocnt = 0;

    while(tot < sz) {
        ret = read(fd, (char *)data+tot, sz-tot);
        if(ret<0)
        {
            perror("ERROR: file I/O problem in 'safe_read()'\n");
            errcnt++;
            if (errcnt==3) 
                break;
        }
        else if (ret > 0)
        {
            tot += ret;
        }

        if (ret == 0) // EOF returns 0
        {
            zerocnt++;
            if (zerocnt >= 20)
            {
                break;
            }
        }
        if(tot < sz)
           usleep(1000);
        if (stopreads)
        {
           break;
        }
    }
    return tot;
}

int RingBuffer::safe_read(RemoteFile *rf, void *data, unsigned sz)
{
    int ret;
    unsigned tot = 0;
    unsigned zerocnt = 0;

    QSocket *sock = rf->getSocket();

    while (sock->bytesAvailable() < sz) 
    {
        int reqsize = 128000;
        if (rf->RequestBlock(reqsize))
            break;

        zerocnt++;
        if (zerocnt >= 10)
        {
            break;
        }
        usleep(100);
        if (stopreads)
            break;
    }

    if (sock->bytesAvailable() >= sz)
    {
        ret = sock->readBlock(((char *)data) + tot, sz - tot);
        tot += ret;
    }
    return tot;
}


int RingBuffer::Read(void *buf, int count)
{
    pthread_rwlock_rdlock(&rwlock);

    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            if (remotefile)
            {
                ret = safe_read(remotefile, buf, count);
                totalreadpos += ret;
                readpos += ret;
            }
            else
            {
                ret = safe_read(fd2, buf, count);
	        totalreadpos += ret;
                readpos += ret;
            }
        }
        else
        {
            fprintf(stderr, "Attempt to read from a write only file\n");
        }
    }
    else
    {
        if (remotefile)
        {
            ret = safe_read(remotefile, buf, count);
            readpos += ret;
            totalreadpos += ret;
        } 
        else if (readpos + count > filesize)
        {
	    int toread = filesize - readpos;

	    ret = safe_read(fd2, buf, toread);
	    
	    int left = count - toread;
	    lseek64(fd2, 0, SEEK_SET);

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
        if (remotefile)
            ret = remotefile->Seek(pos, whence, readpos);
        else
            ret = lseek64(fd2, pos, whence);

        if (whence == SEEK_SET)
            readpos = ret;
        else if (whence == SEEK_CUR)
            readpos += pos;
        totalreadpos = readpos;
    }
    else
    {
        if (remotefile)
            ret = remotefile->Seek(pos, whence, readpos);
        else
            ret = lseek64(fd2, pos, whence);

	if (whence == SEEK_SET)
        {
	    totalreadpos = pos; // only used for file open
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
    {
        if (remoteencoder)
            return remoteencoder->GetFreeSpace(totalreadpos);
        return totalreadpos + filesize - totalwritepos - smudgeamount;
    }
    else
        return -1;
}

long long RingBuffer::GetFreeSpaceWithReadChange(long long readchange)
{
    if (!normalfile)
    {
        if (readchange > 0)
            readchange = 0 - (filesize - readchange);

	return GetFreeSpace() + readchange; 
    }
    else
    {
        return readpos + readchange;
    }
}

long long RingBuffer::GetReadPosition(void)
{
    return readpos;
}

long long RingBuffer::GetTotalReadPosition(void)
{
    return totalreadpos;
}

long long RingBuffer::GetWritePosition(void)
{
    return writepos;
}

long long RingBuffer::GetTotalWritePosition(void) 
{ 
    return totalwritepos; 
}


