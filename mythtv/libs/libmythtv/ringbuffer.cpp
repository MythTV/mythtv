// ANSI C headers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

// POSIX C headers
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

// Qt headers
#include <QFile>
#include <QDateTime>

#include "ThreadedFileWriter.h"
#include "fileringbuffer.h"
#include "dvdringbuffer.h"
#include "bdringbuffer.h"
#include "streamingringbuffer.h"
#include "livetvchain.h"
#include "mythcontext.h"
#include "ringbuffer.h"
#include "mythconfig.h"
#include "remotefile.h"
#include "compat.h"
#include "util.h"

#if HAVE_POSIX_FADVISE < 1
static int posix_fadvise(int, off_t, off_t, int) { return 0; }
#define POSIX_FADV_SEQUENTIAL 0
#define POSIX_FADV_WILLNEED 0
#define POSIX_FADV_DONTNEED 0
#endif

// about one second at 35mbit
const uint RingBuffer::kBufferSize = 4 * 1024 * 1024;
const int  RingBuffer::kDefaultOpenTimeout = 2000; // ms
const int  RingBuffer::kLiveTVOpenTimeout  = 10000;

#define CHUNK 32768 /* readblocksize increments */

#define LOC      QString("RingBuf(%1): ").arg(filename)
#define LOC_WARN QString("RingBuf(%1) Warning: ").arg(filename)
#define LOC_ERR  QString("RingBuf(%1) Error: ").arg(filename)

#define PNG_MIN_SIZE   20 /* header plus one empty chunk */
#define NUV_MIN_SIZE  204 /* header size? */
#define MPEG_MIN_SIZE 376 /* 2 TS packets */

/* should be minimum of the above test sizes */
const uint RingBuffer::kReadTestSize = PNG_MIN_SIZE;

QMutex      RingBuffer::subExtLock;
QStringList RingBuffer::subExt;
QStringList RingBuffer::subExtNoCheck;

/*
  Locking relations:
    rwlock->poslock->rbrlock->rbwlock

  A child should never lock any of the parents without locking
  the parent lock before the child lock.
  void RingBuffer::Example1()
  {
      poslock.lockForWrite();
      rwlock.lockForRead(); // error!
      blah(); // <- does not implicitly aquire any locks
      rwlock.unlock();
      poslock.unlock();
  }
  void RingBuffer::Example2()
  {
      rwlock.lockForRead();
      rbrlock.lockForWrite(); // ok!
      blah(); // <- does not implicitly aquire any locks
      rbrlock.unlock();
      rwlock.unlock();
  }
*/

/** \class RingBuffer
 *  \brief Implements a file/stream reader/writer.
 *
 *  This class, despite its name, no-longer provides a ring buffer.
 *  It can buffer reads and provide support for streaming files.
 *  It also provides a wrapper for the ThreadedFileWriter which
 *  makes sure that the file reader does not read past where the
 *  wrapped TFW has written new data.
 *
 */

/** \brief Creates a RingBuffer instance.
 *
 *   You can explicitly disable the readahead thread by setting
 *   readahead to false, or by just not calling Start(void).
 *
 *  \param lfilename    Name of file to read or write.
 *  \param write        If true an encapsulated writer is created
 *  \param readahead    If false a call to Start(void) will not
 *                      a pre-buffering thread, otherwise Start(void)
 *                      will start a pre-buffering thread.
 *  \param timeout_ms   if < 0, then we will not open the file.
 *                      Otherwise it's how long to try opening 
 *                      the file after the first failure in
 *                      milliseconds before giving up.
 */
RingBuffer *RingBuffer::Create(
    const QString &xfilename, bool write,
    bool usereadahead, int timeout_ms, bool stream_only)
{
    QString lfilename = xfilename;

    if (write)
    {
        return new FileRingBuffer(
            lfilename, write, usereadahead, timeout_ms);
    }

    bool is_dvd = false;
    bool is_bd  = false;

    if (lfilename.startsWith("http://") ||
        lfilename.contains(QRegExp("^rtmp.?://")))
    {
        return new StreamingRingBuffer(lfilename);
    }

    if (!stream_only && lfilename.startsWith("myth://"))
    {
        struct stat fileInfo;
        if ((RemoteFile::Exists(lfilename, &fileInfo)) &&
            (S_ISDIR(fileInfo.st_mode)))
        {
            QString tmpFile = lfilename + "/VIDEO_TS";
            if (RemoteFile::Exists(tmpFile))
            {
                is_dvd = true;
            }
            else
            {
                tmpFile = lfilename + "/BDMV";
                if (RemoteFile::Exists(tmpFile))
                    is_bd = true;
            }
        }
    }

    if ((lfilename.left(1) == "/") || (QFile::exists(lfilename)))
    {
    }
    else if ((!stream_only) &&
             ((lfilename.startsWith("dvd:")) || is_dvd ||
              ((lfilename.startsWith("myth://")) &&
               ((lfilename.endsWith(".img")) ||
                (lfilename.endsWith(".iso"))))))
    {
        is_dvd = true;

        if (lfilename.left(6) == "dvd://")     // 'Play DVD' sends "dvd:/" + dev
            lfilename.remove(0,5);             // e.g. "dvd://dev/sda"
        else if (lfilename.left(5) == "dvd:/") // Less correct URI "dvd:" + path
            lfilename.remove(0,4);             // e.g. "dvd:/videos/ET"
        else if (lfilename.left(4) == "dvd:")   // Win32 URI "dvd:" + abs path
            lfilename.remove(0,4);              //             e.g. "dvd:D:\"

        if (QFile::exists(lfilename) || lfilename.startsWith("myth://"))
        {
            VERBOSE(VB_PLAYBACK, "Trying DVD at " + lfilename);
        }
        else
        {
            lfilename = "/dev/dvd";
        }

        return new DVDRingBuffer(lfilename);
    }
    else if ((!stream_only) && (lfilename.left(3) == "bd:" || is_bd))
    {
        is_bd = true;

        if (lfilename.left(5) == "bd://")      // 'Play DVD' sends "bd:/" + dev
            lfilename.remove(0,4);             // e.g. "bd://dev/sda"
        else if (lfilename.left(4) == "bd:/")  // Less correct URI "bd:" + path
            lfilename.remove(0,3);             // e.g. "bd:/videos/ET"

        if (QFile::exists(lfilename) || lfilename.startsWith("myth://"))
        {
            VERBOSE(VB_PLAYBACK, "Trying BD at " + lfilename);
        }
        else
        {
            lfilename = "/dev/dvd";
        }

        return new BDRingBuffer(lfilename);
    }

    return new FileRingBuffer(
        lfilename, write, usereadahead, timeout_ms);
}

RingBuffer::RingBuffer(void) :
    readpos(0),               writepos(0),
    internalreadpos(0),       ignorereadpos(-1),
    rbrpos(0),                rbwpos(0),
    stopreads(false),
    filename(),               subtitlefilename(),
    tfw(NULL),                fd2(-1),
    writemode(false),         remotefile(NULL),
    startreadahead(false),    readAheadBuffer(NULL),
    readaheadrunning(false),  reallyrunning(false),
    request_pause(false),     paused(false),
    ateof(false),             readsallowed(false),
    setswitchtonext(false),
    rawbitrate(8000),         playspeed(1.0f),
    fill_threshold(65536),    fill_min(-1),
    readblocksize(CHUNK),     wanttoread(0),
    numfailures(0),           commserror(false),
    oldfile(false),           livetvchain(NULL),
    ignoreliveeof(false),     readAdjust(0)
{
    {
        QMutexLocker locker(&subExtLock);
        if (subExt.empty())
        {
            // Possible subtitle file extensions '.srt', '.sub', '.txt'
            subExt += ".srt";
            subExt += ".sub";
            subExt += ".txt";

            // Extensions for which a subtitle file should not exist
            subExtNoCheck = subExt;
            subExtNoCheck += ".gif";
            subExtNoCheck += ".png";
        }
    }
}

/** \fn RingBuffer::~RingBuffer(void)
 *  \brief Shuts down any threads and closes any files.
 */
RingBuffer::~RingBuffer(void)
{
    KillReadAheadThread();

    if (readAheadBuffer) // this only runs if thread is terminated
    {
        delete [] readAheadBuffer;
        readAheadBuffer = NULL;
    }
}

/** \fn RingBuffer::Reset(bool, bool, bool)
 *  \brief Resets the read-ahead thread and our position in the file
 */
void RingBuffer::Reset(bool full, bool toAdjust, bool resetInternal)
{
    VERBOSE(VB_FILE, LOC + QString("Reset(%1,%2,%3)")
            .arg(full).arg(toAdjust).arg(resetInternal));

    rwlock.lockForWrite();
    poslock.lockForWrite();

    numfailures = 0;
    commserror = false;
    setswitchtonext = false;

    writepos = 0;
    readpos = (toAdjust) ? (readpos - readAdjust) : 0;

    if (readpos != 0)
    {
        VERBOSE(VB_IMPORTANT, LOC + QString(
                "RingBuffer::Reset() nonzero readpos.  toAdjust: %1 readpos: %2"
                " readAdjust: %3").arg(toAdjust).arg(readpos).arg(readAdjust));
    }

    readAdjust = 0;
    readpos = (readpos < 0) ? 0 : readpos;

    if (full)
        ResetReadAhead(readpos);

    if (resetInternal)
        internalreadpos = readpos;

    generalWait.wakeAll();
    poslock.unlock();
    rwlock.unlock();
}

/** \fn RingBuffer::UpdateRawBitrate(uint)
 *  \brief Set the raw bit rate, to allow RingBuffer adjust effective bitrate.
 *  \param raw_bitrate Streams average number of kilobits per second when
 *                     playspeed is 1.0
 */
void RingBuffer::UpdateRawBitrate(uint raw_bitrate)
{
    VERBOSE(VB_FILE, LOC + QString("UpdateRawBitrate(%1Kb)").arg(raw_bitrate));
    if (raw_bitrate < 2500)
    {
        VERBOSE(VB_FILE, LOC +
                QString("UpdateRawBitrate(%1Kb) - ignoring bitrate,")
                .arg(raw_bitrate) +
                "\n\t\t\tappears to be abnormally low.");
        return;
    }

    rwlock.lockForWrite();
    rawbitrate = raw_bitrate;
    CalcReadAheadThresh();
    rwlock.unlock();
}

/** \fn RingBuffer::UpdatePlaySpeed(float)
 *  \brief Set the play speed, to allow RingBuffer adjust effective bitrate.
 *  \param play_speed Speed to set. (1.0 for normal speed)
 */
void RingBuffer::UpdatePlaySpeed(float play_speed)
{
    rwlock.lockForWrite();
    playspeed = play_speed;
    CalcReadAheadThresh();
    rwlock.unlock();
}

/** \fn RingBuffer::CalcReadAheadThresh(void)
 *  \brief Calculates fill_min, fill_threshold, and readblocksize
 *         from the estimated effective bitrate of the stream.
 *
 *   WARNING: Must be called with rwlock in write lock state.
 *
 */
void RingBuffer::CalcReadAheadThresh(void)
{
    uint estbitrate = 0;

    readsallowed   = false;
    readblocksize  = max(readblocksize, CHUNK);

    // loop without sleeping if the buffered data is less than this
    fill_threshold = kBufferSize / 8;

    const uint KB32  =  32*1024;
    const uint KB64  =  64*1024;
    const uint KB128 = 128*1024;
    const uint KB256 = 256*1024;
    const uint KB512 = 512*1024;

    estbitrate     = (uint) max(abs(rawbitrate * playspeed),
                                0.5f * rawbitrate);
    estbitrate     = min(rawbitrate * 3, estbitrate);
    int rbs        = (estbitrate > 2500)  ? KB64  : KB32;
    rbs            = (estbitrate > 5000)  ? KB128 : rbs;
    rbs            = (estbitrate > 9000)  ? KB256 : rbs;
    rbs            = (estbitrate > 18000) ? KB512 : rbs;
    readblocksize  = max(rbs,readblocksize);

    // minumum seconds of buffering before allowing read
    float secs_min = 0.25;
    // set the minimum buffering before allowing ffmpeg read
    fill_min        = (uint) ((estbitrate * secs_min) * 0.125f);
    // make this a multiple of ffmpeg block size..
    fill_min        = ((fill_min / KB32) + 1) * KB32;

    VERBOSE(VB_FILE, LOC +
            QString("CalcReadAheadThresh(%1 Kb)\n\t\t\t -> "
                    "threshhold(%2 KB) min read(%3 KB) blk size(%4 KB)")
            .arg(estbitrate).arg(fill_threshold/1024)
            .arg(fill_min/1024).arg(readblocksize/1024));
}

bool RingBuffer::IsNearEnd(double fps, uint vvf) const
{
    rwlock.lockForRead();
    int    sz  = ReadBufAvail();
    uint   rbs = readblocksize;
    // telecom kilobytes (i.e. 1000 per k not 1024)
    uint   tmp = (uint) max(abs(rawbitrate * playspeed), 0.5f * rawbitrate);
    uint   kbits_per_sec = min(rawbitrate * 3, tmp);
    rwlock.unlock();

    // WARNING: readahead_frames can greatly overestimate or underestimate
    //          the number of frames available in the read ahead buffer
    //          when rh_frames is less than the keyframe distance.
    double bytes_per_frame = kbits_per_sec * (1000.0/8.0) / fps;
    double readahead_frames = sz / bytes_per_frame;

    bool near_end = ((vvf + readahead_frames) < 10.0) || (sz < rbs*1.5);

    VERBOSE(VB_PLAYBACK, LOC + "IsReallyNearEnd()"
            <<" br("<<(kbits_per_sec/8)<<"KB)"
            <<" sz("<<(sz / 1000)<<"KB)"
            <<" vfl("<<vvf<<")"
            <<" frh("<<((uint)readahead_frames)<<")"
            <<" ne:"<<near_end);

    return near_end;
}

/// \brief Returns number of bytes available for reading into buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufFree(void) const
{
    rbrlock.lockForRead();
    rbwlock.lockForRead();
    int ret = ((rbwpos >= rbrpos) ? rbrpos + kBufferSize : rbrpos) - rbwpos - 1;
    rbwlock.unlock();
    rbrlock.unlock();
    return ret;
}

/// \brief Returns number of bytes available for reading from buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufAvail(void) const
{
    rbrlock.lockForRead();
    rbwlock.lockForRead();
    int ret = (rbwpos >= rbrpos) ? rbwpos - rbrpos : kBufferSize - rbrpos + rbwpos;
    rbwlock.unlock();
    rbrlock.unlock();
    return ret;
}

/** \fn RingBuffer::ResetReadAhead(long long)
 *  \brief Restart the read-ahead thread at the 'newinternal' position.
 *
 *   This is called after a Seek(long long, int) so that the read-ahead
 *   buffer doesn't contain any stale data, and so that it will read
 *   any new data from the new position in the file.
 *
 *   WARNING: Must be called with rwlock and poslock in write lock state.
 *
 *  \param newinternal Position in file to start reading data from
 */
void RingBuffer::ResetReadAhead(long long newinternal)
{
    VERBOSE(VB_FILE, LOC + QString("ResetReadAhead(internalreadpos = %1->%2)")
            .arg(internalreadpos).arg(newinternal));

    rbrlock.lockForWrite();
    rbwlock.lockForWrite();

    CalcReadAheadThresh();
    rbrpos = 0;
    rbwpos = 0;
    internalreadpos = newinternal;
    ateof = false;
    readsallowed = false;
    setswitchtonext = false;
    generalWait.wakeAll();

    rbwlock.unlock();
    rbrlock.unlock();
}

/**
 *  \brief Starts the read-ahead thread.
 *
 *   If the RingBuffer constructor was not called with a usereadahead
 *   of true of if this was reset to false because we're dealing with
 *   a DVD the read ahead thread will not be started.
 *
 *   If this RingBuffer is in write-mode a warning will be printed and
 *   the read ahead thread will not be started.
 *
 *   If the read ahead thread is already running a warning will be printed
 *   and the read ahead thread will not be started.
 *
 */
void RingBuffer::Start(void)
{
    bool do_start = true;

    rwlock.lockForWrite();
    if (!startreadahead)
    {
        do_start = false;
    }
    else if (writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Not starting read ahead thread, "
                "this is a write only RingBuffer");
        do_start = false;
    }
    else if (readaheadrunning)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Not starting read ahead thread, "
                "already running");
        do_start = false;
    }

    if (!do_start)
    {
        rwlock.unlock();
        return;
    }

    StartReads();

    QThread::start();

    while (readaheadrunning && !reallyrunning)
        generalWait.wait(&rwlock);

    rwlock.unlock();
}

/** \fn RingBuffer::KillReadAheadThread(void)
 *  \brief Stops the read-ahead thread, and waits for it to stop.
 */
void RingBuffer::KillReadAheadThread(void)
{
    while (isRunning())
    {
        rwlock.lockForWrite();
        readaheadrunning = false;
        StopReads();
        generalWait.wakeAll();
        rwlock.unlock();
        QThread::wait(5000);
    }
}

/** \fn RingBuffer::StopReads(void)
 *  \brief ????
 *  \sa StartReads(void), Pause(void)
 */
void RingBuffer::StopReads(void)
{
    stopreads = true;
    generalWait.wakeAll();
}

/** \fn RingBuffer::StartReads(void)
 *  \brief ????
 *  \sa StopReads(void), Unpause(void)
 */
void RingBuffer::StartReads(void)
{
    stopreads = false;
    generalWait.wakeAll();
}

/** \fn RingBuffer::Pause(void)
 *  \brief Pauses the read-ahead thread. Calls StopReads(void).
 *  \sa Unpause(void), WaitForPause(void)
 */
void RingBuffer::Pause(void)
{
    StopReads();

    rwlock.lockForWrite();
    request_pause = true;
    rwlock.unlock();
}

/** \fn RingBuffer::Unpause(void)
 *  \brief Unpauses the read-ahead thread. Calls StartReads(void).
 *  \sa Pause(void)
 */
void RingBuffer::Unpause(void)
{
    StartReads();

    rwlock.lockForWrite();
    request_pause = false;
    generalWait.wakeAll();
    rwlock.unlock();
}

/// Returns false iff read-ahead is not running and read-ahead is not paused.
bool RingBuffer::isPaused(void) const
{
    rwlock.lockForRead();
    bool ret = !readaheadrunning || paused;
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::WaitForPause(void)
 *  \brief Waits for Pause(void) to take effect.
 */
void RingBuffer::WaitForPause(void)
{
    MythTimer t;
    t.start();

    rwlock.lockForRead();
    while (readaheadrunning && !paused && request_pause)
    {
        generalWait.wait(&rwlock, 1000);
        if (readaheadrunning && !paused && request_pause && t.elapsed() > 1000)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    QString("Waited %1 ms for ringbuffer pause..")
                    .arg(t.elapsed()));
        }
    }
    rwlock.unlock();
}

bool RingBuffer::PauseAndWait(void)
{
    const uint timeout = 500; // ms

    if (request_pause)
    {
        if (!paused)
        {
            rwlock.unlock();
            rwlock.lockForWrite();

            if (request_pause)
            {
                paused = true;
                generalWait.wakeAll();
            }

            rwlock.unlock();
            rwlock.lockForRead();
        }

        if (request_pause && paused && readaheadrunning)
            generalWait.wait(&rwlock, timeout);
    }

    if (!request_pause && paused)
    {
        rwlock.unlock();
        rwlock.lockForWrite();

        if (!request_pause)
        {
            paused = false;
            generalWait.wakeAll();
        }

        rwlock.unlock();
        rwlock.lockForRead();
    }

    return request_pause || paused;
}

void RingBuffer::run(void)
{
    // These variables are used to adjust the read block size
    struct timeval lastread, now;
    int readtimeavg = 300;
    bool ignore_for_read_timing = true;

    gettimeofday(&lastread, NULL); // this is just to keep gcc happy

    rwlock.lockForWrite();
    poslock.lockForWrite();
    request_pause = false;
    readAheadBuffer = new char[kBufferSize + 1024];
    ResetReadAhead(0);
    readaheadrunning = true;
    reallyrunning = true;
    generalWait.wakeAll();
    poslock.unlock();
    rwlock.unlock();

    // NOTE: this must loop at some point hold only
    // a read lock on rwlock, so that other functions
    // such as reset and seek can take priority.

    rwlock.lockForRead();

    VERBOSE(VB_FILE, LOC + QString("Initial readblocksize %1K & fill_min %2K")
            .arg(readblocksize/1024).arg(fill_min/1024));

    while (readaheadrunning)
    {
        if (PauseAndWait())
        {
            ignore_for_read_timing = true;
            continue;
        }

        long long totfree = ReadBufFree();

        // These are conditions where we don't want to go through
        // the loop if they are true.
        if (((totfree < readblocksize) && readsallowed) ||
            (ignorereadpos >= 0) || commserror || stopreads)
        {
            ignore_for_read_timing |=
                (ignorereadpos >= 0) || commserror || stopreads;
            generalWait.wait(&rwlock, (stopreads) ? 50 : 1000);
            continue;
        }

        // These are conditions where we want to sleep to allow
        // other threads to do stuff.
        if (setswitchtonext || (ateof && readsallowed))
        {
            ignore_for_read_timing = true;
            generalWait.wait(&rwlock, 1000);
            totfree = ReadBufFree();
        }

        int read_return = -1;
        if (totfree >= readblocksize && !commserror &&
            !ateof && !setswitchtonext)
        {
            // limit the read size
            totfree = readblocksize;

            // adapt blocksize
            gettimeofday(&now, NULL);
            if (!ignore_for_read_timing)
            {
                int readinterval = (now.tv_sec  - lastread.tv_sec ) * 1000 +
                    (now.tv_usec - lastread.tv_usec) / 1000;
                readtimeavg = (readtimeavg * 9 + readinterval) / 10;

                if (readtimeavg < 150 && (uint)readblocksize < (kBufferSize>>2))
                {
                    int old_block_size = readblocksize;
                    readblocksize = 3 * readblocksize / 2;
                    readblocksize = ((readblocksize+CHUNK-1) / CHUNK) * CHUNK;
                    VERBOSE(VB_FILE, LOC +
                            QString("Avg read interval was %1 msec. "
                                    "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg(old_block_size/1024)
                            .arg(readblocksize/1024));
                    readtimeavg = 225;
                }
                else if (readtimeavg > 300 && readblocksize > CHUNK)
                {
                    readblocksize -= CHUNK;
                    VERBOSE(VB_FILE, LOC +
                            QString("Avg read interval was %1 msec. "
                                    "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg((readblocksize+CHUNK)/1024)
                            .arg(readblocksize/1024));
                    readtimeavg = 225;
                }
            }
            ignore_for_read_timing = false;
            lastread = now;

            rbwlock.lockForRead();
            if (rbwpos + totfree > kBufferSize)
            {
                totfree = kBufferSize - rbwpos;
                VERBOSE(VB_FILE|VB_EXTRA, LOC +
                        "Shrinking read, near end of buffer");
            }

            if (internalreadpos == 0)
            {
                totfree = max(fill_min, readblocksize);
                VERBOSE(VB_FILE|VB_EXTRA, LOC +
                        "Reading enough data to start playback");
            }

            if (remotefile && livetvchain && livetvchain->HasNext())
                remotefile->SetTimeout(true);

            VERBOSE(VB_FILE|VB_EXTRA,
                    LOC + QString("safe_read(...@%1, %2) -- begin")
                    .arg(rbwpos).arg(totfree));

            read_return = safe_read(readAheadBuffer + rbwpos, totfree);

            VERBOSE(VB_FILE|VB_EXTRA, LOC +
                    QString("safe_read(...@%1, %2) -> %3")
                    .arg(rbwpos).arg(totfree).arg(read_return));

            rbwlock.unlock();
        }

        if (read_return >= 0)
        {
            poslock.lockForWrite();
            rbwlock.lockForWrite();
            internalreadpos += read_return;
            off_t donotneed = internalreadpos;
            rbwpos = (rbwpos + read_return) % kBufferSize;
            VERBOSE(VB_FILE|VB_EXTRA,
                    LOC + QString("rbwpos += %1K requested %2K in read")
                    .arg(read_return/1024,3).arg(totfree/1024,3));
            rbwlock.unlock();
            poslock.unlock();

            if (fd2 >=0 && donotneed > 0)
                posix_fadvise(fd2, 0, donotneed, POSIX_FADV_DONTNEED);
        }

        int used = kBufferSize - ReadBufFree();

        if ((0 == read_return) || (numfailures > 5) ||
            (readsallowed != (used >= fill_min || ateof ||
                              setswitchtonext || commserror)))
        {
            // If readpos changes while the lock is released
            // we should not handle the 0 read_return now.
            long long old_readpos = readpos;

            rwlock.unlock();
            rwlock.lockForWrite();

            commserror |= (numfailures > 5);

            readsallowed = used >= fill_min || ateof ||
                setswitchtonext || commserror;

            if (0 == read_return && old_readpos == readpos)
            {
                if (livetvchain)
                {
                    if (!setswitchtonext && !ignoreliveeof &&
                        livetvchain->HasNext())
                    {
                        livetvchain->SwitchToNext(true);
                        setswitchtonext = true;
                    }
                }
                else
                {
                    VERBOSE(VB_FILE|VB_EXTRA,
                            LOC + "setting ateof (read_return == 0)");
                    ateof = true;
                }
            }

            rwlock.unlock();
            rwlock.lockForRead();
            used = kBufferSize - ReadBufFree();
        }

        VERBOSE(VB_FILE|VB_EXTRA, LOC + "@ end of read ahead loop");

        if (readsallowed || commserror || ateof || setswitchtonext ||
            (wanttoread <= used && wanttoread > 0))
        {
            // To give other threads a good chance to handle these
            // conditions, even if they are only requesting a read lock
            // like us, yield (currently implemented with short usleep).
            generalWait.wakeAll();
            rwlock.unlock();
            usleep(5 * 1000);
            rwlock.lockForRead();            
        }
        else
        {
            // yield if we have nothing to do...
            if (!request_pause &&
                (used >= fill_threshold || ateof || setswitchtonext))
            {
                generalWait.wait(&rwlock, 1000);
            }
        }
    }

    rwlock.unlock();

    rwlock.lockForWrite();
    rbrlock.lockForWrite();
    rbwlock.lockForWrite();

    rbrpos = 0;
    rbwpos = 0;
    reallyrunning = false;
    readsallowed = false;
    delete [] readAheadBuffer;

    readAheadBuffer = NULL;
    rbwlock.unlock();
    rbrlock.unlock();
    rwlock.unlock();
}

long long RingBuffer::SetAdjustFilesize(void)
{
    rwlock.lockForWrite();
    poslock.lockForRead();
    readAdjust += internalreadpos;
    long long ra = readAdjust;
    poslock.unlock();
    rwlock.unlock();
    return ra;
}

int RingBuffer::Peek(void *buf, int count)
{
    int ret = ReadPriv(buf, count, true);
    if (ret != count)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                QString("Peek() requested %1 bytes, but only returning %2")
                .arg(count).arg(ret));
    }
    return ret;
}

bool RingBuffer::WaitForReadsAllowed(void)
{
    MythTimer t;
    t.start();

    while (!readsallowed && !stopreads &&
           !request_pause && !commserror && readaheadrunning)
    {
        generalWait.wait(&rwlock, 1000);
        if (!readsallowed && t.elapsed() > 1000)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Taking too long to be allowed to read..");

            if (t.elapsed() > 10000)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR + "Took more than "
                        "10 seconds to be allowed to read, aborting.");
                return false;
            }
        }
    }

    return readsallowed;
}

bool RingBuffer::WaitForAvail(int count)
{
    int avail = ReadBufAvail();
    count = (ateof && avail < count) ? avail : count;

    MythTimer t;
    t.start();
    while ((avail < count) && !stopreads &&
           !request_pause && !commserror && readaheadrunning)
    {
        wanttoread = count;
        generalWait.wait(&rwlock, 250);
        avail = ReadBufAvail();

        if ((ateof || setswitchtonext) && avail < count)
            count = avail;

        if (avail < count)
        {
            int elapsed = t.elapsed();
            if  (((elapsed > 250)  && (elapsed < 500))  ||
                 ((elapsed > 500)  && (elapsed < 750))  ||
                 ((elapsed > 1000) && (elapsed < 1250)) ||
                 ((elapsed > 2000) && (elapsed < 2250)) ||
                 ((elapsed > 4000) && (elapsed < 4250)) ||
                 ((elapsed > 8000) && (elapsed < 8250)) ||
                 ((elapsed > 9000)))
            {
                VERBOSE(VB_IMPORTANT, LOC + "Waited " +
                        QString("%1").arg((elapsed / 250) * 0.25f, 3, 'f', 1) +
                        " seconds for data \n\t\t\tto become available..." +
                        QString(" %2 < %3")
                        .arg(avail).arg(count));
                if (livetvchain)
                {
                    VERBOSE(VB_IMPORTANT, "Checking to see if there's a "
                                          "new livetv program to switch to..");
                    livetvchain->ReloadAll();
                }
            }

            bool quit = livetvchain && (livetvchain->NeedsToSwitch() ||
                                        livetvchain->NeedsToJump() ||
                                        setswitchtonext);

            if (elapsed > 16000 || quit)
            {
                if (!quit)
                    VERBOSE(VB_IMPORTANT, LOC_ERR + "Waited " +
                            QString("%1").arg(elapsed/1000) +
                            " seconds for data, aborting.");
                else
                    VERBOSE(VB_IMPORTANT, LOC + "Timing out wait due to "
                            "impending livetv switch.");

                return false;
            }
        }
    }

    wanttoread = 0;

    return avail >= count;
}

int RingBuffer::ReadDirect(void *buf, int count, bool peek)
{
    long long old_pos = 0;
    if (peek)
    {
        poslock.lockForRead();
        old_pos = (ignorereadpos >= 0) ? ignorereadpos : readpos;
        poslock.unlock();
    }

    int ret = safe_read(buf, count);

    poslock.lockForWrite();
    if (ignorereadpos >= 0 && ret > 0)
    {
        if (peek)
        {
            // seek should always succeed since we were at this position
            if (remotefile)
                remotefile->Seek(old_pos, SEEK_SET);
            else
            {
                lseek64(fd2, old_pos, SEEK_SET);
                posix_fadvise(fd2, old_pos, 1*1024*1024, POSIX_FADV_WILLNEED);
            }
        }
        else
        {
            ignorereadpos += ret;
        }
        poslock.unlock();
        return ret;
    }
    poslock.unlock();

    if (peek && ret > 0)
    {
        if ((IsDVD() || IsBD()) && old_pos != 0)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "DVD and Blu-Ray do not support arbitrary "
                    "peeks except when read-ahead is enabled."
                    "\n\t\t\tWill seek to beginning of video.");
            old_pos = 0;
        }

        long long new_pos = Seek(old_pos, SEEK_SET, true);

        if (new_pos != old_pos)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Peek() Failed to return from new "
                            "position %1 to old position %2, now "
                            "at position %3")
                    .arg(old_pos - ret).arg(old_pos).arg(new_pos));
        }
    }

    return ret;
}

/** \brief When possible reads from the read-ahead buffer,
 *         otherwise reads directly from the device.
 *
 *  \param buf   Pointer to where data will be written
 *  \param count Number of bytes to read
 *  \param peek  If true, don't increment read count
 *  \return Returns number of bytes read
 */
int RingBuffer::ReadPriv(void *buf, int count, bool peek)
{
    QString loc_desc = 
            QString("ReadPriv(..%1, %2)")
        .arg(count).arg(peek?"peek":"normal");
    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc +
            QString(" @%1 -- begin").arg(rbrpos));

    rwlock.lockForRead();
    if (writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + loc_desc +
                ": Attempt to read from a write only file");
        errno = EBADF;
        rwlock.unlock();
        return -1;
    }

    if (commserror)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + loc_desc +
                ": Attempt to read after commserror set");
        errno = EIO;
        rwlock.unlock();
        return -1;
    }

    if (request_pause || stopreads || !readaheadrunning || (ignorereadpos>=0))
    {
        rwlock.unlock();
        rwlock.lockForWrite();
        // we need a write lock so the read-ahead thread
        // can't start mucking with the read position.
        // If the read ahead thread was started while we
        // didn't hold the lock, we proceed with a normal
        // read from the buffer, otherwise we read directly.
        if (request_pause || stopreads ||
            !readaheadrunning || (ignorereadpos >= 0))
        {
            int ret = ReadDirect(buf, count, peek);
            VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc +
                    QString(": ReadDirect checksum %1")
                    .arg(qChecksum((char*)buf,count)));
            rwlock.unlock();
            return ret;
        }
        rwlock.unlock();
        rwlock.lockForRead();
    }

    if (!WaitForReadsAllowed())
    {
        VERBOSE(VB_FILE, LOC + loc_desc + ": !WaitForReadsAllowed()");
        rwlock.unlock();
        rwlock.lockForWrite();
        wanttoread = 0;
        stopreads = true;
        rwlock.unlock();
        return 0;
    }

    if (!WaitForAvail(count))
    {
        VERBOSE(VB_FILE, LOC + loc_desc + ": !WaitForAvail()");
        rwlock.unlock();
        rwlock.lockForWrite();
        ateof = true;
        wanttoread = 0;
        stopreads = true;
        rwlock.unlock();
        return 0;
    }

    count = min(ReadBufAvail(), count);

    if (count <= 0)
    {
        // this can happen under a few conditions but the most
        // notable is an exit from the read ahead thread or
        // the end of the file stream has been reached.
        VERBOSE(VB_FILE, LOC + loc_desc + ": ReadBufAvail() == 0");
        rwlock.unlock();
        return count;
    }

    if (peek)
        rbrlock.lockForRead();
    else
        rbrlock.lockForWrite();

    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc + " -- copying data");

    if (rbrpos + count > (int) kBufferSize)
    {
        int firstsize = kBufferSize - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
    {
        memcpy(buf, readAheadBuffer + rbrpos, count);
    }
    VERBOSE(VB_FILE|VB_EXTRA, LOC + loc_desc + QString(" -- checksum %1")
            .arg(qChecksum((char*)buf,count)));

    if (!peek)
    {
        rbrpos = (rbrpos + count) % kBufferSize;
        generalWait.wakeAll();
    }
    rbrlock.unlock();
    rwlock.unlock();

    return count;
}

/** \fn RingBuffer::Read(void*, int)
 *  \brief This is the public method for reading from a file,
 *         it calls the appropriate read method if the file
 *         is remote or buffered, or a BD/DVD.
 *  \param buf   Pointer to where data will be written
 *  \param count Number of bytes to read
 *  \return Returns number of bytes read
 */
int RingBuffer::Read(void *buf, int count)
{
    int ret = ReadPriv(buf, count, false);
    if (ret > 0)
    {
        poslock.lockForWrite();
        readpos += ret;
        poslock.unlock();
    }
    return ret;
}

/** \fn RingBuffer::IsIOBound(void) const
 *  \brief Returns true if a RingBuffer::Write(void*,int) is likely to block.
 */
bool RingBuffer::IsIOBound(void) const
{
    bool ret = false;
    int used, free;
    rwlock.lockForRead();

    if (!tfw)
    {
        rwlock.unlock();
        return ret;
    }

    used = tfw->BufUsed();
    free = tfw->BufFree();

    ret = (used * 5 > free);

    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::Write(const void*, uint)
 *  \brief Writes buffer to ThreadedFileWriter::Write(const void*,uint)
 *  \return Bytes written, or -1 on error.
 */
int RingBuffer::Write(const void *buf, uint count)
{
    rwlock.lockForRead();

    if (!writemode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Tried to write to a read only file.");
        rwlock.unlock();
        return -1;
    }

    if (!tfw && !remotefile)
    {
        rwlock.unlock();
        return -1;
    }

    int ret = -1;
    if (tfw)
        ret = tfw->Write(buf, count);
    else
        ret = remotefile->Write(buf, count);

    if (ret > 0)
    {
        poslock.lockForWrite();
        writepos += ret;
        poslock.unlock();
    }

    rwlock.unlock();

    return ret;
}

/** \fn RingBuffer::Sync(void)
 *  \brief Calls ThreadedFileWriter::Sync(void)
 */
void RingBuffer::Sync(void)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->Sync();
    rwlock.unlock();
}

/** \brief Calls ThreadedFileWriter::Seek(long long,int).
 */
long long RingBuffer::WriterSeek(long long pos, int whence, bool has_lock)
{
    long long ret = -1;

    if (!has_lock)
        rwlock.lockForRead();

    poslock.lockForWrite();

    if (tfw)
    {
        ret = tfw->Seek(pos, whence);
        writepos = ret;
    }

    poslock.unlock();

    if (!has_lock)
        rwlock.unlock();

    return ret;
}

/** \fn RingBuffer::WriterFlush(void)
 *  \brief Calls ThreadedFileWriter::Flush(void) and
 *         ThreadedFileWriter::Sync(void).
 */
void RingBuffer::WriterFlush(void)
{
    rwlock.lockForRead();
    if (tfw)
    {
        tfw->Flush();
        tfw->Sync();
    }
    rwlock.unlock();
}

/** \fn RingBuffer::SetWriteBufferSize(int)
 *  \brief Calls ThreadedFileWriter::SetWriteBufferSize(int)
 */
void RingBuffer::SetWriteBufferSize(int newSize)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->SetWriteBufferSize(newSize);
    rwlock.unlock();
}

/** \fn RingBuffer::SetWriteBufferMinWriteSize(int)
 *  \brief Calls ThreadedFileWriter::SetWriteBufferMinWriteSize(int)
 */
void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->SetWriteBufferMinWriteSize(newMinSize);
    rwlock.unlock();
}

/** \brief Tell RingBuffer if this is an old file or not.
 *
 *  Normally the RingBuffer determines that the file is old
 *  if it has not been modified in the last minute. This
 *  allows one to override that determination externally.
 *
 *  If for instance you are slowly writing to the file you
 *  could call this with the value of false. If you just
 *  finished writing the file you could call it with the
 *  value true. Knowing that the file is old allows MythTV
 *  to determine that a read at the end of the file is
 *  really an end-of-file condition more quickly. But if
 *  the file is growing it can also cause the RingBuffer to
 *  report an end-of-file condition prematurely.
 */
void RingBuffer::SetOldFile(bool is_old)
{
    rwlock.lockForWrite();
    oldfile = is_old;
    rwlock.unlock();
}

/// Returns name of file used by this RingBuffer
QString RingBuffer::GetFilename(void) const
{
    rwlock.lockForRead();
    QString tmp = filename;
    rwlock.unlock();
    return tmp;
}

QString RingBuffer::GetSubtitleFilename(void) const
{
    rwlock.lockForRead();
    QString tmp = subtitlefilename;
    rwlock.unlock();
    return tmp;
}

/** \fn RingBuffer::GetWritePosition(void) const
 *  \brief Returns how far into a ThreadedFileWriter file we have written.
 */
long long RingBuffer::GetWritePosition(void) const
{
    poslock.lockForRead();
    long long ret = writepos;
    poslock.unlock();
    return ret;
}

/** \fn RingBuffer::LiveMode(void) const
 *  \brief Returns true if this RingBuffer has been assigned a LiveTVChain.
 *  \sa SetLiveMode(LiveTVChain*)
 */
bool RingBuffer::LiveMode(void) const
{
    rwlock.lockForRead();
    bool ret = (livetvchain);
    rwlock.unlock();
    return ret;
}

/** \fn RingBuffer::SetLiveMode(LiveTVChain*)
 *  \brief Assigns a LiveTVChain to this RingBuffer
 *  \sa LiveMode(void)
 */
void RingBuffer::SetLiveMode(LiveTVChain *chain)
{
    rwlock.lockForWrite();
    livetvchain = chain;
    rwlock.unlock();
}

/// Tells RingBuffer whether to igonre the end-of-file
void RingBuffer::IgnoreLiveEOF(bool ignore)
{
    rwlock.lockForWrite();
    ignoreliveeof = ignore;
    rwlock.unlock();
}

const DVDRingBuffer *RingBuffer::DVD(void) const
{
    return dynamic_cast<const DVDRingBuffer*>(this);
}

const BDRingBuffer  *RingBuffer::BD(void) const
{
    return dynamic_cast<const BDRingBuffer*>(this);
}

const StreamingRingBuffer  *RingBuffer::Stream(void) const
{
    return dynamic_cast<const StreamingRingBuffer*>(this);
}

DVDRingBuffer *RingBuffer::DVD(void)
{
    return dynamic_cast<DVDRingBuffer*>(this);
}

BDRingBuffer  *RingBuffer::BD(void)
{
    return dynamic_cast<BDRingBuffer*>(this);
}

StreamingRingBuffer  *RingBuffer::Stream(void)
{
    return dynamic_cast<StreamingRingBuffer*>(this);
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
