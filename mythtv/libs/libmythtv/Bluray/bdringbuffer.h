#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

#include "config.h"

#define BD_BLOCK_SIZE 6144LL

//Qt headers
#include <QString>
#include <QRect>
#include <QHash>
#include <QImage>
#include <QCoreApplication>

// external/libmythbluray
#include "libbluray/bluray.h"
#if CONFIG_LIBBLURAY_EXTERNAL
#include "libbluray/overlay.h"
#else
#include "libbluray/decoders/overlay.h"
#endif

#include "ringbuffer.h"

class MTV_PUBLIC BDInfo
{
    friend class BDRingBuffer;
    Q_DECLARE_TR_FUNCTIONS(BDInfo);

  public:
    explicit BDInfo(const QString &filename);
   ~BDInfo(void) = default;
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
    bool        m_isValid      {true};
};

class BDOverlay
{
  public:
    BDOverlay() = default;
    explicit BDOverlay(const bd_overlay_s * overlay);
    explicit BDOverlay(const bd_argb_overlay_s * overlay);

    void    setPalette(const BD_PG_PALETTE_ENTRY *palette);
    void    wipe();
    void    wipe(int x, int y, int width, int height);

    QImage  m_image;
    int64_t m_pts   {-1};
    int     m_x     {0};
    int     m_y     {0};
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
    ~BDRingBuffer() override;

    bool IsStreamed(void) override { return true; } // RingBuffer

    void ProgressUpdate(void);

    // Player interaction
    bool BDWaitingForPlayer(void)     { return m_playerWait;  }
    void SkipBDWaitingForPlayer(void) { m_playerWait = false; }
    void IgnoreWaitStates(bool ignore) override { m_ignorePlayerWait = ignore; } // RingBuffer
    bool StartFromBeginning(void) override; // RingBuffer
    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool GetBDStateSnapshot(QString& state);
    bool RestoreBDStateSnapshot(const QString &state);

    void ClearOverlays(void);
    BDOverlay* GetOverlay(void);
    void SubmitOverlay(const bd_overlay_s * overlay);
    void SubmitARGBOverlay(const bd_argb_overlay_s * overlay);

    uint32_t GetNumTitles(void) const { return m_numTitles; }
    int      GetCurrentTitle(void);
    uint64_t GetCurrentAngle(void) const { return m_currentAngle; }
    int      GetTitleDuration(int title);
    // Get the size in bytes of the current title (playlist item).
    uint64_t GetTitleSize(void) const { return m_titlesize; }
    // Get The total duration of the current title in 90Khz ticks.
    uint64_t GetTotalTimeOfTitle(void) const { return (m_currentTitleLength / 90000); }
    uint64_t GetCurrentTime(void) { return (m_currentTime / 90000); }
    long long GetReadPosition(void) const override; // RingBuffer
    uint64_t GetTotalReadPosition(void);
    uint32_t GetNumChapters(void);
    uint32_t GetCurrentChapter(void);
    uint64_t GetNumAngles(void) { return m_currentTitleAngleCount; }
    uint64_t GetChapterStartTime(uint32_t chapter);
    uint64_t GetChapterStartFrame(uint32_t chapter);
    bool IsOpen(void) const override { return m_bdnav; } // RingBuffer
    bool IsHDMVNavigation(void) const { return m_isHDMVNavigation; }
    bool IsInMenu(void) const override { return m_inMenu; } // RingBuffer
    bool IsInStillFrame(void) const override; // RingBuffer
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
    bool HandleAction(const QStringList &actions, int64_t pts) override; // RingBuffer
    bool OpenFile(const QString &lfilename,
                  uint retry_ms = kDefaultOpenTimeout) override; // RingBuffer
    void close(void);

    bool GoToMenu(const QString &str, int64_t pts);
    bool SwitchTitle(uint32_t index);
    bool SwitchPlaylist(uint32_t index);
    bool SwitchAngle(uint angle);

  protected:
    int safe_read(void *data, uint sz) override; // RingBuffer
    long long SeekInternal(long long pos, int whence) override; // RingBuffer
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

    BLURAY            *m_bdnav                       {nullptr};
    bool               m_isHDMVNavigation            {false};
    bool               m_tryHDMVNavigation           {false};
    bool               m_topMenuSupported            {false};
    bool               m_firstPlaySupported          {false};

    uint32_t           m_numTitles                   {0};
                       // Index number of main title
    uint32_t           m_mainTitle                   {0};
                       // Selected title's duration, in ticks (90Khz)
    uint64_t           m_currentTitleLength          {0};
                       // Selected title info from struct in bluray.h
    BLURAY_TITLE_INFO *m_currentTitleInfo            {nullptr};
    uint64_t           m_titlesize                   {0};
    uint64_t           m_currentTitleAngleCount      {0};
    uint64_t           m_currentTime                 {0};

    int                m_imgHandle                   {-1};

    int                m_currentAngle                {0};
    int                m_currentTitle                {-1};
    int                m_currentPlaylist             {0};
    int                m_currentPlayitem             {0};
    int                m_currentChapter              {0};

    int                m_currentAudioStream          {0};
    int                m_currentIGStream             {0};
    int                m_currentPGTextSTStream       {0};
    int                m_currentSecondaryAudioStream {0};
    int                m_currentSecondaryVideoStream {0};

    bool               m_PGTextSTEnabled             {false};
    bool               m_secondaryAudioEnabled       {false};
    bool               m_secondaryVideoEnabled       {false};
    bool               m_secondaryVideoIsFullscreen  {false};

    bool               m_titleChanged                {false};

    bool               m_playerWait                  {false};
    bool               m_ignorePlayerWait            {true};

    QMutex             m_overlayLock;
    QList<BDOverlay*>  m_overlayImages;
    QVector<BDOverlay*> m_overlayPlanes;

    uint8_t            m_stillTime                   {0};
    uint8_t            m_stillMode                   {BLURAY_STILL_NONE};
    volatile bool      m_inMenu                      {false};
    BD_EVENT           m_lastEvent                   {BD_EVENT_NONE, 0};
    processState_t     m_processState                {PROCESS_NORMAL};
    QByteArray         m_pendingData;
    int64_t            m_timeDiff                    {0};

    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedTitleInfo;
    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedPlaylistInfo;
    QMutex             m_infoLock                    {QMutex::Recursive};
    QString            m_name;
    QString            m_serialNumber;

    QThread           *m_mainThread                  {nullptr};
};
#endif
