// -*- Mode: c++ -*-

#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <QReadWriteLock>
#include <QWaitCondition>
#include <QString>
#include <QThread>
#include <QMutex>

#include "mythconfig.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "mythexp.h"

class ThreadedFileWriter;
class DVDRingBuffer;
class BDRingBuffer;
class LiveTVChain;
class RemoteFile;

class MPUBLIC RingBuffer : protected QThread
{
  public:
    static RingBuffer *Create(const QString &lfilename, bool write,
                              bool usereadahead = true,
                              int timeout_ms = kDefaultOpenTimeout,
                              bool stream_only = false);
    ~RingBuffer();

    // Sets
    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);
    void SetOldFile(bool is_old);
    void UpdateRawBitrate(uint rawbitrate);
    void UpdatePlaySpeed(float playspeed);

    // Gets
    QString   GetFilename(void)      const;
    QString   GetSubtitleFilename(void) const;
    /// Returns value of stopreads
    /// \sa StartReads(void), StopReads(void)
    bool      GetStopReads(void)     const { return stopreads; }
    bool      isPaused(void)         const;
    /// \brief Returns how far into the file we have read.
    virtual long long GetReadPosition(void)  const = 0;
    long long GetWritePosition(void) const;
    /// \brief Returns the size of the file we are reading/writing,
    ///        or -1 if the query fails.
    virtual long long GetRealFileSize(void)  const { return -1; }
    bool      IsNearEnd(double fps, uint vvf) const;
    /// \brief Returns true if open for either reading or writing.
    virtual bool IsOpen(void) const = 0;

    // DVD and bluray methods
    bool IsDisc(void) const { return IsDVD() || IsBD(); }
    bool IsDVD(void)  const { return DVD() != NULL;     }
    bool IsBD(void)   const { return BD()  != NULL;     }
    const DVDRingBuffer *DVD(void) const;
    const BDRingBuffer  *BD(void)  const;
    DVDRingBuffer *DVD(void);
    BDRingBuffer  *BD(void);

    virtual bool IsInDiscMenuOrStillFrame(void) const       { return false; }
    virtual bool HandleAction(const QStringList &, int64_t) { return false; }

    // General Commands

    /** \brief Opens a file for reading.
     *
     *  \param lfilename  Name of file to read
     *  \param retry_ms   How many ms to retry reading the file
     *                    after the first try before giving up.
     */
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout) = 0;

    int  Read(void *buf, int count);
    int  Peek(void *buf, int count); // only works with readahead

    void Reset(bool full          = false,
               bool toAdjust      = false,
               bool resetInternal = false);

    /// \brief Seeks to a particular position in the file.
    virtual long long Seek(
        long long pos, int whence, bool has_lock = false) = 0;

    // Pause commands
    void Pause(void);
    void Unpause(void);
    void WaitForPause(void);

    // Start/Stop commands
    void Start(void);
    void StopReads(void);
    void StartReads(void);

    // LiveTVChain support
    bool LiveMode(void) const;
    void SetLiveMode(LiveTVChain *chain);
    void IgnoreLiveEOF(bool ignore);

    // ThreadedFileWriter proxies
    int  Write(const void *buf, uint count);
    bool IsIOBound(void) const;
    void WriterFlush(void);
    void Sync(void);
    long long WriterSeek(long long pos, int whence, bool has_lock = false);

    long long SetAdjustFilesize(void);

    /// Calls SetOldFile(), do not use
    void SetTimeout(bool is_old) MDEPRECATED { SetOldFile(is_old); }

    static const int kDefaultOpenTimeout;
    static const int kLiveTVOpenTimeout;

  protected:
    RingBuffer();

    void run(void); // QThread
    void CalcReadAheadThresh(void);
    bool PauseAndWait(void);
    virtual int safe_read(void *data, uint sz) = 0;

    int ReadPriv(void *buf, int count, bool peek);
    int ReadDirect(void *buf, int count, bool peek);
    bool WaitForReadsAllowed(void);
    bool WaitForAvail(int count);

    int ReadBufFree(void) const;
    int ReadBufAvail(void) const;

    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

  protected:
    mutable QReadWriteLock poslock;
    long long readpos;            // protected by poslock
    long long writepos;           // protected by poslock
    long long internalreadpos;    // protected by poslock
    long long ignorereadpos;      // protected by poslock
    mutable QReadWriteLock rbrlock;
    int       rbrpos;             // protected by rbrlock
    mutable QReadWriteLock rbwlock;
    int       rbwpos;             // protected by rbwlock

    // note should not go under rwlock..
    // this is used to break out of read_safe where rwlock is held
    volatile bool stopreads;

    mutable QReadWriteLock rwlock;

    QString filename;             // protected by rwlock
    QString subtitlefilename;     // protected by rwlock

    ThreadedFileWriter *tfw;      // protected by rwlock
    int fd2;                      // protected by rwlock

    bool writemode;               // protected by rwlock

    RemoteFile *remotefile;       // protected by rwlock

    bool      startreadahead;     // protected by rwlock
    char     *readAheadBuffer;    // protected by rwlock
    bool      readaheadrunning;   // protected by rwlock
    bool      reallyrunning;      // protected by rwlock
    bool      request_pause;      // protected by rwlock
    bool      paused;             // protected by rwlock
    bool      ateof;              // protected by rwlock
    bool      readsallowed;       // protected by rwlock
    bool      setswitchtonext;    // protected by rwlock
    bool      ignorereadahead;    // protected by rwlock
    uint      rawbitrate;         // protected by rwlock
    float     playspeed;          // protected by rwlock
    int       fill_threshold;     // protected by rwlock
    int       fill_min;           // protected by rwlock
    int       readblocksize;      // protected by rwlock
    int       wanttoread;         // protected by rwlock
    int       numfailures;        // protected by rwlock (see note 1)
    bool      commserror;         // protected by rwlock

    bool oldfile;                 // protected by rwlock

    LiveTVChain *livetvchain;     // protected by rwlock
    bool ignoreliveeof;           // protected by rwlock

    long long readAdjust;         // protected by rwlock

    // note 1: numfailures is modified with only a read lock in the
    // read ahead thread, but this is safe since all other places
    // that use it are protected by a write lock. But this is a
    // fragile state of affairs and care must be taken when modifying
    // code or locking around this variable.

    /// Condition to signal that the read ahead thread is running
    QWaitCondition generalWait;         // protected by rwlock

  public:
    static QMutex subExtLock;
    static QStringList subExt;
    static QStringList subExtNoCheck;

    // constants
  public:
    static const uint kBufferSize;
    static const uint kReadTestSize;
};

#endif // _RINGBUFFER_H_
