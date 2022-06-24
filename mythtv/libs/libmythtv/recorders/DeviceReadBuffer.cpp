#include <algorithm>

#include "libmythbase/compat.h"
#include "libmythbase/mthread.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythbase/sizetliteral.h"

#include "DeviceReadBuffer.h"
#include "mpeg/tspacket.h"

#ifndef _WIN32
#include <sys/poll.h>
#endif

/// Set this to 1 to report on statistics
#define REPORT_RING_STATS 0  // NOLINT(cppcoreguidelines-macro-usage)

#define LOC QString("DevRdB(%1): ").arg(m_videoDevice)

DeviceReadBuffer::DeviceReadBuffer(
    DeviceReaderCB *cb, bool use_poll, bool error_exit_on_poll_timeout)
    : MThread("DeviceReadBuffer"),
      m_readerCB(cb),
      m_usingPoll(use_poll),
      m_pollTimeoutIsError(error_exit_on_poll_timeout)
{
#ifdef USING_MINGW
#warning mingw DeviceReadBuffer::Poll
    if (m_usingPoll)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "mingw DeviceReadBuffer::Poll is not implemented");
        m_usingPoll = false;
    }
#endif
}

DeviceReadBuffer::~DeviceReadBuffer()
{
    Stop();
    if (m_buffer)
    {
        delete[] m_buffer;
        m_buffer = nullptr;
    }
}

bool DeviceReadBuffer::Setup(const QString &streamName, int streamfd,
                             uint readQuanta, uint deviceBufferSize,
                             uint deviceBufferCount)
{
    QMutexLocker locker(&m_lock);

    delete[] m_buffer;

    m_videoDevice   = streamName;
    m_videoDevice   = m_videoDevice.isNull() ? "" : m_videoDevice;
    m_streamFd      = streamfd;

    // Setup device ringbuffer
    m_eof           = false;
    m_error         = false;
    m_requestPause  = false;
    m_paused        = false;

    m_readQuanta   = (readQuanta) ? readQuanta : m_readQuanta;
    m_devBufferCount = deviceBufferCount;
    m_size          = gCoreContext->GetNumSetting(
        "HDRingbufferSize", static_cast<int>(50 * m_readQuanta)) * 1024_UZ;
    m_used          = 0;
    m_devReadSize = m_readQuanta * (m_usingPoll ? 256 : 48);
    m_devReadSize = (deviceBufferSize) ?
        std::min(m_devReadSize, (size_t)deviceBufferSize) : m_devReadSize;
    m_readThreshold = m_readQuanta * 128;

    m_buffer        = new (std::nothrow) unsigned char[m_size + m_devReadSize];
    m_readPtr       = m_buffer;
    m_writePtr      = m_buffer;

    // Initialize buffer, if it exists
    if (!m_buffer)
    {
        m_endPtr = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to allocate buffer of size %1 = %2 + %3")
                .arg(m_size+m_devReadSize).arg(m_size).arg(m_devReadSize));
        return false;
    }
    m_endPtr = m_buffer + m_size;
    memset(m_buffer, 0xFF, m_size + m_readQuanta);

    // Initialize statistics
    m_maxUsed        = 0;
    m_avgUsed        = 0;
    m_avgBufWriteCnt = 0;
    m_avgBufReadCnt  = 0;
    m_avgBufSleepCnt = 0;
    m_lastReport.start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("buffer size %1 KB").arg(m_size/1024));

    return true;
}

void DeviceReadBuffer::Start(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- begin");

    QMutexLocker locker(&m_lock);
    if (isRunning() || m_doRun)
    {
        m_doRun = false;
        locker.unlock();
        WakePoll();
        wait();
        locker.relock();
    }

    m_doRun = true;
    m_error = false;
    m_eof   = false;

    start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- middle");

    while (m_doRun && !isRunning())
        m_runWait.wait(locker.mutex(), 100);

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- end");
}

void DeviceReadBuffer::Reset(const QString &streamName, int streamfd)
{
    QMutexLocker locker(&m_lock);

    m_videoDevice   = streamName;
    m_videoDevice   = m_videoDevice.isNull() ? "" : m_videoDevice;
    m_streamFd      = streamfd;

    m_used          = 0;
    m_readPtr       = m_buffer;
    m_writePtr      = m_buffer;

    m_error         = false;
}

void DeviceReadBuffer::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");
    QMutexLocker locker(&m_lock);
    if (isRunning() || m_doRun)
    {
        m_doRun = false;
        locker.unlock();
        WakePoll();
        wait();
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void DeviceReadBuffer::SetRequestPause(bool req)
{
    QMutexLocker locker(&m_lock);
    m_requestPause = req;
    WakePoll();
}

void DeviceReadBuffer::SetPaused(bool val)
{
    QMutexLocker locker(&m_lock);
    m_paused = val;
    if (val)
        m_pauseWait.wakeAll();
    else
        m_unpauseWait.wakeAll();
}

// The WakePoll code is copied from MythSocketThread::WakeReadyReadThread()
void DeviceReadBuffer::WakePoll(void) const
{
    std::string buf(1,'\0');
    ssize_t wret = 0;
    while (isRunning() && (wret <= 0) && (m_wakePipe[1] >= 0))
    {
        wret = ::write(m_wakePipe[1], buf.data(), buf.size());
        if ((wret < 0) && (EAGAIN != errno) && (EINTR != errno))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "WakePoll failed.");
            ClosePipes();
            break;
        }
    }
}

void DeviceReadBuffer::ClosePipes(void) const
{
    for (uint i = 0; i < 2; i++)
    {
        if (m_wakePipe[i] >= 0)
        {
            ::close(m_wakePipe[i]);
            m_wakePipe[i] = -1;
            m_wakePipeFlags[i] = 0;
        }
    }
}

bool DeviceReadBuffer::IsPaused(void) const
{
    QMutexLocker locker(&m_lock);
    return m_paused;
}

bool DeviceReadBuffer::WaitForPaused(unsigned long timeout)
{
    QMutexLocker locker(&m_lock);

    if (!m_paused)
        m_pauseWait.wait(&m_lock, timeout);

    return m_paused;
}

bool DeviceReadBuffer::WaitForUnpause(unsigned long timeout)
{
    QMutexLocker locker(&m_lock);

    if (m_paused)
        m_unpauseWait.wait(&m_lock, timeout);

    return m_paused;
}

bool DeviceReadBuffer::IsPauseRequested(void) const
{
    QMutexLocker locker(&m_lock);
    return m_requestPause;
}

bool DeviceReadBuffer::IsErrored(void) const
{
    QMutexLocker locker(&m_lock);
    return m_error;
}

bool DeviceReadBuffer::IsEOF(void) const
{
    QMutexLocker locker(&m_lock);
    return m_eof;
}

bool DeviceReadBuffer::IsRunning(void) const
{
    QMutexLocker locker(&m_lock);
    return isRunning();
}

uint DeviceReadBuffer::GetUnused(void) const
{
    QMutexLocker locker(&m_lock);
    return m_size - m_used;
}

uint DeviceReadBuffer::GetUsed(void) const
{
    QMutexLocker locker(&m_lock);
    return m_used;
}

uint DeviceReadBuffer::GetContiguousUnused(void) const
{
    QMutexLocker locker(&m_lock);
    return m_endPtr - m_writePtr;
}

void DeviceReadBuffer::IncrWritePointer(uint len)
{
    QMutexLocker locker(&m_lock);
    m_used     += len;
    m_writePtr += len;
    m_writePtr  = (m_writePtr >= m_endPtr) ? m_buffer + (m_writePtr - m_endPtr) : m_writePtr;
#if REPORT_RING_STATS
    m_maxUsed = std::max(m_used, m_maxUsed);
    m_avgUsed = ((m_avgUsed * m_avgBufWriteCnt) + m_used) / (m_avgBufWriteCnt+1);
    ++m_avgBufWriteCnt;
#endif
    m_dataWait.wakeAll();
}

void DeviceReadBuffer::IncrReadPointer(uint len)
{
    QMutexLocker locker(&m_lock);
    m_used    -= len;
    m_readPtr += len;
    m_readPtr  = (m_readPtr == m_endPtr) ? m_buffer : m_readPtr;
#if REPORT_RING_STATS
    ++m_avgBufReadCnt;
#endif
}

void DeviceReadBuffer::run(void)
{
    RunProlog();

    uint      errcnt = 0;
    uint      cnt = 0;
    ssize_t   read_len = 0;
    size_t    total = 0;
    size_t    throttle = m_devReadSize * m_devBufferCount / 2;

    m_lock.lock();
    m_runWait.wakeAll();
    m_lock.unlock();

    if (m_usingPoll)
        setup_pipe(m_wakePipe, m_wakePipeFlags);

    while (m_doRun)
    {
        if (!HandlePausing())
            continue;

        if (!IsOpen())
        {
            usleep(5ms);
            continue;
        }

        if (m_usingPoll && !Poll())
            continue;

        {
            QMutexLocker locker(&m_lock);
            if (m_error)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "fill_ringbuffer: error state");
                break;
            }
        }

        /* Some device drivers segment their buffer into small pieces,
         * So allow for the reading of multiple buffers */
        for (cnt = 0, read_len = 0, total = 0;
             m_doRun && read_len >= 0 && cnt < m_devBufferCount; ++cnt)
        {
            // Limit read size for faster return from read
            auto unused = static_cast<size_t>(WaitForUnused(m_readQuanta));
            size_t read_size = std::min(m_devReadSize, unused);

            // if read_size > 0 do the read...
            if (read_size)
            {
                read_len = read(m_streamFd, m_writePtr, read_size);
                if (!CheckForErrors(read_len, read_size, errcnt))
                    break;
                errcnt = 0;

                // if we wrote past the official end of the buffer,
                // copy to start
                if (m_writePtr + read_len > m_endPtr)
                    memcpy(m_buffer, m_endPtr, m_writePtr + read_len - m_endPtr);
                IncrWritePointer(read_len);
                total += read_len;
            }
        }
        if (errcnt > 5)
            break;

        // Slow down reading if not under load
        if (errcnt == 0 && total < throttle)
            usleep(1ms);
    }

    ClosePipes();

    m_lock.lock();
    m_eof     = true;
    m_runWait.wakeAll();
    m_dataWait.wakeAll();
    m_pauseWait.wakeAll();
    m_unpauseWait.wakeAll();
    m_lock.unlock();

    RunEpilog();
}

bool DeviceReadBuffer::HandlePausing(void)
{
    if (IsPauseRequested())
    {
        SetPaused(true);

        if (m_readerCB)
            m_readerCB->ReaderPaused(m_streamFd);

        usleep(5ms);
        return false;
    }
    if (IsPaused())
    {
        Reset(m_videoDevice, m_streamFd);
        SetPaused(false);
    }
    return true;
}

bool DeviceReadBuffer::Poll(void) const
{
#ifdef _WIN32
# ifdef _MSC_VER
#  pragma message( "mingw DeviceReadBuffer::Poll" )
# else
#  warning mingw DeviceReadBuffer::Poll
# endif
    LOG(VB_GENERAL, LOG_ERR, LOC +
        "mingw DeviceReadBuffer::Poll is not implemented");
    return false;
#else
    bool retval = true;
    MythTimer timer;
    timer.start();

    int poll_cnt = 1;
    std::array<struct pollfd,2> polls {};

    polls[0].fd      = m_streamFd;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    if (m_wakePipe[0] >= 0)
    {
        poll_cnt = 2;
        polls[1].fd      = m_wakePipe[0];
        polls[1].events  = POLLIN;
        polls[1].revents = 0;
    }

    while (true)
    {
        polls[0].revents = 0;
        polls[1].revents = 0;
        poll_cnt = (m_wakePipe[0] >= 0) ? poll_cnt : 1;

        std::chrono::milliseconds timeout = m_maxPollWait;
        if (1 == poll_cnt)
            timeout = 10ms;
        else if (m_pollTimeoutIsError)
            // subtract a bit to allow processing time.
            timeout = std::max(m_maxPollWait - timer.elapsed() - 15ms, 10ms);

        int ret = poll(polls.data(), poll_cnt, timeout.count());

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        if (polls[0].revents & POLLNVAL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll error" + ENO);
            m_error = true;
            return true;
        }

        if (!m_doRun || !IsOpen() || IsPauseRequested())
        {
            retval = false;
            break; // are we supposed to pause, stop, etc.
        }

        if (polls[0].revents & POLLPRI)
        {
            m_readerCB->PriorityEvent(polls[0].fd);
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
                break; // we have data to read :)
            if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                    break; // we have an error to handle

                if ((EAGAIN == errno) || (EINTR  == errno))
                    continue; // errors that tell you to try again

                usleep(2.5ms);
            }
            else //  ret == 0
            {
                if (m_pollTimeoutIsError &&
                    (timer.elapsed() >= m_maxPollWait))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "Poll giving up 1");
                    QMutexLocker locker(&m_lock);
                    m_error = true;
                    return true;
                }
            }
        }

        // Clear out any pending pipe reads
        if ((poll_cnt > 1) && (polls[1].revents & POLLIN))
        {
            std::array<char,128> dummy {};
            int cnt = (m_wakePipeFlags[0] & O_NONBLOCK) ? 128 : 1;
            ::read(m_wakePipe[0], dummy.data(), cnt);
        }

        if (m_pollTimeoutIsError && (timer.elapsed() >= m_maxPollWait))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Poll giving up after %1ms")
                .arg(m_maxPollWait.count()));
            QMutexLocker locker(&m_lock);
            m_error = true;
            return true;
        }
    }

    std::chrono::milliseconds e = timer.elapsed();
    if (e > m_maxPollWait)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Poll took an unusually long time %1 ms")
            .arg(timer.elapsed().count()));
    }

    return retval;
#endif //!_WIN32
}

bool DeviceReadBuffer::CheckForErrors(
    ssize_t len, size_t requested_len, uint &errcnt)
{
    if (len > (ssize_t)requested_len)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            "Driver is returning bogus values on read");
        if (++errcnt > 5)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Too many errors.");
            QMutexLocker locker(&m_lock);
            m_error = true;
        }
        return false;
    }

#ifdef _WIN32
# ifdef _MSC_VER
#  pragma message( "mingw DeviceReadBuffer::CheckForErrors" )
# else
#  warning mingw DeviceReadBuffer::CheckForErrors
# endif
    LOG(VB_GENERAL, LOG_ERR, LOC +
        "mingw DeviceReadBuffer::CheckForErrors is not implemented");
    return false;
#else
    if (len < 0)
    {
        if (EINTR == errno)
            return false;
        if (EAGAIN == errno)
        {
            usleep(2.5ms);
            return false;
        }
        if (EOVERFLOW == errno)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Driver buffers overflowed");
            return false;
        }

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Problem reading fd(%1)").arg(m_streamFd) + ENO);

        if (++errcnt > 5)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Too many errors.");
            QMutexLocker locker(&m_lock);
            m_error = true;
            return false;
        }

        usleep(500ms);
        return false;
    }
    if (len == 0)
    {
        if (++errcnt > 5)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("End-Of-File? fd(%1)").arg(m_streamFd));

            m_lock.lock();
            m_eof = true;
            m_lock.unlock();

            return false;
        }
        usleep(500ms);
        return false;
    }
    return true;
#endif
}

/** \fn DeviceReadBuffer::Read(unsigned char*, const uint)
 *  \brief Try to Read count bytes from into buffer
 *  \param buf    Buffer to put data in
 *  \param count  Number of bytes to attempt to read
 *  \return number of bytes actually read
 */
uint DeviceReadBuffer::Read(unsigned char *buf, const uint count)
{
    uint avail = WaitForUsed(std::min(count, (uint)m_readThreshold), 20ms);
    size_t cnt = std::min(count, avail);

    if (!cnt)
        return 0;

    if (m_readPtr + cnt > m_endPtr)
    {
        // Process as two pieces
        size_t len = m_endPtr - m_readPtr;
        if (len)
        {
            memcpy(buf, m_readPtr, len);
            buf += len;
            IncrReadPointer(len);
        }
        if (cnt > len)
        {
            len = cnt - len;
            memcpy(buf, m_readPtr, len);
            IncrReadPointer(len);
        }
    }
    else
    {
        memcpy(buf, m_readPtr, cnt);
        IncrReadPointer(cnt);
    }

#if REPORT_RING_STATS
    ReportStats();
#endif

    return cnt;
}

/** \fn DeviceReadBuffer::WaitForUnused(uint) const
 *  \param needed Number of bytes we want to write
 *  \return bytes available for writing
 */
uint DeviceReadBuffer::WaitForUnused(uint needed) const
{
    size_t unused = GetUnused();

    if (unused > m_readQuanta)
    {
        while (unused < needed)
        {
            unused = GetUnused();
            if (IsPauseRequested() || !IsOpen() || !m_doRun)
                return 0;
            usleep(5ms);
        }
        if (IsPauseRequested() || !IsOpen() || !m_doRun)
            return 0;
        unused = GetUnused();
    }

    return unused;
}

/** \fn DeviceReadBuffer::WaitForUsed(uint,uint) const
 *  \param needed Number of bytes we want to read
 *  \param max_wait Number of milliseconds to wait for the needed data
 *  \return bytes available for reading
 */
uint DeviceReadBuffer::WaitForUsed(uint needed, std::chrono::milliseconds max_wait) const
{
    MythTimer timer;
    timer.start();

    QMutexLocker locker(&m_lock);
    size_t avail = m_used;
    while ((needed > avail) && isRunning() &&
           !m_requestPause && !m_error && !m_eof &&
           (timer.elapsed() < max_wait))
    {
        m_dataWait.wait(locker.mutex(), 10);
        avail = m_used;
    }
    return avail;
}

void DeviceReadBuffer::ReportStats(void)
{
#if REPORT_RING_STATS
    static constexpr std::chrono::seconds secs { 20s }; // msg every 20 seconds
    static constexpr double d1_s = 1.0 / secs.count();
    if (m_lastReport.elapsed() > duration_cast<std::chrono::milliseconds>(secs))
    {
        QMutexLocker locker(&m_lock);
        double rsize = 100.0 / m_size;
        QString msg  = QString("fill avg(%1%) ").arg(m_avgUsed*rsize,5,'f',2);
        msg         += QString("fill max(%1%) ").arg(m_maxUsed*rsize,5,'f',2);
        msg         += QString("writes/sec(%1) ").arg(m_avgBufWriteCnt*d1_s);
        msg         += QString("reads/sec(%1) ").arg(m_avgBufReadCnt*d1_s);
        msg         += QString("sleeps/sec(%1)").arg(m_avgBufSleepCnt*d1_s);

        m_avgUsed        = 0;
        m_avgBufWriteCnt = 0;
        m_avgBufReadCnt  = 0;
        m_avgBufSleepCnt = 0;
        m_maxUsed        = 0;
        m_lastReport.start();

        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }
#endif
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
