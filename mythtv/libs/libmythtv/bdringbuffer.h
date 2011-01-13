#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

#define BD_BLOCK_SIZE 6144LL

#include <QString>

#include "libmythbluray/bluray.h"
#include "libmythbluray/keys.h"

#include "ringbuffer.h"
#include "util.h"

/** \class BDRingBufferPriv
 *  \brief RingBuffer class for Blu-rays
 *
 *   A class to allow a RingBuffer to read from BDs.
 */

class MPUBLIC BDRingBuffer : public RingBuffer
{
  public:
    BDRingBuffer(const QString &lfilename);
    virtual ~BDRingBuffer();

    // Player interaction
    bool BDWaitingForPlayer(void)     { return m_playerWait;  }
    void SkipBDWaitingForPlayer(void) { m_playerWait = false; }
    void IgnoreWaitStates(bool ignore) { m_ignorePlayerWait = ignore; }
    bool StartFromBeginning(void);

    uint32_t GetNumTitles(void) const { return m_numTitles; }
    int      GetCurrentTitle(void) const;
    uint64_t GetCurrentAngle(void) const { return m_currentAngle; }
    int      GetTitleDuration(int title) const;
    // Get the size in bytes of the current title (playlist item).
    uint64_t GetTitleSize(void) const { return m_titlesize; }
    // Get The total duration of the current title in 90Khz ticks.
    uint64_t GetTotalTimeOfTitle(void) const { return (m_currentTitleLength / 90000); }
    uint64_t GetCurrentTime(void) { return (m_currentTime / 90000); }
    virtual long long GetReadPosition(void) const; // RingBuffer
    uint64_t GetTotalReadPosition(void);
    uint32_t GetNumChapters(void);
    uint64_t GetNumAngles(void) { return m_currentTitleAngleCount; }
    uint64_t GetChapterStartTime(uint32_t chapter);
    uint64_t GetChapterStartFrame(uint32_t chapter);
    bool IsOpen(void)        const { return bdnav; }
    bool IsHDMVNavigation(void) const { return m_is_hdmv_navigation; }
    bool IsInMenu(void) const { return m_inMenu; }
    bool IsInStillFrame(void) const { return m_still > 0; }
    virtual bool IsInDiscMenuOrStillFrame(void) const
        { return IsInMenu() || IsInStillFrame(); } // RingBuffer
    bool TitleChanged(void);

    void GetDescForPos(QString &desc) const;
    double GetFrameRate(void);

    int GetAudioLanguage(uint streamID);
    int GetSubtitleLanguage(uint streamID);

    // commands
    virtual bool OpenFile(const QString &filename,
                          uint retry_ms = kDefaultOpenTimeout);
    void close(void);

    bool GoToMenu(const QString str, int64_t pts);
    bool SwitchTitle(uint32_t index);
    bool SwitchPlaylist(uint32_t index);
    bool SwitchAngle(uint angle);

    bool UpdateTitleInfo(uint32_t index);

    virtual int safe_read(void *data, uint sz);
    virtual long long Seek(long long pos, int whence, bool has_lock);
    uint64_t Seek(uint64_t pos);

    bool HandleBDEvents(void);
    void HandleBDEvent(BD_EVENT &event);

    // navigation
    void PressButton(int32_t key, int64_t pts); // Keyboard
    void ClickButton(int64_t pts, uint16_t x, uint16_t y); // Mouse

  protected:
    void WaitForPlayer(void);

    BLURAY            *bdnav;
    bool               m_is_hdmv_navigation;
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

  public:
    uint8_t            m_still;
    volatile bool      m_inMenu;

};
#endif
