// Std
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <thread>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

// Qt
#include <QFile>
#include <QDateTime>
#include <QReadLocker>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythbase/compat.h"
#include "libmythbase/mythcdrom.h"
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythdate.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/mythmiscutil.h"
#include "libmythbase/mythtimer.h"
#include "libmythbase/remotefile.h"
#include "libmythbase/threadedfilewriter.h"

#include "Bluray/mythbdbuffer.h"
#include "DVD/mythdvdbuffer.h"
#include "DVD/mythdvdstream.h"
#include "HLS/httplivestreambuffer.h"
#include "io/mythfilebuffer.h"
#include "io/mythmediabuffer.h"
#include "io/mythstreamingbuffer.h"
#include "livetvchain.h"

// FFmpeg
extern "C" {
#include "libavformat/avformat.h"
}

#define LOC      QString("RingBuf(%1): ").arg(m_filename)


/*
  Locking relations:
    rwlock->poslock->rbrlock->rbwlock

  A child should never lock any of the parents without locking
  the parent lock before the child lock.
  void MythMediaBuffer::Example1()
  {
      poslock.lockForWrite();
      rwlock.lockForRead(); // error!
      blah(); // <- does not implicitly acquire any locks
      rwlock.unlock();
      poslock.unlock();
  }
  void MythMediaBuffer::Example2()
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

/*! \brief Creates a RingBuffer instance.
 *
 *   You can explicitly disable the readahead thread by setting
 *   readahead to false or by just not calling Start(void).
 *
 *  \param Filename     Name of file to read or write.
 *  \param Write        If true an encapsulated writer is created
 *  \param UseReadAhead If false a call to Start(void) will not
 *                      a pre-buffering thread, otherwise Start(void)
 *                      will start a pre-buffering thread.
 *  \param Timeout      if < 0, then we will not open the file.
 *                      Otherwise it's how long to try opening
 *                      the file after the first failure in
 *                      milliseconds before giving up.
 *  \param StreamOnly   If true disallow DVD and Bluray (used by FileTransfer)
*/
MythMediaBuffer *MythMediaBuffer::Create(const QString &Filename, bool Write,
                               bool UseReadAhead, std::chrono::milliseconds Timeout, bool StreamOnly)
{
    QString filename = Filename;
    QString lower = filename.toLower();

    if (Write)
        return new MythFileBuffer(filename, Write, UseReadAhead, Timeout);

    bool dvddir  = false;
    bool bddir   = false;
    bool httpurl = lower.startsWith("http://") || lower.startsWith("https://");
    bool iptvurl = lower.startsWith("rtp://") || lower.startsWith("tcp://") ||
                   lower.startsWith("udp://");
    bool mythurl = lower.startsWith("myth://");
    bool bdurl   = lower.startsWith("bd:");
    bool dvdurl  = lower.startsWith("dvd:");
    bool imgext  = lower.endsWith(".img") || lower.endsWith(".iso");

    if (imgext)
    {
        switch (MythCDROM::inspectImage(filename))
        {
            case MythCDROM::kBluray:
                bdurl = true;
                break;
            case MythCDROM::kDVD:
                dvdurl = true;
                break;
            default: break;
        }
    }

    if (httpurl || iptvurl)
    {
        if (!iptvurl && HLSRingBuffer::TestForHTTPLiveStreaming(filename))
            return new HLSRingBuffer(filename);
        return new MythStreamingBuffer(filename);
    }
    if (!StreamOnly && mythurl)
    {
        struct stat fileInfo {};
        if ((RemoteFile::Exists(filename, &fileInfo)) &&
            (S_ISDIR(fileInfo.st_mode)))
        {
            if (RemoteFile::Exists(filename + "/VIDEO_TS"))
                dvddir = true;
            else if (RemoteFile::Exists(filename + "/BDMV"))
                bddir = true;
        }
    }
    else if (!StreamOnly && !mythurl)
    {
        if (QFile::exists(filename + "/VIDEO_TS"))
            dvddir = true;
        else if (QFile::exists(filename + "/BDMV"))
            bddir  = true;
    }

    if (!StreamOnly && (dvdurl || dvddir))
    {
        if (filename.startsWith("dvd:"))        // URI "dvd:" + path
            filename.remove(0,4);              // e.g. "dvd:/dev/dvd"

        if (!(mythurl || QFile::exists(filename)))
            filename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying DVD at " + filename);
        return new MythDVDBuffer(filename);
    }

    if (!StreamOnly && (bdurl || bddir))
    {
        if (filename.startsWith("bd:"))        // URI "bd:" + path
            filename.remove(0,3);             // e.g. "bd:/videos/ET"

        if (!(mythurl || QFile::exists(filename)))
            filename = "/dev/dvd";
        LOG(VB_PLAYBACK, LOG_INFO, "Trying BD at " + filename);
        return new MythBDBuffer(filename);
    }

    if (!mythurl && imgext && filename.startsWith("dvd:"))
    {
        LOG(VB_PLAYBACK, LOG_INFO, "DVD image at " + filename);
        return new MythDVDStream(filename);
    }

    if (!mythurl && lower.endsWith(".vob") && filename.contains("/VIDEO_TS/"))
    {
        LOG(VB_PLAYBACK, LOG_INFO, "DVD VOB at " + filename);
        auto *dvdstream = new MythDVDStream(filename);
        if (dvdstream && dvdstream->IsOpen())
            return dvdstream;
        delete dvdstream;
    }

    return new MythFileBuffer(filename, Write, UseReadAhead, Timeout);
}

MythMediaBuffer::MythMediaBuffer(MythBufferType Type)
  : MThread("RingBuffer"),
    m_type(Type)
{
}

MythBufferType MythMediaBuffer::GetType(void) const
{
    return m_type;
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
MythMediaBuffer::~MythMediaBuffer(void)
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

/** \fn MythMediaBuffer::Reset(bool, bool, bool)
 *  \brief Resets the read-ahead thread and our position in the file
 */
void MythMediaBuffer::Reset(bool Full, bool ToAdjust, bool ResetInternal)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Reset(%1,%2,%3)")
            .arg(Full).arg(ToAdjust).arg(ResetInternal));

    m_rwLock.lockForWrite();
    m_posLock.lockForWrite();

    m_numFailures = 0;
    m_commsError = false;
    m_setSwitchToNext = false;

    m_writePos = 0;
    m_readPos = (ToAdjust) ? (m_readPos - m_readAdjust) : 0;

    if (m_readPos != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("MythMediaBuffer::Reset() nonzero readpos.  toAdjust: %1 "
                    "readpos: %2 readAdjust: %3")
                .arg(ToAdjust).arg(m_readPos).arg(m_readAdjust));
    }

    m_readAdjust = 0;
    m_readPos = (m_readPos < 0) ? 0 : m_readPos;

    if (Full)
        ResetReadAhead(m_readPos);

    if (ResetInternal)
        m_internalReadPos = m_readPos;

    m_generalWait.wakeAll();
    m_posLock.unlock();
    m_rwLock.unlock();
}

/**
 *  \brief Set the raw bit rate, to allow RingBuffer adjust effective bitrate.
 *  \param RawBitrate Streams average number of kilobits per second when playspeed is 1.0
 */
void MythMediaBuffer::UpdateRawBitrate(uint RawBitrate)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("UpdateRawBitrate(%1Kb)").arg(RawBitrate));

    // an audio only stream could be as low as 64Kb (DVB radio) and
    // an MHEG only stream is likely to be reported as 0Kb
    if (RawBitrate < 64)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("Bitrate too low - setting to 64Kb"));
        RawBitrate = 64;
    }
    else if (RawBitrate > 100000)
    {
        LOG(VB_FILE, LOG_INFO, LOC + QString("Bitrate too high - setting to 100Mb"));
        RawBitrate = 100000;
    }

    m_rwLock.lockForWrite();
    m_rawBitrate = RawBitrate;
    CalcReadAheadThresh();
    m_bitrateInitialized = true;
    m_rwLock.unlock();
}

/** \fn MythMediaBuffer::UpdatePlaySpeed(float)
 *  \brief Set the play speed, to allow RingBuffer adjust effective bitrate.
 *  \param play_speed Speed to set. (1.0 for normal speed)
 */
void MythMediaBuffer::UpdatePlaySpeed(float PlaySpeed)
{
    m_rwLock.lockForWrite();
    m_playSpeed = PlaySpeed;
    CalcReadAheadThresh();
    m_rwLock.unlock();
}

void MythMediaBuffer::EnableBitrateMonitor(bool Enable)
{
    m_bitrateMonitorEnabled = Enable;
}

void MythMediaBuffer::SetWaitForWrite(void)
{
    m_waitForWrite = true;
}

/** \fn MythMediaBuffer::SetBufferSizeFactors(bool, bool)
 *  \brief Tells RingBuffer that the raw bitrate may be inaccurate and the
 *         underlying container is matroska, both of which may require a larger
 *         buffer size.
 */
void MythMediaBuffer::SetBufferSizeFactors(bool EstBitrate, bool Matroska)
{
    m_rwLock.lockForWrite();
    m_unknownBitrate = EstBitrate;
    m_fileIsMatroska = Matroska;
    m_rwLock.unlock();
    CreateReadAheadBuffer();
}

static inline uint estbitrate_to_rbs(uint estbitrate)
{
    if (estbitrate > 18000)
        return 512*1024;
    if (estbitrate >  9000)
        return 256*1024;
    if (estbitrate >  5000)
        return 128*1024;
    if (estbitrate >  2500)
        return 64*1024;
    if (estbitrate >  1250)
        return 32*1024;
    if (estbitrate >=  500)
        return 16*1024;
    if (estbitrate >   250)
        return 8*1024;
    if (estbitrate >   125)
        return 4*1024;
    return 2*1024;
}

/** \fn MythMediaBuffer::CalcReadAheadThresh(void)
 *  \brief Calculates m_fillMin, m_fillThreshold, and m_readBlockSize
 *         from the estimated effective bitrate of the stream.
 *
 *  \warning Must be called with rwlock in write lock state.
 *
 */
void MythMediaBuffer::CalcReadAheadThresh(void)
{
    uint estbitrate = 0;

    m_readsAllowed   = false;
    m_readsDesired   = false;

    // loop without sleeping if the buffered data is less than this
    m_fillThreshold = 7 * m_bufferSize / 8;

    estbitrate     = static_cast<uint>(std::max(abs(m_rawBitrate * m_playSpeed), 0.5F * m_rawBitrate));
    estbitrate     = std::min(m_rawBitrate * 3, estbitrate);
    int const rbs = estbitrate_to_rbs(estbitrate);

    if (rbs < DEFAULT_CHUNK_SIZE)
        m_readBlockSize = rbs;
    else
        m_readBlockSize = m_bitrateInitialized ? std::max(rbs, m_readBlockSize) : rbs;

    // minimum seconds of buffering before allowing read
    float secs_min = 0.3F;
    // set the minimum buffering before allowing ffmpeg read
    m_fillMin = static_cast<int>((estbitrate * 1000 * secs_min) * 0.125F);
    // make this a multiple of ffmpeg block size..
    if (m_fillMin >= DEFAULT_CHUNK_SIZE || rbs >= DEFAULT_CHUNK_SIZE)
    {
        if (m_lowBuffers)
            LOG(VB_GENERAL, LOG_INFO, LOC + "Buffering optimisations disabled.");
        m_lowBuffers = false;
        m_fillMin = ((m_fillMin / DEFAULT_CHUNK_SIZE) + 1) * DEFAULT_CHUNK_SIZE;
        m_fillMin = std::min(m_fillMin, static_cast<int>(m_bufferSize / 2));
    }
    else
    {
        m_lowBuffers = true;
        LOG(VB_GENERAL, LOG_WARNING, "Enabling buffering optimisations for low bitrate stream.");
    }

    LOG(VB_FILE, LOG_INFO, LOC +
        QString("CalcReadAheadThresh(%1 Kb)\n\t\t\t -> "
                "threshold(%2 KB) min read(%3 KB) blk size(%4 KB)")
            .arg(estbitrate).arg(m_fillThreshold/1024)
            .arg(m_fillMin/1024).arg(m_readBlockSize/1024));
}

bool MythMediaBuffer::IsNearEnd(double /*Framerate*/, uint Frames) const
{
    QReadLocker lock(&m_rwLock);

    // file is still being read, so can't be finished
    if (!m_ateof && !m_setSwitchToNext)
        return false;

    m_posLock.lockForRead();
    long long readpos = m_readPos;
    long long size = m_internalReadPos - m_readPos;
    m_posLock.unlock();

    // telecom kilobytes (i.e. 1000 per k not 1024)
    uint tmp = static_cast<uint>(std::max(abs(m_rawBitrate * m_playSpeed), 0.5F * m_rawBitrate));
    uint kbitspersec = std::min(m_rawBitrate * 3, tmp);
    if (kbitspersec == 0)
        return false;

    double readahead_time = size / (kbitspersec * (1000.0 / 8.0));

    bool near_end = readahead_time <= 1.5;
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "IsReallyNearEnd()" +
            QString(" br(%1KB)").arg(kbitspersec/8) +
            QString(" sz(%1KB)").arg(size / 1000LL) +
            QString(" vfl(%1)").arg(Frames) +
            QString(" time(%1)").arg(readahead_time) +
            QString(" rawbitrate(%1)").arg(m_rawBitrate) +
            QString(" avail(%1)").arg(size) +
            QString(" internal_size(%1)").arg(m_internalReadPos) +
            QString(" readposition(%1)").arg(readpos) +
            QString(" stopreads(%1)").arg(m_stopReads) +
            QString(" paused(%1)").arg(m_paused) +
            QString(" ne:%1").arg(near_end));
    return near_end;
}

/// \brief Returns number of bytes available for reading into buffer.
/// \warning Must be called with rwlock in locked state.
int MythMediaBuffer::ReadBufFree(void) const
{
    m_rbrLock.lockForRead();
    m_rbwLock.lockForRead();
    int ret = ((m_rbwPos >= m_rbrPos) ? m_rbrPos + static_cast<int>(m_bufferSize) : m_rbrPos) - m_rbwPos - 1;
    m_rbwLock.unlock();
    m_rbrLock.unlock();
    return ret;
}

/// \brief Returns number of bytes available for reading from buffer.
int MythMediaBuffer::GetReadBufAvail(void) const
{
    QReadLocker lock(&m_rwLock);
    return ReadBufAvail();
}

long long MythMediaBuffer::GetRealFileSize(void) const
{
    {
        QReadLocker lock(&m_rwLock);
        if (m_readInternalMode)
            return ReadBufAvail();
    }

    return GetRealFileSizeInternal();
}

long long MythMediaBuffer::Seek(long long Position, int Whence, bool HasLock)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("Seek: Position:%1 Type: %2 Locked: %3)")
        .arg(Position)
        .arg((SEEK_SET == Whence) ? "SEEK_SET" : ((SEEK_CUR == Whence) ? "SEEK_CUR" : "SEEK_END"),
             HasLock?"locked":"unlocked"));

    if (!HasLock)
        m_rwLock.lockForWrite();

    long long ret = 0;

    if (m_readInternalMode)
    {
        m_posLock.lockForWrite();
        // only valid for SEEK_SET & SEEK_CUR
        switch (Whence)
        {
            case SEEK_SET:
                m_readPos = Position;
                break;
            case SEEK_CUR:
                m_readPos += Position;
                break;
            case SEEK_END:
                m_readPos = ReadBufAvail() - Position;
                break;
        }
        m_readOffset = static_cast<int>(m_readPos);
        m_posLock.unlock();
        ret = m_readPos;
    }
    else
    {
        ret = SeekInternal(Position, Whence);
    }

    m_generalWait.wakeAll();

    if (!HasLock)
        m_rwLock.unlock();
    return ret;
}

bool MythMediaBuffer::SetReadInternalMode(bool Mode)
{
    QWriteLocker lock(&m_rwLock);
    bool old = m_readInternalMode;

    if (Mode == old)
        return old;

    m_readInternalMode = Mode;

    if (!Mode)
    {
        // adjust real read position in ringbuffer
        m_rbrLock.lockForWrite();
        m_rbrPos = (m_rbrPos + m_readOffset) % static_cast<int>(m_bufferSize);
        m_generalWait.wakeAll();
        m_rbrLock.unlock();
        // reset the read offset as we are exiting the internal read mode
        m_readOffset = 0;
    }

    LOG(VB_FILE, LOG_DEBUG, LOC + QString("SetReadInternalMode: %1").arg(Mode ? "on" : "off"));
    return old;
}

bool MythMediaBuffer::IsReadInternalMode(void) const
{
    return m_readInternalMode;
}

/// \brief Returns number of bytes available for reading from buffer.
/// \warning Must be called with rwlock in locked state.
int MythMediaBuffer::ReadBufAvail(void) const
{
    m_rbrLock.lockForRead();
    m_rbwLock.lockForRead();
    int ret = (m_rbwPos >= m_rbrPos) ? m_rbwPos - m_rbrPos : static_cast<int>(m_bufferSize) - m_rbrPos + m_rbwPos;
    m_rbwLock.unlock();
    m_rbrLock.unlock();
    return ret;
}

/** \fn MythMediaBuffer::ResetReadAhead(long long)
 *  \brief Restart the read-ahead thread at the 'newinternal' position.
 *
 *   This is called after a Seek(long long, int) so that the read-ahead
 *   buffer doesn't contain any stale data, and so that it will read
 *   any new data from the new position in the file.
 *
 *  \warning Must be called with rwlock and poslock in write lock state.
 *  \param NewInternal Position in file to start reading data from
 */
void MythMediaBuffer::ResetReadAhead(long long NewInternal)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("ResetReadAhead(internalreadpos = %1->%2)")
            .arg(m_internalReadPos).arg(NewInternal));

    m_readInternalMode = false;
    m_readOffset = 0;

    m_rbrLock.lockForWrite();
    m_rbwLock.lockForWrite();

    CalcReadAheadThresh();

    m_rbrPos          = 0;
    m_rbwPos          = 0;
    m_internalReadPos = NewInternal;
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
void MythMediaBuffer::Start(void)
{
    bool dostart = true;

    m_rwLock.lockForWrite();
    if (!m_startReadAhead)
    {
        dostart = false;
    }
    else if (m_writeMode)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread - write only RingBuffer");
        dostart = false;
    }
    else if (m_readAheadRunning)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not starting read ahead thread - already running");
        dostart = false;
    }

    if (!dostart)
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

/** \fn MythMediaBuffer::KillReadAheadThread(void)
 *  \brief Stops the read-ahead thread, and waits for it to stop.
 */
void MythMediaBuffer::KillReadAheadThread(void)
{
    while (isRunning())
    {
        m_rwLock.lockForWrite();
        m_readAheadRunning = false;
        StopReads();
        m_generalWait.wakeAll();
        m_rwLock.unlock();
        MThread::wait(5s);
    }
}

/** \fn MythMediaBuffer::StopReads(void)
 *  \sa StartReads(void), Pause(void)
 */
void MythMediaBuffer::StopReads(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "StopReads()");
    m_stopReads = true;
    m_generalWait.wakeAll();
}

/** \fn MythMediaBuffer::StartReads(void)
 *  \sa StopReads(void), Unpause(void)
 */
void MythMediaBuffer::StartReads(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "StartReads()");
    m_stopReads = false;
    m_generalWait.wakeAll();
}

/** \fn MythMediaBuffer::Pause(void)
 *  \brief Pauses the read-ahead thread. Calls StopReads(void).
 *  \sa Unpause(void), WaitForPause(void)
 */
void MythMediaBuffer::Pause(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "Pausing read ahead thread");
    StopReads();

    m_rwLock.lockForWrite();
    m_requestPause = true;
    m_rwLock.unlock();
}

/** \fn MythMediaBuffer::Unpause(void)
 *  \brief Unpauses the read-ahead thread. Calls StartReads(void).
 *  \sa Pause(void)
 */
void MythMediaBuffer::Unpause(void)
{
    LOG(VB_FILE, LOG_INFO, LOC + "Unpausing readahead thread");
    StartReads();

    m_rwLock.lockForWrite();
    m_requestPause = false;
    m_generalWait.wakeAll();
    m_rwLock.unlock();
}

/** \fn MythMediaBuffer::WaitForPause(void)
 *  \brief Waits for Pause(void) to take effect.
 */
void MythMediaBuffer::WaitForPause(void)
{
    MythTimer t;
    t.start();

    m_rwLock.lockForRead();
    while (m_readAheadRunning && !m_paused && m_requestPause)
    {
        m_generalWait.wait(&m_rwLock, 1000);
        if (m_readAheadRunning && !m_paused && m_requestPause && t.elapsed() > 1s)
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Waited %1 ms for ringbuffer pause")
                .arg(t.elapsed().count()));
    }
    m_rwLock.unlock();
}

bool MythMediaBuffer::PauseAndWait(void)
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

void MythMediaBuffer::CreateReadAheadBuffer(void)
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
    if (m_readAheadBuffer && (oldsize >= newsize))
    {
        m_posLock.unlock();
        m_rwLock.unlock();
        return;
    }

    m_bufferSize = newsize;
    if (m_readAheadBuffer)
    {
        char* newbuffer = new char[m_bufferSize + 1024];
        memcpy(newbuffer, m_readAheadBuffer + m_rbwPos, oldsize - static_cast<uint>(m_rbwPos));
        memcpy(newbuffer + (oldsize - static_cast<uint>(m_rbwPos)), m_readAheadBuffer, static_cast<uint>(m_rbwPos));
        delete [] m_readAheadBuffer;
        m_readAheadBuffer = newbuffer;
        m_rbrPos = (m_rbrPos > m_rbwPos) ? (m_rbrPos - m_rbwPos) :
                                           (m_rbrPos + static_cast<int>(oldsize) - m_rbwPos);
        m_rbwPos = static_cast<int>(oldsize);
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

void MythMediaBuffer::run(void)
{
    RunProlog();

    // These variables are used to adjust the read block size
    std::chrono::milliseconds readTimeAvg = 300ms;
    bool ignoreForReadTiming = true;
    int  eofreads = 0;

    auto lastread = nowAsDuration<std::chrono::milliseconds>();

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

    LOG(VB_FILE, LOG_INFO, LOC + QString("Initial readblocksize %1K fillMin %2K")
            .arg(m_readBlockSize/1024).arg(m_fillMin/1024));

    while (m_readAheadRunning)
    {
        m_rwLock.unlock();
        bool isopened = IsOpen();
        m_rwLock.lockForRead();

        if (!isopened)
        {
            LOG(VB_FILE, LOG_WARNING, LOC + QString("File not opened, terminating readahead thread"));
            m_posLock.lockForWrite();
            m_readAheadRunning = false;
            m_generalWait.wakeAll();
            m_posLock.unlock();
            break;
        }
        if (PauseAndWait())
        {
            ignoreForReadTiming = true;
            LOG(VB_FILE, LOG_DEBUG, LOC + "run: PauseAndWait Not reading continuing");
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
            ignoreForReadTiming |= (m_ignoreReadPos >= 0) || m_commsError || m_stopReads;
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
            ignoreForReadTiming = true;
            m_generalWait.wait(&m_rwLock, 1000);
            totfree = ReadBufFree();
        }

        int readResult = -1;
        if (totfree >= KB32 && !m_commsError && !m_ateof && !m_setSwitchToNext)
        {
            // limit the read size
            if (m_readBlockSize > totfree)
                totfree = (totfree / KB32) * KB32; // must be multiple of 32KB
            else
                totfree = m_readBlockSize;

            // adapt blocksize
            auto now = nowAsDuration<std::chrono::milliseconds>();
            if (!ignoreForReadTiming)
            {
                readTimeAvg = (readTimeAvg * 9 + (now - lastread)) / 10;

                if (readTimeAvg < 150ms &&
                    static_cast<uint>(m_readBlockSize) < (BUFFER_SIZE_MINIMUM >>2) &&
                    m_readBlockSize >= DEFAULT_CHUNK_SIZE /* low_buffers */ &&
                    m_readBlockSize <= KB512)
                {
                    int old_block_size = m_readBlockSize;
                    m_readBlockSize = 3 * m_readBlockSize / 2;
                    m_readBlockSize = ((m_readBlockSize+DEFAULT_CHUNK_SIZE-1) / DEFAULT_CHUNK_SIZE) * DEFAULT_CHUNK_SIZE;
                    m_readBlockSize = std::min(m_readBlockSize, KB512);
                    LOG(VB_FILE, LOG_INFO, LOC + QString("Avg read interval was %1 msec. "
                                                         "%2K -> %3K block size")
                        .arg(readTimeAvg.count()).arg(old_block_size/1024).arg(m_readBlockSize/1024));
                    readTimeAvg = 225ms;
                }
                else if (readTimeAvg > 300ms && m_readBlockSize > DEFAULT_CHUNK_SIZE)
                {
                    m_readBlockSize -= DEFAULT_CHUNK_SIZE;
                    LOG(VB_FILE, LOG_INFO, LOC +
                        QString("Avg read interval was %1 msec. %2K -> %3K block size")
                            .arg(readTimeAvg.count())
                            .arg((m_readBlockSize+DEFAULT_CHUNK_SIZE)/1024)
                            .arg(m_readBlockSize/1024));
                    readTimeAvg = 225ms;
                }
            }
            lastread = now;

            m_rbwLock.lockForRead();
            if (m_rbwPos + totfree > m_bufferSize)
            {
                totfree = m_bufferSize - static_cast<uint>(m_rbwPos);
                LOG(VB_FILE, LOG_DEBUG, LOC + "Shrinking read, near end of buffer");
            }

            if (m_internalReadPos == 0)
            {
                totfree = std::max(m_fillMin, m_readBlockSize);
                LOG(VB_FILE, LOG_DEBUG, LOC + "Reading enough data to start playback");
            }

            LOG(VB_FILE, LOG_DEBUG, LOC + QString("safe_read(...@%1, %2) -- begin")
                .arg(m_rbwPos).arg(totfree));

            MythTimer sr_timer;
            sr_timer.start();

            int rbwposcopy = m_rbwPos;

            // MythFileBuffer::SafeRead(RemoteFile*...) acquires poslock;
            // so we need to unlock this here to preserve locking order.
            m_rbwLock.unlock();

            readResult = SafeRead(m_readAheadBuffer + rbwposcopy, static_cast<uint>(totfree));

            int sr_elapsed = sr_timer.elapsed().count();
            uint64_t bps = !sr_elapsed ? 1000000001 :
                           static_cast<uint64_t>((readResult * 8000.0) / static_cast<double>(sr_elapsed));
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("safe_read(...@%1, %2) -> %3, took %4 ms %5 avg %6 ms")
                .arg(rbwposcopy).arg(totfree).arg(readResult).arg(sr_elapsed)
                .arg(QString("(%1Mbps)").arg(static_cast<double>(bps) / 1000000.0))
                .arg(readTimeAvg.count()));
            UpdateStorageRate(bps);

            if (readResult >= 0)
            {
                m_posLock.lockForWrite();
                m_rbwLock.lockForWrite();

                if (rbwposcopy == m_rbwPos)
                {
                    m_internalReadPos += readResult;
                    m_rbwPos = (m_rbwPos + readResult) % static_cast<int>(m_bufferSize);
                    LOG(VB_FILE, LOG_DEBUG, LOC + QString("rbwpos += %1K requested %2K in read")
                        .arg(readResult/1024,3).arg(totfree/1024,3));
                }
                m_numFailures = 0;

                m_rbwLock.unlock();
                m_posLock.unlock();

                LOG(VB_FILE, LOG_DEBUG, LOC + QString("total read so far: %1 bytes")
                    .arg(m_internalReadPos));
            }
        }
        else
        {
            LOG(VB_FILE, LOG_DEBUG, LOC +
                QString("We are not reading anything (totfree: %1 commserror:%2 ateof:%3 setswitchtonext:%4")
                .arg(totfree).arg(m_commsError).arg(m_ateof).arg(m_setSwitchToNext));
        }

        int used = static_cast<int>(m_bufferSize) - ReadBufFree();
        bool readsWereAllowed = m_readsAllowed;

        ignoreForReadTiming = (totfree < m_readBlockSize) || (readResult < totfree);

        if ((0 == readResult) || (m_numFailures > 5) ||
            (m_readsAllowed != (used >= 1 || m_ateof || m_setSwitchToNext || m_commsError)) ||
            (m_readsDesired != (used >= m_fillMin || m_ateof || m_setSwitchToNext || m_commsError)))
        {
            // If readpos changes while the lock is released
            // we should not handle the 0 read_return now.
            long long oldreadposition = m_readPos;

            m_rwLock.unlock();
            m_rwLock.lockForWrite();

            m_commsError |= (m_numFailures > 5);

            m_readsAllowed = used >= 1 || m_ateof || m_setSwitchToNext || m_commsError;
            m_readsDesired = used >= m_fillMin || m_ateof || m_setSwitchToNext || m_commsError;

            if ((0 == readResult) && (oldreadposition == m_readPos))
            {
                eofreads++;
                if (eofreads >= 3 && m_readBlockSize >= KB512)
                {
                    // not reading anything
                    m_readBlockSize = DEFAULT_CHUNK_SIZE;
                    CalcReadAheadThresh();
                }

                if (m_liveTVChain)
                {
                    if (!m_setSwitchToNext && !m_ignoreLiveEOF && m_liveTVChain->HasNext())
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
                        LOG(VB_FILE, LOG_DEBUG, LOC + "Waiting for file to grow large enough to process.");
                        m_generalWait.wait(&m_rwLock, 300);
                    }
                    else
                    {
                        LOG(VB_FILE, LOG_DEBUG, LOC + "setting ateof (readResult == 0)");
                        m_ateof = true;
                    }
                }
            }

            m_rwLock.unlock();
            m_rwLock.lockForRead();
            used = static_cast<int>(m_bufferSize) - ReadBufFree();
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
            std::this_thread::sleep_for(5ms);
            m_rwLock.lockForRead();
        }
        else
        {
            // yield if we have nothing to do...
            if (!m_requestPause && readsWereAllowed &&
               (used >= m_fillThreshold || m_ateof || m_setSwitchToNext))
            {
                m_generalWait.wait(&m_rwLock, 50);
            }
            else if (m_readsAllowed)
            {
                // if reads are allowed release the lock and yield so the
                // reader gets a chance to read before the buffer is full.
                m_generalWait.wakeAll();
                m_rwLock.unlock();
                std::this_thread::sleep_for(5ms);
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

long long MythMediaBuffer::SetAdjustFilesize(void)
{
    m_rwLock.lockForWrite();
    m_posLock.lockForRead();
    m_readAdjust += m_internalReadPos;
    long long readadjust = m_readAdjust;
    m_posLock.unlock();
    m_rwLock.unlock();
    return readadjust;
}

/// \note Only works with readahead
int MythMediaBuffer::Peek(void *Buffer, int Count)
{
    int result = ReadPriv(Buffer, Count, true);
    if (result != Count)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Peek requested %1 bytes but only have %2")
            .arg(Count).arg(result));
    }
    return result;
}

int MythMediaBuffer::Peek(std::vector<char>& Buffer)
{
    return Peek(Buffer.data(), Buffer.size());
};

bool MythMediaBuffer::WaitForReadsAllowed(void)
{
    // Wait up to 30000 ms for reads allowed (or readsdesired if post seek/open)
    bool &check = (m_recentSeek || m_readInternalMode) ? m_readsDesired : m_readsAllowed;
    m_recentSeek = false;
    std::chrono::milliseconds timeoutms = 30s;
    int count = 0;
    MythTimer timer;
    timer.start();

    while ((timer.elapsed() < timeoutms) && !check && !m_stopReads &&
           !m_requestPause && !m_commsError && m_readAheadRunning)
    {
        std::chrono::milliseconds delta = clamp(timeoutms - timer.elapsed(), 10ms, 100ms);
        m_generalWait.wait(&m_rwLock, delta.count());
        if (!check && timer.elapsed() > 1s && (count % 100) == 0)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Taking too long to be allowed to read..");
        count++;
    }

    if (timer.elapsed() >= timeoutms)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Took more than %1 seconds to be allowed to read, aborting.")
            .arg(duration_cast<std::chrono::seconds>(timeoutms).count()));
        return false;
    }
    return check;
}

int MythMediaBuffer::WaitForAvail(int Count, std::chrono::milliseconds  Timeout)
{
    int available = ReadBufAvail();
    if (available >= Count)
        return available;

    if (m_ateof)
    {
        m_wantToRead = 0;
        return available;
    }

    if (m_liveTVChain && m_setSwitchToNext)
        return available;

    // Make sure that if the read ahead thread is sleeping and
    // it should be reading that we start reading right away.
    if (!m_stopReads && !m_requestPause && !m_commsError && m_readAheadRunning)
        m_generalWait.wakeAll();

    MythTimer timer;
    timer.start();
    while ((available < Count) && !m_stopReads && !m_requestPause && !m_commsError && m_readAheadRunning)
    {
        m_wantToRead = Count;
        std::chrono::milliseconds delta = clamp(Timeout - timer.elapsed(), 10ms, 250ms);
        m_generalWait.wait(&m_rwLock, delta.count());
        available = ReadBufAvail();
        if (m_ateof)
            break;
        if (m_lowBuffers && available >= m_fillMin)
            break;
        if (timer.elapsed() > Timeout)
            break;
    }

    m_wantToRead = 0;
    return available;
}

int MythMediaBuffer::ReadDirect(void *Buffer, int Count, bool Peek)
{
    long long oldposition = 0;
    if (Peek)
    {
        m_posLock.lockForRead();
        oldposition = (m_ignoreReadPos >= 0) ? m_ignoreReadPos : m_readPos;
        m_posLock.unlock();
    }

    MythTimer timer;
    timer.start();
    int result = SafeRead(Buffer, static_cast<uint>(Count));
    int elapsed = timer.elapsed().count();
    uint64_t bps = !elapsed ? 1000000001 : static_cast<uint64_t>((result * 8000.0) / static_cast<double>(elapsed));
    UpdateStorageRate(bps);

    m_posLock.lockForWrite();
    if (m_ignoreReadPos >= 0 && result > 0)
    {
        if (Peek)
        {
            // seek should always succeed since we were at this position
            long long cur_pos = -1;
            if (m_remotefile)
                cur_pos = m_remotefile->Seek(oldposition, SEEK_SET);
            else if (m_fd2 >= 0)
                cur_pos = lseek64(m_fd2, oldposition, SEEK_SET);
            if (cur_pos < 0)
            {
                LOG(VB_FILE, LOG_ERR, LOC + "Seek failed repositioning to previous position");
            }
        }
        else
        {
            m_ignoreReadPos += result;
        }
        m_posLock.unlock();
        return result;
    }
    m_posLock.unlock();

    if (Peek && (result > 0))
    {
        if ((IsDVD() || IsBD()) && oldposition != 0)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                    "DVD and Blu-Ray do not support arbitrary "
                    "peeks except when read-ahead is enabled."
                    "\n\t\t\tWill seek to beginning of video.");
            oldposition = 0;
        }

        long long newposition = Seek(oldposition, SEEK_SET, true);

        if (newposition != oldposition)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Peek() Failed to return from new position %1 to old position %2, now "
                        "at position %3")
                    .arg(oldposition - result).arg(oldposition).arg(newposition));
        }
    }

    return result;
}

/** \brief When possible reads from the read-ahead buffer,
 *         otherwise reads directly from the device.
 *
 *  \param  Buffer Pointer to where data will be written
 *  \param  Count  Number of bytes to read
 *  \param  Peek   If true, don't increment read count
 *  \return Returns number of bytes read
 */
int MythMediaBuffer::ReadPriv(void *Buffer, int Count, bool Peek)
{
    QString desc = QString("ReadPriv(..%1, %2)").arg(Count).arg(Peek ? "peek" : "normal");
    LOG(VB_FILE, LOG_DEBUG, LOC + desc + QString(" @%1 -- begin").arg(m_rbrPos));

    m_rwLock.lockForRead();
    if (m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + desc + ": Attempt to read from a write only file");
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
        if (m_requestPause || m_stopReads || !m_readAheadRunning || (m_ignoreReadPos >= 0))
        {
            int result = ReadDirect(Buffer, Count, Peek);
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
            quint16 checksum = qChecksum(reinterpret_cast<char*>(Buffer), static_cast<uint>(Count));
#else
            QByteArrayView BufferView(reinterpret_cast<char*>(Buffer), Count);
            quint16 checksum = qChecksum(BufferView);
#endif
           LOG(VB_FILE, LOG_DEBUG, LOC + desc + QString(": ReadDirect checksum %1")
                    .arg(checksum));
            m_rwLock.unlock();
            return result;
        }
        m_rwLock.unlock();
        m_rwLock.lockForRead();
    }

    if (!WaitForReadsAllowed())
    {
        LOG(VB_FILE, LOG_NOTICE, LOC + desc + ": !WaitForReadsAllowed()");
        m_rwLock.unlock();
        m_stopReads = true; // this needs to be outside the lock
        m_rwLock.lockForWrite();
        m_wantToRead = 0;
        m_rwLock.unlock();
        return 0;
    }

    int available = ReadBufAvail();
    MythTimer timer(MythTimer::kStartRunning);

    // Wait up to 10000 ms for any data
    std::chrono::milliseconds timeout_ms = 10s;
    while (!m_readInternalMode && !m_ateof && (timer.elapsed() < timeout_ms) && m_readAheadRunning &&
           !m_stopReads && !m_requestPause && !m_commsError)
    {
        available = WaitForAvail(Count, std::min(timeout_ms - timer.elapsed(), 100ms));
        if (m_liveTVChain && m_setSwitchToNext && available < Count)
        {
            LOG(VB_GENERAL, LOG_INFO, LOC + "Checking to see if there's a new livetv program to switch to..");
            m_liveTVChain->ReloadAll();
            break;
        }
        if (available > 0)
            break;
    }
    if (timer.elapsed() > 6s)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + desc + QString(" -- waited %1 ms for avail(%2) > count(%3)")
            .arg(timer.elapsed().count()).arg(available).arg(Count));
    }

    if (m_readInternalMode)
    {
        LOG(VB_FILE, LOG_DEBUG, LOC + QString("ReadPriv: %1 bytes available, %2 left")
            .arg(available).arg(available-m_readOffset));
    }
    Count = std::min(available - m_readOffset, Count);

    if ((Count <= 0) && (m_ateof || m_readInternalMode))
    {
        // If we're at the end of file return 0 bytes
        m_rwLock.unlock();
        return Count;
    }

    if (Count <= 0)
    {
        // If we're not at the end of file but have no data
        // at this point time out and shutdown read ahead.
        LOG(VB_GENERAL, LOG_ERR, LOC + desc + QString(" -- timed out waiting for data (%1 ms)")
            .arg(timer.elapsed().count()));

        m_rwLock.unlock();
        m_stopReads = true; // this needs to be outside the lock
        m_rwLock.lockForWrite();
        m_ateof = true;
        m_wantToRead = 0;
        m_generalWait.wakeAll();
        m_rwLock.unlock();
        return Count;
    }

    if (Peek || m_readInternalMode)
        m_rbrLock.lockForRead();
    else
        m_rbrLock.lockForWrite();

    LOG(VB_FILE, LOG_DEBUG, LOC + desc + ": Copying data");

    int readposition = 0;
    if (m_rbrPos + m_readOffset > static_cast<int>(m_bufferSize))
        readposition = (m_rbrPos + m_readOffset) - static_cast<int>(m_bufferSize);
    else
        readposition = m_rbrPos + m_readOffset;

    if (readposition + Count > static_cast<int>(m_bufferSize))
    {
        int firstsize = static_cast<int>(m_bufferSize) - readposition;
        int secondsize = Count - firstsize;

        memcpy(Buffer, m_readAheadBuffer + readposition, static_cast<size_t>(firstsize));
        memcpy(reinterpret_cast<char*>(Buffer) + firstsize, m_readAheadBuffer, static_cast<size_t>(secondsize));
    }
    else
    {
        memcpy(Buffer, m_readAheadBuffer + readposition, static_cast<uint>(Count));
    }
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    quint16 checksum = qChecksum(reinterpret_cast<char*>(Buffer), static_cast<uint>(Count));
#else
    QByteArrayView BufferView(reinterpret_cast<char*>(Buffer), Count);
    quint16 checksum = qChecksum(BufferView);
#endif
    LOG(VB_FILE, LOG_DEBUG, LOC + desc + QString(": Checksum %1").arg(checksum));

    if (!Peek)
    {
        if (m_readInternalMode)
        {
            m_readOffset += Count;
        }
        else
        {
            m_rbrPos = (m_rbrPos + Count) % static_cast<int>(m_bufferSize);
            m_generalWait.wakeAll();
        }
    }
    m_rbrLock.unlock();
    m_rwLock.unlock();

    return Count;
}

/** \fn MythMediaBuffer::Read(void*, int)
 *  \brief This is the public method for reading from a file,
 *         it calls the appropriate read method if the file
 *         is remote or buffered, or a BD/DVD.
 *  \param buf   Pointer to where data will be written
 *  \param count Number of bytes to read
 *  \return Returns number of bytes read
 */
int MythMediaBuffer::Read(void *Buffer, int Count)
{
    int ret = ReadPriv(Buffer, Count, false);
    if (ret > 0)
    {
        m_posLock.lockForWrite();
        m_readPos += ret;
        m_posLock.unlock();
        UpdateDecoderRate(static_cast<uint64_t>(ret));
    }

    return ret;
}

QString MythMediaBuffer::BitrateToString(uint64_t Rate, bool Hz)
{
    if (Rate < 1)
        return "-";

    if (Rate > 1000000000)
        return QObject::tr(">1Gbps");

    QString msg;
    auto bitrate = static_cast<double>(NAN);
    auto rate = static_cast<double>(Rate);
    int range = 0;

    if (Rate >= 1000000)
    {
        msg = Hz ? QObject::tr("%1MHz") : QObject::tr("%1Mbps");
        bitrate  = rate / 1000000.0;
        range = Hz ? 3 : 1;
    }
    else if (Rate >= 1000)
    {
        msg = Hz ? QObject::tr("%1kHz") : QObject::tr("%1kbps");
        bitrate = rate / 1000.0;
        range = Hz ? 1 : 0;
    }
    else
    {
        msg = Hz ? QObject::tr("%1Hz") : QObject::tr("%1bps");
        bitrate = rate;
    }
    return msg.arg(bitrate, 0, 'f', range);
}

QString MythMediaBuffer::GetDecoderRate(void)
{
    return BitrateToString(UpdateDecoderRate());
}

QString MythMediaBuffer::GetStorageRate(void)
{
    return BitrateToString(UpdateStorageRate());
}

QString MythMediaBuffer::GetAvailableBuffer(void)
{
    if (m_type == kMythBufferDVD || m_type == kMythBufferBD)
        return "N/A";

    int avail = (m_rbwPos >= m_rbrPos) ? m_rbwPos - m_rbrPos
                                       : static_cast<int>(m_bufferSize) - m_rbrPos + m_rbwPos;
    return QString("%1%").arg(lroundf((static_cast<float>(avail) / static_cast<float>(m_bufferSize) * 100.0F)));
}

uint MythMediaBuffer::GetBufferSize(void) const
{
    return m_bufferSize;
}

uint64_t MythMediaBuffer::UpdateDecoderRate(uint64_t Latest)
{
    if (!m_bitrateMonitorEnabled)
        return 0;

    auto current = std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
    std::chrono::milliseconds expire = current - 1s;

    m_decoderReadLock.lock();
    if (Latest)
        m_decoderReads.insert(current, Latest);
    uint64_t total = 0;
    QMutableMapIterator<std::chrono::milliseconds,uint64_t> it(m_decoderReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < expire || it.key() > current)
            it.remove();
        else
            total += it.value();
    }

    int size = m_decoderReads.size();
    m_decoderReadLock.unlock();

    auto average = static_cast<uint64_t>(static_cast<double>(total) * 8.0);
    LOG(VB_FILE, LOG_INFO, LOC + QString("Decoder read speed: %1 %2")
            .arg(average).arg(size));
    return average;
}

uint64_t MythMediaBuffer::UpdateStorageRate(uint64_t Latest)
{
    if (!m_bitrateMonitorEnabled)
        return 0;

    auto current = std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
    auto expire = current - 1s;

    m_storageReadLock.lock();
    if (Latest)
        m_storageReads.insert(current, Latest);
    uint64_t total = 0;
    QMutableMapIterator<std::chrono::milliseconds,uint64_t> it(m_storageReads);
    while (it.hasNext())
    {
        it.next();
        if (it.key() < expire || it.key() > current)
            it.remove();
        else
            total += it.value();
    }

    int size = m_storageReads.size();
    m_storageReadLock.unlock();

    uint64_t average = size ? static_cast<uint64_t>(static_cast<double>(total) / size) : 0;
    LOG(VB_FILE, LOG_INFO, LOC + QString("Average storage read speed: %1 (Reads %2")
            .arg(average).arg(m_storageReads.size()));
    return average;
}

/** \fn MythMediaBuffer::Write(const void*, uint)
 *  \brief Writes buffer to ThreadedFileWriter::Write(const void*,uint)
 *  \return Bytes written, or -1 on error.
 */
int MythMediaBuffer::Write(const void *Buffer, uint Count)
{
    m_rwLock.lockForRead();
    int result = -1;

    if (!m_writeMode)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Tried to write to a read only file.");
        m_rwLock.unlock();
        return result;
    }

    if (!m_tfw && !m_remotefile)
    {
        m_rwLock.unlock();
        return result;
    }

    if (m_tfw)
        result = m_tfw->Write(Buffer, Count);
    else
        result = m_remotefile->Write(Buffer, static_cast<int>(Count));

    if (result > 0)
    {
        m_posLock.lockForWrite();
        m_writePos += result;
        m_posLock.unlock();
    }

    m_rwLock.unlock();
    return result;
}

/** \fn MythMediaBuffer::Sync(void)
 *  \brief Calls ThreadedFileWriter::Sync(void)
 */
void MythMediaBuffer::Sync(void)
{
    m_rwLock.lockForRead();
    if (m_tfw)
        m_tfw->Sync();
    m_rwLock.unlock();
}

/** \brief Calls ThreadedFileWriter::Seek(long long,int).
 */
long long MythMediaBuffer::WriterSeek(long long Position, int Whence, bool HasLock)
{
    long long result = -1;

    if (!HasLock)
        m_rwLock.lockForRead();

    m_posLock.lockForWrite();

    if (m_tfw)
    {
        result = m_tfw->Seek(Position, Whence);
        m_writePos = result;
    }

    m_posLock.unlock();

    if (!HasLock)
        m_rwLock.unlock();

    return result;
}

/** \fn MythMediaBuffer::WriterFlush(void)
 *  \brief Calls ThreadedFileWriter::Flush(void)
 */
void MythMediaBuffer::WriterFlush(void)
{
    m_rwLock.lockForRead();
    if (m_tfw)
        m_tfw->Flush();
    m_rwLock.unlock();
}

/** \fn MythMediaBuffer::WriterSetBlocking(bool)
 *  \brief Calls ThreadedFileWriter::SetBlocking(bool)
 */
bool MythMediaBuffer::WriterSetBlocking(bool Lock)
{
    QReadLocker lock(&m_rwLock);
    if (m_tfw)
        return m_tfw->SetBlocking(Lock);
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
void MythMediaBuffer::SetOldFile(bool Old)
{
    LOG(VB_FILE, LOG_INFO, LOC + QString("SetOldFile: %1)").arg(Old));
    m_rwLock.lockForWrite();
    m_oldfile = Old;
    m_rwLock.unlock();
}

QString MythMediaBuffer::GetFilename(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_filename;
    m_rwLock.unlock();
    return tmp;
}

QString MythMediaBuffer::GetSafeFilename(void)
{
    return m_safeFilename;
}

QString MythMediaBuffer::GetSubtitleFilename(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_subtitleFilename;
    m_rwLock.unlock();
    return tmp;
}

QString MythMediaBuffer::GetLastError(void) const
{
    m_rwLock.lockForRead();
    QString tmp = m_lastError;
    m_rwLock.unlock();
    return tmp;
}

bool MythMediaBuffer::GetCommsError() const
{
    return m_commsError;
}

void MythMediaBuffer::ResetCommsError(void)
{
    m_commsError = false;
}

bool MythMediaBuffer::GetStopReads(void) const
{
    return m_stopReads;
}

/** \fn MythMediaBuffer::GetWritePosition(void) const
 *  \brief Returns how far into a ThreadedFileWriter file we have written.
 */
long long MythMediaBuffer::GetWritePosition(void) const
{
    m_posLock.lockForRead();
    long long ret = m_writePos;
    m_posLock.unlock();
    return ret;
}

/** \fn MythMediaBuffer::LiveMode(void) const
 *  \brief Returns true if this RingBuffer has been assigned a LiveTVChain.
 *  \sa SetLiveMode(LiveTVChain*)
 */
bool MythMediaBuffer::LiveMode(void) const
{
    m_rwLock.lockForRead();
    bool ret = (m_liveTVChain);
    m_rwLock.unlock();
    return ret;
}

/** \fn MythMediaBuffer::SetLiveMode(LiveTVChain*)
 *  \brief Assigns a LiveTVChain to this RingBuffer
 *  \sa LiveMode(void)
 */
void MythMediaBuffer::SetLiveMode(LiveTVChain *Chain)
{
    m_rwLock.lockForWrite();
    m_liveTVChain = Chain;
    m_rwLock.unlock();
}

/// Tells RingBuffer whether to ignore the end-of-file
void MythMediaBuffer::IgnoreLiveEOF(bool Ignore)
{
    m_rwLock.lockForWrite();
    m_ignoreLiveEOF = Ignore;
    m_rwLock.unlock();
}

bool MythMediaBuffer::IsDisc(void) const
{
    return IsDVD() || IsBD();
}

bool MythMediaBuffer::IsDVD(void) const
{
    return m_type == kMythBufferDVD;
}

bool MythMediaBuffer::IsBD(void) const
{
    return m_type == kMythBufferBD;
}

const MythDVDBuffer *MythMediaBuffer::DVD(void) const
{
    return dynamic_cast<const MythDVDBuffer*>(this);
}

const MythBDBuffer  *MythMediaBuffer::BD(void) const
{
    return dynamic_cast<const MythBDBuffer*>(this);
}

MythDVDBuffer *MythMediaBuffer::DVD(void)
{
    return dynamic_cast<MythDVDBuffer*>(this);
}

MythBDBuffer  *MythMediaBuffer::BD(void)
{
    return dynamic_cast<MythBDBuffer*>(this);
}

void MythMediaBuffer::AVFormatInitNetwork(void)
{
    static QRecursiveMutex s_avnetworkLock;
    static bool s_avnetworkInitialised = false;
    QMutexLocker lock(&s_avnetworkLock);
    if (!s_avnetworkInitialised)
    {
        avformat_network_init();
        s_avnetworkInitialised = true;
    }
}
