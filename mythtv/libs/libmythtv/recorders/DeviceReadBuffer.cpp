#include <algorithm>
using namespace std;

#include "DeviceReadBuffer.h"
#include "mythcorecontext.h"
#include "mythbaseutil.h"
#include "mythlogging.h"
#include "tspacket.h"
#include "mthread.h"
#include "compat.h"

#ifndef USING_MINGW
#include <sys/poll.h>
#endif

/// Set this to 1 to report on statistics
#define REPORT_RING_STATS 0

#define LOC QString("DevRdB(%1): ").arg(videodevice)

DeviceReadBuffer::DeviceReadBuffer(
    DeviceReaderCB *cb, bool use_poll, bool error_exit_on_poll_timeout)
    : MThread("DeviceReadBuffer"),
      videodevice(""),              _stream_fd(-1),
      readerCB(cb),

      // Data for managing the device ringbuffer
      dorun(false),
      eof(false),                   error(false),
      request_pause(false),         paused(false),
      using_poll(use_poll),
      poll_timeout_is_error(error_exit_on_poll_timeout),
      max_poll_wait(2500 /*ms*/),

      size(0),                      used(0),
      read_quanta(0),
      dev_read_size(0),             min_read(0),

      buffer(NULL),                 readPtr(NULL),
      writePtr(NULL),               endPtr(NULL),

      // statistics
      max_used(0),                  avg_used(0),
      avg_buf_write_cnt(0),         avg_buf_read_cnt(0),
      avg_buf_sleep_cnt(0)
{
    for (int i = 0; i < 2; i++)
    {
        wake_pipe[i] = -1;
        wake_pipe_flags[i] = 0;
    }

#if defined( USING_MINGW ) && !defined( _MSC_VER )
#warning mingw DeviceReadBuffer::Poll
    if (using_poll)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "mingw DeviceReadBuffer::Poll is not implemented");
        using_poll = false;
    }
#endif
}

DeviceReadBuffer::~DeviceReadBuffer()
{
    Stop();
    if (buffer)
    {
        delete[] buffer;
        buffer = NULL;
    }
}

bool DeviceReadBuffer::Setup(const QString &streamName, int streamfd,
                             uint readQuanta, uint deviceBufferSize)
{
    QMutexLocker locker(&lock);

    if (buffer)
        delete[] buffer;

    videodevice   = streamName;
    videodevice   = (videodevice == QString::null) ? "" : videodevice;
    _stream_fd    = streamfd;

    // Setup device ringbuffer
    eof           = false;
    error         = false;
    request_pause = false;
    paused        = false;

    read_quanta   = (readQuanta) ? readQuanta : read_quanta;
    size          = gCoreContext->GetNumSetting(
        "HDRingbufferSize", 50 * read_quanta) * 1024;
    used          = 0;
    dev_read_size = read_quanta * (using_poll ? 256 : 48);
    dev_read_size = (deviceBufferSize) ?
        min(dev_read_size, (size_t)deviceBufferSize) : dev_read_size;
    min_read      = read_quanta * 4;

    buffer        = new (nothrow) unsigned char[size + dev_read_size];
    readPtr       = buffer;
    writePtr      = buffer;
    endPtr        = buffer + size;

    // Initialize buffer, if it exists
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Failed to allocate buffer of size %1 = %2 + %3")
                .arg(size+dev_read_size).arg(size).arg(dev_read_size));
        return false;
    }
    memset(buffer, 0xFF, size + read_quanta);

    // Initialize statistics
    max_used      = 0;
    avg_used      = 0;
    avg_buf_write_cnt = 0;
    avg_buf_read_cnt  = 0;
    avg_buf_sleep_cnt = 0;
    lastReport.start();

    LOG(VB_RECORD, LOG_INFO, LOC + QString("buffer size %1 KB").arg(size/1024));

    return true;
}

void DeviceReadBuffer::Start(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- begin");

    QMutexLocker locker(&lock);
    if (isRunning() || dorun)
    {
        dorun = false;
        locker.unlock();
        WakePoll();
        wait();
        locker.relock();
    }

    dorun = true;
    error = false;
    eof   = false;

    start();

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- middle");

    while (dorun && !isRunning())
        runWait.wait(locker.mutex(), 100);

    LOG(VB_RECORD, LOG_INFO, LOC + "Start() -- end");
}

void DeviceReadBuffer::Reset(const QString &streamName, int streamfd)
{
    QMutexLocker locker(&lock);

    videodevice   = streamName;
    videodevice   = (videodevice == QString::null) ? "" : videodevice;
    _stream_fd    = streamfd;

    used          = 0;
    readPtr       = buffer;
    writePtr      = buffer;

    error         = false;
}

void DeviceReadBuffer::Stop(void)
{
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- begin");
    QMutexLocker locker(&lock);
    if (isRunning() || dorun)
    {
        dorun = false;
        locker.unlock();
        WakePoll();
        wait();
    }
    LOG(VB_RECORD, LOG_INFO, LOC + "Stop() -- end");
}

void DeviceReadBuffer::SetRequestPause(bool req)
{
    QMutexLocker locker(&lock);
    request_pause = req;
    WakePoll();
}

void DeviceReadBuffer::SetPaused(bool val)
{
    QMutexLocker locker(&lock);
    paused = val;
    if (val)
        pauseWait.wakeAll();
    else
        unpauseWait.wakeAll();
}

// The WakePoll code is copied from MythSocketThread::WakeReadyReadThread()
void DeviceReadBuffer::WakePoll(void) const
{
    char buf[1];
    buf[0] = '0';
    ssize_t wret = 0;
    while (isRunning() && (wret <= 0) && (wake_pipe[1] >= 0))
    {
        wret = ::write(wake_pipe[1], &buf, 1);
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
        if (wake_pipe[i] >= 0)
        {
            ::close(wake_pipe[i]);
            wake_pipe[i] = -1;
            wake_pipe_flags[i] = 0;
        }
    }
}

bool DeviceReadBuffer::IsPaused(void) const
{
    QMutexLocker locker(&lock);
    return paused;
}

bool DeviceReadBuffer::WaitForPaused(unsigned long timeout)
{
    QMutexLocker locker(&lock);

    if (!paused)
        pauseWait.wait(&lock, timeout);

    return paused;
}

bool DeviceReadBuffer::WaitForUnpause(unsigned long timeout)
{
    QMutexLocker locker(&lock);

    if (paused)
        unpauseWait.wait(&lock, timeout);

    return paused;
}

bool DeviceReadBuffer::IsPauseRequested(void) const
{
    QMutexLocker locker(&lock);
    return request_pause;
}

bool DeviceReadBuffer::IsErrored(void) const
{
    QMutexLocker locker(&lock);
    return error;
}

bool DeviceReadBuffer::IsEOF(void) const
{
    QMutexLocker locker(&lock);
    return eof;
}

bool DeviceReadBuffer::IsRunning(void) const
{
    QMutexLocker locker(&lock);
    return isRunning();
}

uint DeviceReadBuffer::GetUnused(void) const
{
    QMutexLocker locker(&lock);
    return size - used;
}

uint DeviceReadBuffer::GetUsed(void) const
{
    QMutexLocker locker(&lock);
    return used;
}

uint DeviceReadBuffer::GetContiguousUnused(void) const
{
    QMutexLocker locker(&lock);
    return endPtr - writePtr;
}

void DeviceReadBuffer::IncrWritePointer(uint len)
{
    QMutexLocker locker(&lock);
    used     += len;
    writePtr += len;
    writePtr  = (writePtr >= endPtr) ? buffer + (writePtr - endPtr) : writePtr;
#if REPORT_RING_STATS
    max_used = max(used, max_used);
    avg_used = ((avg_used * avg_buf_write_cnt) + used) / (avg_buf_write_cnt+1);
    ++avg_buf_write_cnt;
#endif
    dataWait.wakeAll();
}

void DeviceReadBuffer::IncrReadPointer(uint len)
{
    QMutexLocker locker(&lock);
    used    -= len;
    readPtr += len;
    readPtr  = (readPtr == endPtr) ? buffer : readPtr;
#if REPORT_RING_STATS
    ++avg_buf_read_cnt;
#endif
}

void DeviceReadBuffer::run(void)
{
    RunProlog();

    uint      errcnt = 0;

    lock.lock();
    runWait.wakeAll();
    lock.unlock();

    if (using_poll)
        setup_pipe(wake_pipe, wake_pipe_flags);

    while (dorun)
    {
        if (!HandlePausing())
            continue;

        if (!IsOpen())
        {
            usleep(5000);
            continue;
        }

        if (using_poll && !Poll())
            continue;

        {
            QMutexLocker locker(&lock);
            if (error)
            {
                LOG(VB_RECORD, LOG_ERR, LOC + "fill_ringbuffer: error state");
                break;
            }
        }

        // Limit read size for faster return from read
        size_t unused = (size_t) WaitForUnused(read_quanta);
        size_t read_size = min(dev_read_size, unused);

        // if read_size > 0 do the read...
        ssize_t len = 0;
        if (read_size)
        {
            len = read(_stream_fd, writePtr, read_size);
            if (!CheckForErrors(len, read_size, errcnt))
            {
                if (errcnt > 5)
                    break;
                else
                    continue;
            }
            errcnt = 0;
            // if we wrote past the official end of the buffer, copy to start
            if (writePtr + len > endPtr)
                memcpy(buffer, endPtr, writePtr + len - endPtr);
            IncrWritePointer(len);
        }
    }

    ClosePipes();

    lock.lock();
    eof     = true;
    runWait.wakeAll();
    dataWait.wakeAll();
    pauseWait.wakeAll();
    unpauseWait.wakeAll();
    lock.unlock();

    RunEpilog();
}

bool DeviceReadBuffer::HandlePausing(void)
{
    if (IsPauseRequested())
    {
        SetPaused(true);

        if (readerCB)
            readerCB->ReaderPaused(_stream_fd);

        usleep(5000);
        return false;
    }
    else if (IsPaused())
    {
        Reset(videodevice, _stream_fd);
        SetPaused(false);
    }
    return true;
}

bool DeviceReadBuffer::Poll(void) const
{
#ifdef USING_MINGW
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

    polls[0].fd      = _stream_fd;
    polls[0].events  = POLLIN | POLLPRI;
    polls[0].revents = 0;

    if (wake_pipe[0] >= 0)
    {
        poll_cnt = 2;
        polls[1].fd      = wake_pipe[0];
        polls[1].events  = POLLIN;
        polls[1].revents = 0;
    }

    while (true)
    {
        polls[0].revents = 0;
        polls[1].revents = 0;
        poll_cnt = (wake_pipe[0] >= 0) ? poll_cnt : 1;

        int timeout = max_poll_wait;
        if (1 == poll_cnt)
            timeout = 10;
        else if (poll_timeout_is_error)
            timeout = max((int)max_poll_wait - timer.elapsed(), 10);

        int ret = poll(polls, poll_cnt, timeout);

        if (polls[0].revents & POLLHUP)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll eof (POLLHUP)");
            break;
        }
        else if (polls[0].revents & POLLNVAL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "poll error");
            error = true;
            return true;
        }

        if (!dorun || !IsOpen() || IsPauseRequested())
        {
            retval = false;
            break; // are we supposed to pause, stop, etc.
        }

        if (polls[0].revents & POLLPRI)
        {
            readerCB->PriorityEvent(polls[0].fd);
        }

        if (polls[0].revents & POLLIN)
        {
            if (ret > 0)
                break; // we have data to read :)
            else if (ret < 0)
            {
                if ((EOVERFLOW == errno))
                    break; // we have an error to handle

                if ((EAGAIN == errno) || (EINTR  == errno))
                    continue; // errors that tell you to try again

                usleep(2500 /*2.5 ms*/);
            }
            else //  ret == 0
            {
                if (poll_timeout_is_error &&
                    (timer.elapsed() >= (int)max_poll_wait))
                {
                    LOG(VB_GENERAL, LOG_ERR, LOC + "Poll giving up 1");
                    QMutexLocker locker(&lock);
                    error = true;
                    return true;
                }
            }
        }

        // Clear out any pending pipe reads
        if ((poll_cnt > 1) && (polls[1].revents & POLLIN))
        {
            char dummy[128];
            int cnt = (wake_pipe_flags[0] & O_NONBLOCK) ? 128 : 1;
            cnt = ::read(wake_pipe[0], dummy, cnt);
        }

        if (poll_timeout_is_error && (timer.elapsed() >= (int)max_poll_wait))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Poll giving up 2");
            QMutexLocker locker(&lock);
            error = true;
            return true;
        }
    }

    int e = timer.elapsed();
    if (e > (int)max_poll_wait)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            QString("Poll took an unusually long time %1 ms")
            .arg(timer.elapsed()));
    }

    return retval;
#endif //!USING_MINGW
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
            QMutexLocker locker(&lock);
            error = true;
        }
        return false;
    }

#ifdef USING_MINGW
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
            QString("Problem reading fd(%1)").arg(_stream_fd) + ENO);

        if (++errcnt > 5)
        {
            LOG(VB_RECORD, LOG_ERR, LOC + "Too many errors.");
            QMutexLocker locker(&lock);
            error = true;
            return false;
        }

        usleep(500);
        return false;
    }
    else if (len == 0)
    {
        if (++errcnt > 5)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("End-Of-File? fd(%1)").arg(_stream_fd));

            lock.lock();
            eof = true;
            lock.unlock();

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
    uint avail = WaitForUsed(min(count, (uint)dev_read_size), 20);
    size_t cnt = min(count, avail);

    if (!cnt)
        return 0;

    if (readPtr + cnt > endPtr)
    {
        // Process as two pieces
        size_t len = endPtr - readPtr;
        if (len)
        {
            memcpy(buf, readPtr, len);
            buf += len;
            IncrReadPointer(len);
        }
        if (cnt > len)
        {
            len = cnt - len;
            memcpy(buf, readPtr, len);
            IncrReadPointer(len);
        }
    }
    else
    {
        memcpy(buf, readPtr, cnt);
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

    if (unused > read_quanta)
    {
        while (unused < needed)
        {
            unused = GetUnused();
            if (IsPauseRequested() || !IsOpen() || !dorun)
                return 0;
            usleep(5000);
        }
        if (IsPauseRequested() || !IsOpen() || !dorun)
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

    QMutexLocker locker(&lock);
    size_t avail = used;
    while ((needed > avail) && isRunning() &&
           !request_pause && !error && !eof &&
           (timer.elapsed() < (int)max_wait))
    {
        dataWait.wait(locker.mutex(), 10);
        avail = used;
    }
    return avail;
}

void DeviceReadBuffer::ReportStats(void)
{
#if REPORT_RING_STATS
    static const int secs = 20;
    static const double d1_s = 1.0 / secs;
    if (lastReport.elapsed() > secs * 1000 /* msg every 20 seconds */)
    {
        QMutexLocker locker(&lock);
        double rsize = 100.0 / size;
        QString msg  = QString("fill avg(%1%) ").arg(avg_used*rsize,5,'f',2);
        msg         += QString("fill max(%1%) ").arg(max_used*rsize,5,'f',2);
        msg         += QString("writes/sec(%1) ").arg(avg_buf_write_cnt*d1_s);
        msg         += QString("reads/sec(%1) ").arg(avg_buf_read_cnt*d1_s);
        msg         += QString("sleeps/sec(%1)").arg(avg_buf_sleep_cnt*d1_s);

        avg_used    = 0;
        avg_buf_write_cnt = 0;
        avg_buf_read_cnt = 0;
        avg_buf_sleep_cnt = 0;
        max_used    = 0;
        lastReport.start();

        LOG(VB_GENERAL, LOG_INFO, LOC + msg);
    }
#endif
}

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
