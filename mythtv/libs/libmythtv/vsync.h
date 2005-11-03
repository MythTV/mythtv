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

#include <sys/time.h>
#include <time.h>

class VideoOutput;

#ifdef USING_OPENGL_VSYNC
typedef unsigned long XID;
typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;
typedef struct _XDisplay Display;
#endif

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
 *   A/V sync methods may supply an additonal delay per frame. Other
 *   than that, video timing is entirely up to these classes. When
 *   WaitForFrame returns, it is time to show the frame.
 *
 *   There is some basic support for interlaced video timing where the
 *   fields need to be displayed sequentially. Passing true for the
 *   interlaced flags for the constructors, BestMethod, and
 *   SetFrameInterval will cause us to wait for half the specified frame
 *   interval, if the refresh rate is sufficient to do so.
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
     *   interactions with pthreads.
     */
    virtual void Start(void);

    /** \brief Waits for next a frame or field.
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should
     *   always be called from same thread, to prevent bad
     *   interactions with pthreads.
     *
     *  \param sync_delay time until the desired frame or field
     *  \sa CalcDelay(void), KeepPhase(void)
     */
    virtual void WaitForFrame(int sync_delay) = 0;

    /// \brief Use the next frame or field for CalcDelay(void)
    ///        and WaitForFrame(int).
    virtual void AdvanceTrigger(void) = 0;

    void SetFrameInterval(int fi, bool interlaced);

    /// \brief Returns true AdvanceTrigger(void) advances a field at a time.
    bool UsesFieldInterval(void) const { return m_interlaced; }
    /// \brief Returns true AdvanceTrigger(void) advances a frame at a time.
    bool UsesFrameInterval(void) const { return !m_interlaced; }

    /** \brief Stops VSync; must be called from main thread.
     *
     *   Start(void), WaitForFrame(void), and Stop(void) should
     *   always be called from same thread, to prevent bad
     *   interactions with pthreads.
     */
    virtual void Stop(void) {}

    // documented in vsync.cpp
    static VideoSync *BestMethod(VideoOutput*,
                                 uint frame_interval, uint refresh_interval,
                                 bool interlaced);
  protected:
    static void OffsetTimeval(struct timeval& tv, int offset);
    void UpdateNexttrigger(void);
    int CalcDelay(void);
    void KeepPhase(void);

    VideoOutput *m_video_output;
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
    DRMVideoSync(VideoOutput*,
                 int frame_interval, int refresh_interval, bool interlaced);
    ~DRMVideoSync();

    QString getName(void) const { return QString("DRM"); }
    bool TryInit(void);
    void Start(void);
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);

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
    nVidiaVideoSync(VideoOutput*,
                    int frame_interval, int refresh_interval, bool interlaced);
    ~nVidiaVideoSync();

    QString getName(void) const { return QString("nVidia polling"); }
    bool TryInit(void);
    void Start(void);
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);

  private:
    bool dopoll(void) const;
    int m_nvidia_fd;
    static char *sm_nvidia_dev;
};

/** \brief Video synchronization class employing SGI_video_sync
 *         OpenGL extension.
 *
 *   Uses glXWaitVideoSyncSGI() to wait for retrace. Phase-maintaining,
 *   meaning WaitForFrame should always return approximately the same time 
 *   after a vertical retrace.
 *
 *   This works with version 50 or later of the nVidia vendor drivers.
 *
 *   Special care must be taken with this video sync method due 
 *   to a bad interaction between some pthread implementations
 *   and OpenGL. OpenGL DIRECT contexts can not be shared between
 *   processes. And some pthread implementations, notably a common
 *   one on Linux, treat each thread as a seperate process.
 *   Hence Start(void), Stop(void) and WaitForFrame(void) must all be called
 *   from the same thread.
 *
 *  \sa http://osgcvs.no-ip.com/osgarchiver/archives/June2002/0022.html
 *  \sa http://www.ac3.edu.au/SGI_Developer/books/OpenGLonSGI/sgi_html/ch10.html#id37188
 *  \sa http://www.inb.mu-luebeck.de/~boehme/xvideo_sync.html
 */
class OpenGLVideoSync : public VideoSync
{
  public:
    OpenGLVideoSync(VideoOutput*,
                    int frame_interval, int refresh_interval, bool interlaced);
    ~OpenGLVideoSync();

    QString getName(void) const { return QString("SGI OpenGL"); }
    bool TryInit(void);
    void Start(void);
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);
    void Stop(void);

  private:
    Display *m_display;
    GLXDrawable m_drawable;
    GLXContext m_context;
};

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
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);

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
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);

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
    USleepVideoSync(VideoOutput*,
                    int frame_interval, int refresh_interval, 
                    bool interlaced);
    ~USleepVideoSync();

    QString getName(void) const { return QString("USleep"); }
    bool TryInit(void);
    void WaitForFrame(int sync_delay);
    void AdvanceTrigger(void);
};
#endif /* VSYNC_H_INCLUDED */
