#ifndef BD_RING_BUFFER_H_
#define BD_RING_BUFFER_H_

#define BD_BLOCK_SIZE 6144LL

#include <QString>

#include "libmythbluray/bluray.h"
#include "libmythbluray/keys.h"

#include "util.h"

/** \class BDRingBufferPriv
 *  \brief RingBuffer class for Blu-rays
 *
 *   A class to allow a RingBuffer to read from BDs.
 */

class NuppelVideoPlayer;

class MPUBLIC BDRingBufferPriv
{
  public:
    BDRingBufferPriv();
    virtual ~BDRingBufferPriv();

    uint32_t GetNumTitles(void) const { return m_numTitles; }
    int      GetCurrentTitle(void) const;
    uint64_t GetCurrentAngle(void) const { return m_currentAngle; };
    int      GetTitleDuration(int title) const;
    // Get the size in bytes of the current title (playlist item).
    uint64_t GetTitleSize(void) const { return m_titlesize; }
    // Get The total duration of the current title in 90Khz ticks.
    uint64_t GetTotalTimeOfTitle(void) const { return (m_currentTitleLength / 90000); }
    uint64_t GetCurrentTime(void) { return (m_currentTime / 90000); }
    uint64_t GetReadPosition(void);
    uint64_t GetTotalReadPosition(void);
    uint32_t GetNumChapters(void);
    uint64_t GetNumAngles(void) { return m_currentTitleAngleCount; };
    uint64_t GetChapterStartTime(uint32_t chapter);
    uint64_t GetChapterStartFrame(uint32_t chapter);
    bool IsOpen(void)        const { return bdnav; }

    void GetDescForPos(QString &desc) const;
    double GetFrameRate(void);

    int GetAudioLanguage(uint streamID);
    int GetSubtitleLanguage(uint streamID);

    // commands
    bool OpenFile(const QString &filename);
    void close(void);

    bool GoToMenu(const QString str);
    bool SwitchTitle(uint title);
    bool SwitchAngle(uint angle);

    int  safe_read(void *data, unsigned sz);
    uint64_t Seek(uint64_t pos);

    // navigation
    void PressButton(int32_t key, int64_t pts);

  protected:
    BLURAY            *bdnav;
    bd_overlay_proc_f  m_overlay;
    bool               m_is_hdmv_navigation;
    BD_EVENT          *m_currentEvent;
    uint32_t           m_numTitles;
    uint32_t           m_mainTitle; // Index number of main title
    uint64_t           m_currentTitleLength; // Selected title's duration, in ticks (90Khz)
    BLURAY_TITLE_INFO *m_currentTitleInfo; // Selected title info from struct in bluray.h
    uint64_t           m_titlesize;
    uint64_t           m_currentAngle;
    uint64_t           m_currentTitleAngleCount;
    uint64_t           m_currentTime;
};
#endif
