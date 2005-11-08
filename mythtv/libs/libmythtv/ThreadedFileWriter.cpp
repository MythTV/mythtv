#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "ThreadedFileWriter.h"

#include "mythcontext.h"

#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
#define HAVE_FDATASYNC
#endif

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
        {
            printf("triggered funky usleep\n");
            usleep(1000);
        }
    }
    return tot;
}

void *ThreadedFileWriter::boot_writer(void *wotsit)
{
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->DiskLoop();
    return NULL;
}

void *ThreadedFileWriter::boot_syncer(void *wotsit)
{
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->SyncLoop();
    return NULL;
}

ThreadedFileWriter::ThreadedFileWriter(const char *fname,
                                       int pflags, mode_t pmode)
    : filename(fname), flags(pflags), mode(pmode), fd(-1),
      no_writes(false), flush(false), tfw_buf_size(0),
      tfw_min_write_size(0), rpos(0), wpos(0), buf(NULL),
      in_dtor(0)
{
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
        buf = new char[TFW_DEF_BUF_SIZE + 1024];
        bzero(buf, TFW_DEF_BUF_SIZE + 64);

        tfw_buf_size = TFW_DEF_BUF_SIZE;
        tfw_min_write_size = TFW_MIN_WRITE_SIZE;
        pthread_create(&writer, NULL, boot_writer, this);
        pthread_create(&syncer, NULL, boot_syncer, this);
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

        bufferSyncWait.wakeAll();
        pthread_join(syncer, NULL);

        bufferHasData.wakeAll();
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
        bufferWroteData.wait(100);
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

    buflock.lock();
    wpos = (wpos + count) % tfw_buf_size;
    buflock.unlock();

    bufferHasData.wakeAll();

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
        if (!bufferEmpty.wait(2000))
            VERBOSE(VB_IMPORTANT, "taking a long time to flush..");

    flush = false;
}

void ThreadedFileWriter::Sync(void)
{
    if (fd >= 0)
    {
#ifdef HAVE_FDATASYNC
        fdatasync(fd);
#else
        fsync(fd);
#endif
    }
}

void ThreadedFileWriter::SetWriteBufferSize(int newSize)
{
    if (newSize <= 0)
        return;

    Flush();

    buflock.lock();
    delete [] buf;
    rpos = wpos = 0;
    buf = new char[newSize + 1024];
    bzero(buf, newSize + 64);
    tfw_buf_size = newSize;
    buflock.unlock();
}

void ThreadedFileWriter::SetWriteBufferMinWriteSize(int newMinSize)
{
    if (newMinSize <= 0)
        return;

    tfw_min_write_size = newMinSize;
}

void ThreadedFileWriter::SyncLoop()
{
    while (!in_dtor)
    {
        bufferSyncWait.wait(1000);
        Sync();
    }
}

void ThreadedFileWriter::DiskLoop()
{
    int size;
    int written = 0;

    while (!in_dtor || BufUsed() > 0)
    {
        size = BufUsed();

        if (size == 0)
            bufferEmpty.wakeAll();

        if (!size || (!in_dtor && !flush &&
            (((unsigned)size < tfw_min_write_size) &&
             ((unsigned)written >= tfw_min_write_size))))
        {
            bufferHasData.wait(100);
            continue;
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

        buflock.lock();
        rpos = (rpos + size) % tfw_buf_size;
        buflock.unlock();

        bufferWroteData.wakeAll();
    }
}

unsigned ThreadedFileWriter::BufUsed()
{
    unsigned ret;
    buflock.lock();

    if (wpos >= rpos)
        ret = wpos - rpos;
    else
        ret = tfw_buf_size - rpos + wpos;

    buflock.unlock();
    return ret;
}

unsigned ThreadedFileWriter::BufFree()
{
    unsigned ret;
    buflock.lock();

    if (wpos >= rpos)
        ret = rpos + tfw_buf_size - wpos - 1;
    else
        ret = rpos - wpos - 1;

    buflock.unlock();
    return ret;
}

