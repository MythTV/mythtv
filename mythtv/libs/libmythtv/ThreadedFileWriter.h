// -*- Mode: c++ -*-
#ifndef TFW_H_
#define TFW_H_

#include <QWaitCondition>
#include <QString>
#include <QMutex>
#include <QThread>

#include <fcntl.h>
#include <stdint.h>

class ThreadedFileWriter;

class TFWWriteThread : public QThread
{
    Q_OBJECT
  public:
    TFWWriteThread(void) : m_ptr(NULL) {};
    void SetPtr(ThreadedFileWriter *ptr) { m_ptr = ptr; };
    void run(void);
  private:
    ThreadedFileWriter *m_ptr;
};

class TFWSyncThread : public QThread
{
    Q_OBJECT
  public:
    TFWSyncThread(void) : m_ptr(NULL) {};
    void SetPtr(ThreadedFileWriter *ptr) { m_ptr = ptr; };
    void run(void);
  private:
    ThreadedFileWriter *m_ptr;
};

class ThreadedFileWriter
{
    friend class TFWWriteThread;
    friend class TFWSyncThread;
  public:
    ThreadedFileWriter(const QString &fname, int flags, mode_t mode);
    ~ThreadedFileWriter();

    bool Open(void);

    long long Seek(long long pos, int whence);
    uint Write(const void *data, uint count);

    void SetWriteBufferSize(uint newSize = TFW_DEF_BUF_SIZE);
    void SetWriteBufferMinWriteSize(uint newMinSize = TFW_MIN_WRITE_SIZE);

    uint BufUsed(void) const;
    uint BufFree(void) const;

    void Sync(void);
    void Flush(void);

  protected:
    void DiskLoop(void);
    void SyncLoop(void);

    uint BufUsedPriv(void) const;
    uint BufFreePriv(void) const;

  private:
    // file info
    QString         filename;
    int             flags;
    mode_t          mode;
    int             fd;
    uint64_t        m_file_sync;  ///< offset synced to disk
    uint64_t        m_file_wpos; ///< offset written to disk

    // state
    bool            no_writes;
    bool            flush;
    bool            write_is_blocked;
    bool            in_dtor;
    bool            ignore_writes;
    long long       tfw_min_write_size;

    // buffer position state
    volatile uint   rpos;    ///< points to end of data written to disk
    volatile uint   wpos;    ///< points to end of data added to buffer
    mutable QMutex  buflock; ///< lock needed to update rpos and wpos
    long long       written;

    // buffer
    char           *buf;
    unsigned long   tfw_buf_size;

    // threads
    TFWWriteThread  writer;
    TFWSyncThread   syncer;

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
