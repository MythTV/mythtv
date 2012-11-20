#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

#define BD_BLOCK_SIZE 6144LL

#include <QString>
#include <QRect>
#include <QHash>

#include "libmythbluray/bluray.h"
#include "libmythbluray/keys.h"

#include "ringbuffer.h"
#include "mythdate.h"

/** \class BDRingBufferPriv
 *  \brief RingBuffer class for Blu-rays
 *
 *   A class to allow a RingBuffer to read from BDs.
 */

class BDOverlay
{
  public:
    static void DeleteOverlay(BDOverlay *overlay)
    {
        if (!overlay)
            return;
        if (overlay->m_data)
            av_free(overlay->m_data);
        if (overlay->m_palette)
            av_free(overlay->m_palette);
        delete overlay;
        overlay = NULL;
    }

    BDOverlay(uint8_t *data, uint8_t *palette, QRect position, int plane,
              int64_t pts)
     : m_data(data), m_palette(palette), m_position(position),
       m_plane(plane), m_pts(pts) { }

    uint8_t *m_data;
    uint8_t *m_palette;
    QRect    m_position;
    int      m_plane;
    int64_t  m_pts;
};

class MTV_PUBLIC BDRingBuffer : public RingBuffer
{
  public:
    BDRingBuffer(const QString &lfilename);
    virtual ~BDRingBuffer();

    virtual bool IsStreamed(void) { return true; }

    void ProgressUpdate(void);

    // Player interaction
    bool BDWaitingForPlayer(void)     { return m_playerWait;  }
    void SkipBDWaitingForPlayer(void) { m_playerWait = false; }
    virtual void IgnoreWaitStates(bool ignore) { m_ignorePlayerWait = ignore; }
    virtual bool StartFromBeginning(void);
    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);

    void ClearOverlays(void);
    BDOverlay* GetOverlay(void);
    void SubmitOverlay(const bd_overlay_s * const overlay);

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

    void GetDescForPos(QString &desc);
    double GetFrameRate(void);

    int GetAudioLanguage(uint streamID);
    int GetSubtitleLanguage(uint streamID);

    // commands
    virtual bool HandleAction(const QStringList &actions, int64_t pts);
    virtual bool OpenFile(const QString &filename,
                          uint retry_ms = kDefaultOpenTimeout);
    void close(void);

    bool GoToMenu(const QString str, int64_t pts);
    bool SwitchTitle(uint32_t index);
    bool SwitchPlaylist(uint32_t index);
    bool SwitchAngle(uint angle);

    virtual int safe_read(void *data, uint sz);
    virtual long long Seek(long long pos, int whence, bool has_lock);
    uint64_t Seek(uint64_t pos);

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

    BLURAY            *bdnav;
    meta_dl           *m_metaDiscLibrary;
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

    uint8_t            m_stillTime;
    uint8_t            m_stillMode;
    volatile bool      m_inMenu;

    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedTitleInfo;
    QHash<uint32_t,BLURAY_TITLE_INFO*> m_cachedPlaylistInfo;
    QMutex             m_infoLock;

    QThread           *m_mainThread;
};
#endif
