// -*- Mode: c++ -*-
#ifndef TFW_H_
#define TFW_H_

#include <pthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qstring.h>

class ThreadedFileWriter
{
  public:
    ThreadedFileWriter(const QString &fname, int flags, mode_t mode);
    ~ThreadedFileWriter();

    bool Open(void);

    long long Seek(long long pos, int whence);
    uint Write(const void *data, uint count);

    void SetWriteBufferSize(uint newSize = TFW_DEF_BUF_SIZE);
    void SetWriteBufferMinWriteSize(uint newMinSize = TFW_MIN_WRITE_SIZE);

    uint BufUsed(void);
    uint BufFree(void);

    void Sync(void);
    void Flush(void);

  protected:
    static void *boot_writer(void *);
    void DiskLoop(void);

    static void *boot_syncer(void *);
    void SyncLoop(void);

  private:
    // file info
    QString         filename;
    int             flags;
    mode_t          mode;
    int             fd;

    // state
    bool            no_writes;
    bool            flush;
    bool            write_is_blocked;
    bool            in_dtor;
    bool            ignore_writes;
    long long       tfw_min_write_size;

    // buffer position state
    uint            rpos;    ///< points to end of data written to disk
    uint            wpos;    ///< points to end of data added to buffer
    QMutex          buflock; ///< lock needed to update rpos and wpos
    long long       written;

    // buffer
    char           *buf;
    unsigned long   tfw_buf_size;

    // threads
    pthread_t       writer;
    pthread_t       syncer;

    // wait conditions
    QWaitCondition  bufferEmpty;
    QWaitCondition  bufferHasData;
    QWaitCondition  bufferSyncWait;
    QWaitCondition  bufferWroteData;

  private:
    // constants
    /// Default buffer size.
    static const uint TFW_DEF_BUF_SIZE;
    /// Maximum to write to disk in a single write.
    static const uint TFW_MAX_WRITE_SIZE;
    /// Minimum to write to disk in a single write, when not flushing buffer.
    static const uint TFW_MIN_WRITE_SIZE;
};

#endif
