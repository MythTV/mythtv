// -*- Mode: c++ -*-
#ifndef TFW_H_
#define TFW_H_

#include <cstdint>
#include <fcntl.h>
#include <utility>
#include <vector>

// Qt headers
#include <QWaitCondition>
#include <QDateTime>
#include <QString>
#include <QMutex>

// MythTV headers
#include "mythbaseexp.h"
#include "mthread.h"

class ThreadedFileWriter;

class TFWWriteThread : public MThread
{
  public:
    explicit TFWWriteThread(ThreadedFileWriter *p) : MThread("TFWWrite"), m_parent(p) {}
    ~TFWWriteThread() override { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    ThreadedFileWriter *m_parent {nullptr};
};

class TFWSyncThread : public MThread
{
  public:
    explicit TFWSyncThread(ThreadedFileWriter *p) : MThread("TFWSync"), m_parent(p) {}
    ~TFWSyncThread() override { wait(); m_parent = nullptr; }
    void run(void) override; // MThread
  private:
    ThreadedFileWriter *m_parent {nullptr};
};

class MBASE_PUBLIC ThreadedFileWriter
{
    friend class TFWWriteThread;
    friend class TFWSyncThread;
  public:
    /** \fn ThreadedFileWriter::ThreadedFileWriter(const QString&,int,mode_t)
     *  \brief Creates a threaded file writer.
     */
    ThreadedFileWriter(QString fname, int flags, mode_t mode)
        : m_filename(std::move(fname)), m_flags(flags), m_mode(mode) {}
    ~ThreadedFileWriter();

    bool Open(void);
    bool ReOpen(const QString& newFilename = "");

    long long Seek(long long pos, int whence);
    int Write(const void *data, uint count);

    void SetWriteBufferMinWriteSize(uint newMinSize = kMinWriteSize);

    void Sync(void) const;
    void Flush(void);
    bool SetBlocking(bool block = true);
    bool WritesFailing(void) const { return m_ignoreWrites; }

  protected:
    void DiskLoop(void);
    void SyncLoop(void);
    void TrimEmptyBuffers(void);

  private:
    // file info
    QString         m_filename;
    int             m_flags;
    mode_t          m_mode;
    int             m_fd                 {-1};

    // state
    bool            m_flush              {false};         // protected by buflock
    bool            m_inDtor             {false};         // protected by buflock
    bool            m_ignoreWrites       {false};         // protected by buflock
    uint            m_tfwMinWriteSize    {kMinWriteSize}; // protected by buflock
    uint            m_totalBufferUse     {0};             // protected by buflock

    // buffers
    class TFWBuffer
    {
      public:
        std::vector<char> data;
        QDateTime    lastUsed;
    };
    mutable QMutex    m_bufLock;
    QList<TFWBuffer*> m_writeBuffers;     // protected by buflock
    QList<TFWBuffer*> m_emptyBuffers;     // protected by buflock

    // threads
    TFWWriteThread *m_writeThread        {nullptr};
    TFWSyncThread  *m_syncThread         {nullptr};

    // wait conditions
    QWaitCondition  m_bufferEmpty;
    QWaitCondition  m_bufferHasData;
    QWaitCondition  m_bufferSyncWait;
    QWaitCondition  m_bufferWasFreed;

    // constants
    static const uint kMaxBufferSize;
    /// Minimum to write to disk in a single write, when not flushing buffer.
    static const uint kMinWriteSize;
    /// Maximum block size to write at a time
    static const uint kMaxBlockSize;

    bool m_warned                        {false};
    bool m_blocking                      {false};
    bool m_registered                    {false};
};

#endif
