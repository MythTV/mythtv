// -*- Mode: c++ -*-

#ifndef RINGBUFFER
#define RINGBUFFER

#include <QReadWriteLock>
#include <QWaitCondition>
#include <QString>
#include <QThread>
#include <QMutex>

#include "mythconfig.h"

extern "C" {
#include "avcodec.h"
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
               bool usereadahead = true, uint read_retries = 12/*6*/);
   ~RingBuffer();

    // Sets
    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);
    void UpdateRawBitrate(uint rawbitrate);
    void UpdatePlaySpeed(float playspeed);

    // Gets
    /// Returns name of file used by this RingBuffer
    QString   GetFilename(void)      const { return filename; }
    QString   GetSubtitleFilename(void) const { return subtitlefilename; }
    /// Returns ReadBufAvail(void)
    int       DataInReadAhead(void)  const { return ReadBufAvail(); }
    /// Returns value of stopreads
    /// \sa StartReads(void), StopReads(void)
    bool      GetStopReads(void)     const { return stopreads; }
    /// Returns false iff read-ahead is not
    /// running and read-ahead is not paused.
    bool      isPaused(void)         const
        { return (!readaheadrunning) ? true : readaheadpaused; }
    long long GetReadPosition(void)  const;
    long long GetWritePosition(void) const;
    long long GetRealFileSize(void)  const;
    uint      GetBitrate(void)       const;
    uint      GetReadBlockSize(void) const;
    bool      IsOpen(void)           const;

    // General Commands
    void OpenFile(const QString &lfilename, uint retryCount = 12/*4*/);
    int  Read(void *buf, int count);
    int  Peek(void *buf, int count); // only works with readahead

    void Reset(bool full          = false,
               bool toAdjust      = false,
               bool resetInternal = false);

    // Seeks
    long long Seek(long long pos, int whence);

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
    /// Tells RingBuffer whether to igonre the end-of-file
    void IgnoreLiveEOF(bool ignore) { ignoreliveeof = ignore; }

    // ThreadedFileWriter proxies
    int  Write(const void *buf, uint count);
    bool IsIOBound(void) const;
    void WriterFlush(void);
    void Sync(void);
    long long WriterSeek(long long pos, int whence);

    // DVDRingBuffer proxies
    /// Returns true if this is a DVD backed RingBuffer.
    inline bool isDVD(void) const { return dvdPriv; }
    DVDRingBufferPriv *DVD() { return dvdPriv; }
    bool InDVDMenuOrStillFrame(void);

    // BDRingBuffer proxies
    /// Returns true if this is a Blu-ray backed RingBuffer.
    inline bool isBD(void) const { return bdPriv; }
    BDRingBufferPriv *BD() { return bdPriv; }

    long long SetAdjustFilesize(void);
    void SetTimeout(bool fast) { oldfile = fast; }

  protected:
    void run(void); // QThread
    void CalcReadAheadThresh(void);
    int safe_read_bd(void *data, uint sz);
    int safe_read_dvd(void *data, uint sz);
    int safe_read(int fd, void *data, uint sz);
    int safe_read(RemoteFile *rf, void *data, uint sz);

    int ReadFromBuf(void *buf, int count, bool peek = false);

    int ReadBufFree(void) const;
    int ReadBufAvail(void) const;

    void StartupReadAheadThread(void);
    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

  private:
    // NR == trivial risk, not protected, but only modified in single thread
    // LR == low risk, not protected, but only modified on Open,Close,ctor,dtor
    // HR == high risk, likely to cause unexpected behaviour
    // MR == medium risk, unsafe methods unlikely to be called at wrong moment

    QString filename;             // not protected by a lock LR
    QString subtitlefilename;     // not protected by a lock LR

    ThreadedFileWriter *tfw;      // not protected by a lock LR
    int fd2;                      // not protected by a lock LR

    bool writemode;               // not protected by a lock LR

    long long readpos;            // not protected by a lock HR
    long long writepos;           // not protected by a lock HR

    bool stopreads;               // not protected by a lock HR

    mutable QReadWriteLock rwlock;

    RemoteFile *remotefile;       // not protected by a lock LR

    // this lock does not consistently protect anything,
    // but seems to be intented to protect rbrpos & rbwpos
    mutable QMutex readAheadLock;

    bool startreadahead;          // not protected by a lock HR
    char *readAheadBuffer;        // not protected by a lock MR
    bool readaheadrunning;        // not protected by a lock HR
    bool readaheadpaused;         // not protected by a lock HR
    bool pausereadthread;         // not protected by a lock HR
    int rbrpos;                   // not protected by a lock HR
    int rbwpos;                   // not protected by a lock HR
    long long internalreadpos;    // not protected by a lock HR
    bool ateof;                   // not protected by a lock HR
    bool readsallowed;            // not protected by a lock HR
    volatile bool wantseek;       // not protected by a lock HR
    bool setswitchtonext;         // protected by rwlock

    uint           rawbitrate;    // protected by rwlock
    float          playspeed;     // protected by rwlock
    int            fill_threshold;// not protected by a lock HR
    int            fill_min;      // protected by rwlock
    int            readblocksize; // protected by rwlock

    QWaitCondition pauseWait;     // not protected by a lock HR

    int wanttoread;               // not protected by a lock HR
    QWaitCondition availWait;     // protected by availWaitMutex
    QMutex availWaitMutex;

    QWaitCondition readsAllowedWait;// protected by readsAllowedWaitMutex
    QMutex readsAllowedWaitMutex;

    int numfailures;              // not protected by a lock MR

    bool commserror;              // not protected by a lock MR

    DVDRingBufferPriv *dvdPriv;   // not protected by a lock LR
    BDRingBufferPriv  *bdPriv;    // not protected by a lock LR

    bool oldfile;                 // not protected by a lock LR

    LiveTVChain *livetvchain;     // not protected by a lock HR
    bool ignoreliveeof;           // not protected by a lock HR

    long long readAdjust;         // not protected by a lock HR

    /// Condition to signal that the read ahead thread is running
    QWaitCondition readAheadRunningCond;//protected by readAheadRunningCondLock
    QMutex readAheadRunningCondLock;

  public:
    static QMutex subExtLock;
    static QStringList subExt;
    static QStringList subExtNoCheck;

    // constants
  public:
    static const uint kBufferSize;
    static const uint kReadTestSize;
};

#endif
