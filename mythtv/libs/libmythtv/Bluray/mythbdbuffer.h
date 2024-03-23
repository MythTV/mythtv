#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

// Qt
#include <QString>
#include <QRect>
#include <QHash>
#include <QCoreApplication>

// MythTV
#include "libmythtv/Bluray/mythbdoverlay.h"
#include "libmythtv/io/mythopticalbuffer.h"

static constexpr int32_t BD_BLOCK_SIZE { 6144 };

/** \class MythBDBuffer
 *   A class to allow a MythMediaBuffer to read from BDs.
 */
class MTV_PUBLIC MythBDBuffer : public MythOpticalBuffer
{
    Q_DECLARE_TR_FUNCTIONS(MythBDBuffer)

  public:
    explicit MythBDBuffer(const QString &Filename);
    ~MythBDBuffer() override;

    bool      GetNameAndSerialNum(QString& Name, QString& SerialNum) override;
    void      IgnoreWaitStates   (bool Ignore) override;
    bool      StartFromBeginning (void) override;
    long long GetReadPosition    (void) const override;
    bool      IsOpen             (void) const override;
    bool      IsInStillFrame     (void) const override;
    bool      HandleAction       (const QStringList &Actions, mpeg::chrono::pts Pts) override;
    bool      OpenFile           (const QString &Filename,
                                  std::chrono::milliseconds Retry = kDefaultOpenTimeout) override;

    void      ProgressUpdate     (void);
    bool      BDWaitingForPlayer (void) const;
    void      SkipBDWaitingForPlayer(void);

    bool      GetBDStateSnapshot (QString& State);
    bool      RestoreBDStateSnapshot(const QString &State);
    void      ClearOverlays      (void);
    MythBDOverlay* GetOverlay    (void);
    void      SubmitOverlay      (const bd_overlay_s* Overlay);
    void      SubmitARGBOverlay  (const bd_argb_overlay_s* Overlay);
    uint32_t  GetNumTitles       (void) const;
    int       GetCurrentTitle    (void);
    uint64_t  GetCurrentAngle    (void) const;
    std::chrono::seconds  GetTitleDuration   (int Title);
    uint64_t  GetTitleSize       (void) const;
    std::chrono::seconds  GetTotalTimeOfTitle(void) const;
    std::chrono::seconds  GetCurrentTime     (void) const;
    uint64_t  GetTotalReadPosition(void);
    uint32_t  GetNumChapters     (void);
    uint32_t  GetCurrentChapter  (void);
    uint64_t  GetNumAngles       (void) const;
    std::chrono::milliseconds  GetChapterStartTimeMs(uint32_t Chapter);
    std::chrono::seconds       GetChapterStartTime  (uint32_t Chapter);
    uint64_t  GetChapterStartFrame (uint32_t Chapter);
    bool      IsHDMVNavigation   (void) const;
    bool      TitleChanged       (void);
    bool      IsValidStream      (uint StreamId);
    void      UnblockReading     (void);
    bool      IsReadingBlocked   (void);
    int64_t   AdjustTimestamp    (int64_t Timestamp) const;
    void      GetDescForPos      (QString &Desc);
    double    GetFrameRate       (void);
    int       GetAudioLanguage   (uint StreamID);
    int       GetSubtitleLanguage(uint StreamID);
    void      Close              (void);
    bool      GoToMenu           (const QString &Menu, mpeg::chrono::pts Pts);
    bool      SwitchTitle        (uint32_t Index);
    bool      SwitchPlaylist     (uint32_t Index);
    bool      SwitchAngle        (uint Angle);

  protected:
    int       SafeRead           (void *Buffer, uint Size) override;
    long long SeekInternal       (long long Position, int Whence) override;
    uint64_t  SeekInternal       (uint64_t Position);

  private:
    void      WaitForPlayer      (void);
    bool      UpdateTitleInfo    (void);
    BLURAY_TITLE_INFO* GetTitleInfo   (uint32_t Index);
    BLURAY_TITLE_INFO* GetPlaylistInfo(uint32_t Index);
    void      PressButton        (int32_t Key, mpeg::chrono::pts Pts);
    void      ClickButton        (int64_t Pts, uint16_t X, uint16_t Y);
    bool      HandleBDEvents     (void);
    void      HandleBDEvent      (BD_EVENT &Event);

    static const BLURAY_STREAM_INFO* FindStream(uint StreamID,
                                                BLURAY_STREAM_INFO* Streams,
                                                int StreamCount);

    BLURAY            *m_bdnav                       { nullptr };
    bool               m_isHDMVNavigation            { false   };
    bool               m_tryHDMVNavigation           { false   };
    bool               m_topMenuSupported            { false   };
    bool               m_firstPlaySupported          { false   };
    uint32_t           m_numTitles                   { 0       };
    uint32_t           m_mainTitle                   { 0       };
    mpeg::chrono::pts  m_currentTitleLength          { 0_pts   };
    BLURAY_TITLE_INFO *m_currentTitleInfo            { nullptr };
    uint64_t           m_titlesize                   { 0       };
    uint64_t           m_currentTitleAngleCount      { 0       };
    mpeg::chrono::pts  m_currentTime                 { 0_pts   };
    int                m_imgHandle                   { -1      };
    int                m_currentTitle                { -1      };
    int                m_currentPlaylist             { 0       };
    int                m_currentPlayitem             { 0       };
    int                m_currentChapter              { 0       };
    int                m_currentAudioStream          { 0       };
    int                m_currentIGStream             { 0       };
    int                m_currentPGTextSTStream       { 0       };
    int                m_currentSecondaryAudioStream { 0       };
    int                m_currentSecondaryVideoStream { 0       };
    bool               m_pgTextSTEnabled             { false   };
    bool               m_secondaryAudioEnabled       { false   };
    bool               m_secondaryVideoEnabled       { false   };
    bool               m_secondaryVideoIsFullscreen  { false   };
    bool               m_titleChanged                { false   };
    bool               m_playerWait                  { false   };
    bool               m_ignorePlayerWait            { true    };
    QMutex             m_overlayLock;
    QList<MythBDOverlay*>   m_overlayImages;
    QVector<MythBDOverlay*> m_overlayPlanes;
    std::chrono::seconds    m_stillTime              { 0       };
    int                m_stillMode                   { BLURAY_STILL_NONE};
    BD_EVENT           m_lastEvent                   { BD_EVENT_NONE, 0};
    QByteArray         m_pendingData;
    int64_t            m_timeDiff                    { 0       };
    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedTitleInfo;
    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedPlaylistInfo;
    QRecursiveMutex    m_infoLock;
    QThread           *m_mainThread                  { nullptr };
};
#endif
