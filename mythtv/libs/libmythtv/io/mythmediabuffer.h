#ifndef MYTHMEDIABUFFER_H
#define MYTHMEDIABUFFER_H

// Qt
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QString>
#include <QMutex>
#include <QMap>

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythtv/mythtvexp.h"
#include "libmythbase/mthread.h"
#include "libmythbase/mythchrono.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

// Size of PNG header plus one empty chunk
static constexpr qint64 kReadTestSize { 20 };

// about one second at 35Mb
static constexpr uint32_t BUFFER_SIZE_MINIMUM    { 4 * 1024 * 1024 };
static constexpr uint8_t  BUFFER_FACTOR_NETWORK  { 2 };
static constexpr uint8_t  BUFFER_FACTOR_BITRATE  { 2 };
static constexpr uint8_t  BUFFER_FACTOR_MATROSKA { 2 };

static constexpr int32_t  DEFAULT_CHUNK_SIZE     { 32768 };

static inline QString seek2string(int Whence)
{
    if (SEEK_SET == Whence)
        return "SEEK_SET";
    if (SEEK_CUR == Whence)
        return "SEEK_CUR";
    return "SEEK_END";
}

class ThreadedFileWriter;
class MythDVDBuffer;
class MythBDBuffer;
class LiveTVChain;
class RemoteFile;

enum MythBufferType : std::uint8_t
{
    kMythBufferUnknown = 0,
    kMythBufferFile,
    kMythBufferDVD,
    kMythBufferBD,
    kMythBufferHTTP,
    kMythBufferHLS,
    kMythBufferMHEG
};

class MTV_PUBLIC MythMediaBuffer : protected MThread
{
    friend class MythInteractiveBuffer;

  public:
    static MythMediaBuffer *Create(const QString &Filename, bool Write,
                                   bool UseReadAhead = true,
                                   std::chrono::milliseconds Timeout = kDefaultOpenTimeout,
                                   bool StreamOnly = false);
    ~MythMediaBuffer() override = 0;
    MythBufferType GetType() const;

    static constexpr std::chrono::milliseconds kDefaultOpenTimeout {  2s };
    static constexpr std::chrono::milliseconds kLiveTVOpenTimeout  { 10s };
    static QString   BitrateToString     (uint64_t Rate, bool Hz = false);
    static void      AVFormatInitNetwork (void);

    void      SetOldFile           (bool Old);
    void      UpdateRawBitrate     (uint RawBitrate);
    void      UpdatePlaySpeed      (float PlaySpeed);
    void      EnableBitrateMonitor (bool Enable);
    void      SetBufferSizeFactors (bool EstBitrate, bool Matroska);
    void      SetWaitForWrite      (void);
    QString   GetSafeFilename      (void);
    QString   GetFilename          (void) const;
    QString   GetSubtitleFilename  (void) const;
    QString   GetLastError         (void) const;
    bool      GetCommsError        (void) const;
    void      ResetCommsError      (void);
    bool      GetStopReads         (void) const;
    QString   GetDecoderRate       (void);
    QString   GetStorageRate       (void);
    QString   GetAvailableBuffer   (void);
    uint      GetBufferSize        (void) const;
    bool      IsNearEnd            (double Framerate, uint Frames) const;
    long long GetWritePosition     (void) const;
    long long GetRealFileSize      (void) const;
    bool      IsDisc               (void) const;
    bool      IsDVD                (void) const;
    bool      IsBD                 (void) const;
    const MythDVDBuffer *DVD       (void) const;
    MythDVDBuffer       *DVD       (void);
    const MythBDBuffer  *BD        (void) const;
    MythBDBuffer        *BD        (void);
    int       Read                 (void *Buffer, int Count);
    int       Peek                 (void *Buffer, int Count);
    int       Peek                 (std::vector<char>& Buffer);
    void      Reset                (bool Full = false, bool ToAdjust = false, bool ResetInternal = false);
    void      Pause                (void);
    void      Unpause              (void);
    void      WaitForPause         (void);
    void      Start                (void);
    void      StopReads            (void);
    void      StartReads           (void);
    long long Seek                 (long long Position, int Whence, bool HasLock = false);
    long long SetAdjustFilesize    (void);

    // LiveTV used utilities
    int       GetReadBufAvail      (void) const;
    bool      SetReadInternalMode  (bool Mode);
    bool      IsReadInternalMode   (void) const;
    bool      LiveMode             (void) const;
    void      SetLiveMode          (LiveTVChain *Chain);
    void      IgnoreLiveEOF        (bool Ignore);

    // ThreadedFileWriter proxies
    int       Write                (const void *Buffer, uint Count);
    bool      IsIOBound            (void) const;
    void      WriterFlush          (void);
    void      Sync                 (void);
    long long WriterSeek           (long long Position, int Whence, bool HasLock = false);
    bool      WriterSetBlocking    (bool Lock = true);

    virtual long long GetReadPosition   (void) const = 0;
    virtual bool      IsOpen            (void) const = 0;
    virtual bool      IsStreamed        (void) { return LiveMode(); }
    virtual bool      IsSeekingAllowed  (void) { return true; }
    virtual bool      IsBookmarkAllowed (void) { return true; }
    virtual int       BestBufferSize    (void) { return DEFAULT_CHUNK_SIZE; }
    virtual bool      StartFromBeginning(void) { return true; }
    virtual void      IgnoreWaitStates  (bool /*Ignore*/) { }
    virtual bool      IsInMenu          (void) const { return false; }
    virtual bool      IsInStillFrame    (void) const { return false; }
    virtual bool      IsInDiscMenuOrStillFrame(void) const { return IsInMenu() || IsInStillFrame(); }
    virtual bool      HandleAction      (const QStringList &/*Action*/, mpeg::chrono::pts /*Pts*/) { return false; }
    virtual bool      OpenFile          (const QString &Filename, std::chrono::milliseconds Retry = kDefaultOpenTimeout) = 0;
    virtual bool      ReOpen            (const QString& /*Filename*/ = "") { return false; }

  protected:
    explicit MythMediaBuffer(MythBufferType Type);

    void     run(void) override;
    void     CreateReadAheadBuffer (void);
    void     CalcReadAheadThresh   (void);
    bool     PauseAndWait          (void);
    int      ReadPriv              (void *Buffer, int Count, bool Peek);
    int      ReadDirect            (void *Buffer, int Count, bool Peek);
    bool     WaitForReadsAllowed   (void);
    int      WaitForAvail          (int Count, std::chrono::milliseconds Timeout);
    int      ReadBufFree           (void) const;
    int      ReadBufAvail          (void) const;
    void     ResetReadAhead        (long long NewInternal);
    void     KillReadAheadThread   (void);
    uint64_t UpdateDecoderRate     (uint64_t Latest = 0);
    uint64_t UpdateStorageRate     (uint64_t Latest = 0);

    virtual int       SafeRead     (void *Buffer, uint Size) = 0;
    virtual long long GetRealFileSizeInternal(void) const { return -1; }
    virtual long long SeekInternal (long long Position, int Whence) = 0;


  protected:
    MythBufferType         m_type;

    mutable QReadWriteLock m_posLock;
    long long              m_readPos         { 0 };
    long long              m_writePos        { 0 };
    long long              m_internalReadPos { 0 };
    long long              m_ignoreReadPos   { -1 };

    mutable QReadWriteLock m_rbrLock;
    int                    m_rbrPos          { 0 };

    mutable QReadWriteLock m_rbwLock;
    int                    m_rbwPos          { 0 };

    // note should not go under rwLock..
    // this is used to break out of read_safe where rwLock is held
    volatile bool          m_stopReads       {false};

    // unprotected (for debugging)
    QString                m_safeFilename;

    mutable QReadWriteLock m_rwLock;
    QString                m_filename;
    QString                m_subtitleFilename;
    QString                m_lastError;
    ThreadedFileWriter    *m_tfw              { nullptr };
    int                    m_fd2              { -1 };
    bool                   m_writeMode        { false   };
    RemoteFile            *m_remotefile       { nullptr };
    uint                   m_bufferSize       { BUFFER_SIZE_MINIMUM };
    bool                   m_lowBuffers       { false };
    bool                   m_fileIsMatroska   { false };
    bool                   m_unknownBitrate   { false };
    bool                   m_startReadAhead   { false };
    char                  *m_readAheadBuffer  { nullptr };
    bool                   m_readAheadRunning { false };
    bool                   m_reallyRunning    { false };
    bool                   m_requestPause     { false };
    bool                   m_paused           { false };
    bool                   m_ateof            { false };
    bool                   m_waitForWrite     { false };
    bool                   m_beingWritten     { false };
    bool                   m_readsAllowed     { false };
    bool                   m_readsDesired     { false };
    volatile bool          m_recentSeek       { true  }; // not protected by rwLock
    bool                   m_setSwitchToNext  { false };
    uint                   m_rawBitrate       { 8000  };
    float                  m_playSpeed        { 1.0F  };
    int                    m_fillThreshold    { 65536 };
    int                    m_fillMin          { -1    };
    int                    m_readBlockSize    { DEFAULT_CHUNK_SIZE};
    int                    m_wantToRead       { 0     };
    int                    m_numFailures      { 0     }; // (see note 1)
    bool                   m_commsError       { false };
    bool                   m_oldfile          { false };
    LiveTVChain           *m_liveTVChain      { nullptr };
    bool                   m_ignoreLiveEOF    { false };
    long long              m_readAdjust       { 0 };
    int                    m_readOffset       { 0 };
    bool                   m_readInternalMode { false };
    // End of section protected by rwLock

    bool                   m_bitrateMonitorEnabled { false };
    QMutex                 m_decoderReadLock;
    QMap<std::chrono::milliseconds, uint64_t> m_decoderReads;
    QMutex                 m_storageReadLock;
    QMap<std::chrono::milliseconds, uint64_t> m_storageReads;

    // note 1: numfailures is modified with only a read lock in the
    // read ahead thread, but this is safe since all other places
    // that use it are protected by a write lock. But this is a
    // fragile state of affairs and care must be taken when modifying
    // code or locking around this variable.

    /// Condition to signal that the read ahead thread is running
    QWaitCondition         m_generalWait; // protected by rwLock

  private:
    bool m_bitrateInitialized { false };
};
#endif
