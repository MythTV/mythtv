// -*- Mode: c++ -*-
#ifndef DVD_RING_BUFFER_H_
#define DVD_RING_BUFFER_H_

#define DVD_BLOCK_SIZE 2048LL
#define DVD_MENU_MAX 7

// Qt headers
#include <QMap>
#include <QString>
#include <QMutex>
#include <QRect>
#include <QCoreApplication>

// MythTV headers
#include "ringbuffer.h"
#include "mythdate.h"
#include "referencecounter.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

#include "dvdnav/dvdnav.h"

/** \class MythDVDContext
 *  \brief Encapsulates playback context at any given moment.
 *
 * This class is mainly represents a single VOBU (video object unit)
 * on a DVD
 */
class MTV_PUBLIC MythDVDContext : public ReferenceCounter
{
    friend class DVDRingBuffer;

  public:
    MythDVDContext() = delete;    // Default constructor should not be called
    ~MythDVDContext() override = default;

    int64_t  GetStartPTS()          const { return (int64_t)m_pci.pci_gi.vobu_s_ptm;    }
    int64_t  GetEndPTS()            const { return (int64_t)m_pci.pci_gi.vobu_e_ptm;    }
    int64_t  GetSeqEndPTS()         const { return (int64_t)m_pci.pci_gi.vobu_se_e_ptm; }
    uint32_t GetLBA()               const { return m_pci.pci_gi.nv_pck_lbn;             }
    uint32_t GetLBAPrevVideoFrame() const;
    int      GetNumFrames()         const;
    int      GetNumFramesPresent()  const;
    int      GetFPS()               const { return (m_pci.pci_gi.e_eltm.frame_u & 0x80) ? 30 : 25; }

  protected:
    MythDVDContext(const dsi_t& dsi, const pci_t& pci);

  protected:
    dsi_t          m_dsi;
    pci_t          m_pci;
};

/** \class DVDRingBufferPriv
 *  \brief RingBuffer class for DVD's
 *
 *   A spiffy little class to allow a RingBuffer to read from DVDs.
 */

class MythDVDPlayer;

class MTV_PUBLIC DVDInfo
{
    friend class DVDRingBuffer;
    Q_DECLARE_TR_FUNCTIONS(DVDInfo);

  public:
    explicit DVDInfo(const QString &filename);
   ~DVDInfo(void);
    bool IsValid(void) const { return m_nav != nullptr; }
    bool GetNameAndSerialNum(QString &name, QString &serialnum);
    QString GetLastError(void) const { return m_lastError; }

  protected:
    static void GetNameAndSerialNum(dvdnav_t* nav,
                                    QString &name,
                                    QString &serialnum,
                                    const QString &filename,
                                    const QString &logPrefix);

  protected:
    dvdnav_t   *m_nav {nullptr};
    QString     m_name;
    QString     m_serialnumber;
    QString     m_lastError;
};

class MTV_PUBLIC DVDRingBuffer : public RingBuffer
{
    Q_DECLARE_TR_FUNCTIONS(DVDRingBuffer);

  public:
    explicit DVDRingBuffer(const QString &lfilename);
    ~DVDRingBuffer() override;

    // gets
    int  GetTitle(void)        const { return m_title;                  }
    bool DVDWaitingForPlayer(void)   { return m_playerWait;             }
    int  GetPart(void)         const { return m_part;                   }
    int  GetCurrentAngle(void) const { return m_currentAngle;           }
    int  GetNumAngles(void)          { return m_currentTitleAngleCount; }
    bool IsOpen(void)          const override { return m_dvdnav;        } // RingBuffer
    long long GetTotalReadPosition(void) { return m_titleLength;        }
    uint GetChapterLength(void)    const { return m_pgLength / 90000;   }
    void GetChapterTimes(QList<long long> &times);
    uint64_t GetChapterTimes(uint title);
    long long GetReadPosition(void) const override; // RingBuffer
    void GetDescForPos(QString &desc);
    void GetPartAndTitle(int &_part, int &_title) const
        { _part  = m_part; _title = m_title; }
    uint GetTotalTimeOfTitle(void);
    float GetAspectOverride(void)     { return m_forcedAspect; }
    bool IsBookmarkAllowed(void) override; // RingBuffer
    bool IsSeekingAllowed(void) override; // RingBuffer
    bool IsStreamed(void) override    { return true; } // RingBuffer
    int  BestBufferSize(void) override { return 2048; } // RingBuffer

    uint GetCellStart(void);
    bool PGCLengthChanged(void);
    bool CellChanged(void);
    bool IsInStillFrame(void) const override { return m_still > 0; } // RingBuffer
    bool IsStillFramePending(void) const { return dvdnav_get_next_still_flag(m_dvdnav) > 0; }
    bool AudioStreamsChanged(void) const { return m_audioStreamsChanged; }
    bool IsWaiting(void) const           { return m_dvdWaiting;          }
    int  NumPartsInTitle(void)     const { return m_titleParts;          }
    void GetMenuSPUPkt(uint8_t *buf, int buf_size, int stream_id, uint32_t startTime);

    uint32_t AdjustTimestamp(uint32_t timestamp);
    int64_t AdjustTimestamp(int64_t timestamp);
    MythDVDContext* GetDVDContext(void);
    int32_t GetLastEvent(void) const     { return m_dvdEvent; }

    // Public menu/button stuff
    AVSubtitle *GetMenuSubtitle(uint &version);
    int         NumMenuButtons(void) const;
    QRect       GetButtonCoords(void);
    void        ReleaseMenuButton(void);
    bool IsInMenu(void) const override { return m_inMenu; } // RingBuffer
    bool HandleAction(const QStringList &actions, int64_t pts) override; // RingBuffer

    // Subtitles
    uint GetSubtitleLanguage(int id);
    int GetSubtitleTrackNum(uint stream_id);
    bool DecodeSubtitles(AVSubtitle * sub, int * gotSubtitles,
                         const uint8_t * spu_pkt, int buf_size, uint32_t startTime);

    uint GetAudioLanguage(int idx);
    int  GetAudioTrackNum(uint stream_id);
    int  GetAudioTrackType(uint idx);

    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool GetDVDStateSnapshot(QString& state);
    bool RestoreDVDStateSnapshot(QString& state);
    double GetFrameRate(void);
    bool StartOfTitle(void) { return (m_part == 0); }
    bool EndOfTitle(void)   { return ((m_titleParts == 0) ||
                                     (m_part == (m_titleParts - 1)) ||
                                     (m_titleParts == 1)); }

    // commands
    bool OpenFile(const QString &lfilename,
                  uint retry_ms = kDefaultOpenTimeout) override; //RingBuffer
    void PlayTitleAndPart(int _title, int _part)
        { dvdnav_part_play(m_dvdnav, _title, _part); }
    bool StartFromBeginning(void) override; //RingBuffer
    void CloseDVD(void);
    bool playTrack(int track);
    bool nextTrack(void);
    void prevTrack(void);
    long long NormalSeek(long long time);
    bool SectorSeek(uint64_t sector);
    void SkipStillFrame(void);
    void WaitSkip(void);
    void SkipDVDWaitingForPlayer(void)    { m_playerWait = false;           }
    void UnblockReading(void)             { m_processState = PROCESS_REPROCESS; }
    bool IsReadingBlocked(void)           { return (m_processState == PROCESS_WAIT); }
    bool GoToMenu(const QString &str);
    void GoToNextProgram(void);
    void GoToPreviousProgram(void);
    bool GoBack(void);

    void IgnoreWaitStates(bool ignore) override { m_skipstillorwait = ignore; } // RingBuffer
    void AudioStreamsChanged(bool change) { m_audioStreamsChanged = change; }
    int64_t GetCurrentTime(void)          { return (m_currentTime / 90000); }
    uint TitleTimeLeft(void);
    void  SetTrack(uint type, int trackNo);
    int   GetTrack(uint type);
    uint8_t GetNumAudioChannels(int idx);
    void SetDVDSpeed(void);
    void SetDVDSpeed(int speed);
    bool SwitchAngle(uint angle);

    void SetParent(MythDVDPlayer *p) { m_parent = p; }

  protected:
    int safe_read(void *data, uint sz) override; //RingBuffer
    long long SeekInternal(long long pos, int whence) override; //RingBuffer

    enum processState_t
    {
        PROCESS_NORMAL,
        PROCESS_REPROCESS,
        PROCESS_WAIT
    };

    dvdnav_t      *m_dvdnav                 {nullptr};
    unsigned char  m_dvdBlockWriteBuf[DVD_BLOCK_SIZE] {0};
    unsigned char *m_dvdBlockReadBuf        {nullptr};
    int            m_dvdBlockRPos           {0};
    int            m_dvdBlockWPos           {0};
    long long      m_pgLength               {0};
    long long      m_pgcLength              {0};
    long long      m_cellStart              {0};
    bool           m_cellChanged            {false};
    bool           m_pgcLengthChanged       {false};
    long long      m_pgStart                {0};
    long long      m_currentpos             {0};
    dvdnav_t      *m_lastNav                {nullptr}; // This really belongs in the player.
    int32_t        m_part                   {0};
    int32_t        m_lastPart               {0};
    int32_t        m_title                  {0};
    int32_t        m_lastTitle              {0};
    bool           m_playerWait             {false};
    int32_t        m_titleParts             {0};
    bool           m_gotStop                {false};
    int            m_currentAngle           {0};
    int            m_currentTitleAngleCount {0};
    int64_t        m_endPts                 {0};
    int64_t        m_timeDiff               {0};

    int            m_still                  {0};
    int            m_lastStill              {0};
    bool           m_audioStreamsChanged    {false};
    bool           m_dvdWaiting             {false};
    long long      m_titleLength            {0};
    bool           m_skipstillorwait        {true};
    long long      m_cellstartPos           {0};
    bool           m_buttonSelected         {false};
    bool           m_buttonExists           {false};
    bool           m_buttonSeenInCell       {false};
    bool           m_lastButtonSeenInCell   {false};
    int            m_cellid                 {0};
    int            m_lastcellid             {0};
    int            m_vobid                  {0};
    int            m_lastvobid              {0};
    bool           m_cellRepeated           {false};

    int            m_curAudioTrack          {0};
    int8_t         m_curSubtitleTrack       {0};
    bool           m_autoselectsubtitle     {true};
    QString        m_dvdname;
    QString        m_serialnumber;
    bool           m_seeking                {false};
    int64_t        m_seektime               {0};
    int64_t        m_currentTime            {0};
    QMap<uint, uint> m_seekSpeedMap;
    QMap<uint, QList<uint64_t> > m_chapterMap;

    MythDVDPlayer  *m_parent                {nullptr};
    float           m_forcedAspect          {-1.0F};

    QMutex          m_contextLock           {QMutex::Recursive};
    MythDVDContext *m_context               {nullptr};
    processState_t  m_processState          {PROCESS_NORMAL};
    dvdnav_status_t m_dvdStat               {DVDNAV_STATUS_OK};
    int32_t         m_dvdEvent              {0};
    int32_t         m_dvdEventSize          {0};

    // Private menu/button stuff
    void ActivateButton(void);
    void MoveButtonLeft(void);
    void MoveButtonRight(void);
    void MoveButtonUp(void);
    void MoveButtonDown(void);
    bool DVDButtonUpdate(bool b_mode);
    void ClearMenuSPUParameters(void);
    void ClearMenuButton(void);

    bool           m_inMenu                 {false};
    uint           m_buttonVersion          {1};
    int            m_buttonStreamID         {0};
    uint32_t       m_clut[16]               {0};
    uint8_t        m_button_color[4]        {0};
    uint8_t        m_button_alpha[4]        {0};
    QRect          m_hl_button              {0,0,0,0};
    uint8_t       *m_menuSpuPkt             {nullptr};
    int            m_menuBuflength          {0};
    AVSubtitle     m_dvdMenuButton          {};
    QMutex m_menuBtnLock;

    QMutex m_seekLock;
    long long Seek(long long time);

    void ClearChapterCache(void);
    static uint ConvertLangCode(uint16_t code);
    void SelectDefaultButton(void);
    void WaitForPlayer(void);

    static int get_nibble(const uint8_t *buf, int nibble_offset);
    static int decode_rle(uint8_t *bitmap, int linesize, int w, int h,
                    const uint8_t *buf, int nibble_offset, int buf_size);
    void guess_palette(uint32_t *rgba_palette,const uint8_t *palette,
                       const uint8_t *alpha);
    static int is_transp(const uint8_t *buf, int pitch, int n,
                         const uint8_t *transp_color);
    static int find_smallest_bounding_rectangle(AVSubtitle *s);
};

#endif // DVD_RING_BUFFER_H_
