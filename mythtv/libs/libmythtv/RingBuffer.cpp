#include <qapplication.h>
#include <qdatetime.h>

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

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

class ThreadedFileWriter
{
public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();      /* commits all writes and closes the file. */

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    // Note, this doesn't even try to flush our queue, only ensure that
    // data which has already been sent to the kernel is written to disk
    void Sync(void);

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
    unsigned tot = 0;
    unsigned errcnt = 0;

    while (tot < sz) 
    {
        ret = write(fd, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            perror("ERROR: file I/O problem in 'safe_write()'\n");
            errcnt++;
            if (errcnt == 3) 
                break;
        }
        else
        {
            tot += ret;
        }
        if (tot < sz) 
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

    no_writes = false;
    buflock = init;
    buf = NULL;
    rpos = wpos = 0;
    in_dtor = 0;

    fd = open(filename, flags, mode);

    if (fd <= 0)
    {
        /* oops! */
        cerr << "ERROR opening file in ThreadedFileWriter.\n";
        perror(filename);
        exit(1);
    }
    else
    {
        buf = new char[TFW_BUF_SIZE + 20];
        pthread_create(&writer, NULL, boot_writer, this);
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    no_writes = true;
    while (BufUsed() > 0)
        usleep(5000);

    in_dtor = 1; /* tells child thread to exit */

    pthread_join(writer, NULL);

    close(fd);
    fd = -1;

    if (buf)
    {
        delete [] buf;
        buf = NULL;
    }
}

unsigned ThreadedFileWriter::Write(const void *data, unsigned count)
{
    if (count == 0)
        return 0;

    int first = 1;

    while (count > BufFree())
    {
        if (first)
            cerr << "IOBOUND - blocking in ThreadedFileWriter::Write()\n";
        first = 0;
        usleep(5000);
    }
   
    if (no_writes)
        return 0;
 
    if ((wpos + count) > TFW_BUF_SIZE)
    {
        int first_chunk_size = TFW_BUF_SIZE - wpos;
        int second_chunk_size = count - first_chunk_size;
        memcpy(buf + wpos, data, first_chunk_size );
        memcpy(buf, (char *)data + first_chunk_size, second_chunk_size );
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
    while (BufUsed() > 0)
        usleep(5000);

    return lseek(fd, pos, whence);
}

void ThreadedFileWriter::Sync(void)
{
    fdatasync(fd);
}

void ThreadedFileWriter::DiskLoop()
{
    int size;
    QTime timer;
    timer.start();

    while (!in_dtor || BufUsed() > 0)
    {
        size = BufUsed();
        
        if (size == 0)
        {
            usleep(500);
            continue;
        }

        if (timer.elapsed() > 1000)
        {
            Sync();
            timer.restart();
        }

        /* cap the max. write size. Prevents the situation where 90% of the
           buffer is valid, and we try to write all of it at once which
           takes a long time. During this time, the other thread fills up
           the 10% that was free... */
        if (size > (TFW_BUF_SIZE / 4))
            size = TFW_BUF_SIZE / 4;

        if ((rpos + size) > TFW_BUF_SIZE)
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

    if (wpos >= rpos)
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

    if (wpos >= rpos)
        ret = rpos + TFW_BUF_SIZE - wpos - 1;
    else
        ret = rpos - wpos - 1;

    pthread_mutex_unlock(&buflock);
    return ret;
}

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool needevents)
{
    Init();

    normalfile = true;
    filename = (QString)lfilename;

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
            int hostlen = filename.find(QRegExp("/"), 7);

            if (hostlen != -1)
            {
                local_pathname += filename.right(filename.length() - hostlen);

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
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
        else
            remotefile = new RemoteFile(filename, needevents);
        writemode = false;
    }

    smudgeamount = 0;
}

RingBuffer::RingBuffer(const QString &lfilename, long long size, 
                       long long smudge, RemoteEncoder *enc)
{
    Init();

    if (enc)
    {
        remoteencoder = enc;
        recorder_num = enc->GetRecorderNumber();
    }

    normalfile = false;
    filename = (QString)lfilename;
    filesize = size;
   
    if (recorder_num == 0)
    {
        tfw = new ThreadedFileWriter(filename.ascii(), 
                                     O_WRONLY|O_CREAT|O_TRUNC|O_LARGEFILE, 
                                     0644);
        fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
    }
    else
    {
        remotefile = new RemoteFile(filename, false, recorder_num);
    }

    wrapcount = 0;
    smudgeamount = smudge;
}

void RingBuffer::Init(void)
{
    readaheadrunning = false;
    readaheadpaused = false;
    wantseek = false;
    fill_threshold = -1;
    fill_min = -1;

    readblocksize = 128000;

    recorder_num = 0;
    remoteencoder = NULL;
    tfw = NULL;
    remotefile = NULL;
    fd2 = -1;

    totalwritepos = writepos = 0;
    totalreadpos = readpos = 0;

    stopreads = false;
    wanttoread = 0;

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
            tfw->Seek(0, SEEK_SET);
            lseek(fd2, 0, SEEK_SET);
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

    while (tot < sz) 
    {
        ret = read(fd, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            perror("ERROR: file I/O problem in 'safe_read()'\n");
            errcnt++;
            if (errcnt == 3) 
                break;
        }
        else if (ret > 0)
        {
            tot += ret;
        }

        if (ret == 0) // EOF returns 0
        {
            if (tot > 0)
                break;

            zerocnt++;
            if ((normalfile && zerocnt > 15) || 
                zerocnt >= 50) // 3 second timeout with usleep(60000)
            {
                break;
            }
        }
        if (stopreads)
            break;
        if (tot < sz)
            usleep(60000);
    }
    return tot;
}

int RingBuffer::safe_read(RemoteFile *rf, void *data, unsigned sz)
{
    int ret = 0;

    ret = rf->Read(data, sz);
    if(ret < 0)
    {
        VERBOSE(VB_IMPORTANT, "RemoteFile::Read() failed in RingBuffer::safe_read().");
        rf->Seek(internalreadpos, SEEK_SET);
        ret = 0;
     }

    return ret;
}

#define READ_AHEAD_SIZE (10 * 256000)

void RingBuffer::CalcReadAheadThresh(int estbitrate)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    fill_threshold = 0;
    fill_min = 0;

    VERBOSE(VB_PLAYBACK, QString("Estimated bitrate = %1").arg(estbitrate));

    if (remotefile)
        fill_threshold += 256000;

    if (estbitrate > 6000)
        fill_threshold += 256000;

    if (estbitrate > 10000)
        fill_threshold += 256000;

    if (estbitrate > 14000)
        fill_threshold += 256000;

    if (estbitrate > 17000)
        readblocksize = 256000;

    fill_min = 1;

    readsallowed = false;

    if (fill_threshold == 0)
        fill_threshold = -1;
    if (fill_min == 0)
        fill_min = -1;

    pthread_rwlock_unlock(&rwlock);
}

int RingBuffer::ReadBufFree(void)
{
    int ret = 0;

    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbrpos + READ_AHEAD_SIZE - rbwpos - 1;
    else
        ret = rbrpos - rbwpos - 1;

    readAheadLock.unlock();
    return ret;
}

int RingBuffer::ReadBufAvail(void)
{
    int ret = 0;
    
    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbwpos - rbrpos;
    else
        ret = READ_AHEAD_SIZE - rbrpos + rbwpos;
    readAheadLock.unlock();
    return ret;
}

// must call with rwlock in write lock
void RingBuffer::ResetReadAhead(long long newinternal)
{
    readAheadLock.lock();
    rbrpos = 0;
    rbwpos = 0;
    internalreadpos = newinternal;
    ateof = false;
    readsallowed = false;
    readAheadLock.unlock();
}

void RingBuffer::StartupReadAheadThread(void)
{
    readaheadrunning = false;

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

void RingBuffer::StopReads(void)
{
    stopreads = true;
    availWait.wakeAll();
}

void RingBuffer::StartReads(void)
{
    stopreads = false;
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

void RingBuffer::WaitForPause(void)
{
    if (!readaheadrunning)
        return;

    if  (!readaheadpaused)
    {
        while (!pauseWait.wait(1000))
            VERBOSE(VB_IMPORTANT, "Waited too long for ringbuffer pause..");
    }
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
            pauseWait.wakeAll();
            usleep(100);
            totfree = ReadBufFree();
            continue;
        }

        readaheadpaused = false;

        pthread_rwlock_rdlock(&rwlock);
        if (totfree > readblocksize)
        {
            // limit the read size
            totfree = readblocksize;

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

            readAheadLock.lock();
            rbwpos = (rbwpos + ret) % READ_AHEAD_SIZE;
            readAheadLock.unlock();

            if (ret == 0 && normalfile && !stopreads)
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

        if (!readsallowed && used >= fill_min)
            readsallowed = true;        

        if (readsallowed && used < fill_min && !ateof)
        {
            readsallowed = false;
            VERBOSE(VB_GENERAL, QString ("rebuffering (%1 %2)").arg(used)
                                                               .arg(fill_min));
        }

        if (readsallowed || stopreads)
            readsAllowedWait.wakeAll();

        availWaitMutex.lock();
        if (ateof || stopreads || (wanttoread <= used && wanttoread > 0))
            availWait.wakeAll();
        availWaitMutex.unlock();
            
        pthread_rwlock_unlock(&rwlock);

        if ((used >= fill_threshold || wantseek) && !pausereadthread)
            usleep(500);
        else if (!pausereadthread)
            sched_yield();
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
        {
            if (!readsAllowedWait.wait(5000))
                cerr << "taking too long to be allowed to read..\n";
        }

    int avail = ReadBufAvail();

    while (avail < count && !stopreads)
    {
        availWaitMutex.lock();
        wanttoread = count;
        if (!availWait.wait(&availWaitMutex, 2000))
            cerr << "Waited 2 seconds for data to become available, waiting "
                    "again...\n";
        wanttoread = 0;
        availWaitMutex.unlock();

        avail = ReadBufAvail();
        if (ateof && avail < count)
            count = avail;
    }

    if ((ateof || stopreads) && avail < count)
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

    readAheadLock.lock();
    rbrpos = (rbrpos + count) % READ_AHEAD_SIZE;
    readAheadLock.unlock();

    if (readone)
    {
        Pause();
        WaitForPause();
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
        else
        {
            if (stopreads)
            {
                pthread_rwlock_unlock(&rwlock);
                return 0;
            }

            availWaitMutex.lock();
            wanttoread = totalreadpos + count;

            while (totalreadpos + count > totalwritepos - tfw->BufUsed())
            {
                if (!availWait.wait(&availWaitMutex, 15000))
                {
                    cerr << "Couldn't read data from the capture card in 15 "
                            "seconds.  Game over, man.\n";
                    StopReads();
                }

                if (stopreads)
                {
                    availWaitMutex.unlock();
                    pthread_rwlock_unlock(&rwlock);
                    return 0;
                }
            }

            availWaitMutex.unlock();

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
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

bool RingBuffer::IsIOBound(void)
{
    bool ret = false;
    int used, free;
    pthread_rwlock_rdlock(&rwlock);

    if (!tfw)
    {
        pthread_rwlock_unlock(&rwlock);
        return ret;
    }

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

        availWaitMutex.lock();
        if (totalwritepos >= wanttoread)
            availWait.wakeAll();
        availWaitMutex.unlock();
    }

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

void RingBuffer::Sync(void)
{
    tfw->Sync();
}

long long RingBuffer::GetFileWritePosition(void)
{
    return totalwritepos;
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

    if (tfw)
    {
        ret = tfw->Seek(pos, whence);
        totalwritepos = ret;
    }

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

long long RingBuffer::GetRealFileSize(void)
{
    if (remotefile)
    {
        if (normalfile)
            return remotefile->GetFileSize();
        else
            return -1;
    }

    struct stat st;
    if (stat(filename.ascii(), &st) == 0)
        return st.st_size;
    return -1;
}
