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

class RemoteFile;
class ThreadedFileWriter;
class DVDRingBufferPriv;
class BDRingBufferPriv;
class LiveTVChain;

class MPUBLIC RingBuffer : protected QThread
{
  public:
    RingBuffer(const QString &lfilename, bool write,
               bool usereadahead = true,
               int timeout_ms = kDefaultOpenTimeout);
   ~RingBuffer();

    // Sets
    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);
    void SetOldFile(bool is_old);
    void SetStreamOnly(bool stream);
    void UpdateRawBitrate(uint rawbitrate);
    void UpdatePlaySpeed(float playspeed);

    // Gets
    QString   GetFilename(void)      const;
    QString   GetSubtitleFilename(void) const;
    /// Returns value of stopreads
    /// \sa StartReads(void), StopReads(void)
    bool      GetStopReads(void)     const { return stopreads; }
    bool      isPaused(void)         const;
    long long GetReadPosition(void)  const;
    long long GetWritePosition(void) const;
    long long GetRealFileSize(void)  const;
    bool      IsOpen(void)           const;
    bool      IsNearEnd(double fps, uint vvf) const;

    // General Commands
    void OpenFile(const QString &lfilename,
                  uint retry_ms = kDefaultOpenTimeout);
    int  Read(void *buf, int count);
    int  Peek(void *buf, int count); // only works with readahead

    void Reset(bool full          = false,
               bool toAdjust      = false,
               bool resetInternal = false);

    // Seeks
    long long Seek(long long pos, int whence, bool has_lock = false);

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

    // DVDRingBuffer proxies
    bool IsDVD(void) const;
    bool InDVDMenuOrStillFrame(void);

    // Temporary DVD locking mechanisms (fixed in 0.25)
    void DVDUnlockRW(void)       { rwlock.unlock();       }
    void DVDLockRWForWrite(void) { rwlock.lockForWrite(); }

    // BDRingBuffer proxies
    bool IsBD(void) const;

    long long SetAdjustFilesize(void);

    /// Calls SetOldFile(), do not use
    void SetTimeout(bool is_old) MDEPRECATED { SetOldFile(is_old); }
    /// Calls IsDVD(), do not use
    bool isDVD(void) const MDEPRECATED { return IsDVD(); }
    /// Calls IsBD(), do not use
    bool isBD(void) const MDEPRECATED { return IsBD(); }
    /// Illicitly manipulating privates is ill advised!
    /// DO NOT USE
    DVDRingBufferPriv *DVD() MDEPRECATED
    {
        return dvdPriv;
    }
    /// Illicitly manipulating privates is ill advised!
    /// DO NOT USE
    BDRingBufferPriv *BD() MDEPRECATED
    {
        return bdPriv;
    }

    static const int kDefaultOpenTimeout;
    static const int kLiveTVOpenTimeout;

  protected:
    void run(void); // QThread
    void CalcReadAheadThresh(void);
    bool PauseAndWait(void);
    int safe_read_bd(void *data, uint sz);
    int safe_read_dvd(void *data, uint sz);
    int safe_read(int fd, void *data, uint sz);
    int safe_read(RemoteFile *rf, void *data, uint sz);

    int ReadPriv(void *buf, int count, bool peek);
    int ReadDirect(void *buf, int count, bool peek);
    bool WaitForReadsAllowed(void);
    bool WaitForAvail(int count);

    int ReadBufFree(void) const;
    int ReadBufAvail(void) const;

    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

  private:
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
    bool      streamOnly;         // protected by rwlock
    bool      ignorereadahead;    // protected by rwlock
    uint      rawbitrate;         // protected by rwlock
    float     playspeed;          // protected by rwlock
    int       fill_threshold;     // protected by rwlock
    int       fill_min;           // protected by rwlock
    int       readblocksize;      // protected by rwlock
    int       wanttoread;         // protected by rwlock
    int       numfailures;        // protected by rwlock (see note 1)
    bool      commserror;         // protected by rwlock

    // We should really subclass for these two sets of functionality..
    // current implementation is not thread-safe.
    DVDRingBufferPriv *dvdPriv; // NOT protected by a lock
    BDRingBufferPriv  *bdPriv;  // NOT protected by a lock

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
