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
    virtual ~MythDVDContext();

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

  private:
    // Default constructor should not be called
    MythDVDContext();

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
    bool IsValid(void) const { return m_nav != NULL; }
    bool GetNameAndSerialNum(QString &name, QString &serialnum);
    QString GetLastError(void) const { return m_lastError; }

  protected:
    static void GetNameAndSerialNum(dvdnav_t* nav,
                                    QString &name,
                                    QString &serialnum,
                                    const QString &filename,
                                    const QString &logPrefix);

  protected:
    dvdnav_t   *m_nav;
    QString     m_name;
    QString     m_serialnumber;
    QString     m_lastError;
};

class MTV_PUBLIC DVDRingBuffer : public RingBuffer
{
    Q_DECLARE_TR_FUNCTIONS(DVDRingBuffer);

  public:
    explicit DVDRingBuffer(const QString &lfilename);
    virtual ~DVDRingBuffer();

    // gets
    int  GetTitle(void)        const { return m_title;                  }
    bool DVDWaitingForPlayer(void)   { return m_playerWait;             }
    int  GetPart(void)         const { return m_part;                   }
    int  GetCurrentAngle(void) const { return m_currentAngle;           }
    int  GetNumAngles(void)          { return m_currentTitleAngleCount; }
    bool IsOpen(void)          const { return m_dvdnav;                 }
    long long GetTotalReadPosition(void) { return m_titleLength;        }
    uint GetChapterLength(void)    const { return m_pgLength / 90000;   }
    void GetChapterTimes(QList<long long> &times);
    uint64_t GetChapterTimes(uint title);
    virtual long long GetReadPosition(void) const;
    void GetDescForPos(QString &desc);
    void GetPartAndTitle(int &_part, int &_title) const
        { _part  = m_part; _title = m_title; }
    uint GetTotalTimeOfTitle(void);
    float GetAspectOverride(void)     { return m_forcedAspect; }
    virtual bool IsBookmarkAllowed(void);
    virtual bool IsSeekingAllowed(void);
    virtual bool IsStreamed(void)     { return true; }
    virtual int  BestBufferSize(void) { return 2048; }

    uint GetCellStart(void);
    bool PGCLengthChanged(void);
    bool CellChanged(void);
    virtual bool IsInStillFrame(void)   const { return m_still > 0;             }
    bool IsStillFramePending(void) const { return dvdnav_get_next_still_flag(m_dvdnav) > 0; }
    bool AudioStreamsChanged(void) const { return m_audioStreamsChanged; }
    bool IsWaiting(void) const           { return m_dvdWaiting;          }
    int  NumPartsInTitle(void)     const { return m_titleParts;          }
    void GetMenuSPUPkt(uint8_t *buf, int len, int stream_id, uint32_t startTime);

    uint32_t AdjustTimestamp(uint32_t timestamp);
    int64_t AdjustTimestamp(int64_t timestamp);
    MythDVDContext* GetDVDContext(void);
    int32_t GetLastEvent(void) const     { return m_dvdEvent; }

    // Public menu/button stuff
    AVSubtitle *GetMenuSubtitle(uint &version);
    int         NumMenuButtons(void) const;
    QRect       GetButtonCoords(void);
    void        ReleaseMenuButton(void);
    virtual bool IsInMenu(void) const { return m_inMenu; }
    virtual bool HandleAction(const QStringList &actions, int64_t pts);

    // Subtitles
    uint GetSubtitleLanguage(int key);
    int GetSubtitleTrackNum(uint stream_id);
    bool DecodeSubtitles(AVSubtitle * sub, int * gotSubtitles,
                         const uint8_t * buf, int buf_size, uint32_t startTime);

    uint GetAudioLanguage(int idx);
    int  GetAudioTrackNum(uint stream_id);
    int  GetAudioTrackType(uint idx);

    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool GetDVDStateSnapshot(QString& state);
    bool RestoreDVDStateSnapshot(QString& state);
    double GetFrameRate(void);
    bool StartOfTitle(void) { return (m_part == 0); }
    bool EndOfTitle(void)   { return ((!m_titleParts) ||
                                     (m_part == (m_titleParts - 1)) ||
                                     (m_titleParts == 1)); }

    // commands
    virtual bool OpenFile(const QString &lfilename,
                          uint retry_ms = kDefaultOpenTimeout);
    void PlayTitleAndPart(int _title, int _part)
        { dvdnav_part_play(m_dvdnav, _title, _part); }
    virtual bool StartFromBeginning(void);
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

    virtual void IgnoreWaitStates(bool ignore) { m_skipstillorwait = ignore; }
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
    virtual int safe_read(void *data, uint sz);
    virtual long long SeekInternal(long long pos, int whence);

    typedef enum
    {
        PROCESS_NORMAL,
        PROCESS_REPROCESS,
        PROCESS_WAIT
    }processState_t;

    dvdnav_t      *m_dvdnav;
    unsigned char  m_dvdBlockWriteBuf[DVD_BLOCK_SIZE];
    unsigned char *m_dvdBlockReadBuf;
    int            m_dvdBlockRPos;
    int            m_dvdBlockWPos;
    long long      m_pgLength;
    long long      m_pgcLength;
    long long      m_cellStart;
    bool           m_cellChanged;
    bool           m_pgcLengthChanged;
    long long      m_pgStart;
    long long      m_currentpos;
    dvdnav_t      *m_lastNav; // This really belongs in the player.
    int32_t        m_part;
    int32_t        m_lastPart;
    int32_t        m_title;
    int32_t        m_lastTitle;
    bool           m_playerWait;
    int32_t        m_titleParts;
    bool           m_gotStop;
    int            m_currentAngle;
    int            m_currentTitleAngleCount;
    int64_t        m_endPts;
    int64_t        m_timeDiff;

    int            m_still;
    int            m_lastStill;
    bool           m_audioStreamsChanged;
    bool           m_dvdWaiting;
    long long      m_titleLength;
    bool           m_skipstillorwait;
    long long      m_cellstartPos;
    bool           m_buttonSelected;
    bool           m_buttonExists;
    bool           m_buttonSeenInCell;
    bool           m_lastButtonSeenInCell;
    int            m_cellid;
    int            m_lastcellid;
    int            m_vobid;
    int            m_lastvobid;
    bool           m_cellRepeated;

    int            m_curAudioTrack;
    int8_t         m_curSubtitleTrack;
    bool           m_autoselectsubtitle;
    QString        m_dvdname;
    QString        m_serialnumber;
    bool           m_seeking;
    int64_t        m_seektime;
    int64_t        m_currentTime;
    QMap<uint, uint> m_seekSpeedMap;
    QMap<uint, QList<uint64_t> > m_chapterMap;

    MythDVDPlayer *m_parent;
    float          m_forcedAspect;

    QMutex          m_contextLock;
    MythDVDContext *m_context;
    processState_t  m_processState;
    dvdnav_status_t m_dvdStat;
    int32_t        m_dvdEvent;
    int32_t        m_dvdEventSize;

    // Private menu/button stuff
    void ActivateButton(void);
    void MoveButtonLeft(void);
    void MoveButtonRight(void);
    void MoveButtonUp(void);
    void MoveButtonDown(void);
    bool DVDButtonUpdate(bool b_mode);
    void ClearMenuSPUParameters(void);
    void ClearMenuButton(void);

    bool           m_inMenu;
    uint           m_buttonVersion;
    int            m_buttonStreamID;
    uint32_t       m_clut[16];
    uint8_t        m_button_color[4];
    uint8_t        m_button_alpha[4];
    QRect          m_hl_button;
    uint8_t       *m_menuSpuPkt;
    int            m_menuBuflength;
    AVSubtitle     m_dvdMenuButton;
    QMutex m_menuBtnLock;

    QMutex m_seekLock;
    long long Seek(long long time);

    void ClearChapterCache(void);
    uint ConvertLangCode(uint16_t code);
    void SelectDefaultButton(void);
    void WaitForPlayer(void);

    int get_nibble(const uint8_t *buf, int nibble_offset);
    int decode_rle(uint8_t *bitmap, int linesize, int w, int h,
                    const uint8_t *buf, int nibble_offset, int buf_size);
    void guess_palette(uint32_t *rgba_palette,uint8_t *palette,
                       uint8_t *alpha);
    int is_transp(const uint8_t *buf, int pitch, int n,
                  const uint8_t *transp_color);
    int find_smallest_bounding_rectangle(AVSubtitle *s);
};

#endif // DVD_RING_BUFFER_H_
