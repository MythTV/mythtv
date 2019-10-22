#include <algorithm>
using namespace std;

#include "DeviceReadBuffer.h"
#include "mythcorecontext.h"
#include "mythbaseutil.h"
#include "mythlogging.h"
#include "tspacket.h"
#include "mthread.h"
#include "compat.h"

#ifndef _WIN32
#include <sys/poll.h>
#endif

/// Set this to 1 to report on statistics
#define REPORT_RING_STATS 0

#define LOC QString("DevRdB(%1): ").arg(m_videodevice)

DeviceReadBuffer::DeviceReadBuffer(
    DeviceReaderCB *cb, bool use_poll, bool error_exit_on_poll_timeout)
    : MThread("DeviceReadBuffer"),
      m_readerCB(cb),
      m_using_poll(use_poll),
      m_poll_timeout_is_error(error_exit_on_poll_timeout)
{
#ifdef USING_MINGW
#warning mingw DeviceReadBuffer::Poll
    if (m_using_poll)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "mingw DeviceReadBuffer::Poll is not implemented");
        m_using_poll = false;
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

    m_videodevice   = streamName;
    m_videodevice   = m_videodevice.isNull() ? "" : m_videodevice;
    m_stream_fd     = streamfd;

    // Setup device ringbuffer
    m_eof           = false;
    m_error         = false;
    m_request_pause = false;
    m_paused        = false;

    m_read_quanta   = (readQuanta) ? readQuanta : m_read_quanta;
    m_dev_buffer_count = deviceBufferCount;
    m_size          = gCoreContext->GetNumSetting(
        "HDRingbufferSize", static_cast<int>(50 * m_read_quanta)) * 1024;
    m_used          = 0;
    m_dev_read_size = m_read_quanta * (m_using_poll ? 256 : 48);
    m_dev_read_size = (deviceBufferSize) ?
        min(m_dev_read_size, (size_t)deviceBufferSize) : m_dev_read_size;
    m_readThreshold = m_read_quanta * 128;

    m_buffer        = new (nothrow) unsigned char[m_size + m_dev_read_size];
    m_readPtr       = m_buffer;
    m_writePtr      = m_buffer;

    // Initialize buffer, if it exists
    if (!m_buffer)
    {
        m_endPtr = nullptr;
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to allocate buffer of size %1 = %2 + %3")
                .arg(m_size+m_dev_read_size).arg(m_size).arg(m_dev_read_size));
        return false;
    }
    m_endPtr = m_buffer + m_size;
    memset(m_buffer, 0xFF, m_size + m_read_quanta);

    // Initialize statistics
    m_max_used      = 0;
    m_avg_used      = 0;
    m_avg_buf_write_cnt = 0;
    m_avg_buf_read_cnt  = 0;
    m_avg_buf_sleep_cnt = 0;
    m_lastReport.start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("buffer size %1 KB").arg(m_size/1024));

    return true;
}

void DeviceReadBuffer::Start(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- begin");

    QMutexLocker locker(&m_lock);
    if (isRunning() || m_dorun)
    {
        m_dorun = false;
        locker.unlock();
        WakePoll();
        wait();
        locker.relock();
    }

    m_dorun = true;
    m_error = false;
    m_eof   = false;

    start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- middle");

    while (m_dorun && !isRunning())
        m_runWait.wait(locker.mutex(), 100);

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- end");
}

void DeviceReadBuffer::Reset(const QString &streamName, int streamfd)
{
    QMutexLocker locker(&m_lock);

    m_videodevice   = streamName;
    m_videodevice   = m_videodevice.isNull() ? "" : m_videodevice;
    m_stream_fd     = streamfd;

    m_used          = 0;
    m_readPtr       = m_buffer;
    m_writePtr      = m_buffer;

    m_error         = false;
}

void DeviceReadBuffer::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");
    QMutexLocker locker(&m_lock);
    if (isRunning() || m_dorun)
    {
        m_dorun = false;
        locker.unlock();
        WakePoll();
        wait();
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void DeviceReadBuffer::SetRequestPause(bool req)
{
    QMutexLocker locker(&m_lock);
    m_request_pause = req;
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
    char buf[1];
    buf[0] = '0';
    ssize_t wret = 0;
    while (isRunning() && (wret <= 0) && (m_wake_pipe[1] >= 0))
    {
        wret = ::write(m_wake_pipe[1], &buf, 1);
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
        if (m_wake_pipe[i] >= 0)
        {
            ::close(m_wake_pipe[i]);
            m_wake_pipe[i] = -1;
            m_wake_pipe_flags[i] = 0;
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
    return m_request_pause;
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
    m_max_used = max(m_used, m_max_used);
    m_avg_used = ((m_avg_used * m_avg_buf_write_cnt) + m_used) / (m_avg_buf_write_cnt+1);
    ++m_avg_buf_write_cnt;
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
    ++m_avg_buf_read_cnt;
#endif
}

void DeviceReadBuffer::run(void)
{
    RunProlog();

    uint      errcnt = 0;
    uint      cnt;
    ssize_t   len;
    size_t    read_size;
    size_t    unused;
    size_t    total;
    size_t    throttle = m_dev_read_size * m_dev_buffer_count / 2;

    m_lock.lock();
    m_runWait.wakeAll();
    m_lock.unlock();

    if (m_using_poll)
        setup_pipe(m_wake_pipe, m_wake_pipe_flags);

    while (m_dorun)
    {
        if (!HandlePausing())
            continue;

        if (!IsOpen())
        {
            usleep(5000);
            continue;
        }

        if (m_using_poll && !Poll())
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
        for (cnt = 0, len = 0, total = 0;
             m_dorun && len >= 0 && cnt < m_dev_buffer_count; ++cnt)
        {
            // Limit read size for faster return from read
            unused = static_cast<size_t>(WaitForUnused(m_read_quanta));
            read_size = min(m_dev_read_size, unused);

            // if read_size > 0 do the read...
            if (read_size)
            {
                len = read(m_stream_fd, m_writePtr, read_size);
                if (!CheckForErrors(len, read_size, errcnt))
                    break;
                errcnt = 0;

                // if we wrote past the official end of the buffer,
                // copy to start
                if (m_writePtr + len > m_endPtr)
                    memcpy(m_buffer, m_endPtr, m_writePtr + len - m_endPtr);
                IncrWritePointer(len);
                total += len;
            }
        }
        if (errcnt > 5)
            break;

        // Slow down reading if not under load
        if (errcnt == 0 && total < throttle)
            usleep(1000);
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
            m_readerCB->ReaderPaused(m_stream_fd);

        usleep(5000);
        return false;
    }
    if (IsPaused())
    {
        Reset(m_videodevice, m_stream_fd);
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
    struct pollfd polls[2];
    memset(polls, 0, sizeof(polls));

    polls[0].fd      = m_stream_fd;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    if (m_wake_pipe[0] >= 0)
    {
        poll_cnt = 2;
        polls[1].fd      = m_wake_pipe[0];
        polls[1].events  = POLLIN;
        polls[1].revents = 0;
    }

    while (true)
    {
        polls[0].revents = 0;
        polls[1].revents = 0;
        poll_cnt = (m_wake_pipe[0] >= 0) ? poll_cnt : 1;

        int timeout = m_max_poll_wait;
        if (1 == poll_cnt)
            timeout = 10;
        else if (m_poll_timeout_is_error)
            // subtract a bit to allow processing time.
            timeout = max((int)m_max_poll_wait - timer.elapsed() - 15, 10);

        int ret = poll(polls, poll_cnt, timeout);

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

        if (!m_dorun || !IsOpen() || IsPauseRequested())
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

                usleep(2500 /*2.5 ms*/);
            }
            else //  ret == 0
            {
                if (m_poll_timeout_is_error &&
                    (timer.elapsed() >= (int)m_max_poll_wait))
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
            char dummy[128];
            int cnt = (m_wake_pipe_flags[0] & O_NONBLOCK) ? 128 : 1;
            ::read(m_wake_pipe[0], dummy, cnt);
        }

        if (m_poll_timeout_is_error && (timer.elapsed() >= (int)m_max_poll_wait))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("Poll giving up after %1ms")
                .arg(m_max_poll_wait));
            QMutexLocker locker(&m_lock);
            m_error = true;
            return true;
        }
    }

    int e = timer.elapsed();
    if (e > (int)m_max_poll_wait)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Poll took an unusually long time %1 ms")
            .arg(timer.elapsed()));
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
            usleep(2500);
            return false;
        }
        if (EOVERFLOW == errno)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Driver buffers overflowed");
            return false;
        }

        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Problem reading fd(%1)").arg(m_stream_fd) + ENO);

        if (++errcnt > 5)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Too many errors.");
            QMutexLocker locker(&m_lock);
            m_error = true;
            return false;
        }

        usleep(500);
        return false;
    }
    if (len == 0)
    {
        if (++errcnt > 5)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("End-Of-File? fd(%1)").arg(m_stream_fd));

            m_lock.lock();
            m_eof = true;
            m_lock.unlock();

            return false;
        }
        usleep(500);
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
    uint avail = WaitForUsed(min(count, (uint)m_readThreshold), 20);
    size_t cnt = min(count, avail);

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

    if (unused > m_read_quanta)
    {
        while (unused < needed)
        {
            unused = GetUnused();
            if (IsPauseRequested() || !IsOpen() || !m_dorun)
                return 0;
            usleep(5000);
        }
        if (IsPauseRequested() || !IsOpen() || !m_dorun)
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
uint DeviceReadBuffer::WaitForUsed(uint needed, uint max_wait) const
{
    MythTimer timer;
    timer.start();

    QMutexLocker locker(&m_lock);
    size_t avail = m_used;
    while ((needed > avail) && isRunning() &&
           !m_request_pause && !m_error && !m_eof &&
           (timer.elapsed() < (int)max_wait))
    {
        m_dataWait.wait(locker.mutex(), 10);
        avail = m_used;
    }
    return avail;
}

void DeviceReadBuffer::ReportStats(void)
{
#if REPORT_RING_STATS
    static const int secs = 20;
    static const double d1_s = 1.0 / secs;
    if (m_lastReport.elapsed() > secs * 1000 /* msg every 20 seconds */)
    {
        QMutexLocker locker(&m_lock);
        double rsize = 100.0 / m_size;
        QString msg  = QString("fill avg(%1%) ").arg(m_avg_used*rsize,5,'f',2);
        msg         += QString("fill max(%1%) ").arg(m_max_used*rsize,5,'f',2);
        msg         += QString("writes/sec(%1) ").arg(m_avg_buf_write_cnt*d1_s);
        msg         += QString("reads/sec(%1) ").arg(m_avg_buf_read_cnt*d1_s);
        msg         += QString("sleeps/sec(%1)").arg(m_avg_buf_sleep_cnt*d1_s);

        m_avg_used    = 0;
        m_avg_buf_write_cnt = 0;
        m_avg_buf_read_cnt = 0;
        m_avg_buf_sleep_cnt = 0;
        m_max_used    = 0;
        m_lastReport.start();

        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }
#endif
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
