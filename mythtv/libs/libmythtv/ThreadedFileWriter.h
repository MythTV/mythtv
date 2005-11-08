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
    QString         filename;
    int             flags;
    mode_t          mode;
    int             fd;

    bool            no_writes;
    bool            flush;

    unsigned long   tfw_buf_size;
    unsigned long   tfw_min_write_size;
    uint            rpos;
    uint            wpos;
    char           *buf;
    int             in_dtor;

    pthread_t       writer;
    QMutex          buflock;

    pthread_t       syncer;

    QWaitCondition  bufferEmpty;
    QWaitCondition  bufferHasData;
    QWaitCondition  bufferSyncWait;
    QWaitCondition  bufferWroteData;
  private:
    static const uint TFW_DEF_BUF_SIZE   = 2*1024*1024;
    static const uint TFW_MAX_WRITE_SIZE = TFW_DEF_BUF_SIZE / 4;
    static const uint TFW_MIN_WRITE_SIZE = TFW_DEF_BUF_SIZE / 8;
};

#endif
