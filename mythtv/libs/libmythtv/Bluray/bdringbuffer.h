#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

#define BD_BLOCK_SIZE 6144LL

//Qt headers
#include <QString>
#include <QRect>
#include <QHash>
#include <QImage>
#include <QCoreApplication>

// external/libmythbluray
#include "libbluray/bluray.h"
#include "libbluray/decoders/overlay.h"

#include "ringbuffer.h"

class MTV_PUBLIC BDInfo
{
    friend class BDRingBuffer;
    Q_DECLARE_TR_FUNCTIONS(BDInfo);

  public:
    explicit BDInfo(const QString &filename);
   ~BDInfo(void);
    bool IsValid(void) const { return m_isValid; }
    bool GetNameAndSerialNum(QString &name, QString &serialnum);
    QString GetLastError(void) const { return m_lastError; }

protected:
  static void GetNameAndSerialNum(BLURAY* nav,
                                  QString &name,
                                  QString &serialnum,
                                  const QString &filename,
                                  const QString &logPrefix);

  protected:
    QString     m_name;
    QString     m_serialnumber;
    QString     m_lastError;
    bool        m_isValid;
};

class BDOverlay
{
  public:
    BDOverlay();
    explicit BDOverlay(const bd_overlay_s * const overlay);
    explicit BDOverlay(const bd_argb_overlay_s * const overlay);

    void    setPalette(const BD_PG_PALETTE_ENTRY *palette);
    void    wipe();
    void    wipe(int x, int y, int width, int height);

    QImage  image;
    int64_t pts;
    int     x;
    int     y;
};

/** \class BDRingBufferPriv
 *  \brief RingBuffer class for Blu-rays
 *
 *   A class to allow a RingBuffer to read from BDs.
 */
class MTV_PUBLIC BDRingBuffer : public RingBuffer
{
    Q_DECLARE_TR_FUNCTIONS(BDRingBuffer);

  public:
    explicit BDRingBuffer(const QString &lfilename);
    virtual ~BDRingBuffer();

    virtual bool IsStreamed(void) { return true; }

    void ProgressUpdate(void);

    // Player interaction
    bool BDWaitingForPlayer(void)     { return m_playerWait;  }
    void SkipBDWaitingForPlayer(void) { m_playerWait = false; }
    virtual void IgnoreWaitStates(bool ignore) { m_ignorePlayerWait = ignore; }
    virtual bool StartFromBeginning(void);
    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool GetBDStateSnapshot(QString& state);
    bool RestoreBDStateSnapshot(const QString &state);

    void ClearOverlays(void);
    BDOverlay* GetOverlay(void);
    void SubmitOverlay(const bd_overlay_s * const overlay);
    void SubmitARGBOverlay(const bd_argb_overlay_s * const overlay);

    uint32_t GetNumTitles(void) const { return m_numTitles; }
    int      GetCurrentTitle(void);
    uint64_t GetCurrentAngle(void) const { return m_currentAngle; }
    int      GetTitleDuration(int title);
    // Get the size in bytes of the current title (playlist item).
    uint64_t GetTitleSize(void) const { return m_titlesize; }
    // Get The total duration of the current title in 90Khz ticks.
    uint64_t GetTotalTimeOfTitle(void) const { return (m_currentTitleLength / 90000); }
    uint64_t GetCurrentTime(void) { return (m_currentTime / 90000); }
    virtual long long GetReadPosition(void) const; // RingBuffer
    uint64_t GetTotalReadPosition(void);
    uint32_t GetNumChapters(void);
    uint32_t GetCurrentChapter(void);
    uint64_t GetNumAngles(void) { return m_currentTitleAngleCount; }
    uint64_t GetChapterStartTime(uint32_t chapter);
    uint64_t GetChapterStartFrame(uint32_t chapter);
    bool IsOpen(void)        const { return bdnav; }
    bool IsHDMVNavigation(void) const { return m_isHDMVNavigation; }
    virtual bool IsInMenu(void) const { return m_inMenu; }
    virtual bool IsInStillFrame(void) const;
    bool TitleChanged(void);
    bool IsValidStream(int streamid);
    void UnblockReading(void)             { m_processState = PROCESS_REPROCESS; }
    bool IsReadingBlocked(void)           { return (m_processState == PROCESS_WAIT); }
    int64_t AdjustTimestamp(int64_t timestamp);

    void GetDescForPos(QString &desc);
    double GetFrameRate(void);

    int GetAudioLanguage(uint streamID);
    int GetSubtitleLanguage(uint streamID);

    // commands
    virtual bool HandleAction(const QStringList &actions, int64_t pts);
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout);
    void close(void);

    bool GoToMenu(const QString &str, int64_t pts);
    bool SwitchTitle(uint32_t index);
    bool SwitchPlaylist(uint32_t index);
    bool SwitchAngle(uint angle);

  protected:
    virtual int safe_read(void *data, uint sz);
    virtual long long SeekInternal(long long pos, int whence);
    uint64_t SeekInternal(uint64_t pos);

  private:
    // private player interaction
    void WaitForPlayer(void);

    // private title handling
    bool UpdateTitleInfo(void);
    BLURAY_TITLE_INFO* GetTitleInfo(uint32_t index);
    BLURAY_TITLE_INFO* GetPlaylistInfo(uint32_t index);

    // private menu handling methods
    void PressButton(int32_t key, int64_t pts); // Keyboard
    void ClickButton(int64_t pts, uint16_t x, uint16_t y); // Mouse

    // private bluray event handling
    bool HandleBDEvents(void);
    void HandleBDEvent(BD_EVENT &event);

    const BLURAY_STREAM_INFO* FindStream(int streamid, BLURAY_STREAM_INFO* streams, int streamCount) const;


    typedef enum
    {
        PROCESS_NORMAL,
        PROCESS_REPROCESS,
        PROCESS_WAIT
    }processState_t;

    BLURAY            *bdnav;
    bool               m_isHDMVNavigation;
    bool               m_tryHDMVNavigation;
    bool               m_topMenuSupported;
    bool               m_firstPlaySupported;

    uint32_t           m_numTitles;
    uint32_t           m_mainTitle; // Index number of main title
    uint64_t           m_currentTitleLength; // Selected title's duration, in ticks (90Khz)
    BLURAY_TITLE_INFO *m_currentTitleInfo; // Selected title info from struct in bluray.h
    uint64_t           m_titlesize;
    uint64_t           m_currentTitleAngleCount;
    uint64_t           m_currentTime;

    int                m_imgHandle;

    int                m_currentAngle;
    int                m_currentTitle;
    int                m_currentPlaylist;
    int                m_currentPlayitem;
    int                m_currentChapter;

    int                m_currentAudioStream;
    int                m_currentIGStream;
    int                m_currentPGTextSTStream;
    int                m_currentSecondaryAudioStream;
    int                m_currentSecondaryVideoStream;

    bool               m_PGTextSTEnabled;
    bool               m_secondaryAudioEnabled;
    bool               m_secondaryVideoEnabled;
    bool               m_secondaryVideoIsFullscreen;

    bool               m_titleChanged;

    bool               m_playerWait;
    bool               m_ignorePlayerWait;

    QMutex             m_overlayLock;
    QList<BDOverlay*>  m_overlayImages;
    QVector<BDOverlay*> m_overlayPlanes;

    uint8_t            m_stillTime;
    uint8_t            m_stillMode;
    volatile bool      m_inMenu;
    BD_EVENT           m_lastEvent;
    processState_t     m_processState;
    QByteArray         m_pendingData;
    int64_t            m_timeDiff;

    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedTitleInfo;
    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedPlaylistInfo;
    QMutex             m_infoLock;
    QString            m_name;
    QString            m_serialNumber;

    QThread           *m_mainThread;
};
#endif
