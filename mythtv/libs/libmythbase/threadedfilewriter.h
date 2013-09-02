// -*- Mode: c++ -*-
#ifndef TFW_H_
#define TFW_H_

#include <vector>
using namespace std;

#include <QWaitCondition>
#include <QDateTime>
#include <QString>
#include <QMutex>

#include <fcntl.h>
#include <stdint.h>

#include "mythbaseexp.h"
#include "mthread.h"

class ThreadedFileWriter;

class TFWWriteThread : public MThread
{
  public:
    TFWWriteThread(ThreadedFileWriter *p) : MThread("TFWWrite"), m_parent(p) {}
    virtual ~TFWWriteThread() { wait(); m_parent = NULL; }
    virtual void run(void);
  private:
    ThreadedFileWriter *m_parent;
};

class TFWSyncThread : public MThread
{
  public:
    TFWSyncThread(ThreadedFileWriter *p) : MThread("TFWSync"), m_parent(p) {}
    virtual ~TFWSyncThread() { wait(); m_parent = NULL; }
    virtual void run(void);
  private:
    ThreadedFileWriter *m_parent;
};

class MBASE_PUBLIC ThreadedFileWriter
{
    friend class TFWWriteThread;
    friend class TFWSyncThread;
  public:
    ThreadedFileWriter(const QString &fname, int flags, mode_t mode);
    ~ThreadedFileWriter();

    bool Open(void);
    bool ReOpen(QString newFilename = "");

    long long Seek(long long pos, int whence);
    uint Write(const void *data, uint count);

    void SetWriteBufferMinWriteSize(uint newMinSize = kMinWriteSize);

    void Sync(void);
    void Flush(void);
    bool SetBlocking(bool block = true);

  protected:
    void DiskLoop(void);
    void SyncLoop(void);
    void TrimEmptyBuffers(void);

  private:
    // file info
    QString         filename;
    int             flags;
    mode_t          mode;
    int             fd;

    // state
    bool            flush;              // protected by buflock
    bool            in_dtor;            // protected by buflock
    bool            ignore_writes;      // protected by buflock
    uint            tfw_min_write_size; // protected by buflock
    uint            totalBufferUse;     // protected by buflock

    // buffers
    class TFWBuffer
    {
      public:
        vector<char> data;
        QDateTime    lastUsed;
    };
    mutable QMutex    buflock;
    QList<TFWBuffer*> writeBuffers;     // protected by buflock
    QList<TFWBuffer*> emptyBuffers;     // protected by buflock

    // threads
    TFWWriteThread *writeThread;
    TFWSyncThread  *syncThread;

    // wait conditions
    QWaitCondition  bufferEmpty;
    QWaitCondition  bufferHasData;
    QWaitCondition  bufferSyncWait;
    QWaitCondition  bufferWasFreed;

    // constants
    static const uint kMaxBufferSize;
    /// Minimum to write to disk in a single write, when not flushing buffer.
    static const uint kMinWriteSize;
    /// Maximum block size to write at a time
    static const uint kMaxBlockSize;

    bool m_warned;
    bool m_blocking;
};

#endif
