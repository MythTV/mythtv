/*
 * Copyright (c) 2004 Doug Larrick <doug@ties.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef VSYNC_H_INCLUDED
#define VSYNC_H_INCLUDED

#include <sys/time.h>
#include <time.h>

class VideoOutput;

extern bool tryingVideoSync;

/** \class VideoSync
 *  \brief Virtual base class for all video synchronization classes.
 *
 *   This set of classes implements a variety of algorithms for synching
 *   video frames to a display. The better algorithms synchronize to
 *   the display's refresh rate, and also attempt to maintain phase with
 *   respect to the vertical retrace. The poorer algorithms simply try
 *   to display frames at a regular interval.
 *
 *   The factory method BestMethod tries subclasses in roughly quality
 *   order until one succeeds.
 *
 *   A/V sync methods may supply the nominal delay per frame. Other
 *   than that, video timing is entirely up to these classes. When
 *   WaitForFrame returns, it is time to show the frame.
 */
class VideoSync
// virtual base class
{
  public:
    VideoSync(VideoOutput*, int fi, int ri, bool intr);
    virtual ~VideoSync() {}

    /// \brief Returns name of instanciated VSync method.
    virtual QString getName(void) const = 0;
    /// \brief Tries to initialize VSync method.
    virtual bool TryInit(void) = 0;

    /** \brief Start VSync; must be called from main thread.
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should 
     *   always be called from same thread, to prevent bad 
     *   interactions with threads.
     */
    virtual void Start(void);

    /** \brief Waits for next a frame or field.
     *   Returns delay to real frame timing in usec
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should
     *   always be called from same thread, to prevent bad
     *   interactions with threads.
     *
     *  \param sync_delay time until the desired frame or field
     *  \sa CalcDelay(void), KeepPhase(void)
     */
    virtual int WaitForFrame(int sync_delay) = 0;

    /// \brief Returns the (minimum) refresh interval of the output device.
    int getRefreshInterval(void) const { return m_refresh_interval; }
    /// \brief Sets the refresh interval of the output device.
    void setRefreshInterval(int ri) { m_refresh_interval = ri; }

    virtual void setFrameInterval(int fi) { m_frame_interval = fi; };

    /** \brief Stops VSync; must be called from main thread.
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should
     *   always be called from same thread, to prevent bad
     *   interactions with threads.
     */
    virtual void Stop(void) {}

    // documented in vsync.cpp
    static VideoSync *BestMethod(VideoOutput*,
                                 uint frame_interval, uint refresh_interval,
                                 bool interlaced);
  protected:
    int64_t GetTime(void);
    int CalcDelay(void);
    void KeepPhase(void) MDEPRECATED;

    VideoOutput *m_video_output;
    int m_frame_interval; // of video
    int m_refresh_interval; // of display
    bool m_interlaced;
    int64_t m_nexttrigger;
    int m_delay;
    
    static int m_forceskip;
};

#ifndef _WIN32
/** \brief Video synchronization class employing /dev/drm0
 *
 *   Polls /dev/drm0 to wait for retrace.  Phase-maintaining, meaning
 *   WaitForFrame should always return approximately the same time after
 *   a vertical retrace.
 */
class DRMVideoSync : public VideoSync
{
  public:
    DRMVideoSync(VideoOutput*,
                 int frame_interval, int refresh_interval, bool interlaced);
    ~DRMVideoSync();

    QString getName(void) const { return QString("DRM"); }
    bool TryInit(void);
    void Start(void);
    int WaitForFrame(int sync_delay);

  private:
    int m_dri_fd;
    static const char *sm_dri_dev;
    
};
#endif // !_WIN32

#ifdef __linux__
/** \brief Video synchronization class employing /dev/rtc
 *  
 *  Non-phase-maintaining. There may occasionally be short periods 
 *  of jitter that eventually go away.
 *
 *  You may need to issue the following command before this will work:
 * \code
 *  echo 1024 > /proc/sys/dev/rtc/max-user-freq
 * \endcode
 */
class RTCVideoSync : public VideoSync
{
  public:
    RTCVideoSync(VideoOutput*,
                 int frame_interval, int refresh_interval, bool interlaced);
    ~RTCVideoSync();

    QString getName(void) const { return QString("RTC"); }
    bool TryInit(void);
    int WaitForFrame(int sync_delay);

  private:
    int m_rtcfd;
};
#endif

/** \brief Video synchronization classes employing usleep() and busy-waits.
 *  
 *  Non-phase-maintaining. There may occasionally be short periods 
 *  of jitter that eventually go away.
 *
 *  This method does not maintain phase and uses the most CPU of any of
 *  the VSync methods. But it is the catch-all fallback algorithm,
 *  TryInit(void) always succeeds.
 *
 */
class BusyWaitVideoSync : public VideoSync
{
  public:
    BusyWaitVideoSync(VideoOutput*,
                      int frame_interval, int refresh_interval, 
                      bool interlaced);
    ~BusyWaitVideoSync();

    QString getName(void) const { return QString("USleep with busy wait"); }
    bool TryInit(void);
    int WaitForFrame(int sync_delay);

  private:
    int m_cheat;
    int m_fudge;
};

/** \brief Video synchronization classes employing only usleep().
 *
 *   Calls usleep() for the entire remaining frame interval.  Horribly
 *   inaccurate on < Linux 2.6 kernels; not very accurate there either.
 *   Not phase-maintaining. Not tried automatically.
 *
 *   This is only used when MythPlayer's 'disablevideo' is true (i.e. for
 *   commercial flagging and for transcoding), since it doesn't
 *   waste CPU cycles busy-waiting like BusyWaitVideoSync.
 */
class USleepVideoSync : public VideoSync
{
  public:
    USleepVideoSync(VideoOutput*,
                    int frame_interval, int refresh_interval, 
                    bool interlaced);
    ~USleepVideoSync();

    QString getName(void) const { return QString("USleep"); }
    bool TryInit(void);
    int WaitForFrame(int sync_delay);
};

class DummyVideoSync : public VideoSync
{
  public:
    DummyVideoSync(VideoOutput* vo, int fr, int ri, bool intl)
     : VideoSync(vo, fr, ri, intl) { }
    ~DummyVideoSync() { }

    QString getName(void) const { return QString("Dummy"); }
    bool TryInit(void) { return true; }
    int WaitForFrame(int sync_delay) { return 0; }
};
#endif /* VSYNC_H_INCLUDED */
