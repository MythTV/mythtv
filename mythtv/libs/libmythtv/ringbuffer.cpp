// ANSI C headers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <chrono> // for milliseconds
#include <thread> // for sleep_for

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
#include "mythmiscutil.h"
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
#include "mythcdrom.h"

const int  RingBuffer::kDefaultOpenTimeout = 2000; // ms
const int  RingBuffer::kLiveTVOpenTimeout  = 10000;

#define LOC      QString("RingBuf(%1): ").arg(m_filename)

QMutex      RingBuffer::s_subExtLock;
QStringList RingBuffer::s_subExt;
QStringList RingBuffer::s_subExtNoCheck;

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
      blah(); // <- does not implicitly acquire any locks
      rwlock.unlock();
      poslock.unlock();
  }
  void RingBuffer::Example2()
  {
      rwlock.lockForRead();
      rbrlock.lockForWrite(); // ok!
      blah(); // <- does not implicitly acquire any locks
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
 *  \param xfilename    Name of file to read or write.
 *  \param write        If true an encapsulated writer is created
 *  \param usereadahead If false a call to Start(void) will not
 *                      a pre-buffering thread, otherwise Start(void)
 *                      will start a pre-buffering thread.
 *  \param timeout_ms   if < 0, then we will not open the file.
 *                      Otherwise it's how long to try opening
 *                      the file after the first failure in
 *                      milliseconds before giving up.
 *  \param stream_only  If true, prevent DVD and Bluray (used by FileTransfer)
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
    bool iptvurl =
        lower.startsWith("rtp://") || lower.startsWith("tcp://") ||
        lower.startsWith("udp://");
    bool mythurl = lower.startsWith("myth://");
    bool bdurl   = lower.startsWith("bd:");
    bool dvdurl  = lower.startsWith("dvd:");
    bool imgext  = lower.endsWith(".img") || lower.endsWith(".iso");

    if (imgext)
    {
        switch (MythCDROM::inspectImage(lfilename))
        {
            case MythCDROM::kBluray:
                bdurl = true;
                break;

            case MythCDROM::kDVD:
                dvdurl = true;
                break;

            default:
                break;
        }
    }

    if (httpurl || iptvurl)
    {
        if (!iptvurl && HLSRingBuffer::TestForHTTPLiveStreaming(lfilename))
        {
            return new HLSRingBuffer(lfilename);
        }
        return new StreamingRingBuffer(lfilename);
    }
    if (!stream_only && mythurl)
    {
        struct stat fileInfo {};
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

    if (!stream_only && (dvdurl || dvddir))
    {
        if (lfilename.startsWith("dvd:"))        // URI "dvd:" + path
            lfilename.remove(0,4);              // e.g. "dvd:/dev/dvd"

        if (!(mythurl || QFile::exists(lfilename)))
            lfilename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying DVD at " + lfilename);

        return new DVDRingBuffer(lfilename);
    }
    if (!stream_only && (bdurl || bddir))
    {
        if (lfilename.startsWith("bd:"))        // URI "bd:" + path
            lfilename.remove(0,3);             // e.g. "bd:/videos/ET"

        if (!(mythurl || QFile::exists(lfilename)))
            lfilename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying BD at " + lfilename);

        return new BDRingBuffer(lfilename);
    }

    if (!mythurl && imgext && lfilename.startsWith("dvd:"))
    {
        LOG(VB_PLAYBACK, LOG_INFO, "DVD image at " + lfilename);
        return new DVDStream(lfilename);
    }
    if (!mythurl && lower.endsWith(".vob") && lfilename.contains("/VIDEO_TS/"))
    {
        LOG(VB_PLAYBACK, LOG_INFO, "DVD VOB at " + lfilename);
        auto *s = new DVDStream(lfilename);
        if (s && s->IsOpen())
            return s;

        delete s;
    }

    return new FileRingBuffer(
        lfilename, write, usereadahead, timeout_ms);
}

RingBuffer::RingBuffer(RingBufferType rbtype) :
    MThread("RingBuffer"),
    m_type(rbtype)
{
    {
        QMutexLocker locker(&s_subExtLock);
        if (s_subExt.empty())
        {
            // Possible subtitle file extensions '.srt', '.sub', '.txt'
            // since #9294 also .ass and .ssa
            s_subExt += ".ass";
            s_subExt += ".srt";
            s_subExt += ".ssa";
            s_subExt += ".sub";
            s_subExt += ".txt";

            // Extensions for which a subtitle file should not exist
            s_subExtNoCheck = s_subExt;
            s_subExtNoCheck += ".gif";
            s_subExtNoCheck += ".png";
        }
    }
}

#undef NDEBUG
#include <cassert>

/** \brief Deletes
 *  \note Classes inheriting from RingBuffer must implement
 *        a destructor that calls KillReadAheadThread().
 *        We can not do that here because this would leave
 *        pure virtual functions without implementations
 *        during destruction.
 */
RingBuffer::~RingBuffer(void)
{
    assert(!isRunning());
    wait();

    delete [] m_readAheadBuffer;
    m_readAheadBuffer = nullptr;

    if (m_tfw)
    {
        m_tfw->Flush();
        delete m_tfw;
        m_tfw = nullptr;
    }
}

/** \fn RingBuffer::Reset(bool, bool, bool)
 *  \brief Resets the read-ahead thread and our position in the file
 */
void RingBuffer::Reset(bool full, bool toAdjust, bool resetInternal)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Reset(%1,%2,%3)")
            .arg(full).arg(toAdjust).arg(resetInternal));

    m_rwLock.lockForWrite();
    m_posLock.lockForWrite();

    m_numFailures = 0;
    m_commsError = false;
    m_setSwitchToNext = false;

    m_writePos = 0;
    m_readPos = (toAdjust) ? (m_readPos - m_readAdjust) : 0;

    if (m_readPos != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("RingBuffer::Reset() nonzero readpos.  toAdjust: %1 "
                    "readpos: %2 readAdjust: %3")
                .arg(toAdjust).arg(m_readPos).arg(m_readAdjust));
    }

    m_readAdjust = 0;
    m_readPos = (m_readPos < 0) ? 0 : m_readPos;

    if (full)
        ResetReadAhead(m_readPos);

    if (resetInternal)
        m_internalReadPos = m_readPos;

    m_generalWait.wakeAll();
    m_posLock.unlock();
    m_rwLock.unlock();
}

/**
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
    else if (raw_bitrate > 100000)
    {
        LOG(VB_FILE, LOG_INFO, LOC +
            QString("Bitrate too high - setting to 100Mb"));
        raw_bitrate = 100000;
    }

    m_rwLock.lockForWrite();
    m_rawBitrate = raw_bitrate;
    CalcReadAheadThresh();
    m_bitrateInitialized = true;
    m_rwLock.unlock();
}

/** \fn RingBuffer::UpdatePlaySpeed(float)
 *  \brief Set the play speed, to allow RingBuffer adjust effective bitrate.
 *  \param play_speed Speed to set. (1.0 for normal speed)
 */
void RingBuffer::UpdatePlaySpeed(float play_speed)
{
    m_rwLock.lockForWrite();
    m_playSpeed = play_speed;
    CalcReadAheadThresh();
    m_rwLock.unlock();
}

/** \fn RingBuffer::SetBufferSizeFactors(bool, bool)
 *  \brief Tells RingBuffer that the raw bitrate may be innacurate and the
 *         underlying container is matroska, both of which may require a larger
 *         buffer size.
 */
void RingBuffer::SetBufferSizeFactors(bool estbitrate, bool matroska)
{
    m_rwLock.lockForWrite();
    m_unknownBitrate = estbitrate;
    m_fileIsMatroska = matroska;
    m_rwLock.unlock();
    CreateReadAheadBuffer();
}

/** \fn RingBuffer::CalcReadAheadThresh(void)
 *  \brief Calculates m_fillMin, m_fillThreshold, and m_readBlockSize
 *         from the estimated effective bitrate of the stream.
 *
 *   WARNING: Must be called with rwlock in write lock state.
 *
 */
void RingBuffer::CalcReadAheadThresh(void)
{
    uint estbitrate = 0;

    m_readsAllowed   = false;
    m_readsDesired   = false;

    // loop without sleeping if the buffered data is less than this
    m_fillThreshold = 7 * m_bufferSize / 8;

    const uint KB2   =   2*1024;
    const uint KB4   =   4*1024;
    const uint KB8   =   8*1024;
    const uint KB16  =  16*1024;
    const uint KB32  =  32*1024;
    const uint KB64  =  64*1024;
    const uint KB128 = 128*1024;
    const uint KB256 = 256*1024;
    const uint KB512 = 512*1024;

    estbitrate     = (uint) max(abs(m_rawBitrate * m_playSpeed),
                                0.5F * m_rawBitrate);
    estbitrate     = min(m_rawBitrate * 3, estbitrate);
    int const rbs  = (estbitrate > 18000) ? KB512 :
                     (estbitrate >  9000) ? KB256 :
                     (estbitrate >  5000) ? KB128 :
                     (estbitrate >  2500) ? KB64  :
                     (estbitrate >  1250) ? KB32  :
                     (estbitrate >=  500) ? KB16  :
                     (estbitrate >   250) ? KB8   :
                     (estbitrate >   125) ? KB4   : KB2;
    if (rbs < CHUNK)
    {
        m_readBlockSize = rbs;
    }
    else
    {
        m_readBlockSize = m_bitrateInitialized ? max(rbs,m_readBlockSize) : rbs;
    }

    // minimum seconds of buffering before allowing read
    float secs_min = 0.3;
    // set the minimum buffering before allowing ffmpeg read
    m_fillMin  = (uint) ((estbitrate * 1000 * secs_min) * 0.125F);
    // make this a multiple of ffmpeg block size..
    if (m_fillMin >= CHUNK || rbs >= CHUNK)
    {
        if (m_lowBuffers)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Buffering optimisations disabled.");
        }
        m_lowBuffers = false;
        m_fillMin = ((m_fillMin / CHUNK) + 1) * CHUNK;
        m_fillMin = min((uint)m_fillMin, m_bufferSize / 2);
    }
    else
    {
        m_lowBuffers = true;
        LOG(VB_GENERAL, LOG_WARNING, "Enabling buffering optimisations "
                                     "for low bitrate stream.");
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("CalcReadAheadThresh(%1 Kb)\n\t\t\t -> "
                "threshold(%2 KB) min read(%3 KB) blk size(%4 KB)")
            .arg(estbitrate).arg(m_fillThreshold/1024)
            .arg(m_fillMin/1024).arg(m_readBlockSize/1024));
}

bool RingBuffer::IsNearEnd(double /*fps*/, uint vvf) const
{
    QReadLocker lock(&m_rwLock);

    if (!m_ateof && !m_setSwitchToNext)
    {
        // file is still being read, so can't be finished
        return false;
    }

    m_posLock.lockForRead();
    long long rp = m_readPos;
    long long sz = m_internalReadPos - m_readPos;
    m_posLock.unlock();

    // telecom kilobytes (i.e. 1000 per k not 1024)
    uint   tmp = (uint) max(abs(m_rawBitrate * m_playSpeed), 0.5F * m_rawBitrate);
    uint   kbits_per_sec = min(m_rawBitrate * 3, tmp);
    if (kbits_per_sec == 0)
        return false;

    double readahead_time   = sz / (kbits_per_sec * (1000.0/8.0));

    bool near_end = readahead_time <= 1.5;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "IsReallyNearEnd()" +
            QString(" br(%1KB)").arg(kbits_per_sec/8) +
            QString(" sz(%1KB)").arg(sz / 1000LL) +
            QString(" vfl(%1)").arg(vvf) +
            QString(" time(%1)").arg(readahead_time) +
            QString(" rawbitrate(%1)").arg(m_rawBitrate) +
            QString(" avail(%1)").arg(sz) +
            QString(" internal_size(%1)").arg(m_internalReadPos) +
            QString(" readposition(%1)").arg(rp) +
            QString(" stopreads(%1)").arg(m_stopReads) +
            QString(" paused(%1)").arg(m_paused) +
            QString(" ne:%1").arg(near_end));

    return near_end;
}

/// \brief Returns number of bytes available for reading into buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufFree(void) const
{
    m_rbrLock.lockForRead();
    m_rbwLock.lockForRead();
    int ret = ((m_rbwPos >= m_rbrPos) ? m_rbrPos + m_bufferSize : m_rbrPos) - m_rbwPos - 1;
    m_rbwLock.unlock();
    m_rbrLock.unlock();
    return ret;
}

/// \brief Returns number of bytes available for reading from buffer.
int RingBuffer::GetReadBufAvail(void) const
{
    QReadLocker lock(&m_rwLock);

    return ReadBufAvail();
}

long long RingBuffer::GetRealFileSize(void) const
{
    {
        QReadLocker lock(&m_rwLock);
        if (m_readInternalMode)
        {
            return ReadBufAvail();
        }
    }

    return GetRealFileSizeInternal();
}

long long RingBuffer::Seek(long long pos, int whence, bool has_lock)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Seek(%1,%2,%3)")
        .arg(pos).arg((SEEK_SET==whence)?"SEEK_SET":
                      ((SEEK_CUR==whence)?"SEEK_CUR":"SEEK_END"))
        .arg(has_lock?"locked":"unlocked"));

    if (!has_lock)
    {
        m_rwLock.lockForWrite();
    }

    long long ret;

    if (m_readInternalMode)
    {
        m_posLock.lockForWrite();
        // only valid for SEEK_SET & SEEK_CUR
        switch (whence)
        {
            case SEEK_SET:
                m_readPos = pos;
                break;
            case SEEK_CUR:
                m_readPos += pos;
                break;
            case SEEK_END:
                m_readPos = ReadBufAvail() - pos;
                break;
        }
        m_readOffset = m_readPos;
        m_posLock.unlock();
        ret = m_readPos;
    }
    else
    {
        ret = SeekInternal(pos, whence);
    }

    m_generalWait.wakeAll();

    if (!has_lock)
    {
        m_rwLock.unlock();
    }
    return ret;
}

bool RingBuffer::SetReadInternalMode(bool mode)
{
    QWriteLocker lock(&m_rwLock);
    bool old = m_readInternalMode;

    if (mode == old)
    {
        return old;
    }

    m_readInternalMode = mode;

    if (!mode)
    {
        // adjust real read position in ringbuffer
        m_rbrLock.lockForWrite();
        m_rbrPos = (m_rbrPos + m_readOffset) % m_bufferSize;
        m_generalWait.wakeAll();
        m_rbrLock.unlock();
        // reset the read offset as we are exiting the internal read mode
        m_readOffset = 0;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC +
        QString("SetReadInternalMode: %1").arg(mode ? "on" : "off"));

    return old;
}

/// \brief Returns number of bytes available for reading from buffer.
/// WARNING: Must be called with rwlock in locked state.
int RingBuffer::ReadBufAvail(void) const
{
    m_rbrLock.lockForRead();
    m_rbwLock.lockForRead();
    int ret = (m_rbwPos >= m_rbrPos) ? m_rbwPos - m_rbrPos : m_bufferSize - m_rbrPos + m_rbwPos;
    m_rbwLock.unlock();
    m_rbrLock.unlock();
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
            .arg(m_internalReadPos).arg(newinternal));

    m_readInternalMode = false;
    m_readOffset = 0;

    m_rbrLock.lockForWrite();
    m_rbwLock.lockForWrite();

    CalcReadAheadThresh();

    m_rbrPos          = 0;
    m_rbwPos          = 0;
    m_internalReadPos = newinternal;
    m_ateof           = false;
    m_readsAllowed    = false;
    m_readsDesired    = false;
    m_recentSeek      = true;
    m_setSwitchToNext = false;

    m_generalWait.wakeAll();

    m_rbwLock.unlock();
    m_rbrLock.unlock();
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

    m_rwLock.lockForWrite();
    if (!m_startReadAhead)
    {
        do_start = false;
    }
    else if (m_writeMode)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread, "
                                           "this is a write only RingBuffer");
        do_start = false;
    }
    else if (m_readAheadRunning)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread, "
                                           "already running");
        do_start = false;
    }

    if (!do_start)
    {
        m_rwLock.unlock();
        return;
    }

    StartReads();

    MThread::start();

    while (!m_readAheadRunning && !m_reallyRunning)
        m_generalWait.wait(&m_rwLock);

    m_rwLock.unlock();
}

/** \fn RingBuffer::KillReadAheadThread(void)
 *  \brief Stops the read-ahead thread, and waits for it to stop.
 */
void RingBuffer::KillReadAheadThread(void)
{
    while (isRunning())
    {
        m_rwLock.lockForWrite();
        m_readAheadRunning = false;
        StopReads();
        m_generalWait.wakeAll();
        m_rwLock.unlock();
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
    m_stopReads = true;
    m_generalWait.wakeAll();
}

/** \fn RingBuffer::StartReads(void)
 *  \brief ????
 *  \sa StopReads(void), Unpause(void)
 */
void RingBuffer::StartReads(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "StartReads()");
    m_stopReads = false;
    m_generalWait.wakeAll();
}

/** \fn RingBuffer::Pause(void)
 *  \brief Pauses the read-ahead thread. Calls StopReads(void).
 *  \sa Unpause(void), WaitForPause(void)
 */
void RingBuffer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "Pause()");
    StopReads();

    m_rwLock.lockForWrite();
    m_requestPause = true;
    m_rwLock.unlock();
}

/** \fn RingBuffer::Unpause(void)
 *  \brief Unpauses the read-ahead thread. Calls StartReads(void).
 *  \sa Pause(void)
 */
void RingBuffer::Unpause(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "Unpause()");
    StartReads();

    m_rwLock.lockForWrite();
    m_requestPause = false;
    m_generalWait.wakeAll();
    m_rwLock.unlock();
}

/// Returns false iff read-ahead is not running and read-ahead is not paused.
bool RingBuffer::isPaused(void) const
{
    m_rwLock.lockForRead();
    bool ret = !m_readAheadRunning || m_paused;
    m_rwLock.unlock();
    return ret;
}

/** \fn RingBuffer::WaitForPause(void)
 *  \brief Waits for Pause(void) to take effect.
 */
void RingBuffer::WaitForPause(void)
{
    MythTimer t;
    t.start();

    m_rwLock.lockForRead();
    while (m_readAheadRunning && !m_paused && m_requestPause)
    {
        m_generalWait.wait(&m_rwLock, 1000);
        if (m_readAheadRunning && !m_paused && m_requestPause && t.elapsed() > 1000)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                QString("Waited %1 ms for ringbuffer pause..")
                    .arg(t.elapsed()));
        }
    }
    m_rwLock.unlock();
}

bool RingBuffer::PauseAndWait(void)
{
    const uint timeout = 500; // ms

    if (m_requestPause)
    {
        if (!m_paused)
        {
            m_rwLock.unlock();
            m_rwLock.lockForWrite();

            if (m_requestPause)
            {
                m_paused = true;
                m_generalWait.wakeAll();
            }

            m_rwLock.unlock();
            m_rwLock.lockForRead();
        }

        if (m_requestPause && m_paused && m_readAheadRunning)
            m_generalWait.wait(&m_rwLock, timeout);
    }

    if (!m_requestPause && m_paused)
    {
        m_rwLock.unlock();
        m_rwLock.lockForWrite();

        if (!m_requestPause)
        {
            m_paused = false;
            m_generalWait.wakeAll();
        }

        m_rwLock.unlock();
        m_rwLock.lockForRead();
    }

    return m_requestPause || m_paused;
}

void RingBuffer::CreateReadAheadBuffer(void)
{
    m_rwLock.lockForWrite();
    m_posLock.lockForWrite();

    uint oldsize = m_bufferSize;
    uint newsize = BUFFER_SIZE_MINIMUM;
    if (m_remotefile)
    {
        newsize *= BUFFER_FACTOR_NETWORK;
        if (m_fileIsMatroska)
            newsize *= BUFFER_FACTOR_MATROSKA;
        if (m_unknownBitrate)
            newsize *= BUFFER_FACTOR_BITRATE;
    }

    // N.B. Don't try and make it smaller - bad things happen...
    if (m_readAheadBuffer && oldsize >= newsize)
    {
        m_posLock.unlock();
        m_rwLock.unlock();
        return;
    }

    m_bufferSize = newsize;
    if (m_readAheadBuffer)
    {
        char* newbuffer = new char[m_bufferSize + 1024];
        memcpy(newbuffer, m_readAheadBuffer + m_rbwPos, oldsize - m_rbwPos);
        memcpy(newbuffer + (oldsize - m_rbwPos), m_readAheadBuffer, m_rbwPos);
        delete [] m_readAheadBuffer;
        m_readAheadBuffer = newbuffer;
        m_rbrPos = (m_rbrPos > m_rbwPos) ? (m_rbrPos - m_rbwPos) :
                                           (m_rbrPos + oldsize - m_rbwPos);
        m_rbwPos = oldsize;
    }
    else
    {
        m_readAheadBuffer = new char[m_bufferSize + 1024];
    }
    CalcReadAheadThresh();
    m_posLock.unlock();
    m_rwLock.unlock();

    LOG(VB_FILE, LOG_INFO, LOC + QString("Created readAheadBuffer: %1Mb")
        .arg(newsize >> 20));
}

void RingBuffer::run(void)
{
    RunProlog();

    // These variables are used to adjust the read block size
    struct timeval lastread {};
    struct timeval now {};
    int readtimeavg = 300;
    bool ignore_for_read_timing = true;
    int eofreads = 0;

    gettimeofday(&lastread, nullptr); // this is just to keep gcc happy

    CreateReadAheadBuffer();
    m_rwLock.lockForWrite();
    m_posLock.lockForWrite();
    m_requestPause = false;
    ResetReadAhead(0);
    m_readAheadRunning = true;
    m_reallyRunning = true;
    m_generalWait.wakeAll();
    m_posLock.unlock();
    m_rwLock.unlock();

    // NOTE: this must loop at some point hold only
    // a read lock on rwlock, so that other functions
    // such as reset and seek can take priority.

    m_rwLock.lockForRead();

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("Initial readblocksize %1K & fill_min %2K")
            .arg(m_readBlockSize/1024).arg(m_fillMin/1024));

    while (m_readAheadRunning)
    {
        m_rwLock.unlock();
        bool isopened = IsOpen();
        m_rwLock.lockForRead();

        if (!isopened)
        {
            LOG(VB_FILE, LOG_WARNING, LOC +
                QString("File not opened, terminating readahead thread"));
            m_posLock.lockForWrite();
            m_readAheadRunning = false;
            m_generalWait.wakeAll();
            m_posLock.unlock();
            break;
        }
        if (PauseAndWait())
        {
            ignore_for_read_timing = true;
            LOG(VB_FILE, LOG_DEBUG, LOC +
                "run: PauseAndWait Not reading continuing");
            continue;
        }

        long long totfree = ReadBufFree();

        const uint KB32  = 32*1024;
        const  int KB512 = 512*1024;
        // These are conditions where we don't want to go through
        // the loop if they are true.
        if (((totfree < KB32) && m_readsAllowed) ||
            (m_ignoreReadPos >= 0) || m_commsError || m_stopReads)
        {
            ignore_for_read_timing |=
                (m_ignoreReadPos >= 0) || m_commsError || m_stopReads;
            m_generalWait.wait(&m_rwLock, (m_stopReads) ? 50 : 1000);
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("run: Not reading continuing: totfree(%1) "
                        "readsallowed(%2) ignorereadpos(%3) commserror(%4) "
                        "stopreads(%5)")
                .arg(totfree).arg(m_readsAllowed).arg(m_ignoreReadPos)
                .arg(m_commsError).arg(m_stopReads));
            continue;
        }

        // These are conditions where we want to sleep to allow
        // other threads to do stuff.
        if (m_setSwitchToNext || (m_ateof && m_readsDesired))
        {
            ignore_for_read_timing = true;
            m_generalWait.wait(&m_rwLock, 1000);
            totfree = ReadBufFree();
        }

        int read_return = -1;
        if (totfree >= KB32 && !m_commsError &&
            !m_ateof && !m_setSwitchToNext)
        {
            // limit the read size
            if (m_readBlockSize > totfree)
                totfree = (totfree / KB32) * KB32; // must be multiple of 32KB
            else
                totfree = m_readBlockSize;

            // adapt blocksize
            gettimeofday(&now, nullptr);
            if (!ignore_for_read_timing)
            {
                int readinterval = (now.tv_sec  - lastread.tv_sec ) * 1000 +
                    (now.tv_usec - lastread.tv_usec) / 1000;
                readtimeavg = (readtimeavg * 9 + readinterval) / 10;

                if (readtimeavg < 150 &&
                    (uint)m_readBlockSize < (BUFFER_SIZE_MINIMUM >>2) &&
                    m_readBlockSize >= CHUNK /* low_buffers */ &&
                    m_readBlockSize <= KB512)
                {
                    int old_block_size = m_readBlockSize;
                    m_readBlockSize = 3 * m_readBlockSize / 2;
                    m_readBlockSize = ((m_readBlockSize+CHUNK-1) / CHUNK) * CHUNK;
                    if (m_readBlockSize > KB512)
                    {
                        m_readBlockSize = KB512;
                    }
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Avg read interval was %1 msec. "
                                "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg(old_block_size/1024)
                            .arg(m_readBlockSize/1024));
                    readtimeavg = 225;
                }
                else if (readtimeavg > 300 && m_readBlockSize > CHUNK)
                {
                    m_readBlockSize -= CHUNK;
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Avg read interval was %1 msec. "
                                "%2K -> %3K block size")
                            .arg(readtimeavg)
                            .arg((m_readBlockSize+CHUNK)/1024)
                            .arg(m_readBlockSize/1024));
                    readtimeavg = 225;
                }
            }
            lastread = now;

            m_rbwLock.lockForRead();
            if (m_rbwPos + totfree > m_bufferSize)
            {
                totfree = m_bufferSize - m_rbwPos;
                LOG(VB_FILE, LOG_DEBUG, LOC +
                    "Shrinking read, near end of buffer");
            }

            if (m_internalReadPos == 0)
            {
                totfree = max(m_fillMin, m_readBlockSize);
                LOG(VB_FILE, LOG_DEBUG, LOC +
                    "Reading enough data to start playback");
            }

            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("safe_read(...@%1, %2) -- begin")
                    .arg(m_rbwPos).arg(totfree));

            MythTimer sr_timer;
            sr_timer.start();

            int rbwposcopy = m_rbwPos;

            // FileRingBuffer::safe_read(RemoteFile*...) acquires poslock;
            // so we need to unlock this here to preserve locking order.
            m_rbwLock.unlock();

            read_return = safe_read(m_readAheadBuffer + rbwposcopy, totfree);

            int sr_elapsed = sr_timer.elapsed();
            uint64_t bps = !sr_elapsed ? 1000000001 :
                           (uint64_t)(((double)read_return * 8000.0) /
                                      (double)sr_elapsed);
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("safe_read(...@%1, %2) -> %3, took %4 ms %5 avg %6 ms")
                    .arg(rbwposcopy).arg(totfree).arg(read_return)
                    .arg(sr_elapsed)
                .arg(QString("(%1Mbps)").arg((double)bps / 1000000.0))
                .arg(readtimeavg));
            UpdateStorageRate(bps);

            if (read_return >= 0)
            {
                m_posLock.lockForWrite();
                m_rbwLock.lockForWrite();

                if (rbwposcopy == m_rbwPos)
                {
                    m_internalReadPos += read_return;
                    m_rbwPos = (m_rbwPos + read_return) % m_bufferSize;
                    LOG(VB_FILE, LOG_DEBUG,
                        LOC + QString("rbwpos += %1K requested %2K in read")
                        .arg(read_return/1024,3).arg(totfree/1024,3));
                }
                m_numFailures = 0;

                m_rbwLock.unlock();
                m_posLock.unlock();

                LOG(VB_FILE, LOG_DEBUG, LOC +
                    QString("total read so far: %1 bytes")
                    .arg(m_internalReadPos));
            }
        }
        else
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("We are not reading anything "
                        "(totfree: %1 commserror:%2 ateof:%3 "
                        "setswitchtonext:%4")
                .arg(totfree).arg(m_commsError).arg(m_ateof).arg(m_setSwitchToNext));
        }

        int used = m_bufferSize - ReadBufFree();

        bool reads_were_allowed = m_readsAllowed;

        ignore_for_read_timing =
            (totfree < m_readBlockSize) || (read_return < totfree);

        if ((0 == read_return) || (m_numFailures > 5) ||
            (m_readsAllowed != (used >= 1 || m_ateof ||
                                m_setSwitchToNext || m_commsError)) ||
            (m_readsDesired != (used >= m_fillMin || m_ateof ||
                                m_setSwitchToNext || m_commsError)))
        {
            // If readpos changes while the lock is released
            // we should not handle the 0 read_return now.
            long long old_readpos = m_readPos;

            m_rwLock.unlock();
            m_rwLock.lockForWrite();

            m_commsError |= (m_numFailures > 5);

            m_readsAllowed = used >= 1 || m_ateof || m_setSwitchToNext || m_commsError;
            m_readsDesired =
                used >= m_fillMin || m_ateof || m_setSwitchToNext || m_commsError;

            if (0 == read_return && old_readpos == m_readPos)
            {
                eofreads++;
                if (eofreads >= 3 && m_readBlockSize >= KB512)
                {
                    // not reading anything
                    m_readBlockSize = CHUNK;
                    CalcReadAheadThresh();
                }

                if (m_liveTVChain)
                {
                    if (!m_setSwitchToNext && !m_ignoreLiveEOF &&
                        m_liveTVChain->HasNext())
                    {
                        // we receive new livetv chain element event
                        // before we receive file closed for writing event
                        // so don't need to test if file is closed for writing
                        m_liveTVChain->SwitchToNext(true);
                        m_setSwitchToNext = true;
                    }
                    else if (gCoreContext->IsRegisteredFileForWrite(m_filename))
                    {
                        LOG(VB_FILE, LOG_DEBUG, LOC +
                            QString("EOF encountered, but %1 still being written to")
                            .arg(m_filename));
                        // We reached EOF, but file still open for writing and
                        // no next program in livetvchain
                        // wait a little bit (60ms same wait as typical safe_read)
                        m_generalWait.wait(&m_rwLock, 60);
                    }
                }
                else if (gCoreContext->IsRegisteredFileForWrite(m_filename))
                {
                    LOG(VB_FILE, LOG_DEBUG, LOC +
                        QString("EOF encountered, but %1 still being written to")
                        .arg(m_filename));
                    // We reached EOF, but file still open for writing,
                    // typically active in-progress recording
                    // wait a little bit (60ms same wait as typical safe_read)
                    m_generalWait.wait(&m_rwLock, 60);
                    m_beingWritten = true;
                }
                else
                {
                    if (m_waitForWrite && !m_beingWritten)
                    {
                        LOG(VB_FILE, LOG_DEBUG, LOC +
                            "Waiting for file to grow large enough to process.");
                        m_generalWait.wait(&m_rwLock, 300);
                    }
                    else
                    {
                        LOG(VB_FILE, LOG_DEBUG,
                            LOC + "setting ateof (read_return == 0)");
                        m_ateof = true;
                    }
                }
            }

            m_rwLock.unlock();
            m_rwLock.lockForRead();
            used = m_bufferSize - ReadBufFree();
        }
        else
        {
            eofreads = 0;
        }

        LOG(VB_FILE, LOG_DEBUG, LOC + "@ end of read ahead loop");

        if (!m_readsAllowed || m_commsError || m_ateof || m_setSwitchToNext ||
            (m_wantToRead <= used && m_wantToRead > 0))
        {
            // To give other threads a good chance to handle these
            // conditions, even if they are only requesting a read lock
            // like us, yield (currently implemented with short usleep).
            m_generalWait.wakeAll();
            m_rwLock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            m_rwLock.lockForRead();
        }
        else
        {
            // yield if we have nothing to do...
            if (!m_requestPause && reads_were_allowed &&
                (used >= m_fillThreshold || m_ateof || m_setSwitchToNext))
            {
                m_generalWait.wait(&m_rwLock, 50);
            }
            else if (m_readsAllowed)
            { // if reads are allowed release the lock and yield so the
              // reader gets a chance to read before the buffer is full.
                m_generalWait.wakeAll();
                m_rwLock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                m_rwLock.lockForRead();
            }
        }
    }

    m_rwLock.unlock();

    m_rwLock.lockForWrite();
    m_rbrLock.lockForWrite();
    m_rbwLock.lockForWrite();

    delete [] m_readAheadBuffer;

    m_readAheadBuffer = nullptr;
    m_rbrPos          = 0;
    m_rbwPos          = 0;
    m_reallyRunning   = false;
    m_readsAllowed    = false;
    m_readsDesired    = false;

    m_rbwLock.unlock();
    m_rbrLock.unlock();
    m_rwLock.unlock();

    LOG(VB_FILE, LOG_INFO, LOC + QString("Exiting readahead thread"));

    RunEpilog();
}

long long RingBuffer::SetAdjustFilesize(void)
{
    m_rwLock.lockForWrite();
    m_posLock.lockForRead();
    m_readAdjust += m_internalReadPos;
    long long ra = m_readAdjust;
    m_posLock.unlock();
    m_rwLock.unlock();
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
    // Wait up to 30000 ms for reads allowed (or readsdesired if post seek/open)
    bool &check = (m_recentSeek || m_readInternalMode) ? m_readsDesired : m_readsAllowed;
    m_recentSeek = false;
    int timeout_ms = 30000;
    int count = 0;
    MythTimer t;
    t.start();

    while ((t.elapsed() < timeout_ms) && !check && !m_stopReads &&
           !m_requestPause && !m_commsError && m_readAheadRunning)
    {
        m_generalWait.wait(&m_rwLock, clamp(timeout_ms - t.elapsed(), 10, 100));
        if (!check && t.elapsed() > 1000 && (count % 100) == 0)
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC +
                "Taking too long to be allowed to read..");
        }
        count++;
    }
    if (t.elapsed() >= timeout_ms)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Took more than %1 seconds to be allowed to read, aborting.")
            .arg(timeout_ms / 1000));
        return false;
    }
    return check;
}

int RingBuffer::WaitForAvail(int count, int timeout)
{
    int avail = ReadBufAvail();
    if (avail >= count)
        return avail;

    count = (m_ateof && avail < count) ? avail : count;

    if (m_liveTVChain && m_setSwitchToNext && avail < count)
    {
        return avail;
    }

    // Make sure that if the read ahead thread is sleeping and
    // it should be reading that we start reading right away.
    if ((avail < count) && !m_stopReads &&
        !m_requestPause && !m_commsError && m_readAheadRunning)
    {
        m_generalWait.wakeAll();
    }

    MythTimer t;
    t.start();
    while ((avail < count) && !m_stopReads &&
           !m_requestPause && !m_commsError && m_readAheadRunning)
    {
        m_wantToRead = count;
        m_generalWait.wait(&m_rwLock, clamp(timeout - t.elapsed(), 10, 250));
        avail = ReadBufAvail();
        if (m_ateof)
            break;
        if (m_lowBuffers && avail >= m_fillMin)
            break;
        if (t.elapsed() > timeout)
            break;
    }

    m_wantToRead = 0;

    return avail;
}

int RingBuffer::ReadDirect(void *buf, int count, bool peek)
{
    long long old_pos = 0;
    if (peek)
    {
        m_posLock.lockForRead();
        old_pos = (m_ignoreReadPos >= 0) ? m_ignoreReadPos : m_readPos;
        m_posLock.unlock();
    }

    MythTimer timer;
    timer.start();
    int ret = safe_read(buf, count);
    int elapsed = timer.elapsed();
    uint64_t bps = !elapsed ? 1000000001 :
                   (uint64_t)(((float)ret * 8000.0F) / (float)elapsed);
    UpdateStorageRate(bps);

    m_posLock.lockForWrite();
    if (m_ignoreReadPos >= 0 && ret > 0)
    {
        if (peek)
        {
            // seek should always succeed since we were at this position
            long long cur_pos = -1;
            if (m_remotefile)
                cur_pos = m_remotefile->Seek(old_pos, SEEK_SET);
            else if (m_fd2 >= 0)
                cur_pos = lseek64(m_fd2, old_pos, SEEK_SET);
            if (cur_pos < 0)
            {
                LOG(VB_FILE, LOG_ERR, LOC +
                    "Seek failed repositioning to previous position");
            }
        }
        else
        {
            m_ignoreReadPos += ret;
        }
        m_posLock.unlock();
        return ret;
    }
    m_posLock.unlock();

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
        QString(" @%1 -- begin").arg(m_rbrPos));

    m_rwLock.lockForRead();
    if (m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + loc_desc +
            ": Attempt to read from a write only file");
        errno = EBADF;
        m_rwLock.unlock();
        return -1;
    }

    if (m_requestPause || m_stopReads || !m_readAheadRunning || (m_ignoreReadPos>=0))
    {
        m_rwLock.unlock();
        m_rwLock.lockForWrite();
        // we need a write lock so the read-ahead thread
        // can't start mucking with the read position.
        // If the read ahead thread was started while we
        // didn't hold the lock, we proceed with a normal
        // read from the buffer, otherwise we read directly.
        if (m_requestPause || m_stopReads ||
            !m_readAheadRunning || (m_ignoreReadPos >= 0))
        {
            int ret = ReadDirect(buf, count, peek);
            LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc +
                QString(": ReadDirect checksum %1")
                    .arg(qChecksum((char*)buf,count)));
            m_rwLock.unlock();
            return ret;
        }
        m_rwLock.unlock();
        m_rwLock.lockForRead();
    }

    if (!WaitForReadsAllowed())
    {
        LOG(VB_FILE, LOG_NOTICE, LOC + loc_desc + ": !WaitForReadsAllowed()");
        m_rwLock.unlock();
        m_stopReads = true; // this needs to be outside the lock
        m_rwLock.lockForWrite();
        m_wantToRead = 0;
        m_rwLock.unlock();
        return 0;
    }

    int avail = ReadBufAvail();
    MythTimer t(MythTimer::kStartRunning);

    // Wait up to 10000 ms for any data
    int timeout_ms = 10000;
    while (!m_readInternalMode && !m_ateof &&
           (t.elapsed() < timeout_ms) && m_readAheadRunning &&
           !m_stopReads && !m_requestPause && !m_commsError)
    {
        avail = WaitForAvail(count, min(timeout_ms - t.elapsed(), 100));
        if (m_liveTVChain && m_setSwitchToNext && avail < count)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC +
                "Checking to see if there's a new livetv program to switch to..");
            m_liveTVChain->ReloadAll();
            break;
        }
        if (avail > 0)
            break;
    }
    if (t.elapsed() > 6000)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + loc_desc +
            QString(" -- waited %1 ms for avail(%2) > count(%3)")
            .arg(t.elapsed()).arg(avail).arg(count));
    }

    if (m_readInternalMode)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC +
            QString("ReadPriv: %1 bytes available, %2 left")
            .arg(avail).arg(avail-m_readOffset));
    }
    count = min(avail - m_readOffset, count);

    if ((count <= 0) && (m_ateof || m_readInternalMode))
    {
        // If we're at the end of file return 0 bytes
        m_rwLock.unlock();
        return count;
    }
    if (count <= 0)
    {
        // If we're not at the end of file but have no data
        // at this point time out and shutdown read ahead.
        LOG(VB_GENERAL, LOG_ERR, LOC + loc_desc +
            QString(" -- timed out waiting for data (%1 ms)")
            .arg(t.elapsed()));

        m_rwLock.unlock();
        m_stopReads = true; // this needs to be outside the lock
        m_rwLock.lockForWrite();
        m_ateof = true;
        m_wantToRead = 0;
        m_generalWait.wakeAll();
        m_rwLock.unlock();
        return count;
    }

    if (peek || m_readInternalMode)
        m_rbrLock.lockForRead();
    else
        m_rbrLock.lockForWrite();

    LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc + " -- copying data");

    int rpos;
    if (m_rbrPos + m_readOffset > (int) m_bufferSize)
    {
        rpos = (m_rbrPos + m_readOffset) - m_bufferSize;
    }
    else
    {
        rpos = m_rbrPos + m_readOffset;
    }
    if (rpos + count > (int) m_bufferSize)
    {
        int firstsize = m_bufferSize - rpos;
        int secondsize = count - firstsize;

        memcpy(buf, m_readAheadBuffer + rpos, firstsize);
        memcpy((char *)buf + firstsize, m_readAheadBuffer, secondsize);
    }
    else
    {
        memcpy(buf, m_readAheadBuffer + rpos, count);
    }
    LOG(VB_FILE, LOG_DEBUG, LOC + loc_desc + QString(" -- checksum %1")
            .arg(qChecksum((char*)buf,count)));

    if (!peek)
    {
        if (m_readInternalMode)
        {
            m_readOffset += count;
        }
        else
        {
            m_rbrPos = (m_rbrPos + count) % m_bufferSize;
            m_generalWait.wakeAll();
        }
    }
    m_rbrLock.unlock();
    m_rwLock.unlock();

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
        m_posLock.lockForWrite();
        m_readPos += ret;
        m_posLock.unlock();
        UpdateDecoderRate(ret);
    }

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
    if (rate > 1000000000)
    {
        return QObject::tr(">1Gbps");
    }
    if (rate >= 1000000)
    {
        msg = hz ? QObject::tr("%1MHz") : QObject::tr("%1Mbps");
        bitrate  = (float)rate / (1000000.0F);
        range = hz ? 3 : 1;
    }
    else if (rate >= 1000)
    {
        msg = hz ? QObject::tr("%1kHz") : QObject::tr("%1kbps");
        bitrate = (float)rate / 1000.0F;
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
    if (m_type == kRingBuffer_DVD || m_type == kRingBuffer_BD)
        return "N/A";

    int avail = (m_rbwPos >= m_rbrPos) ? m_rbwPos - m_rbrPos
                                       : m_bufferSize - m_rbrPos + m_rbwPos;
    return QString("%1%").arg(lroundf((float)avail / (float)m_bufferSize * 100.0F));
}

uint64_t RingBuffer::UpdateDecoderRate(uint64_t latest)
{
    if (!m_bitrateMonitorEnabled)
        return 0;

    // TODO use QDateTime once we've moved to Qt 4.7
    static QTime s_midnight = QTime(0, 0, 0);
    QTime now = QTime::currentTime();
    qint64 age = s_midnight.msecsTo(now);
    qint64 oldest = age - 1000;

    m_decoderReadLock.lock();
    if (latest)
        m_decoderReads.insert(age, latest);

    uint64_t total = 0;
    QMutableMapIterator<qint64,uint64_t> it(m_decoderReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < oldest || it.key() > age)
            it.remove();
        else
            total += it.value();
    }

    auto average = (uint64_t)((double)total * 8.0);
    m_decoderReadLock.unlock();

    LOG(VB_FILE, LOG_INFO, LOC + QString("Decoder read speed: %1 %2")
            .arg(average).arg(m_decoderReads.size()));
    return average;
}

uint64_t RingBuffer::UpdateStorageRate(uint64_t latest)
{
    if (!m_bitrateMonitorEnabled)
        return 0;

    // TODO use QDateTime once we've moved to Qt 4.7
    static QTime s_midnight = QTime(0, 0, 0);
    QTime now = QTime::currentTime();
    qint64 age = s_midnight.msecsTo(now);
    qint64 oldest = age - 1000;

    m_storageReadLock.lock();
    if (latest)
        m_storageReads.insert(age, latest);

    uint64_t total = 0;
    QMutableMapIterator<qint64,uint64_t> it(m_storageReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < oldest || it.key() > age)
            it.remove();
        else
            total += it.value();
    }

    int size = m_storageReads.size();
    m_storageReadLock.unlock();

    uint64_t average = size ? (uint64_t)(((double)total) / (double)size) : 0;

    LOG(VB_FILE, LOG_INFO, LOC + QString("Average storage read speed: %1 %2")
            .arg(average).arg(m_storageReads.size()));
    return average;
}

/** \fn RingBuffer::Write(const void*, uint)
 *  \brief Writes buffer to ThreadedFileWriter::Write(const void*,uint)
 *  \return Bytes written, or -1 on error.
 */
int RingBuffer::Write(const void *buf, uint count)
{
    m_rwLock.lockForRead();

    if (!m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to write to a read only file.");
        m_rwLock.unlock();
        return -1;
    }

    if (!m_tfw && !m_remotefile)
    {
        m_rwLock.unlock();
        return -1;
    }

    int ret = -1;
    if (m_tfw)
        ret = m_tfw->Write(buf, count);
    else
        ret = m_remotefile->Write(buf, count);

    if (ret > 0)
    {
        m_posLock.lockForWrite();
        m_writePos += ret;
        m_posLock.unlock();
    }

    m_rwLock.unlock();

    return ret;
}

/** \fn RingBuffer::Sync(void)
 *  \brief Calls ThreadedFileWriter::Sync(void)
 */
void RingBuffer::Sync(void)
{
    m_rwLock.lockForRead();
    if (m_tfw)
        m_tfw->Sync();
    m_rwLock.unlock();
}

/** \brief Calls ThreadedFileWriter::Seek(long long,int).
 */
long long RingBuffer::WriterSeek(long long pos, int whence, bool has_lock)
{
    long long ret = -1;

    if (!has_lock)
        m_rwLock.lockForRead();

    m_posLock.lockForWrite();

    if (m_tfw)
    {
        ret = m_tfw->Seek(pos, whence);
        m_writePos = ret;
    }

    m_posLock.unlock();

    if (!has_lock)
        m_rwLock.unlock();

    return ret;
}

/** \fn RingBuffer::WriterFlush(void)
 *  \brief Calls ThreadedFileWriter::Flush(void)
 */
void RingBuffer::WriterFlush(void)
{
    m_rwLock.lockForRead();
    if (m_tfw)
        m_tfw->Flush();
    m_rwLock.unlock();
}

/** \fn RingBuffer::SetWriteBufferMinWriteSize(int)
 *  \brief Calls ThreadedFileWriter::SetWriteBufferMinWriteSize(int)
 */
void RingBuffer::SetWriteBufferMinWriteSize(int newMinSize)
{
    m_rwLock.lockForRead();
    if (m_tfw)
        m_tfw->SetWriteBufferMinWriteSize(newMinSize);
    m_rwLock.unlock();
}

/** \fn RingBuffer::WriterSetBlocking(bool)
 *  \brief Calls ThreadedFileWriter::SetBlocking(bool)
 */
bool RingBuffer::WriterSetBlocking(bool block)
{
    QReadLocker lock(&m_rwLock);

    if (m_tfw)
        return m_tfw->SetBlocking(block);
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
    m_rwLock.lockForWrite();
    m_oldfile = is_old;
    m_rwLock.unlock();
}

/// Returns name of file used by this RingBuffer
QString RingBuffer::GetFilename(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_filename;
    m_rwLock.unlock();
    return tmp;
}

QString RingBuffer::GetSubtitleFilename(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_subtitleFilename;
    m_rwLock.unlock();
    return tmp;
}

QString RingBuffer::GetLastError(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_lastError;
    m_rwLock.unlock();
    return tmp;
}

/** \fn RingBuffer::GetWritePosition(void) const
 *  \brief Returns how far into a ThreadedFileWriter file we have written.
 */
long long RingBuffer::GetWritePosition(void) const
{
    m_posLock.lockForRead();
    long long ret = m_writePos;
    m_posLock.unlock();
    return ret;
}

/** \fn RingBuffer::LiveMode(void) const
 *  \brief Returns true if this RingBuffer has been assigned a LiveTVChain.
 *  \sa SetLiveMode(LiveTVChain*)
 */
bool RingBuffer::LiveMode(void) const
{
    m_rwLock.lockForRead();
    bool ret = (m_liveTVChain);
    m_rwLock.unlock();
    return ret;
}

/** \fn RingBuffer::SetLiveMode(LiveTVChain*)
 *  \brief Assigns a LiveTVChain to this RingBuffer
 *  \sa LiveMode(void)
 */
void RingBuffer::SetLiveMode(LiveTVChain *chain)
{
    m_rwLock.lockForWrite();
    m_liveTVChain = chain;
    m_rwLock.unlock();
}

/// Tells RingBuffer whether to ignore the end-of-file
void RingBuffer::IgnoreLiveEOF(bool ignore)
{
    m_rwLock.lockForWrite();
    m_ignoreLiveEOF = ignore;
    m_rwLock.unlock();
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
