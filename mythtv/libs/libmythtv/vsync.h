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
#include <ctime>

class MythVideoOutput;

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
    VideoSync(MythVideoOutput*, int refreshint);
    virtual ~VideoSync() = default;

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
     *  \param nominal_frame_interval the frame interval normally expected
     *  \param extra_delay extra time beyond nominal until the desired frame or field
     *  \sa CalcDelay(int), KeepPhase(void)
     */
    virtual int WaitForFrame(int nominal_frame_interval, int extra_delay) = 0;

    /// \brief Returns the (minimum) refresh interval of the output device.
    int getRefreshInterval(void) const { return m_refresh_interval; }
    /// \brief Sets the refresh interval of the output device.
    void setRefreshInterval(int ri) { m_refresh_interval = ri; }

    /** \brief Stops VSync; must be called from main thread.
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should
     *   always be called from same thread, to prevent bad
     *   interactions with threads.
     */
    virtual void Stop(void) {}

    // documented in vsync.cpp
    static VideoSync *BestMethod(MythVideoOutput *, uint refresh_interval);

  protected:
    static int64_t GetTime(void);
    int CalcDelay(int nominal_frame_interval);

    MythVideoOutput *m_video_output     {nullptr};
    int          m_refresh_interval; // of display
    int64_t      m_nexttrigger      {0};
    int          m_delay            {-1};
    
    static int   s_forceskip;
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
    DRMVideoSync(MythVideoOutput *, int refresh_interval);
    ~DRMVideoSync();

    QString getName(void) const override // VideoSync
        { return QString("DRM"); }
    bool TryInit(void) override; // VideoSync
    void Start(void) override; // VideoSync
    int WaitForFrame(int nominal_frame_interval, int extra_delay) override; // VideoSync

  private:
    int m_driFd;
    static const char *s_driDev;
    
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
    RTCVideoSync(MythVideoOutput *, int refresh_interval);
    ~RTCVideoSync();

    QString getName(void) const override // VideoSync
        { return QString("RTC"); }
    bool TryInit(void) override; // VideoSync
    int WaitForFrame(int nominal_frame_interval, int extra_delay) override; // VideoSync

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
    BusyWaitVideoSync(MythVideoOutput *vo, int ri) : VideoSync(vo, ri) {};
    ~BusyWaitVideoSync() = default;

    QString getName(void) const override // VideoSync
        { return QString("USleep with busy wait"); }
    bool TryInit(void) override {return true; } // VideoSync
    int WaitForFrame(int nominal_frame_interval, int extra_delay) override; // VideoSync

  private:
    int m_cheat {5000};
    int m_fudge {0};
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
    USleepVideoSync(MythVideoOutput *vo, int ri) : VideoSync(vo, ri) {}
    ~USleepVideoSync() = default;

    QString getName(void) const override // VideoSync
        { return QString("USleep"); }
    bool TryInit(void) override {return true; } // VideoSync
    int WaitForFrame(int nominal_frame_interval, int extra_delay) override; // VideoSync
};

class DummyVideoSync : public VideoSync
{
  public:
    DummyVideoSync(MythVideoOutput* vo, int ri) : VideoSync(vo, ri) { }
    ~DummyVideoSync() = default;

    QString getName(void) const override // VideoSync
        { return QString("Dummy"); }
    bool TryInit(void) override // VideoSync
        { return true; }
    int WaitForFrame(int /*nominal_frame_interval*/, int /*extra_delay*/) override // VideoSync
        { return 0; }
};
#endif /* VSYNC_H_INCLUDED */
