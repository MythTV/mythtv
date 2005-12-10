#include <algorithm>
#include <cassert>

#include "DeviceReadBuffer.h"
#include "mythcontext.h"
#include "tspacket.h"

/// Set this to 1 to report on statistics
#define REPORT_RING_STATS 0

#define LOC QString("DevRdB(%1): ").arg(videodevice)
#define LOC_ERR QString("DevRdB(%1) Error: ").arg(videodevice)

DeviceReadBuffer::DeviceReadBuffer(ReaderPausedCB *cb, bool use_poll)
    : videodevice(QString::null),   _stream_fd(-1),
      readerPausedCB(cb),

      // Data for managing the device ringbuffer
      run(false),                   running(false),
      eof(false),                   error(false),
      request_pause(false),         paused(false),
      using_poll(use_poll),

      size(0),                      used(0),
      dev_read_size(0),             min_read(0),

      buffer(NULL),                 readPtr(NULL),
      writePtr(NULL),               endPtr(NULL),

      // statistics
      max_used(0),                  avg_used(0),
      avg_cnt(0)
{
}

DeviceReadBuffer::~DeviceReadBuffer()
{
    if (buffer)
        delete[] buffer;
}

bool DeviceReadBuffer::Setup(const QString &streamName, int streamfd)
{
    QMutexLocker locker(&lock);

    if (buffer)
        delete[] buffer;

    videodevice   = streamName;
    _stream_fd    = streamfd;

    // Setup device ringbuffer
    eof           = false;
    error         = false;
    request_pause = false;
    paused        = false;

    size          = gContext->GetNumSetting("HDRingbufferSize",
                                            50 * TSPacket::SIZE) * 1024;
    used          = 0;
    dev_read_size = TSPacket::SIZE * (using_poll ? 256 : 48);
    min_read      = TSPacket::SIZE * 4;

    buffer        = new unsigned char[size + TSPacket::SIZE];
    readPtr       = buffer;
    writePtr      = buffer;
    endPtr        = buffer + size;

    // Initialize buffer, if it exists
    if (!buffer)
        return false;
    memset(buffer, 0xFF, size + TSPacket::SIZE);

    // Initialize statistics
    max_used      = 0;
    avg_used      = 0;
    avg_cnt       = 0;
    lastReport.start();

    VERBOSE(VB_RECORD, LOC + QString("buffer size %1 KB").arg(size/1024));

    return true;
}

void DeviceReadBuffer::Start(void)
{
    lock.lock();
    bool was_running = running;
    lock.unlock();
    if (was_running)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Start(): Already running.");
        SetRequestPause(false);
        return;
    }

    pthread_create(&thread, NULL, boot_ringbuffer, this);
}

void DeviceReadBuffer::Reset(const QString &streamName, int streamfd)
{
    QMutexLocker locker(&lock);

    videodevice   = streamName;
    _stream_fd    = streamfd;

    used          = 0;
    readPtr       = buffer;
    writePtr      = buffer;
}

void DeviceReadBuffer::Stop(void)
{
    bool was_running = IsRunning();
    lock.lock();
    run = false;
    lock.unlock();

    if (!was_running)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Stop(): Not running.");
        return;
    }

    pthread_join(thread, NULL);
}

void DeviceReadBuffer::SetRequestPause(bool req)
{
    QMutexLocker locker(&lock);
    request_pause = req;
}

void DeviceReadBuffer::SetPaused(bool val)
{
    lock.lock();
    paused = val;
    lock.unlock();
    if (val)
        pauseWait.wakeAll();
    else
        unpauseWait.wakeAll();
}

bool DeviceReadBuffer::IsPaused(void) const
{
    QMutexLocker locker(&lock);
    return paused;
}

bool DeviceReadBuffer::WaitForUnpause(int timeout)
{
    if (IsPaused())
        unpauseWait.wait(timeout);
    return IsPaused();
}

bool DeviceReadBuffer::IsPauseRequested(void) const
{
    QMutexLocker locker(&lock);
    return request_pause;
}

bool DeviceReadBuffer::IsRunning(void) const
{
    QMutexLocker locker(&lock);
    return running;
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
    writePtr  = (writePtr == endPtr) ? buffer : writePtr;
#if REPORT_RING_STATS
    max_used = max(used, max_used);
    avg_used = ((avg_used * avg_cnt) + used) / ++avg_cnt;
#endif
}

void DeviceReadBuffer::IncrReadPointer(uint len)
{
    QMutexLocker locker(&lock);
    used    -= len;
    readPtr += len;
    readPtr  = (readPtr == endPtr) ? buffer : readPtr;
    assert(readPtr <= endPtr);
}

void *DeviceReadBuffer::boot_ringbuffer(void *arg)
{
    ((DeviceReadBuffer*) arg)->fill_ringbuffer();
    return NULL;
}

void DeviceReadBuffer::fill_ringbuffer(void)
{
    uint      errcnt = 0;

    lock.lock();
    run     = true;
    running = true;
    lock.unlock();

    while (run)
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

        // Limit read size for faster return from read
        size_t read_size =
            min(dev_read_size, (size_t) WaitForUnused(TSPacket::SIZE));

        // if read_size > 0 do the read...
        if (read_size)
        {
            ssize_t len = read(_stream_fd, writePtr, read_size);
            if (!CheckForErrors(len, errcnt))
            {
                if (errcnt > 5)
                    break;
                else
                    continue;
            }
            errcnt = 0;
            IncrWritePointer(len);
        }
    }

    lock.lock();
    running = false;
    lock.unlock();
}

bool DeviceReadBuffer::HandlePausing(void)
{
    if (IsPauseRequested())
    {
        SetPaused(true);

        if (readerPausedCB)
            readerPausedCB->ReaderPaused(_stream_fd);

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
    bool retval = true;
    while (true)
    {
        struct pollfd polls;
        polls.fd      = _stream_fd;
        polls.events  = POLLIN;
        polls.revents = 0;

        int ret = poll(&polls, 1 /*number of polls*/, 10 /*msec*/);
        if (IsPauseRequested() || !IsOpen() || !run)
        {
            retval = false;
            break; // are we supposed to pause, stop, etc.
        }

        if (ret > 0)
            break; // we have data to read :)
        if ((-1 == ret) && (EOVERFLOW == errno))
            break; // we have an error to handle

        if ((-1 == ret) && ((EAGAIN == errno) || (EINTR  == errno)))
            continue; // errors that tell you to try again
        if (ret == 0)
            continue; // timed out, try again

        usleep(2500);
    }
    return retval;
}

bool DeviceReadBuffer::CheckForErrors(ssize_t len, uint &errcnt)
{
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
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Driver buffers overflowed");
            return false;
        }

        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Problem reading fd(%1)").arg(_stream_fd) + ENO);

        if (++errcnt > 5)
        {
            lock.lock();
            error = true;
            lock.unlock();
            return false;
        }

        usleep(500);
        return false;
    }
    else if (len == 0)
    {
        if (++errcnt > 5)
        {
            VERBOSE(VB_IMPORTANT, LOC +
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
}

/** \fn DeviceReadBuffer::Read(unsigned char*, uint)
 *  \brief Try to Read count bytes from into buffer
 *  \param buffer Buffer to put data in
 *  \param count  Number of bytes to attempt to read
 *  \return number of bytes actually read
 */
uint DeviceReadBuffer::Read(unsigned char *buf, const uint count)
{
    uint avail = WaitForUsed(min(count, (uint)min_read));
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

/// \return bytes available for writing
uint DeviceReadBuffer::WaitForUnused(uint needed) const
{
    size_t unused = GetUnused();
    size_t contig = GetContiguousUnused();

    if (contig > TSPacket::SIZE)
    {
        while (unused < needed)
        {
            unused = GetUnused();
            if (IsPauseRequested() || !IsOpen() || !run)
                return 0;
            usleep(5000);
        }
        if (IsPauseRequested() || !IsOpen() || !run)
            return 0;
        contig = GetContiguousUnused();
    }

    return min(contig, unused);
}

/// \return bytes available for reading
uint DeviceReadBuffer::WaitForUsed(uint needed) const
{
    size_t avail = GetUsed();
    while ((needed > avail) && running)
    {
        {
            QMutexLocker locker(&lock);
            avail = used;
            if (request_pause || error || eof)
                return 0;
        }
        usleep(5000);
    }
    return avail;
}

void DeviceReadBuffer::ReportStats(void)
{ 
#if REPORT_RING_STATS
    if (lastReport.elapsed() > 20*1000 /* msg every 20 seconds */)
    {
        QMutexLocker locker(&lock);
        double rsize = 100.0 / size;
        QString msg  = QString("fill avg(%1%) ").arg(avg_used*rsize,3,'f',0);
        msg         += QString("fill max(%2%) ").arg(max_used*rsize,3,'f',0);
        msg         += QString("samples(%3)").arg(avg_cnt);

        avg_used    = 0;
        avg_cnt     = 0;
        max_used    = 0;
        lastReport.start();

        VERBOSE(VB_IMPORTANT, LOC + msg);
    }
#endif
}
