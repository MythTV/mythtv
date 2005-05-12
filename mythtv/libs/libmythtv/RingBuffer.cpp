#include <qapplication.h>
#include <qdatetime.h>
#include <qfileinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <qsocket.h>
#include <qfile.h>
#include <qregexp.h>

using namespace std;

#include "exitcodes.h"
#include "RingBuffer.h"
#include "mythcontext.h"
#include "remotefile.h"
#include "remoteencoder.h"

#define TFW_DEF_BUF_SIZE (2*1024*1024)
#define TFW_MAX_WRITE_SIZE (TFW_DEF_BUF_SIZE / 4)
#define TFW_MIN_WRITE_SIZE (TFW_DEF_BUF_SIZE / 8)

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
#define HAVE_FDATASYNC
#endif

#define READ_TEST_SIZE 204
#define OPEN_READ_ATTEMPTS 5


class ThreadedFileWriter
{
public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();      /* commits all writes and closes the file. */

    bool Open();

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    // Note, this doesn't even try to flush our queue, only ensure that
    // data which has already been sent to the kernel is written to disk
    void Sync(void);

    void SetWriteBufferSize(int newSize = TFW_DEF_BUF_SIZE);
    void SetWriteBufferMinWriteSize(int newMinSize = TFW_MIN_WRITE_SIZE);

    unsigned BufUsed();  /* # of bytes queued for write by the write thread */
    unsigned BufFree();  /* # of bytes that can be written, without blocking */

protected:
    static void *boot_writer(void *);
    void DiskLoop(); /* The thread that actually calls write(). */

private:
    // allow DiskLoop() to flush buffer completely ignoring low watermark
    void Flush(void);

    const char     *filename;
    int             flags;
    mode_t          mode;
    int             fd;

    bool            no_writes;
    bool            flush;

    unsigned long   tfw_buf_size;
    unsigned long   tfw_min_write_size;
    unsigned int    rpos;
    unsigned int    wpos;
    char           *buf;
    int             in_dtor;
    pthread_t       writer;
    pthread_mutex_t buflock;
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
            if (errno == EAGAIN)
                continue;

            char msg[128];

            errcnt++;
            snprintf(msg, 127,
                     "ERROR: file I/O problem in safe_write(), errcnt = %d",
                     errcnt);
            perror(msg);
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

ThreadedFileWriter::ThreadedFileWriter(const char *fname,
                                       int pflags, mode_t pmode)
    : filename(fname), flags(pflags), mode(pmode), fd(-1),
      no_writes(false), flush(false), tfw_buf_size(0),
      tfw_min_write_size(0), rpos(0), wpos(0), buf(NULL),
      in_dtor(0)
{
    pthread_mutex_t init = PTHREAD_MUTEX_INITIALIZER;
    buflock = init;
}

bool ThreadedFileWriter::Open()
{
    fd = open(filename, flags, mode);

    if (fd < 0)
    {
        VERBOSE(VB_IMPORTANT,
                QString("ERROR opening file '%1' in ThreadedFileWriter. %2")
            .arg(filename).arg(strerror(errno)));
        return false;
    }
    else
    {
        buf = new char[TFW_DEF_BUF_SIZE + 20];
        tfw_buf_size = TFW_DEF_BUF_SIZE;
        tfw_min_write_size = TFW_MIN_WRITE_SIZE;
        pthread_create(&writer, NULL, boot_writer, this);
        return true;
    }
}

ThreadedFileWriter::~ThreadedFileWriter()
{
    no_writes = true;

    if (fd >= 0)
    {
        Flush();
        in_dtor = 1; /* tells child thread to exit */
        pthread_join(writer, NULL);
        close(fd);
        fd = -1;
    }

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
             VERBOSE(VB_IMPORTANT, "IOBOUND - blocking in ThreadedFileWriter::Write()");
        first = 0;
        usleep(5000);
    }

    if (no_writes)
        return 0;

    if ((wpos + count) > tfw_buf_size)
    {
        int first_chunk_size = tfw_buf_size - wpos;
        int second_chunk_size = count - first_chunk_size;
        memcpy(buf + wpos, data, first_chunk_size );
        memcpy(buf, (char *)data + first_chunk_size, second_chunk_size );
    }
    else
    {
        memcpy(buf + wpos, data, count);
    }

    pthread_mutex_lock(&buflock);
    wpos = (wpos + count) % tfw_buf_size;
    pthread_mutex_unlock(&buflock);

    return count;
}

/** \fn ThreadedFileWriter::Seek(long long pos, int whence)
 *  \brief Seek to a position within stream; May be unsafe.
 *
 *   This method is unsafe if Start() has been called and
 *   the call us not preceeded by StopReads(). You probably
 *   want to follow Seek() with a StartReads() in this case.
 *
 *   This method assumes that we don't seek very often. It does
 *   not use a high performance approach... we just block until
 *   the write thread empties the buffer.
 */
long long ThreadedFileWriter::Seek(long long pos, int whence)
{
    Flush();

    return lseek(fd, pos, whence);
}

void ThreadedFileWriter::Flush(void)
{
    flush = true;
    while (BufUsed() > 0)
        usleep(5000);
    flush = false;
}

void ThreadedFileWriter::Sync(void)
{
#ifdef HAVE_FDATASYNC
    fdatasync(fd);
#else
    fsync(fd);
#endif
}

void ThreadedFileWriter::SetWriteBufferSize(int newSize)
{
    if (newSize <= 0)
        return;

    Flush();

    pthread_mutex_lock(&buflock);
    delete [] buf;
    rpos = wpos = 0;
    buf = new char[newSize + 20];
    tfw_buf_size = newSize;
    pthread_mutex_unlock(&buflock);
}

void ThreadedFileWriter::SetWriteBufferMinWriteSize(int newMinSize)
{
    if (newMinSize <= 0)
        return;

    tfw_min_write_size = newMinSize;
}

void ThreadedFileWriter::DiskLoop()
{
    int size;
    int written = 0;
    QTime timer;
    timer.start();

    while (!in_dtor || BufUsed() > 0)
    {
        size = BufUsed();

        if (!size || (!in_dtor && !flush &&
            (((unsigned)size < tfw_min_write_size) && 
             ((unsigned)written >= tfw_min_write_size))))
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
        if (size > TFW_MAX_WRITE_SIZE)
            size = TFW_MAX_WRITE_SIZE;

        if ((rpos + size) > tfw_buf_size)
        {
            int first_chunk_size  = tfw_buf_size - rpos;
            int second_chunk_size = size - first_chunk_size;
            size = safe_write(fd, buf+rpos, first_chunk_size);
            if (size == first_chunk_size)
                size += safe_write(fd, buf, second_chunk_size);
        }
        else
        {
            size = safe_write(fd, buf+rpos, size);
        }
        
        if ((unsigned) written < tfw_min_write_size)
        {
            written += size;
        }
        
        pthread_mutex_lock(&buflock);
        rpos = (rpos + size) % tfw_buf_size;
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
        ret = tfw_buf_size - rpos + wpos;

    pthread_mutex_unlock(&buflock);
    return ret;
}

unsigned ThreadedFileWriter::BufFree()
{
    unsigned ret;
    pthread_mutex_lock(&buflock);

    if (wpos >= rpos)
        ret = rpos + tfw_buf_size - wpos - 1;
    else
        ret = rpos - wpos - 1;

    pthread_mutex_unlock(&buflock);
    return ret;
}

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool usereadahead)
{
    int openAttempts = OPEN_READ_ATTEMPTS;
    Init();

    startreadahead = usereadahead;

    normalfile = true;
    filename = (QString)lfilename;

    if (filename.right(4) != ".nuv")
        openAttempts = 1;

    if (write)
    {
        tfw = new ThreadedFileWriter(filename.ascii(),
                                     O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                     0644);
        if (tfw->Open())
            writemode = true;
        else
        {
            delete tfw;
            tfw = NULL;
        }
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
        {
            char buf[READ_TEST_SIZE];
            while (openAttempts > 0)
            {
                int ret;
                openAttempts--;
                
                fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
                
                if (fd2 < 0)
                {
                    VERBOSE( VB_IMPORTANT, QString("Could not open %1.  %2 retries remaining.")
                                           .arg(filename).arg(openAttempts));
                    usleep(500000);
                }
                else
                {
                    if ((ret = read(fd2, buf, READ_TEST_SIZE)) != READ_TEST_SIZE)
                    {
                        VERBOSE( VB_IMPORTANT, QString("Invalid file handle when opening %1.  "
                                                       "%2 retries remaining.")
                                                       .arg(filename).arg(openAttempts));
                        close(fd2);
                        fd2 = -1;
                        usleep(500000);
                    }
                    else
                    {
                        lseek(fd2, 0, SEEK_SET);
                        openAttempts = 0;
                    }
                }
            }

            QFileInfo fileInfo(filename);
            if (fileInfo.lastModified().secsTo(QDateTime::currentDateTime()) >
                30 * 60)
            {
                oldfile = true;
            }
        }
        else
        {
            remotefile = new RemoteFile(filename);
            if (!remotefile->isOpen())
            {
                VERBOSE(VB_IMPORTANT,
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));
                delete remotefile;
                remotefile = NULL;
            }
        }

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
        if (tfw->Open())
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
        else
        {
            delete tfw;
            tfw = NULL;
        }
    }
    else
    {
        remotefile = new RemoteFile(filename, recorder_num);
        if (!remotefile->isOpen())
        {
            VERBOSE(VB_IMPORTANT,
                    QString("RingBuffer::RingBuffer(): Failed to open remote "
                            "file (%1)").arg(filename));
            delete remotefile;
            remotefile = NULL;
        }
    }

    wrapcount = 0;
    smudgeamount = smudge;
}

void RingBuffer::Init(void)
{
    oldfile = false; 
    startreadahead = true;

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

    numfailures = 0;
    commserror = false;

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
    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }
}

void RingBuffer::Start(void)
{
    if ((normalfile && writemode) || (!normalfile && !recorder_num))
    {
    }
    else if (!readaheadrunning && startreadahead)
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

    numfailures = 0;
    commserror = false;

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
            if (errno == EAGAIN)
                continue;

            VERBOSE(VB_IMPORTANT,
                    QString("ERROR: file I/O problem in 'safe_read()' %1")
                    .arg(strerror(errno)));

            errcnt++;
            numfailures++;
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
            if (zerocnt >= ((oldfile) ? 2 : 50)) // 3 second timeout with usleep(60000), or .12 if it's an old, unmodified file.
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
    if (ret < 0)
    {
        VERBOSE(VB_IMPORTANT, "RemoteFile::Read() failed in RingBuffer::safe_read().");
        rf->Seek(internalreadpos, SEEK_SET);
        ret = 0;
        numfailures++;
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
            usleep(5000);
            totfree = ReadBufFree();
            continue;
        }

        readaheadpaused = false;

        if (totfree < readblocksize)
        {
            usleep(50000);
            totfree = ReadBufFree();
            continue;
        }

        pthread_rwlock_rdlock(&rwlock);
        if (totfree > readblocksize && !commserror)
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

        if (numfailures > 5)
            commserror = true;

        totfree = ReadBufFree();
        used = READ_AHEAD_SIZE - totfree;

        if (ateof || commserror)
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
        if (commserror || ateof || stopreads ||
            (wanttoread <= used && wanttoread > 0))
        {
            availWait.wakeAll();
        }
        availWaitMutex.unlock();

        pthread_rwlock_unlock(&rwlock);

        if ((used >= fill_threshold || wantseek) && !pausereadthread)
            usleep(500);
    }

    delete [] readAheadBuffer;
    readAheadBuffer = NULL;
    rbrpos = 0;
    rbwpos = 0;
}

int RingBuffer::ReadFromBuf(void *buf, int count)
{
    if (commserror)
        return 0;

    bool readone = false;
    int readErr = 0;
    
    if (readaheadpaused && stopreads)
    {
        readone = true;
        Unpause();
    }
    else
    {
        while (!readsallowed && !stopreads)
        {
            if (!readsAllowedWait.wait(5000))
            {
                 VERBOSE(VB_IMPORTANT, "taking too long to be allowed to read..");
                 readErr++;
                 if (readErr > 2)
                 {
                     VERBOSE(VB_IMPORTANT, "Took more than 10 seconds to be allowed to read, "
                                           "aborting.");
                     wanttoread = 0;
                     stopreads = true;
                     return 0;
                 }
            }
        }
    }
    
    int avail = ReadBufAvail();
    readErr = 0;
    
    while (avail < count && !stopreads)
    {
        availWaitMutex.lock();
        wanttoread = count;
        if (!availWait.wait(&availWaitMutex, 4000))
        {
            VERBOSE(VB_IMPORTANT,"Waited 4 seconds for data to become available, waiting "
                    "again...");
            readErr++;
            if (readErr > 7)
            {
                VERBOSE(VB_IMPORTANT,"Waited 14 seconds for data to become available, "
                        "aborting");
                wanttoread = 0;
                stopreads = true;
                availWaitMutex.unlock();
                return 0;
            }
        }

        wanttoread = 0;
        availWaitMutex.unlock();

        avail = ReadBufAvail();
        if (ateof && avail < count)
            count = avail;

        if (commserror)
            return 0;
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
                    VERBOSE(VB_IMPORTANT,"Couldn't read data from the capture card in 15 "
                            "seconds. Stopping.");
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
                long long tmpPos = pos;
                long long tmpRet;

                if (tmpPos > filesize)
                    tmpPos = tmpPos % filesize;

                tmpRet = lseek(fd2, tmpPos, whence);
                if (tmpRet == tmpPos)
                    ret = pos;
                else
                    ret = tmpRet;
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
        }

        while (readpos > filesize && filesize > 0)
            readpos -= filesize;

        while (readpos < 0 && filesize > 0)
            readpos += filesize;
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

void RingBuffer::SetWriteBufferSize(int newSize)
{
    tfw->SetWriteBufferSize(newSize);
}

void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    tfw->SetWriteBufferMinWriteSize(newMinSize);
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
