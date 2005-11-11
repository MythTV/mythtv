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
#include "ThreadedFileWriter.h"
#include "livetvchain.h"
#include "DVDRingBuffer.h"

#ifndef O_STREAMING
#define O_STREAMING 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

const uint RingBuffer::kBufferSize = 10 * 256000;

#define PNG_MIN_SIZE   20 /* header plus one empty chunk */
#define NUV_MIN_SIZE  204 /* header size? */
#define MPEG_MIN_SIZE 376 /* 2 TS packets */

/* should be minimum of the above test sizes */
const uint RingBuffer::kReadTestSize = PNG_MIN_SIZE;

bool RingBuffer::IsOpen(void) 
{ 
#ifdef HAVE_DVDNAV
    return tfw || (fd2 > -1) || remotefile || (dvdPriv && dvdPriv->IsOpen());
#else // if !HAVE_DVDNAV
    return tfw || (fd2 > -1) || remotefile; 
#endif // !HAVE_DVDNAV
}

/**********************************************************************/

RingBuffer::RingBuffer(const QString &lfilename, bool write, bool usereadahead)
{
    Init();
    
    startreadahead = usereadahead;
    filename = (QString)lfilename;

    if (write)
    {
        tfw = new ThreadedFileWriter(filename,
                                     O_WRONLY|O_TRUNC|O_CREAT|O_LARGEFILE,
                                     0644);
        if (tfw->Open())
            writemode = true;
        else
        {
            delete tfw;
            tfw = NULL;
        }
        writemode = true;
        return;
    }

    OpenFile(filename);
}

void RingBuffer::OpenFile(const QString &lfilename, uint retryCount)
{
    uint openAttempts = retryCount + 1;

    filename = lfilename;

    if (remotefile)
    {
        delete remotefile;
    }

    if (fd2 >= 0)
    {
        close(fd2);
        fd2 = -1;
    }

    bool is_local = false;
    bool is_dvd = false;
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
    else if (filename.left(4) == "dvd:")
    {
        is_dvd = true;
        dvdPriv = new DVDRingBufferPriv();
        startreadahead = false;
        int pathLen = filename.find(QRegExp("/"), 4);
        if (pathLen != -1)
        {
            QString tempFilename = filename.right(filename.length() -  pathLen);

            QFile checkFile(tempFilename);
            if (!checkFile.exists())
                filename = "/dev/dvd";
            else
                filename = tempFilename;
        }
        else
        {
            filename = "/dev/dvd";
        }
    }
    else
        is_local = true;

    if (is_local)
    {
        char buf[kReadTestSize];
        while (openAttempts > 0)
        {
            openAttempts--;
                
            fd2 = open(filename.ascii(), O_RDONLY|O_LARGEFILE|O_STREAMING);
                
            if (fd2 < 0)
            {
                VERBOSE(VB_IMPORTANT,
                        QString("Could not open %1.  %2 retries remaining.")
                        .arg(filename).arg(openAttempts));

                usleep(500000);
            }
            else
            {
                int ret = read(fd2, buf, kReadTestSize);
                if (ret != (int)kReadTestSize)
                {
                    VERBOSE(VB_IMPORTANT,
                            QString("Invalid file handle when opening %1.  "
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
    else if (is_dvd)
    {
        dvdPriv->OpenFile(filename);
        readblocksize = DVD_BLOCK_SIZE * 62;
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
}

void RingBuffer::Init(void)
{
    tfw = NULL;

    writemode = false;
    livetvchain = NULL;
    oldfile = false; 
    startreadahead = true;
    dvdPriv = NULL;
    
    readaheadrunning = false;
    readaheadpaused = false;
    wantseek = false;
    fill_threshold = -1;
    fill_min = -1;

    readblocksize = 128000;
   
    recorder_num = 0;
    remoteencoder = NULL;
    remotefile = NULL;
    
    fd2 = -1;

    writepos = readpos = 0;

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
    
    if (dvdPriv)
    {
        delete dvdPriv;
    }
    
}

void RingBuffer::Start(void)
{
    if (writemode)
    {
    }
    else if (!readaheadrunning && startreadahead)
        StartupReadAheadThread();
}

void RingBuffer::Reset(bool full)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;
    numfailures = 0;
    commserror = false;

    writepos = 0;
    readpos = 0;

    if (full)
        ResetReadAhead(0);

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
            if (zerocnt >= ((oldfile) ? 2 : (livetvchain) ? 6 : 50)) // 3 second timeout with usleep(60000), or .12 if it's an old, unmodified file.
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

void RingBuffer::CalcReadAheadThresh(int estbitrate)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);

    wantseek       = false;
    readsallowed   = false;
    fill_min       = 1;
    readblocksize  = (estbitrate > 12000) ? 256000 : 128000;

#if 1
    fill_threshold = 0;

    if (remotefile)
        fill_threshold += 256000;

    if (estbitrate > 6000)
        fill_threshold += 256000;

    if (estbitrate > 10000)
        fill_threshold += 256000;

    if (estbitrate > 12000)
        fill_threshold += 256000;

    fill_threshold = (fill_threshold) ? fill_threshold : -1;
#else
    fill_threshold = (remotefile) ? 256000 : 0;
    fill_threshold = max(0, estbitrate) * 60*10;
    fill_threshold = (fill_threshold) ? fill_threshold : -1;
#endif

    VERBOSE(VB_PLAYBACK, "RingBuf:" +
            QString("CalcReadAheadThresh(%1 KB) -> ").arg(estbitrate) +
            QString("threshhold(%1 KB) readblocksize(%2 KB)")
            .arg(fill_threshold/1024).arg(readblocksize/1024));

    pthread_rwlock_unlock(&rwlock);
}

int RingBuffer::ReadBufFree(void)
{
    int ret = 0;

    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbrpos + kBufferSize - rbwpos - 1;
    else
        ret = rbrpos - rbwpos - 1;

    readAheadLock.unlock();
    return ret;
}

int RingBuffer::DataInReadAhead(void)
{
    return ReadBufAvail();
}

int RingBuffer::ReadBufAvail(void)
{
    int ret = 0;

    readAheadLock.lock();

    if (rbwpos >= rbrpos)
        ret = rbwpos - rbrpos;
    else
        ret = kBufferSize - rbrpos + rbwpos;
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
    int loops = 0;

    pausereadthread = false;

    readAheadBuffer = new char[kBufferSize + 256000];

    ResetReadAhead(0);
    totfree = ReadBufFree();

    readaheadrunning = true;
    while (readaheadrunning)
    {
        if (pausereadthread || writemode)
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
            ++loops;
            // break out if we've spent lots of time here, just in case things
            // are waiting on a wait condition that never got triggered.
            if (readsallowed && loops < 10)
                continue;
        }
        loops = 0;

        pthread_rwlock_rdlock(&rwlock);
        if (totfree > readblocksize && !commserror)
        {
            // limit the read size
            totfree = readblocksize;

            if (rbwpos + totfree > kBufferSize)
                totfree = kBufferSize - rbwpos;

            if (remotefile)
            {
                ret = safe_read(remotefile, readAheadBuffer + rbwpos,
                                totfree);
                internalreadpos += ret;
            }
            else if (dvdPriv)
            {                        
                ret = dvdPriv->safe_read(readAheadBuffer + rbwpos, totfree);
                internalreadpos += ret;
            }
            else
            {
                ret = safe_read(fd2, readAheadBuffer + rbwpos, totfree);
                internalreadpos += ret;
            }

            readAheadLock.lock();
            if (ret > 0 )
                rbwpos = (rbwpos + ret) % kBufferSize;
            readAheadLock.unlock();

            if (ret == 0 && !stopreads)
            {
                if (livetvchain)
                {
                    if (livetvchain->HasNext())
                    {
                        livetvchain->SwitchToNext(true);
                    }
                }
                else
                {
                    ateof = true;
                }
            }
        }

        if (numfailures > 5)
            commserror = true;

        totfree = ReadBufFree();
        used = kBufferSize - totfree;

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
            if (!readsAllowedWait.wait(1000))
            {
                 VERBOSE(VB_IMPORTANT, "taking too long to be allowed to read..");
                 readErr++;
                 
                 // HACK Sometimes the readhead thread gets borked on startup.
               /*  if ((readErr % 2) && (rbrpos ==0))
                 {
                    VERBOSE(VB_IMPORTANT, "restarting readhead thread..");
                    KillReadAheadThread();
                    StartupReadAheadThread();
                 }                    
                   */ 
                 if (readErr > 10)
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

    if (rbrpos + count > (int) kBufferSize)
    {
        int firstsize = kBufferSize - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
        memcpy(buf, readAheadBuffer + rbrpos, count);

    readAheadLock.lock();
    rbrpos = (rbrpos + count) % kBufferSize;
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
    int ret = -1;
    if (writemode)
    {
        fprintf(stderr, "Attempt to read from a write only file\n");
        return ret;
    }

    pthread_rwlock_rdlock(&rwlock);

    if (!readaheadrunning)
    {
        if (remotefile)
        {
            ret = safe_read(remotefile, buf, count);
            readpos += ret;
        }
        else if (dvdPriv)
        {                        
            ret = dvdPriv->safe_read(buf, count);
            readpos += ret;
        }
        else
        {
            ret = safe_read(fd2, buf, count);
            readpos += ret;
        }
    }
    else
    {
        ret = ReadFromBuf(buf, count);
        readpos += ret;
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
    if (!writemode)
    {
        fprintf(stderr, "Attempt to write to a read only file\n");
        return ret;
    }

    if (!tfw)
        return ret;

    pthread_rwlock_rdlock(&rwlock);

    ret = tfw->Write(buf, count);
    writepos += ret;

    pthread_rwlock_unlock(&rwlock);
    return ret;
}

void RingBuffer::Sync(void)
{
    if (tfw)
        tfw->Sync();
}

long long RingBuffer::Seek(long long pos, int whence)
{
    wantseek = true;
    pthread_rwlock_wrlock(&rwlock);
    wantseek = false;

    long long ret = -1;
    if (remotefile)
        ret = remotefile->Seek(pos, whence, readpos);
    else if (dvdPriv)
    {
        dvdPriv->Seek(pos, whence);
    }
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
        writepos = ret;
    }

    return ret;
}

void RingBuffer::WriterFlush(void)
{
    if (tfw)
    {
        tfw->Flush();
        tfw->Sync();
    }
}

void RingBuffer::SetWriteBufferSize(int newSize)
{
    tfw->SetWriteBufferSize(newSize);
}

void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    tfw->SetWriteBufferMinWriteSize(newMinSize);
}

long long RingBuffer::GetReadPosition(void)
{
    if (dvdPriv)
    {
        return dvdPriv->GetReadPosition();
    }
    else
    {
        return readpos;
    }
}

long long RingBuffer::GetWritePosition(void)
{
    return writepos;
}

long long RingBuffer::GetRealFileSize(void)
{
    if (remotefile)
        return remotefile->GetFileSize();

    struct stat st;
    if (stat(filename.ascii(), &st) == 0)
        return st.st_size;
    return -1;
}

bool RingBuffer::LiveMode(void)
{
    return (livetvchain);
}

void RingBuffer::SetLiveMode(LiveTVChain *chain)
{
    livetvchain = chain;
}

void RingBuffer::getPartAndTitle(int& title, int& part)
{
    if (dvdPriv)
        dvdPriv->GetPartAndTitle(title, part);
}

void RingBuffer::getDescForPos(QString& desc)
{
    if (dvdPriv)
        dvdPriv->GetDescForPos(desc);
}

void RingBuffer::nextTrack()
{
   if (dvdPriv)
       dvdPriv->nextTrack();
}

void RingBuffer::prevTrack()
{
   if (dvdPriv)
       dvdPriv->prevTrack();
}

