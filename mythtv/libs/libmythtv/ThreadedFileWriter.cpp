// ANSI C headers
#include <cstdio>
#include <cstdlib>
#include <cerrno>

// Unix C headers
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

// MythTV headers
#include "ThreadedFileWriter.h"
#include "mythcontext.h"

#if defined(_POSIX_SYNCHRONIZED_IO) && _POSIX_SYNCHRONIZED_IO > 0
#define HAVE_FDATASYNC
#endif

#define LOC QString("TFW: ")
#define LOC_ERR QString("TFW, Error: ")

const uint ThreadedFileWriter::TFW_DEF_BUF_SIZE   = 2*1024*1024;
const uint ThreadedFileWriter::TFW_MAX_WRITE_SIZE = TFW_DEF_BUF_SIZE / 4;
const uint ThreadedFileWriter::TFW_MIN_WRITE_SIZE = TFW_DEF_BUF_SIZE / 8;

/** \class ThreadedFileWriter
 *  \brief This class supports the writing of recordings to disk.
 *
 *   This class allows us manage the buffering when writing to
 *   disk. We write to the kernel image of the disk using one
 *   thread, and sync the kernel's image of the disk to hardware
 *   using another thread. The goal here so to block as little as
 *   possible when the classes using this class want to add data
 *   to the stream.
 */

/** \fn safe_write(int, const void*, uint, bool &ok)
 *  \brief Writes data to disk
 *
 *   This just uses the Standard C write() to write to disk.
 *   We retry forever on EAGAIN errors, and three times on
 *   any other error.
 *
 *  \param fd   File descriptor
 *  \param data Pointer to data to write
 *  \param sz   Size of data to write in bytes
 */
static uint safe_write(int fd, const void *data, uint sz, bool &ok)
{
    int ret;
    uint tot = 0;
    uint errcnt = 0;

    while (tot < sz)
    {
        ret = write(fd, (char *)data + tot, sz - tot);
        if (ret < 0)
        {
            if (errno == EAGAIN)
            {
                VERBOSE(VB_IMPORTANT, LOC + "safe_write(): Got EAGAIN.");
                continue;
            }

            errcnt++;
            VERBOSE(VB_IMPORTANT, LOC_ERR + "safe_write(): File I/O " +
                    QString(" errcnt: %1").arg(errcnt) + ENO);

            if (errcnt == 3)
                break;
        }
        else
        {
            tot += ret;
        }

        if (tot < sz)
        {
            VERBOSE(VB_IMPORTANT, LOC + "safe_write(): funky usleep");
            usleep(1000);
        }
    }
    ok = (errcnt < 3);
    return tot;
}

/** \fn ThreadedFileWriter::boot_writer(void*)
 *  \brief Thunk that runs ThreadedFileWriter::DiskLoop(void)
 */
void *ThreadedFileWriter::boot_writer(void *wotsit)
{
    signal(SIGXFSZ, SIG_IGN);
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->DiskLoop();
    return NULL;
}

/** \fn ThreadedFileWriter::boot_syncer(void*)
 *  \brief Thunk that runs ThreadedFileWriter::SyncLoop(void)
 */
void *ThreadedFileWriter::boot_syncer(void *wotsit)
{
    ThreadedFileWriter *fw = (ThreadedFileWriter *)wotsit;
    fw->SyncLoop();
    return NULL;
}

/** \fn ThreadedFileWriter::ThreadedFileWriter(const QString&,int,mode_t)
 *  \brief Creates a threaded file writer.
 */
ThreadedFileWriter::ThreadedFileWriter(const QString &fname,
                                       int pflags, mode_t pmode) :
    // file stuff
    filename(QDeepCopy<QString>(fname)), flags(pflags),
    mode(pmode),                         fd(-1),
    // state
    no_writes(false),                    flush(false),
    in_dtor(false),                      ignore_writes(false),
    tfw_min_write_size(0),
    // buffer position state
    rpos(0),                             wpos(0),
    written(0),
    // buffer
    buf(NULL),                           tfw_buf_size(0)
{
}

/** \fn ThreadedFileWriter::Open(void)
 *  \brief Opens the file we will be writing to.
 *  \return true if we successfully open the file.
 */
bool ThreadedFileWriter::Open(void)
{
    ignore_writes = false;
    fd = open(filename.ascii(), flags, mode);

    if (fd < 0)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + 
                QString("Opening file '%1'.").arg(filename) + ENO);
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

/** \fn ThreadedFileWriter::~ThreadedFileWriter()
 *  \brief Commits all writes and closes the file.
 */
ThreadedFileWriter::~ThreadedFileWriter()
{
    no_writes = true;

    if (fd >= 0)
    {
        Flush();
        in_dtor = true; /* tells child thread to exit */

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

/** \fn ThreadedFileWriter::Write(const void*, uint)
 *  \brief Writes data to the end of the write buffer
 *
 *   NOTE: This blocks while buffer is in use by the write to disk thread.
 *
 *  \param data  pointer to data to write to disk
 *  \param count size of data in bytes
 */
uint ThreadedFileWriter::Write(const void *data, uint count)
{
    if (count == 0)
        return 0;

    bool first = true;

    while (count > BufFree())
    {
        if (first)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Write() -- IOBOUND begin " + 
                    QString("cnt(%1) free(%2)").arg(count).arg(BufFree()));
            first = false;
        }

        bufferWroteData.wait(100);
    }
    if (!first)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Write() -- IOBOUND end");

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

/** \fn ThreadedFileWriter::Flush(void)
 *  \brief Allow DiskLoop() to flush buffer completely ignoring low watermark.
 */
void ThreadedFileWriter::Flush(void)
{
    flush = true;
    while (BufUsed() > 0)
    {
        if (!bufferEmpty.wait(2000))
            VERBOSE(VB_IMPORTANT, LOC + "Taking a long time to flush..");
    }
    flush = false;
}

/** \fn ThreadedFileWriter::Sync(void)
 *  \brief flush data written to the file descriptor to disk.
 *
 *   NOTE: This doesn't even try flush our queue of data.
 *   This only ensures that data which has already been sent
 *   to the kernel for this file is written to disk. This 
 *   means that if this backend is writing the data over a 
 *   network filesystem like NFS, then the data will be visible
 *   to the NFS server after this is called. It is also useful
 *   in preventing the kernel from buffering up so many writes
 *   that they steal the CPU for a long time when the write
 *   to disk actually occurs.
 */
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

/** \fn ThreadedFileWriter::SetWriteBufferSize(uint)
 *  \brief Sets the total size of the write buffer.
 *  WARNING: This is not safe when another thread is writing to the buffer.
 */
void ThreadedFileWriter::SetWriteBufferSize(uint newSize)
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

/** \fn ThreadedFileWriter::SetWriteBufferMinWriteSize(uint)
 *  \brief Sets the minumum number of bytes to write to disk in a single write.
 *         This is ignored during a Flush(void)
 */
void ThreadedFileWriter::SetWriteBufferMinWriteSize(uint newMinSize)
{
    if (newMinSize <= 0)
        return;

    tfw_min_write_size = newMinSize;
}

/** \fn ThreadedFileWriter::SyncLoop(void)
 *  \brief The thread run method that calls Sync(void).
 */
void ThreadedFileWriter::SyncLoop(void)
{
    while (!in_dtor)
    {
        bufferSyncWait.wait(written > tfw_min_write_size ? 1000 : 100);
        Sync();
    }
}

/** \fn ThreadedFileWriter::DiskLoop(void)
 *  \brief The thread run method that actually calls safe_write().
 */
void ThreadedFileWriter::DiskLoop(void)
{
    uint size = 0;
    written = 0;

    while (!in_dtor || BufUsed() > 0)
    {
        size = BufUsed();

        if (size == 0)
            bufferEmpty.wakeAll();

        if (!size || (!in_dtor && !flush &&
            ((size < tfw_min_write_size) &&
             (written >= tfw_min_write_size))))
        {
            bufferHasData.wait(100);
            continue;
        }

        /* cap the max. write size. Prevents the situation where 90% of the
           buffer is valid, and we try to write all of it at once which
           takes a long time. During this time, the other thread fills up
           the 10% that was free... */
        size = (size > TFW_MAX_WRITE_SIZE) ? TFW_MAX_WRITE_SIZE : size;

        bool write_ok;
        if (ignore_writes)
            ;
        else if ((rpos + size) > tfw_buf_size)
        {
            int first_chunk_size  = tfw_buf_size - rpos;
            int second_chunk_size = size - first_chunk_size;
            size = safe_write(fd, buf+rpos, first_chunk_size, write_ok);
            if ((int)size == first_chunk_size && write_ok)
                size += safe_write(fd, buf, second_chunk_size, write_ok);
        }
        else
        {
            size = safe_write(fd, buf+rpos, size, write_ok);
        }

        QString msg =
            "Maximum file size exceeded by '%1'\n\t\t\t"
            "You must either change the process ulimits, configure\n\t\t\t"
            "your operating system with \"Large File\" support, or use\n\t\t\t"
            "a filesystem which supports 64-bit or 128-bit files.\n\t\t\t"
            "HINT: FAT32 is a 32-bit filesystem.";

        if (!ignore_writes && !write_ok && (EFBIG == errno))
        {
            VERBOSE(VB_IMPORTANT, msg.arg(filename));
            ignore_writes = true;
        }

        if (written < tfw_min_write_size)
        {
            written += size;
        }

        buflock.lock();
        rpos = (rpos + size) % tfw_buf_size;
        buflock.unlock();

        bufferWroteData.wakeAll();
    }
}

/** \fn ThreadedFileWriter::BufUsed(void)
 *  \brief Number of bytes queued for write by the write thread.
 */
uint ThreadedFileWriter::BufUsed(void)
{
    QMutexLocker locker(&buflock);
    return (wpos >= rpos) ? wpos - rpos : tfw_buf_size - rpos + wpos;
}

/** \fn ThreadedFileWriter::BufFree(void)
 *  \brief Number of bytes that can be written without blocking.
 */
uint ThreadedFileWriter::BufFree(void)
{
    QMutexLocker locker(&buflock);
    return ((wpos >= rpos) ? (rpos + tfw_buf_size) : rpos) - wpos - 1;
}

