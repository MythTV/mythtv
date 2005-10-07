#ifndef TFW_H_
#define TFW_H_

#include <pthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>

#define TFW_DEF_BUF_SIZE (2*1024*1024)
#define TFW_MAX_WRITE_SIZE (TFW_DEF_BUF_SIZE / 4)
#define TFW_MIN_WRITE_SIZE (TFW_DEF_BUF_SIZE / 8)

class ThreadedFileWriter
{
  public:
    ThreadedFileWriter(const char *filename, int flags, mode_t mode);
    ~ThreadedFileWriter();      /* commits all writes and closes the file. */

    bool Open();

    long long Seek(long long pos, int whence);
    unsigned Write(const void *data, unsigned count);

    // Note, this doesn't even try to flush our queue, only ensure that
    // data which has already been sent to the kernel is written to disk
    void Sync(void);

    void SetWriteBufferSize(int newSize = TFW_DEF_BUF_SIZE);
    void SetWriteBufferMinWriteSize(int newMinSize = TFW_MIN_WRITE_SIZE);

    unsigned BufUsed();  /* # of bytes queued for write by the write thread */
    unsigned BufFree();  /* # of bytes that can be written, without blocking */

    // allow DiskLoop() to flush buffer completely ignoring low watermark
    void Flush(void);

  protected:
    static void *boot_writer(void *);
    void DiskLoop(); /* The thread that actually calls write(). */

    static void *boot_syncer(void *);
    void SyncLoop(); /* The thread that calls sync(). */

  private:
    const char     *filename;
    int             flags;
    mode_t          mode;
    int             fd;

    bool            no_writes;
    bool            flush;

    unsigned long   tfw_buf_size;
    unsigned long   tfw_min_write_size;
    unsigned int    rpos;
    unsigned int    wpos;
    char           *buf;
    int             in_dtor;

    pthread_t       writer;
    QMutex          buflock;

    pthread_t       syncer;

    QWaitCondition  bufferEmpty;
    QWaitCondition  bufferHasData;
    QWaitCondition  bufferSyncWait;
    QWaitCondition  bufferWroteData;
};

#endif
