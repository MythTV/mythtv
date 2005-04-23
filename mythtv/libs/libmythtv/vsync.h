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
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef VSYNC_H_INCLUDED
#define VSYNC_H_INCLUDED

/* Notes 

   This set of classes implements a variety of algorithms for synching
   video frames to a display.  The better algorithms synchronize to
   the display's refresh rate, and also attempt to maintain phase with
   respect to the vertical retrace.  The poorer algorithms simply try
   to display frames at a regular interval.

   The factory method BestMethod tries subclasses in roughly quality
   order until one succeeds.

   A/V sync methods may supply an additonal delay per frame.  Other
   than that, video timing is entirely up to these classes.  When
   WaitForFrame returns, it is time to show the frame.

   There is some basic support for interlaced video timing where the
   fields need to be displayed sequentially.  Passing true for the
   interlaced flags for the constructors, BestMethod, and
   SetFrameInterval will cause us to wait for half the specified frame
   interval, if the refresh rate is sufficient to do so.
 */

#include <sys/time.h>
#include <time.h>

#ifdef USING_OPENGL_VSYNC
typedef unsigned long XID;
typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;
typedef struct _XDisplay Display;
#endif

extern bool tryingVideoSync;

/// \brief Virtual base class for all video synchronization classes.
class VideoSync
// virtual base class
{
 public:
    VideoSync(int fi, int ri, bool intr);
    virtual ~VideoSync() {}

    /// \brief Returns name of instanciated VSync method.
    virtual QString getName() const = 0;
    /// \brief Tries to initialize VSync method.
    virtual bool TryInit() = 0;

    /// \brief Start VSync; must be called from main thread.
    virtual void Start();

    /** \brief Waits for next a frame or field.
     *  \param sync_delay time until the desired frame or field
     *  \sa CalcDelay(), KeepPhase()
     */
    virtual void WaitForFrame(int sync_delay) = 0;

    /// \brief Use the next frame or field for CalcDelay() and WaitForFrame(int).
    virtual void AdvanceTrigger() = 0;

    void SetFrameInterval(int fi, bool interlaced);

    /// \brief Returns whether AdvanceTrigger() advances
    ///        a field or frame at a time.
    bool isInterlaced() const { return m_interlaced; }

    /// \brief Stops VSync; must be called from main thread.
    virtual void Stop() {}
    static VideoSync *BestMethod(int frame_interval, int refresh_interval,
                                 bool interlaced);
 protected:
    static void OffsetTimeval(struct timeval& tv, int offset);
    void UpdateNexttrigger();
    int CalcDelay();
    void KeepPhase();

    int m_frame_interval; // of video
    int m_refresh_interval; // of display
    bool m_interlaced;
    struct timeval m_nexttrigger;
    int m_delay;
    
    static int m_forceskip;
};

/** \brief Video synchronization class employing /dev/drm0
 *
 *   Polls /dev/drm0 to wait for retrace.  Phase-maintaining, meaning
 *   WaitForFrame should always return approximately the same time after
 *   a vertical retrace.
 */
class DRMVideoSync : public VideoSync
{
 public:
    DRMVideoSync(int frame_interval, int refresh_interval, bool interlaced);
    ~DRMVideoSync();

    QString getName() const { return QString("DRM"); }
    bool TryInit();
    void Start();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();

 private:
    int m_dri_fd;
    static char *sm_dri_dev;
    
};

/** \brief Video synchronization class employing /dev/nvidia0
 *
 *   Polls /dev/nvidia0 to wait for retrace.  Phase-maintaining, meaning
 *   WaitForFrame should always return approximately the same time after
 *   a vertical retrace. This does not work with version 50 or later
 *   of the nVidia vendor drivers.
 */
class nVidiaVideoSync : public VideoSync
{
 public:
    nVidiaVideoSync(int frame_interval, int refresh_interval, bool interlaced);
    ~nVidiaVideoSync();

    QString getName() const { return QString("nVidia polling"); }
    bool TryInit();
    void Start();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();

 private:
    bool dopoll() const;
    int m_nvidia_fd;
    static char *sm_nvidia_dev;
};

#ifdef USING_OPENGL_VSYNC
/** \brief Video synchronization class employing SGI_video_sync
 *         OpenGL extension.
 *
 *   Uses glXWaitVideoSyncSGI() to wait for retrace.  Phase-maintaining,
 *   meaning WaitForFrame should always return approximately the same time 
 *   after a vertical retrace.
 *
 *   This works with version 50 or later of the nVidia vendor drivers.
 */
class OpenGLVideoSync : public VideoSync
{
public:
    OpenGLVideoSync(int frame_interval, int refresh_interval, bool interlaced);
    ~OpenGLVideoSync();

    QString getName() const { return QString("SGI OpenGL"); }
    bool TryInit();
    void Start();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();
    void Stop();

private:
    Display *m_display;
    GLXDrawable m_drawable;
    GLXContext m_context;
};
#endif /* USING_OPENGL_VSYNC */

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
    RTCVideoSync(int frame_interval, int refresh_interval, bool interlaced);
    ~RTCVideoSync();

    QString getName() const { return QString("RTC"); }
    bool TryInit();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();

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
 *  TryInit() always succeeds.
 *
 */
class BusyWaitVideoSync : public VideoSync
{
public:
    BusyWaitVideoSync(int frame_interval, int refresh_interval, 
                      bool interlaced);
    ~BusyWaitVideoSync();

    QString getName() const { return QString("USleep with busy wait"); }
    bool TryInit();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();

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
 *   This only used when NVP's 'disablevideo' is true (i.e. for 
 *   commercial flagging and for transcoding), since it doesn't
 *   waste CPU cycles busy-waiting like BusyWaitVideoSync.
 */
class USleepVideoSync : public VideoSync
{
public:
    USleepVideoSync(int frame_interval, int refresh_interval, 
                          bool interlaced);
    ~USleepVideoSync();

    QString getName() const { return QString("USleep"); }
    bool TryInit();
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger();
};
#endif /* VSYNC_H_INCLUDED */
