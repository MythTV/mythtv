#ifndef MYTH_DVD_BUFFER_H_
#define MYTH_DVD_BUFFER_H_

// C++
#include <array>

// Qt
#include <QMap>
#include <QString>
#include <QMutex>
#include <QRect>
#include <QRecursiveMutex>
#include <QCoreApplication>

// MythTV
#include "libmythbase/mythdate.h"
#include "libmythbase/referencecounter.h"
#include "libmythtv/io/mythopticalbuffer.h"

#include "mythdvdcontext.h"
#include "mythdvdinfo.h"

// FFmpeg
extern "C" {
#include "libavcodec/avcodec.h"
}

static constexpr int32_t DVD_MENU_MAX { 7 };

class MythDVDPlayer;

using CLUTArray    = std::array<uint32_t,16>;
using AlphaArray   = std::array<uint8_t,4>;
using PaletteArray = std::array<uint8_t,4>;
using ColorArray   = std::array<uint8_t,256>;

class MTV_PUBLIC MythDVDBuffer : public MythOpticalBuffer
{
    Q_DECLARE_TR_FUNCTIONS(MythDVDBuffer)

  public:
    explicit MythDVDBuffer(const QString &Filename);
    ~MythDVDBuffer() override;

    bool      IsOpen               (void) const override;
    bool      IsBookmarkAllowed    (void) override;
    bool      IsSeekingAllowed     (void) override;
    int       BestBufferSize       (void) override { return 2048; }
    bool      IsInStillFrame       (void) const override;
    bool      OpenFile             (const QString &Filename,
                                    std::chrono::milliseconds Retry = kDefaultOpenTimeout) override;
    bool      HandleAction         (const QStringList &Actions, mpeg::chrono::pts Pts) override;
    void      IgnoreWaitStates     (bool Ignore) override;
    bool      StartFromBeginning   (void) override;
    long long GetReadPosition      (void) const override;
    bool      GetNameAndSerialNum  (QString& Name, QString& SerialNumber) override;

    int       GetTitle             (void) const;
    bool      DVDWaitingForPlayer  (void) const;
    int       GetPart              (void) const;
    int       GetCurrentAngle      (void) const;
    int       GetNumAngles         (void) const;
    long long GetTotalReadPosition (void) const;
    std::chrono::seconds  GetChapterLength     (void) const;
    void                  GetChapterTimes      (QList<std::chrono::seconds> &Times);
    std::chrono::seconds  GetChapterTimes      (int Title);
    void      GetDescForPos        (QString &Description) const;
    void      GetPartAndTitle      (int &Part, int &Title) const;
    std::chrono::seconds  GetTotalTimeOfTitle  (void) const;
    float     GetAspectOverride    (void) const;
    std::chrono::seconds  GetCellStart         (void) const;
    bool      PGCLengthChanged     (void);
    bool      CellChanged          (void);
    bool      IsStillFramePending  (void) const;
    bool      AudioStreamsChanged  (void) const;
    bool      IsWaiting            (void) const;
    int       NumPartsInTitle      (void) const;
    void      GetMenuSPUPkt        (uint8_t *Buffer, int Size, int StreamID, uint32_t StartTime);
    uint32_t  AdjustTimestamp      (uint32_t Timestamp) const;
    int64_t   AdjustTimestamp      (int64_t  Timestamp) const;
    MythDVDContext* GetDVDContext  (void);
    int32_t   GetLastEvent         (void) const;
    AVSubtitle* GetMenuSubtitle    (uint &Version);
    int       NumMenuButtons       (void) const;
    QRect     GetButtonCoords      (void);
    void      ReleaseMenuButton    (void);
    uint      GetSubtitleLanguage  (int Id);
    int8_t    GetSubtitleTrackNum  (uint StreamId);
    bool      DecodeSubtitles      (AVSubtitle* Subtitle, int* GotSubtitles,
                                    const uint8_t* SpuPkt, int BufSize, uint32_t StartTime);
    uint      GetAudioLanguage     (int Index);
    int       GetAudioTrackNum     (uint StreamId);
    int       GetAudioTrackType    (uint Index);
    bool      GetDVDStateSnapshot  (QString& State);
    bool      RestoreDVDStateSnapshot(const QString& State);
    double    GetFrameRate         (void);
    bool      StartOfTitle         (void) const;
    bool      EndOfTitle           (void) const;
    void      PlayTitleAndPart     (int Title, int Part);
    void      CloseDVD             (void);
    bool      PlayTrack            (int Track);
    bool      NextTrack            (void);
    void      PrevTrack            (void);
    long long NormalSeek           (long long Time);
    bool      SectorSeek           (uint64_t Sector);
    void      SkipStillFrame       (void);
    void      WaitSkip             (void);
    void      SkipDVDWaitingForPlayer(void);
    void      UnblockReading       (void);
    bool      IsReadingBlocked     (void);
    bool      GoToMenu             (const QString &str);
    void      GoToNextProgram      (void);
    void      GoToPreviousProgram  (void);
    bool      GoBack               (void);
    void      AudioStreamsChanged  (bool Change);
    std::chrono::seconds  GetCurrentTime       (void) const;
    std::chrono::seconds  TitleTimeLeft        (void) const;
    void      SetTrack             (uint Type, int TrackNo);
    int       GetTrack             (uint Type) const;
    uint16_t  GetNumAudioChannels  (int Index);
    void      SetDVDSpeed          (void);
    void      SetDVDSpeed          (int Speed);
    bool      SwitchAngle          (int Angle);
    void      SetParent            (MythDVDPlayer *Parent);

  protected:
    int       SafeRead          (void *Buffer, uint Size) override;
    long long SeekInternal      (long long Position, int Whence) override;

    void      ActivateButton    (void);
    void      MoveButtonLeft    (void);
    void      MoveButtonRight   (void);
    void      MoveButtonUp      (void);
    void      MoveButtonDown    (void);
    bool      DVDButtonUpdate   (bool ButtonMode);
    void      ClearMenuSPUParameters (void);
    void      ClearMenuButton   (void);
    long long Seek              (long long Time);
    void      ClearChapterCache (void);
    void      SelectDefaultButton(void);
    void      WaitForPlayer     (void);

    static uint ConvertLangCode (uint16_t Code);
    static uint GetNibble       (const uint8_t *Buffer, int NibbleOffset);
    static int DecodeRLE        (uint8_t *Bitmap, int Linesize, int Width, int Height,
                                 const uint8_t *Buffer, int NibbleOffset, int BufferSize);
    void       GuessPalette     (uint32_t *RGBAPalette, PaletteArray Palette,
                                 AlphaArray Alpha);
    static int IsTransparent    (const uint8_t *Buffer, int Pitch, int Num,
                                 const ColorArray& Colors);
    static int FindSmallestBoundingRectangle(AVSubtitle *Subtitle);

    dvdnav_t      *m_dvdnav                 { nullptr };
    DvdBuffer      m_dvdBlockWriteBuf       { 0 };
    unsigned char *m_dvdBlockReadBuf        { nullptr };
    int            m_dvdBlockRPos           { 0       };
    int            m_dvdBlockWPos           { 0       };
    mpeg::chrono::pts  m_pgLength           { 0_pts   };
    mpeg::chrono::pts  m_pgcLength          { 0_pts   };
    mpeg::chrono::pts  m_cellStart          { 0_pts   };
    bool           m_cellChanged            { false   };
    bool           m_pgcLengthChanged       { false   };
    long long      m_pgStart                { 0       };
    long long      m_currentpos             { 0       };
    int32_t        m_part                   { 0       };
    int32_t        m_lastPart               { 0       };
    int32_t        m_title                  { 0       };
    int32_t        m_lastTitle              { 0       };
    bool           m_playerWait             { false   };
    int32_t        m_titleParts             { 0       };
    bool           m_gotStop                { false   };
    int            m_currentTitleAngleCount { 0       };
    int64_t        m_endPts                 { 0       };
    int64_t        m_timeDiff               { 0       };
    std::chrono::seconds  m_still           { 0s      };
    std::chrono::seconds  m_lastStill       { 0s      };
    bool           m_audioStreamsChanged    { false   };
    bool           m_dvdWaiting             { false   };
    long long      m_titleLength            { 0       };
    bool           m_skipstillorwait        { true    };
    long long      m_cellstartPos           { 0       };
    bool           m_buttonSelected         { false   };
    bool           m_buttonExists           { false   };
    bool           m_buttonSeenInCell       { false   };
    bool           m_lastButtonSeenInCell   { false   };
    int            m_cellid                 { 0       };
    int            m_lastcellid             { 0       };
    int            m_vobid                  { 0       };
    int            m_lastvobid              { 0       };
    bool           m_cellRepeated           { false   };
    int            m_curAudioTrack          { 0       };
    int8_t         m_curSubtitleTrack       { 0       };
    bool           m_autoselectsubtitle     { true    };
    bool           m_seeking                { false   };
    mpeg::chrono::pts  m_seektime           { 0_pts   };
    mpeg::chrono::pts  m_currentTime        { 0_pts   };
    static const QMap<int, int> kSeekSpeedMap;
    QMap<int, QList<std::chrono::seconds> > m_chapterMap;
    MythDVDPlayer  *m_parent                { nullptr };
    float           m_forcedAspect          { -1.0F   };
    QRecursiveMutex m_contextLock;
    MythDVDContext *m_context               { nullptr };
    dvdnav_status_t m_dvdStat               { DVDNAV_STATUS_OK };
    int32_t         m_dvdEvent              { 0       };
    int32_t         m_dvdEventSize          { 0       };
    uint            m_buttonVersion         { 1       };
    int             m_buttonStreamID        { 0       };
    CLUTArray       m_clut                  { 0       };
    AlphaArray      m_buttonColor           { 0       };
    PaletteArray    m_buttonAlpha           { 0       };
    QRect           m_hlButton              { 0, 0, 0, 0 };
    uint8_t        *m_menuSpuPkt            { nullptr };
    int             m_menuBuflength         { 0       };
    AVSubtitle      m_dvdMenuButton         {         };
    QMutex          m_menuBtnLock;
    QMutex          m_seekLock;
};

#endif
