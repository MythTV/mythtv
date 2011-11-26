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
#include <QReadLocker>

#include "threadedfilewriter.h"
#include "fileringbuffer.h"
#include "streamingringbuffer.h"
#include "dvdstream.h"
#include "livetvchain.h"
#include "mythcontext.h"
#include "ringbuffer.h"
#include "mythconfig.h"
#include "remotefile.h"
#include "compat.h"
#include "mythdate.h"
#include "mythtimer.h"
#include "mythlogging.h"
#include "DVD/dvdringbuffer.h"
#include "Bluray/bdringbuffer.h"
#include "HLS/httplivestreambuffer.h"

// about one second at 35mbit
#define BUFFER_SIZE_MINIMUM 4 * 1024 * 1024
#define BUFFER_FACTOR_NETWORK  2
#define BUFFER_FACTOR_BITRATE  2
#define BUFFER_FACTOR_MATROSKA 2

const int  RingBuffer::kDefaultOpenTimeout = 2000; // ms
const int  RingBuffer::kLiveTVOpenTimeout  = 10000;

#define CHUNK 32768 /* readblocksize increments */

#define LOC      QString("RingBuf(%1): ").arg(filename)

QMutex      RingBuffer::subExtLock;
QStringList RingBuffer::subExt;
QStringList RingBuffer::subExtNoCheck;

extern "C" {
#include "libavformat/avformat.h"
}
bool        RingBuffer::gAVformat_net_initialised = false;

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
    QString lower = lfilename.toLower();

    if (write)
        return new FileRingBuffer(lfilename, write, usereadahead, timeout_ms);

    bool dvddir  = false;
    bool bddir   = false;
    bool httpurl = lower.startsWith("http://") || lower.startsWith("https://");
    bool mythurl = lower.startsWith("myth://");
    bool bdurl   = lower.startsWith("bd:");
    bool dvdurl  = lower.startsWith("dvd:");
    bool dvdext  = lower.endsWith(".img") || lower.endsWith(".iso");

    if (httpurl)
    {
        if (HLSRingBuffer::TestForHTTPLiveStreaming(lfilename))
        {
            return new HLSRingBuffer(lfilename);
        }
        return new StreamingRingBuffer(lfilename);
    }
    if (!stream_only && mythurl)
    {
        struct stat fileInfo;
        if ((RemoteFile::Exists(lfilename, &fileInfo)) &&
            (S_ISDIR(fileInfo.st_mode)))
        {
            if (RemoteFile::Exists(lfilename + "/VIDEO_TS"))
                dvddir = true;
            else if (RemoteFile::Exists(lfilename + "/BDMV"))
                bddir = true;
        }
    }
    else if (!stream_only && !mythurl)
    {
        if (QFile::exists(lfilename + "/VIDEO_TS"))
            dvddir = true;
        else if (QFile::exists(lfilename + "/BDMV"))
            bddir  = true;
    }

    if (!stream_only && (dvdurl || dvddir || dvdext))
    {
        if (lfilename.startsWith("dvd:"))        // URI "dvd:" + path
            lfilename.remove(0,4);              // e.g. "dvd:/dev/dvd"

        if (!(mythurl || QFile::exists(lfilename)))
            lfilename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying DVD at " + lfilename);

        return new DVDRingBuffer(lfilename);
    }
    else if (!stream_only && (bdurl || bddir))
    {
        if (lfilename.startsWith("bd:"))        // URI "bd:" + path
            lfilename.remove(0,3);             // e.g. "bd:/videos/ET"

        if (!(mythurl || QFile::exists(lfilename)))
            lfilename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying BD at " + lfilename);

        return new BDRingBuffer(lfilename);
    }

    if (!mythurl && dvdext)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "ISO image at " + lfilename);
        return new DVDStream(lfilename);
    }
    if (!mythurl && lower.endsWith(".vob") && lfilename.contains("/VIDEO_TS/"))
    {
        LOG(VB_PLAYBACK, LOG_INFO, "DVD VOB at " + lfilename);
        return new DVDStream(lfilename);
    }

    return new FileRingBuffer(
        lfilename, write, usereadahead, timeout_ms);
}

RingBuffer::RingBuffer(RingBufferType rbtype) :
    MThread("RingBuffer"),
    type(rbtype),
    readpos(0),               writepos(0),
    internalreadpos(0),       ignorereadpos(-1),
    rbrpos(0),                rbwpos(0),
    stopreads(false),         safefilename(QString()),
    filename(),               subtitlefilename(),
    tfw(NULL),                fd2(-1),
    writemode(false),         remotefile(NULL),
    bufferSize(BUFFER_SIZE_MINIMUM),
    low_buffers(false),
    fileismatroska(false),    unknownbitrate(false),
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
    ignoreliveeof(false),     readAdjust(0),
    bitrateMonitorEnabled(false)
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

#undef NDEBUG
#include <cassert>

/** \brief Deletes 
 *  \Note Classes inheriting from RingBuffer must implement
 *        a destructor that calls KillReadAheadThread().
 *        We can not do that here because this would leave
 *        pure virtual functions without implementations
 *        during destruction.
 */
RingBuffer::~RingBuffer(void)
{
    assert(!isRunning());
    wait();

    delete [] readAheadBuffer;
    readAheadBuffer = NULL;

    if (tfw)
    {
        tfw->Flush();
        delete tfw;
        tfw = NULL;
    }
}

/** \fn RingBuffer::Reset(bool, bool, bool)
 *  \brief Resets the read-ahead thread and our position in the file
 */
void RingBuffer::Reset(bool full, bool toAdjust, bool resetInternal)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Reset(%1,%2,%3)")
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
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RingBuffer::Reset() nonzero readpos.  toAdjust: %1 "
                    "readpos: %2 readAdjust: %3")
                .arg(toAdjust).arg(readpos).arg(readAdjust));
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
    LOG(VB_FILE, LOG_INFO, LOC +
        QString("UpdateRawBitrate(%1Kb)").arg(raw_bitrate));

    // an audio only stream could be as low as 64Kb (DVB radio) and
    // an MHEG only stream is likely to be reported as 0Kb
    if (raw_bitrate < 64)
    {
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Bitrate too low - setting to 64Kb"));
        raw_bitrate = 64;
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

/** \fn RingBuffer::SetBufferSizeFactors(bool, bool)
 *  \brief Tells RingBuffer that the raw bitrate may be innacurate and the
 *         underlying container is matroska, both of which may require a larger
 *         buffer size.
 */
void RingBuffer::SetBufferSizeFactors(bool estbitrate, bool matroska)
{
    rwlock.lockForWrite();
    unknownbitrate = estbitrate;
    fileismatroska = matroska;
    rwlock.unlock();
    CreateReadAheadBuffer();
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
    fill_threshold = 7 * bufferSize / 8;

    const uint KB2   =   2*1024;
    const uint KB4   =   4*1024;
    const uint KB8   =   8*1024;
    const uint KB32  =  32*1024;
    const uint KB64  =  64*1024;
    const uint KB128 = 128*1024;
    const uint KB256 = 256*1024;
    const uint KB512 = 512*1024;

    estbitrate     = (uint) max(abs(rawbitrate * playspeed),
                                0.5f * rawbitrate);
    estbitrate     = min(rawbitrate * 3, estbitrate);
    int const rbs  = (estbitrate > 18000) ? KB512 :
                     (estbitrate >  9000) ? KB256 :
                     (estbitrate >  5000) ? KB128 :
                     (estbitrate >  2500) ? KB64  :
                     (estbitrate >=  500) ? KB32  :
                     (estbitrate >   250) ? KB8   :
                     (estbitrate >   125) ? KB4   : KB2;
    if (rbs < CHUNK)
        readblocksize = rbs;
    else
        readblocksize = max(rbs,readblocksize);

    // minumum seconds of buffering before allowing read
    float secs_min = 0.35;
    // set the minimum buffering before allowing ffmpeg read
    fill_min  = (uint) ((estbitrate * 1000 * secs_min) * 0.125f);
    // make this a multiple of ffmpeg block size..
    if (fill_min >= CHUNK || rbs >= CHUNK)
    {
        if (low_buffers)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Buffering optimisations disabled.");
        }
        low_buffers = false;
        fill_min = ((fill_min / CHUNK) + 1) * CHUNK;
    }
    else
    {
        low_buffers = true;
        LOG(VB_GENERAL, LOG_WARNING, "Enabling buffering optimisations "
                                     "for low bitrate stream.");
    }

    LOG(VB_FILE, LOG_INFO, LOC +
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

    bool near_end = ((vvf + readahead_frames) < 20.0) || (sz < rbs*1.5);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "IsReallyNearEnd()" +
            QString(" br(%1KB)").arg(kbits_per_sec/8) +
            QString(" sz(%1KB)").arg(sz / 1000) +
            QString(" vfl(%1)").arg(vvf) +
            QString(" frh(%1)").arg(((uint)readahead_frames)) +
            QString(" ne:%1").arg(near_end));

    return near_end;
}

/// \brief Returns number of bytes available for reading into buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufFree(void) const
{
    rbrlock.lockForRead();
    rbwlock.lockForRead();
    int ret = ((rbwpos >= rbrpos) ? rbrpos + bufferSize : rbrpos) - rbwpos - 1;
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
    int ret = (rbwpos >= rbrpos) ? rbwpos - rbrpos : bufferSize - rbrpos + rbwpos;
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
    LOG(VB_FILE, LOG_INFO, LOC +
        QString("ResetReadAhead(internalreadpos = %1->%2)")
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
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread, "
                                           "this is a write only RingBuffer");
        do_start = false;
    }
    else if (readaheadrunning)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread, "
                                           "already running");
        do_start = false;
    }

    if (!do_start)
    {
        rwlock.unlock();
        return;
    }

    StartReads();

    MThread::start();

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
        MThread::wait(5000);
    }
}

/** \fn RingBuffer::StopReads(void)
 *  \brief ????
 *  \sa StartReads(void), Pause(void)
 */
void RingBuffer::StopReads(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "StopReads()");
    stopreads = true;
    generalWait.wakeAll();
}

/** \fn RingBuffer::StartReads(void)
 *  \brief ????
 *  \sa StopReads(void), Unpause(void)
 */
void RingBuffer::StartReads(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "StartReads()");
    stopreads = false;
    generalWait.wakeAll();
}

/** \fn RingBuffer::Pause(void)
 *  \brief Pauses the read-ahead thread. Calls StopReads(void).
 *  \sa Unpause(void), WaitForPause(void)
 */
void RingBuffer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "Pause()");
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
    LOG(VB_FILE, LOG_INFO, LOC + "Unpause()");
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
            LOG(VB_GENERAL, LOG_WARNING, LOC +
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

void RingBuffer::CreateReadAheadBuffer(void)
{
    rwlock.lockForWrite();
    poslock.lockForWrite();

    uint oldsize = bufferSize;
    uint newsize = BUFFER_SIZE_MINIMUM;
    if (remotefile)
    {
        newsize *= BUFFER_FACTOR_NETWORK;
        if (fileismatroska)
            newsize *= BUFFER_FACTOR_MATROSKA;
        if (unknownbitrate)
            newsize *= BUFFER_FACTOR_BITRATE;
    }

    // N.B. Don't try and make it smaller - bad things happen...
    if (readAheadBuffer && oldsize >= newsize)
    {
        poslock.unlock();
        rwlock.unlock();
        return;
    }

    bufferSize = newsize;
    if (readAheadBuffer)
    {
        char* newbuffer = new char[bufferSize + 1024];
        memcpy(newbuffer, readAheadBuffer + rbwpos, oldsize - rbwpos);
        memcpy(newbuffer + (oldsize - rbwpos), readAheadBuffer, rbwpos);
        delete [] readAheadBuffer;
        readAheadBuffer = newbuffer;
        rbrpos = (rbrpos > rbwpos) ? (rbrpos - rbwpos) :
                                     (rbrpos + oldsize - rbwpos);
        rbwpos = oldsize;
    }
    else
    {
        readAheadBuffer = new char[bufferSize + 1024];
    }
    CalcReadAheadThresh();
    poslock.unlock();
    rwlock.unlock();

    LOG(VB_FILE, LOG_INFO, LOC + QString("Created readAheadBuffer: %1Mb")
        .arg(newsize >> 20));
}

void RingBuffer::run(void)
{
    RunProlog();

    // These variables are used to adjust the read block size
    struct timeval lastread, now;
    int readtimeavg = 300;
    bool ignore_for_read_timing = true;

    gettimeofday(&lastread, NULL); // this is just to keep gcc happy

    CreateReadAheadBuffer();
    rwlock.lockForWrite();
    poslock.lockForWrite();
    request_pause = false;
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

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Initial readblocksize %1K & fill_min %2K")
            .arg(readblocksize/1024).arg(fill_min/1024));

    while (readaheadrunning)
    {
        if (PauseAndWait())
        {
            ignore_for_read_timing = true;
            continue;
        }

        long long totfree = ReadBufFree();

        const uint KB32 = 32*1024;
        // These are conditions where we don't want to go through
        // the loop if they are true.
        if (((totfree < KB32) && readsallowed) ||
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
        if (totfree >= KB32 && !commserror &&
            !ateof && !setswitchtonext)
        {
            // limit the read size
            if (readblocksize > totfree)
                totfree = (long long)(totfree / KB32) * KB32; // must be multiple of 32KB
            else
                totfree = readblocksize;

            // adapt blocksize
            gettimeofday(&now, NULL);
            if (!ignore_for_read_timing)
            {
                int readinterval = (now.tv_sec  - lastread.tv_sec ) * 1000 +
                    (now.tv_usec - lastread.tv_usec) / 1000;
                readtimeavg = (readtimeavg * 9 + readinterval) / 10;

                if (readtimeavg < 150 &&
                    (uint)readblocksize < (BUFFER_SIZE_MINIMUM >>2) &&
                    readblocksize >= CHUNK /* low_buffers */)
                {
                    int old_block_size = readblocksize;
                    readblocksize = 3 * readblocksize / 2;
                    readblocksize = ((readblocksize+CHUNK-1) / CHUNK) * CHUNK;
                    LOG(VB_FILE, LOG_INFO, LOC +
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
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Avg read interval was %1 msec. "
                                "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg((readblocksize+CHUNK)/1024)
                            .arg(readblocksize/1024));
                    readtimeavg = 225;
                }
            }
            ignore_for_read_timing = (totfree < readblocksize) ? true : false;
            lastread = now;

            rbwlock.lockForRead();
            if (rbwpos + totfree > bufferSize)
            {
                totfree = bufferSize - rbwpos;
                LOG(VB_FILE, LOG_DEBUG, LOC +
                    "Shrinking read, near end of buffer");
            }

            if (internalreadpos == 0)
            {
                totfree = max(fill_min, readblocksize);
                LOG(VB_FILE, LOG_DEBUG, LOC +
                    "Reading enough data to start playback");
            }

            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("safe_read(...@%1, %2) -- begin")
                    .arg(rbwpos).arg(totfree));

            MythTimer sr_timer;
            sr_timer.start();

            int rbwposcopy = rbwpos;

            // FileRingBuffer::safe_read(RemoteFile*...) acquires poslock;
            // so we need to unlock this here to preserve locking order.
            rbwlock.unlock();

            read_return = safe_read(readAheadBuffer + rbwposcopy, totfree);

            int sr_elapsed = sr_timer.elapsed();
            uint64_t bps = !sr_elapsed ? 1000000001 :
                           (uint64_t)(((double)read_return * 8000.0) /
                                      (double)sr_elapsed);
            LOG(VB_FILE, LOG_INFO, LOC +
                QString("safe_read(...@%1, %2) -> %3, took %4 ms %5")
                    .arg(rbwposcopy).arg(totfree).arg(read_return)
                    .arg(sr_elapsed)
                    .arg(QString("(%1Mbps)").arg((double)bps / 1000000.0)));
            UpdateStorageRate(bps);

            if (read_return >= 0)
            {
                poslock.lockForWrite();
                rbwlock.lockForWrite();
                if (rbwposcopy == rbwpos)
                {
                    internalreadpos += read_return;
                    rbwpos = (rbwpos + read_return) % bufferSize;
                    LOG(VB_FILE, LOG_DEBUG,
                        LOC + QString("rbwpos += %1K requested %2K in read")
                        .arg(read_return/1024,3).arg(totfree/1024,3));
                }
                rbwlock.unlock();
                poslock.unlock();
            }
        }

        int used = bufferSize - ReadBufFree();

        bool reads_were_allowed = readsallowed;

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
                    LOG(VB_FILE, LOG_DEBUG,
                        LOC + "setting ateof (read_return == 0)");
                    ateof = true;
                }
            }

            rwlock.unlock();
            rwlock.lockForRead();
            used = bufferSize - ReadBufFree();
        }

        LOG(VB_FILE, LOG_DEBUG, LOC + "@ end of read ahead loop");

        if (!readsallowed || commserror || ateof || setswitchtonext ||
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
            if (!request_pause && reads_were_allowed &&
                (used >= fill_threshold || ateof || setswitchtonext))
            {
                generalWait.wait(&rwlock, 50);
            }
            else if (readsallowed)
            { // if reads are allowed release the lock and yield so the
              // reader gets a chance to read before the buffer is full.
                generalWait.wakeAll();
                rwlock.unlock();
                usleep(5 * 1000);
                rwlock.lockForRead();
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

    RunEpilog();
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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
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
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Taking too long to be allowed to read..");

            if (t.elapsed() > 10000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Took more than 10 seconds to "
                                               "be allowed to read, aborting.");
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

    if (livetvchain && setswitchtonext && avail < count)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Checking to see if there's a new livetv program to switch to..");
        livetvchain->ReloadAll();
        return false;
    }

    // Make sure that if the read ahead thread is sleeping and
    // it should be reading that we start reading right away.
    if ((avail < count) && !stopreads &&
        !request_pause && !commserror && readaheadrunning)
    {
        generalWait.wakeAll();
    }

    MythTimer t;
    t.start();
    while ((avail < count) && !stopreads &&
           !request_pause && !commserror && readaheadrunning)
    {
        wanttoread = count;
        generalWait.wait(&rwlock, 250);
        avail = ReadBufAvail();

        if (ateof && avail < count)
            count = avail;

        if (avail < count)
        {
            int elapsed = t.elapsed();
            if (elapsed > 500 && low_buffers && avail >= fill_min)
                count = avail;
            else if  (((elapsed > 250) && (elapsed < 500))  ||
                     ((elapsed >  500) && (elapsed < 750))  ||
                     ((elapsed > 1000) && (elapsed < 1250)) ||
                     ((elapsed > 2000) && (elapsed < 2250)) ||
                     ((elapsed > 4000) && (elapsed < 4250)) ||
                     ((elapsed > 8000) && (elapsed < 8250)) ||
                     ((elapsed > 9000)))
            {
                LOG(VB_GENERAL, LOG_INFO, LOC + "Waited " +
                    QString("%1").arg((elapsed / 250) * 0.25f, 3, 'f', 1) +
                    " seconds for data \n\t\t\tto become available..." +
                    QString(" %2 < %3") .arg(avail).arg(count));
            }

            if (elapsed > 16000)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Waited " +
                    QString("%1").arg(elapsed/1000) +
                    " seconds for data, aborting.");
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

    MythTimer timer;
    timer.start();
    int ret = safe_read(buf, count);
    int elapsed = timer.elapsed();
    uint64_t bps = !elapsed ? 1000000001 :
                   (uint64_t)(((float)ret * 8000.0) / (float)elapsed);
    UpdateStorageRate(bps);

    poslock.lockForWrite();
    if (ignorereadpos >= 0 && ret > 0)
    {
        if (peek)
        {
            // seek should always succeed since we were at this position
            if (remotefile)
                remotefile->Seek(old_pos, SEEK_SET);
            else if (fd2 >= 0)
                lseek64(fd2, old_pos, SEEK_SET);
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
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    "DVD and Blu-Ray do not support arbitrary "
                    "peeks except when read-ahead is enabled."
                    "\n\t\t\tWill seek to beginning of video.");
            old_pos = 0;
        }

        long long new_pos = Seek(old_pos, SEEK_SET, true);

        if (new_pos != old_pos)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
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
    QString loc_desc = QString("ReadPriv(..%1, %2)")
        .arg(count).arg(peek?"peek":"normal");
    LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc +
        QString(" @%1 -- begin").arg(rbrpos));

    rwlock.lockForRead();
    if (writemode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + loc_desc +
            ": Attempt to read from a write only file");
        errno = EBADF;
        rwlock.unlock();
        return -1;
    }

    if (commserror)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + loc_desc +
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
            LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc +
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
        LOG(VB_FILE, LOG_NOTICE, LOC + loc_desc + ": !WaitForReadsAllowed()");
        rwlock.unlock();
        stopreads = true; // this needs to be outside the lock
        rwlock.lockForWrite();
        wanttoread = 0;
        rwlock.unlock();
        return 0;
    }

    if (!WaitForAvail(count))
    {
        LOG(VB_FILE, LOG_NOTICE, LOC + loc_desc + ": !WaitForAvail()");
        rwlock.unlock();
        stopreads = true; // this needs to be outside the lock
        rwlock.lockForWrite();
        ateof = true;
        wanttoread = 0;
        rwlock.unlock();
        return 0;
    }

    count = min(ReadBufAvail(), count);

    if (count <= 0)
    {
        // this can happen under a few conditions but the most
        // notable is an exit from the read ahead thread or
        // the end of the file stream has been reached.
        LOG(VB_FILE, LOG_NOTICE, LOC + loc_desc + ": ReadBufAvail() == 0");
        rwlock.unlock();
        return count;
    }

    if (peek)
        rbrlock.lockForRead();
    else
        rbrlock.lockForWrite();

    LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc + " -- copying data");

    if (rbrpos + count > (int) bufferSize)
    {
        int firstsize = bufferSize - rbrpos;
        int secondsize = count - firstsize;

        memcpy(buf, readAheadBuffer + rbrpos, firstsize);
        memcpy((char *)buf + firstsize, readAheadBuffer, secondsize);
    }
    else
    {
        memcpy(buf, readAheadBuffer + rbrpos, count);
    }
    LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc + QString(" -- checksum %1")
            .arg(qChecksum((char*)buf,count)));

    if (!peek)
    {
        rbrpos = (rbrpos + count) % bufferSize;
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

    UpdateDecoderRate(ret);
    return ret;
}

QString RingBuffer::BitrateToString(uint64_t rate, bool hz)
{
    QString msg;
    float bitrate;
    int range = 0;
    if (rate < 1)
    {
        return "-";
    }
    else if (rate > 1000000000)
    {
        return QObject::tr(">1Gbps");
    }
    else if (rate >= 1000000)
    {
        msg = hz ? QObject::tr("%1MHz") : QObject::tr("%1Mbps");
        bitrate  = (float)rate / (1000000.0);
        range = hz ? 3 : 1;
    }
    else if (rate >= 1000)
    {
        msg = hz ? QObject::tr("%1kHz") : QObject::tr("%1kbps");
        bitrate = (float)rate / 1000.0;
        range = hz ? 1 : 0;
    }
    else
    {
        msg = hz ? QObject::tr("%1Hz") : QObject::tr("%1bps");
        bitrate = (float)rate;
    }
    return msg.arg(bitrate, 0, 'f', range);
}

QString RingBuffer::GetDecoderRate(void)
{
    return BitrateToString(UpdateDecoderRate());
}

QString RingBuffer::GetStorageRate(void)
{
    return BitrateToString(UpdateStorageRate());
}

QString RingBuffer::GetAvailableBuffer(void)
{
    if (type == kRingBuffer_DVD || type == kRingBuffer_BD)
        return "N/A";

    int avail = (rbwpos >= rbrpos) ? rbwpos - rbrpos : bufferSize - rbrpos + rbwpos;
    return QString("%1%").arg((int)(((float)avail / (float)bufferSize) * 100.0));
}

uint64_t RingBuffer::UpdateDecoderRate(uint64_t latest)
{
    if (!bitrateMonitorEnabled)
        return 0;

    // TODO use QDateTime once we've moved to Qt 4.7
    static QTime midnight = QTime(0, 0, 0);
    QTime now = QTime::currentTime();
    qint64 age = midnight.msecsTo(now);
    qint64 oldest = age - 1000;

    decoderReadLock.lock();
    if (latest)
        decoderReads.insert(age, latest);

    uint64_t total = 0;
    QMutableMapIterator<qint64,uint64_t> it(decoderReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < oldest || it.key() > age)
            it.remove();
        else
            total += it.value();
    }

    uint64_t average = (uint64_t)((double)total * 8.0);
    decoderReadLock.unlock();

    LOG(VB_FILE, LOG_INFO, LOC + QString("Decoder read speed: %1 %2")
            .arg(average).arg(decoderReads.size()));
    return average;
}

uint64_t RingBuffer::UpdateStorageRate(uint64_t latest)
{
    if (!bitrateMonitorEnabled)
        return 0;

    // TODO use QDateTime once we've moved to Qt 4.7
    static QTime midnight = QTime(0, 0, 0);
    QTime now = QTime::currentTime();
    qint64 age = midnight.msecsTo(now);
    qint64 oldest = age - 1000;

    storageReadLock.lock();
    if (latest)
        storageReads.insert(age, latest);

    uint64_t total = 0;
    QMutableMapIterator<qint64,uint64_t> it(storageReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < oldest || it.key() > age)
            it.remove();
        else
            total += it.value();
    }

    int size = storageReads.size();
    storageReadLock.unlock();

    uint64_t average = size ? (uint64_t)(((double)total) / (double)size) : 0;

    LOG(VB_FILE, LOG_INFO, LOC + QString("Average storage read speed: %1 %2")
            .arg(average).arg(storageReads.size()));
    return average;
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
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to write to a read only file.");
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
 *  \brief Calls ThreadedFileWriter::Flush(void)
 */
void RingBuffer::WriterFlush(void)
{
    rwlock.lockForRead();
    if (tfw)
        tfw->Flush();
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

/** \fn RingBuffer::WriterSetBlocking(bool)
 *  \brief Calls ThreadedFileWriter::SetBlocking(bool)
 */
bool RingBuffer::WriterSetBlocking(bool block)
{
    QReadLocker lock(&rwlock);

    if (tfw)
        return tfw->SetBlocking(block);
    return false;
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
    LOG(VB_FILE, LOG_INFO, LOC + QString("SetOldFile(%1)").arg(is_old));
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

QString RingBuffer::GetLastError(void) const
{
    rwlock.lockForRead();
    QString tmp = lastError;
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

DVDRingBuffer *RingBuffer::DVD(void)
{
    return dynamic_cast<DVDRingBuffer*>(this);
}

BDRingBuffer  *RingBuffer::BD(void)
{
    return dynamic_cast<BDRingBuffer*>(this);
}

void RingBuffer::AVFormatInitNetwork(void)
{
    QMutexLocker lock(avcodeclock);

    if (!gAVformat_net_initialised)
    {
        avformat_network_init();
        gAVformat_net_initialised = true;
    }
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
