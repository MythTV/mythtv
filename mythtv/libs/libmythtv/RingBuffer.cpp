#include <qapplication.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <qsocket.h>
#include <qfile.h>
#include <qregexp.h>

#include <iostream>
using namespace std;

#include "RingBuffer.h"
#include "mythcontext.h"
#include "remotefile.h"
#include "remoteencoder.h"

#define TFW_BUF_SIZE (2*1024*1024)

class ThreadedFileWriter
{
public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();                 /* commits all writes and closes the file. */

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    unsigned BufUsed();  /* # of bytes queued for write by the write thread */
    unsigned BufFree();  /* # of bytes that can be written, without blocking */

protected:
    static void *boot_writer(void *);
    void DiskLoop(); /* The thread that actually calls write(). */

private:
    int fd;
    char *buf;
    unsigned rpos,wpos;
    pthread_mutex_t buflock;
    int in_dtor;
    pthread_t writer;
    bool no_writes;
};

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

    no_writes = false;
    buflock = init;
    buf = NULL;
    rpos = wpos = 0;
    in_dtor = 0;

    fd = open(filename, flags, mode);

    if(fd<=0)
    {
	/* oops! */
	printf("ERROR opening file '%s' in ThreadedFileWriter.\n", filename);
        exit(0);
    }
    else
    {
	buf = (char *)malloc(TFW_BUF_SIZE);

	pthread_create(&writer, NULL, boot_writer, this);
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    no_writes = true;
    while(BufUsed() > 0)
        usleep(5000);

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
   
    if (no_writes)
        return 0;
 
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
    while(BufUsed() > 0)
	usleep(5000);

    return lseek(fd, pos, whence);
}

void ThreadedFileWriter::DiskLoop()
{
    int size;

    while(!in_dtor || BufUsed() > 0)
    {
	size = BufUsed();
	
	if(size == 0)
	{
	    usleep(500);
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

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool needevents)
{
    readaheadrunning = false;
    readaheadpaused = false;
    wantseek = false;

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
        bool is_local = false;
        if ((filename.left(7) == "myth://") &&
            (filename.length() > 7 ))
        {
            QString local_pathname = gContext->GetSetting("RecordFilePrefix");
            int hostlen = filename.find( QRegExp("/"), 7 );

            if (hostlen != -1)
            {
                local_pathname +=
                        filename.right(filename.length() - hostlen);

                QFile checkFile(local_pathname);

                if (checkFile.exists())
                {
                    is_local = true;
                    filename = local_pathname;
                }
            }
        }
        else
            is_local = true;

        if (is_local)
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE);
        else
            remotefile = new RemoteFile(filename, needevents);
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
                       long long smudge, RemoteEncoder *enc)
{
    readaheadrunning = false;
    readaheadpaused = false;
    wantseek = false;

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
    KillReadAheadThread();

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
    if ((normalfile && writemode) || (!normalfile && !recorder_num))
    {
    }
    else if (!readaheadrunning)
        StartupReadAheadThread();
}

// guaranteed to be paused, so don't need to lock this
void RingBuffer::TransitionToFile(const QString &lfilename)
{
    dumpfw = new ThreadedFileWriter(lfilename.ascii(), 
                  O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE, 0644);
    dumpwritepos = 0;
}

// guaranteed to be paused, so don't need to lock this
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
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

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

        if (readaheadrunning)
            ResetReadAhead(0);
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
        if (stopreads)
            break;
        if(tot < sz)
           usleep(1000);
    }
    return tot;
}

int RingBuffer::safe_read(RemoteFile *rf, void *data, unsigned sz)
{
    int ret;
    unsigned tot = 0;
    unsigned zerocnt = 0;

    QSocket *sock = rf->getSocket();

    qApp->lock();
    unsigned int available = sock->bytesAvailable();
    qApp->unlock();

    while (available < sz) 
    {
        int reqsize = 128000;

        if (rf->RequestBlock(reqsize))
            break;

        zerocnt++;
        if (zerocnt >= 10)
        {
            break;
        }
        if (stopreads)
            break;

        usleep(100);

        qApp->lock();
        available = sock->bytesAvailable();
        qApp->unlock();
    }

    qApp->lock();
    available = sock->bytesAvailable();
    qApp->unlock();

    if (available > 0)
    {
        qApp->lock();
        ret = sock->readBlock(((char *)data) + tot, sz - tot);
        qApp->unlock();

        tot += ret;
    }
    return tot;
}

#define READ_AHEAD_SIZE (10 * 1024 * 1024)
#define READ_AHEAD_MIN_THRESHOLD (1 * 1024 * 1024)
#define READ_AHEAD_MIN_FULL (256000)
#define READ_AHEAD_MIN_FREE (128000)

int RingBuffer::ReadBufFree(void)
{
    int ret = 0;

    pthread_mutex_lock(&readAheadLock);

    if (rbwpos >= rbrpos)
        ret = rbrpos + READ_AHEAD_SIZE - rbwpos - 1;
    else
        ret = rbrpos - rbwpos - 1;

    pthread_mutex_unlock(&readAheadLock);
    return ret;
}

int RingBuffer::ReadBufAvail(void)
{
    int ret = 0;
    
    pthread_mutex_lock(&readAheadLock);

    if (rbwpos >= rbrpos)
        ret = rbwpos - rbrpos;
    else
        ret = READ_AHEAD_SIZE - rbrpos + rbwpos;

    pthread_mutex_unlock(&readAheadLock);

    return ret;
}

// must call with rwlock in write lock
void RingBuffer::ResetReadAhead(long long newinternal)
{
    pthread_mutex_lock(&readAheadLock);
    rbrpos = 0;
    rbwpos = 0;
    internalreadpos = newinternal;
    ateof = false;
    readsallowed = false;
    pthread_mutex_unlock(&readAheadLock);
}

void RingBuffer::StartupReadAheadThread(void)
{
    readaheadrunning = false;

    pthread_mutex_init(&readAheadLock, NULL);
    pthread_create(&reader, NULL, startReader, this);

    while (!readaheadrunning)
        usleep(50);
}

void RingBuffer::KillReadAheadThread(void)
{
    if (!readaheadrunning)
        return;

    readaheadrunning = false;
    pthread_join(reader, NULL);
}

void RingBuffer::Pause(void)
{
    pausereadthread = true;
    StopReads();
}

void RingBuffer::Unpause(void)
{
    StartReads();
    pausereadthread = false;
}

bool RingBuffer::isPaused(void)
{
    if (!readaheadrunning)
        return true;

    return readaheadpaused;
}

void *RingBuffer::startReader(void *type)
{
    RingBuffer *rbuffer = (RingBuffer *)type;
    rbuffer->ReadAheadThread();
    return NULL;
}

void RingBuffer::ReadAheadThread(void)
{
    long long totfree = 0;
    int ret = -1;
    int used = 0;

    pausereadthread = false;

    readAheadBuffer = new char[READ_AHEAD_SIZE + 256000];
    
    ResetReadAhead(0);
    totfree = ReadBufFree();

    readaheadrunning = true;
    while (readaheadrunning)
    {
        if (pausereadthread)
        {
            readaheadpaused = true;
            usleep(100);
            totfree = ReadBufFree();
            continue;
        }

        readaheadpaused = false;

        pthread_rwlock_rdlock(&rwlock);
        if (totfree > READ_AHEAD_MIN_FREE)
        {
            // limit the read size
            totfree = READ_AHEAD_MIN_FREE;

            if (rbwpos + totfree > READ_AHEAD_SIZE)
                totfree = READ_AHEAD_SIZE - rbwpos;

            if (normalfile)
            {
                if (!writemode)
                {
                    if (remotefile)
                    {
                        ret = safe_read(remotefile, readAheadBuffer + rbwpos,
                                        totfree);
                        internalreadpos += ret;
                    }
                    else
                    {
                        ret = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
                        internalreadpos += ret;
                    }
                }
            }
            else
            {
                if (remotefile)
                {
                    ret = safe_read(remotefile, readAheadBuffer + rbwpos, 
                                    totfree);

                    if (internalreadpos + totfree > filesize)
                    {
                        int toread = filesize - readpos;
                        int left = totfree - toread;
 
                        internalreadpos = left;
                    }
                    else
                        internalreadpos += ret;
                }
                else if (internalreadpos + totfree > filesize)
                {
                    int toread = filesize - internalreadpos;

                    ret = safe_read(fd2, readAheadBuffer + rbwpos, toread);

                    int left = totfree - toread;
                    lseek(fd2, 0, SEEK_SET);

                    ret = safe_read(fd2, readAheadBuffer + rbwpos + toread, 
                                    left);
                    ret += toread;

                    internalreadpos = left;
                }
                else
                {
                    ret = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
                    internalreadpos += ret;
                }
            }

            pthread_mutex_lock(&readAheadLock);
            rbwpos = (rbwpos + ret) % READ_AHEAD_SIZE;
            pthread_mutex_unlock(&readAheadLock);

            if (ret != totfree && normalfile)
            {
                ateof = true;
            }
        }

        totfree = ReadBufFree();
        used = READ_AHEAD_SIZE - totfree;

        if (ateof)
        {
            readsallowed = true;
            totfree = 0;
        }

        if (!readsallowed && used >= READ_AHEAD_MIN_THRESHOLD)
            readsallowed = true;        

        if (readsallowed && used < READ_AHEAD_MIN_FULL && !ateof)
        {
            readsallowed = false;
            cerr << "rebuffering...\n";
        }

        pthread_rwlock_unlock(&rwlock);

        if (totfree < READ_AHEAD_MIN_FREE || wantseek)
            usleep(500);
    }

    delete [] readAheadBuffer;
    readAheadBuffer = NULL;
    rbrpos = 0;
    rbwpos = 0;
}

int RingBuffer::ReadFromBuf(void *buf, int count)
{
    bool readone = false;

    if (readaheadpaused && stopreads)
    {
        readone = true;
        Unpause();
    }
    else 
        while (!readsallowed && !stopreads)
            usleep(100);

    int avail = ReadBufAvail();

    while (avail < count && !stopreads)
    {
        usleep(100);
        avail = ReadBufAvail();
        if (ateof && avail < count)
            count = avail;
    }

    if (stopreads && avail < count)
        count = avail;

    if (rbrpos + count > READ_AHEAD_SIZE)
    {
        int firstsize = READ_AHEAD_SIZE - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
        memcpy(buf, readAheadBuffer + rbrpos, count);

    pthread_mutex_lock(&readAheadLock);
    rbrpos = (rbrpos + count) % READ_AHEAD_SIZE;
    pthread_mutex_unlock(&readAheadLock);

    if (readone)
    {
        Pause();
        while (!isPaused())
            usleep(50);
    }

    return count;
}

int RingBuffer::Read(void *buf, int count)
{
    pthread_rwlock_rdlock(&rwlock);

    int ret = -1;
    if (normalfile)
    {
        if (!writemode)
        {
            if (!readaheadrunning)
            {
                ret = safe_read(fd2, buf, count);
                totalreadpos += ret;
                readpos += ret;
            }
            else
            {
                ret = ReadFromBuf(buf, count);
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
//cout << "reading: " << totalreadpos << " " << readpos << " " << count << " " << filesize << endl;
        if (remotefile)
        {
            ret = ReadFromBuf(buf, count);
  
            if (readpos + ret > filesize)
            {
                int toread = filesize - readpos;
                int left = count - toread;

                readpos = left;
            }
            else
                readpos += ret;
            totalreadpos += ret;
        }
        else if (readpos + count > filesize)
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
    pthread_rwlock_rdlock(&rwlock);

    if (!dumpfw)
    {
        pthread_rwlock_unlock(&rwlock);
        return -1;
    }

    int ret = dumpfw->Write(buf, count);
    dumpwritepos += ret;
    pthread_rwlock_unlock(&rwlock);

    return ret;
}

bool RingBuffer::IsIOBound(void)
{
    bool ret = false;
    int used, free;
    ThreadedFileWriter *fw;
    pthread_rwlock_rdlock(&rwlock);

    fw = dumpfw ? dumpfw : tfw;

    if (!fw)
    {
        pthread_rwlock_unlock(&rwlock);
        return ret;
    }

    used = fw->BufUsed();
    free = fw->BufFree();

    ret = (used * 5 > free);

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

int RingBuffer::Write(const void *buf, int count)
{
    int ret = -1;

    pthread_rwlock_rdlock(&rwlock);

    if (!tfw)
    {
        pthread_rwlock_unlock(&rwlock);
        return ret;
    }

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
//cout << "write: " << totalwritepos << " " << writepos << " " << count << " " << filesize << endl;
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
    wantseek = true;  
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    long long ret = -1;
    if (normalfile)
    {
        if (remotefile)
            ret = remotefile->Seek(pos, whence, readpos);
        else
        {
            if (whence == SEEK_SET)
                ret = lseek(fd2, pos, whence);
            else
            {
                long long realseek = readpos + pos;        
                ret = lseek(fd2, realseek, SEEK_SET);
            }
        }

        if (whence == SEEK_SET)
            readpos = ret;
        else if (whence == SEEK_CUR)
            readpos += pos;
        totalreadpos = readpos;
    }
    else
    {
        if (remotefile)
            ret = remotefile->Seek(pos, whence, totalreadpos);
        else
        {
            if (whence == SEEK_SET)
            {
                ret = lseek(fd2, pos, whence);
            }
            else if (whence == SEEK_CUR)
            {
                long long realseek = totalreadpos + pos;
                while (realseek > filesize)
                    realseek -= filesize;

                ret = lseek(fd2, realseek, SEEK_SET);
            }
	}

        if (whence == SEEK_SET)
        {
            totalreadpos = pos; // only used for file open
            readpos = ret;
        }
        else if (whence == SEEK_CUR)
        {
            readpos += pos;
            totalreadpos += pos;
            while (readpos > filesize)
                readpos -= filesize;
            while (readpos < 0)
                readpos += filesize;
        }
    }

    if (readaheadrunning)
        ResetReadAhead(readpos);

    pthread_rwlock_unlock(&rwlock);

    return ret;
}

long long RingBuffer::WriterSeek(long long pos, int whence)
{
    long long ret = -1;

    if (dumpfw)
	ret = dumpfw->Seek(pos, whence);
    else if (tfw)
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


