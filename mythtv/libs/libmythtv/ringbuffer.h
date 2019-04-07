// -*- Mode: c++ -*-

#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <QReadWriteLock>
#include <QWaitCondition>
#include <QString>
#include <QMutex>
#include <QMap>

#include "mythconfig.h"
#include "mthread.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "mythtvexp.h"

#define PNG_MIN_SIZE   20 /* header plus one empty chunk */
#define NUV_MIN_SIZE  204 /* header size? */
#define MPEG_MIN_SIZE 376 /* 2 TS packets */

/* should be minimum of the above test sizes */
#define kReadTestSize PNG_MIN_SIZE

// about one second at 35mbit
#define BUFFER_SIZE_MINIMUM (4 * 1024 * 1024)
#define BUFFER_FACTOR_NETWORK  2
#define BUFFER_FACTOR_BITRATE  2
#define BUFFER_FACTOR_MATROSKA 2

#define CHUNK 32768 /* readblocksize increments */

class ThreadedFileWriter;
class DVDRingBuffer;
class BDRingBuffer;
class LiveTVChain;
class RemoteFile;

enum RingBufferType
{
    kRingBuffer_Unknown = 0,
    kRingBuffer_File,
    kRingBuffer_DVD,
    kRingBuffer_BD,
    kRingBuffer_HTTP,
    kRingBuffer_HLS,
    kRingBuffer_MHEG
};

class MTV_PUBLIC RingBuffer : protected MThread
{
    friend class ICRingBuffer;

  public:
    static RingBuffer *Create(const QString &xfilename, bool write,
                              bool usereadahead = true,
                              int timeout_ms = kDefaultOpenTimeout,
                              bool stream_only = false);
    virtual ~RingBuffer() = 0;

    // Sets
    void SetWriteBufferSize(int newSize);
    void SetWriteBufferMinWriteSize(int newMinSize);
    void SetOldFile(bool is_old);
    void UpdateRawBitrate(uint raw_bitrate);
    void UpdatePlaySpeed(float play_speed);
    void EnableBitrateMonitor(bool enable) { m_bitrateMonitorEnabled = enable; }
    void SetBufferSizeFactors(bool estbitrate, bool matroska);
    void SetWaitForWrite(void) { m_waitForWrite = true; }

    // Gets
    QString   GetSafeFilename(void) { return m_safeFilename; }
    QString   GetFilename(void)      const;
    QString   GetSubtitleFilename(void) const;
    QString   GetLastError(void)     const;
    bool      GetCommsError(void) const { return m_commsError; }
    void      ResetCommsError(void) { m_commsError = 0; }

    /// Returns value of stopreads
    /// \sa StartReads(void), StopReads(void)
    bool      GetStopReads(void)     const { return m_stopReads; }
    bool      isPaused(void)         const;
    /// \brief Returns how far into the file we have read.
    virtual long long GetReadPosition(void)  const = 0;
    QString GetDecoderRate(void);
    QString GetStorageRate(void);
    QString GetAvailableBuffer(void);
    uint    GetBufferSize(void) { return m_bufferSize; }
    long long GetWritePosition(void) const;
    /// \brief Returns the size of the file we are reading/writing,
    ///        or -1 if the query fails.
    long long GetRealFileSize(void) const;
    bool      IsNearEnd(double fps, uint vvf) const;
    /// \brief Returns true if open for either reading or writing.
    virtual bool IsOpen(void) const = 0;
    virtual bool IsStreamed(void)       { return LiveMode(); }
    virtual bool IsSeekingAllowed(void) { return true;  }
    virtual bool IsBookmarkAllowed(void) { return true; }
    virtual int  BestBufferSize(void)   { return 32768; }
    static QString BitrateToString(uint64_t rate, bool hz = false);
    RingBufferType GetType() const { return m_type; }

    // LiveTV used utilities
    int GetReadBufAvail() const;
    bool SetReadInternalMode(bool mode);
    bool IsReadInternalMode(void) { return m_readInternalMode; }

    // DVD and bluray methods
    bool IsDisc(void) const { return IsDVD() || IsBD(); }
    bool IsDVD(void)  const { return m_type == kRingBuffer_DVD; }
    bool IsBD(void)   const { return m_type == kRingBuffer_BD;  }
    const DVDRingBuffer *DVD(void) const;
    const BDRingBuffer  *BD(void)  const;
    DVDRingBuffer *DVD(void);
    BDRingBuffer  *BD(void);
    virtual bool StartFromBeginning(void)                   { return true;  }
    virtual void IgnoreWaitStates(bool /*ignore*/)          { }
    virtual bool IsInMenu(void) const                       { return false; }
    virtual bool IsInStillFrame(void) const                 { return false; }
    virtual bool IsInDiscMenuOrStillFrame(void) const       { return IsInMenu() || IsInStillFrame(); }
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
    virtual bool ReOpen(QString /*newFilename*/ = "") { return false; }

    int  Read(void *buf, int count);
    int  Peek(void *buf, int count); // only works with readahead

    void Reset(bool full          = false,
               bool toAdjust      = false,
               bool resetInternal = false);

    /// \brief Seeks to a particular position in the file.
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
    bool WriterSetBlocking(bool lock = true);

    long long SetAdjustFilesize(void);

    static const int kDefaultOpenTimeout;
    static const int kLiveTVOpenTimeout;

    static void AVFormatInitNetwork(void);

  protected:
    explicit RingBuffer(RingBufferType rbtype);

    void run(void) override; // MThread
    void CreateReadAheadBuffer(void);
    void CalcReadAheadThresh(void);
    bool PauseAndWait(void);
    virtual int safe_read(void *data, uint sz) = 0;

    int ReadPriv(void *buf, int count, bool peek);
    int ReadDirect(void *buf, int count, bool peek);
    bool WaitForReadsAllowed(void);
    int WaitForAvail(int count, int timeout);
    virtual long long GetRealFileSizeInternal(void) const { return -1; }
    virtual long long SeekInternal(long long pos, int whence) = 0;

    int ReadBufFree(void) const;
    int ReadBufAvail(void) const;

    void ResetReadAhead(long long newinternal);
    void KillReadAheadThread(void);

    uint64_t UpdateDecoderRate(uint64_t latest = 0);
    uint64_t UpdateStorageRate(uint64_t latest = 0);

  protected:
    RingBufferType         m_type;

    //
    // The following are protected by posLock
    //
    mutable QReadWriteLock m_posLock;
    long long              m_readPos         {0};
    long long              m_writePos        {0};
    long long              m_internalReadPos {0};
    long long              m_ignoreReadPos   {-1};

    //
    // The following are protected by rbrLock
    //
    mutable QReadWriteLock m_rbrLock;
    int                    m_rbrPos          {0};

    //
    // The following are protected by rbwLock
    //
    mutable QReadWriteLock m_rbwLock;
    int                    m_rbwPos          {0};

    // note should not go under rwLock..
    // this is used to break out of read_safe where rwLock is held
    volatile bool          m_stopReads       {false};

    //
    // unprotected (for debugging)
    //
    QString                m_safeFilename;

    //
    // The following are protected by rwLock
    //
    mutable QReadWriteLock m_rwLock;
    QString                m_filename;
    QString                m_subtitleFilename;
    QString                m_lastError;

    ThreadedFileWriter    *m_tfw              {nullptr};
    int                    m_fd2              {-1};

    bool                   m_writeMode        {false};

    RemoteFile            *m_remotefile       {nullptr};

    uint                   m_bufferSize       {BUFFER_SIZE_MINIMUM};
    bool                   m_lowBuffers       {false};
    bool                   m_fileIsMatroska   {false};
    bool                   m_unknownBitrate   {false};
    bool                   m_startReadAhead   {false};
    char                  *m_readAheadBuffer  {nullptr};
    bool                   m_readAheadRunning {false};
    bool                   m_reallyRunning    {false};
    bool                   m_requestPause     {false};
    bool                   m_paused           {false};
    bool                   m_ateof            {false};
    bool                   m_waitForWrite     {false};
    bool                   m_beingWritten     {false};
    bool                   m_readsAllowed     {false};
    bool                   m_readsDesired     {false};
    volatile bool          m_recentSeek       {true}; // not protected by rwLock
    bool                   m_setSwitchToNext  {false};
    uint                   m_rawBitrate       {8000};
    float                  m_playSpeed        {1.0F};
    int                    m_fillThreshold    {65536};
    int                    m_fillMin          {-1};
    int                    m_readBlockSize    {CHUNK};
    int                    m_wantToRead       {0};
    int                    m_numFailures      {0};    // (see note 1)
    bool                   m_commsError       {false};

    bool                   m_oldfile          {false};

    LiveTVChain           *m_liveTVChain      {nullptr};
    bool                   m_ignoreLiveEOF    {false};

    long long              m_readAdjust       {0};

    // Internal RingBuffer Method
    int                    m_readOffset       {0};
    bool                   m_readInternalMode {false};
    //
    // End of section protected by rwLock
    //

    // bitrate monitors
    bool                   m_bitrateMonitorEnabled {false};
    QMutex                 m_decoderReadLock;
    QMap<qint64, uint64_t> m_decoderReads;
    QMutex                 m_storageReadLock;
    QMap<qint64, uint64_t> m_storageReads;

    // note 1: numfailures is modified with only a read lock in the
    // read ahead thread, but this is safe since all other places
    // that use it are protected by a write lock. But this is a
    // fragile state of affairs and care must be taken when modifying
    // code or locking around this variable.

    /// Condition to signal that the read ahead thread is running
    QWaitCondition         m_generalWait; // protected by rwLock

  public:
    static QMutex      s_subExtLock;
    static QStringList s_subExt;
    static QStringList s_subExtNoCheck;

  private:
    static bool gAVformat_net_initialised;
    bool m_bitrateInitialized {false};
};

#endif // _RINGBUFFER_H_
