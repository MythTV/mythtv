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

#include <cstdio>
#include <cerrno>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include <mythcontext.h>
#include "util-x11.h"

#ifdef USING_OPENGL_VSYNC
#define GLX_GLXEXT_PROTOTYPES
#define XMD_H 1
#include <GL/glx.h>
#include <GL/gl.h>
#undef GLX_ARB_get_proc_address
#include <GL/glxext.h>
#endif

#ifdef __linux__
#include <linux/rtc.h>
#endif

using namespace std;
#include "vsync.h"

bool tryingVideoSync = false;
int VideoSync::m_forceskip = 0;

#define TESTVIDEOSYNC(NAME) \
    if (++m_forceskip > skip) \
    { \
        trial = new NAME (frame_interval, refresh_interval, interlaced); \
        if (trial->TryInit()) \
        { \
	    m_forceskip = skip; \
	    tryingVideoSync=false; \
	    return trial; \
        } \
        delete trial; \
    }

/** \fn VideoSync::BestMethod(int, int, bool)
 *  \brief Returns the most sophisticated video sync method available.
 */
VideoSync *VideoSync::BestMethod(int frame_interval, int refresh_interval,
                                 bool interlaced)
{
    VideoSync *trial;

    tryingVideoSync=true;
    int skip=0;
    if (m_forceskip!=0)
    {
        VERBOSE(VB_PLAYBACK, QString("A previous trial crashed,"
		" skipping %1").arg(m_forceskip));
    
	skip=m_forceskip;
	m_forceskip=0;
    }
    
    TESTVIDEOSYNC(nVidiaVideoSync)
    TESTVIDEOSYNC(DRMVideoSync)
#ifdef USING_OPENGL_VSYNC
    TESTVIDEOSYNC(OpenGLVideoSync)
#endif
#ifdef __linux__
    TESTVIDEOSYNC(RTCVideoSync)
#endif
    TESTVIDEOSYNC(BusyWaitVideoSync)

    tryingVideoSync=false;
    return NULL;
}

/** \fn VideoSync::VideoSync(int, int, bool)
 *  \brief Used by BestMethod(int, int, bool) to initialize
 *         video synchronization method.
 */
VideoSync::VideoSync(int frameint, int refreshint, bool interlaced) :
    m_frame_interval(frameint), m_refresh_interval(refreshint), 
    m_interlaced(interlaced) 
{
    if (m_interlaced && m_refresh_interval > m_frame_interval/2)
        m_interlaced = false; // can't display both fields at 2x rate

    //cout << "Frame interval: " << m_frame_interval << endl;
}

void VideoSync::Start()
{
    gettimeofday(&m_nexttrigger, NULL); // now
}

/** \fn VideoSync::SetFrameInterval(int fr, bool intr)
 *  \brief Change frame interval and interlacing attributes
 */
void VideoSync::SetFrameInterval(int fr, bool intr)
{
    m_frame_interval = fr;
    m_interlaced = intr;
    if (m_interlaced && m_refresh_interval > m_frame_interval/2)
        m_interlaced = false; // can't display both fields at 2x rate

    VERBOSE(VB_PLAYBACK, QString("Set video sync frame interval to %1")
                                 .arg(m_frame_interval));
}

void VideoSync::OffsetTimeval(struct timeval& tv, int offset)
{
    tv.tv_usec += offset;
    while (tv.tv_usec > 999999)
    {
        tv.tv_sec++;
        tv.tv_usec -= 1000000;
    }
    while (tv.tv_usec < 0)
    {
        tv.tv_sec--;
        tv.tv_usec += 1000000;
    }
}

/** \fn VideoSync::UpdateNexttrigger()
 *  \brief Internal method to tells video synchronization method to use
 *         the next frame (or field, if interlaced) for CalcDelay() 
 *         and WaitForFrame().
 */
void VideoSync::UpdateNexttrigger()
{
    // Offset by frame interval -- if interlaced, only delay by half
    // frame interval
    if (m_interlaced)
        OffsetTimeval(m_nexttrigger, m_frame_interval/2);
    else
        OffsetTimeval(m_nexttrigger, m_frame_interval);
}

/** \fn VideoSync::CalcDelay()
 *  \brief Calculates the delay to the next frame.
 *
 *   Regardless of the timing method, if delay is greater than two full
 *   frames (could be greater than 20 or greater than 200), we don't want
 *   to freeze while waiting for a huge delay. Instead, contine playing 
 *   video at half speed and continue to read new audio and video frames
 *   from the file until the sync is 'in the ballpark'.
 */
int VideoSync::CalcDelay()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    //cout << "CalcDelay: next: " << timeval_str(m_nexttrigger) << " now " 
    // << timeval_str(now) << endl;
        
    int ret_val = (m_nexttrigger.tv_sec - now.tv_sec) * 1000000 +
                  (m_nexttrigger.tv_usec - now.tv_usec);

    //cout << "delay " << ret_val << endl;

    if (ret_val > m_frame_interval * 2)
    {
        if (m_interlaced)
            ret_val = m_frame_interval; // same as / 2 * 2.
        else
            ret_val = m_frame_interval * 2;

        // set nexttrigger to our new target time
        m_nexttrigger.tv_sec = now.tv_sec;
        m_nexttrigger.tv_usec = now.tv_usec;
        OffsetTimeval(m_nexttrigger, ret_val);
    }
    return ret_val;
}

/** \fn VideoSync::KeepPhase()
 *  \brief Keep our nexttrigger from drifting too close to the exact retrace.
 *
 *   If delay is near zero, some frames will be delay < 0 and others
 *   delay > 0 which would cause continous rapid fire stuttering.
 *   This method is only useful for those sync methods where WaitForFrame
 *   targets hardware retrace rather than targeting nexttrigger.
 */
void VideoSync::KeepPhase()
{
    // cerr << m_delay << endl;
    if (m_delay < -(m_refresh_interval/2))
        OffsetTimeval(m_nexttrigger, 200);
    else if (m_delay > -500)
        OffsetTimeval(m_nexttrigger, -2000);
}

#define DRM_VBLANK_RELATIVE 0x1;

struct drm_wait_vblank_request {
    int type;
    unsigned int sequence;
    unsigned long signal;
};

struct drm_wait_vblank_reply {
    int type;
    unsigned int sequence;
    long tval_sec;
    long tval_usec;
};

typedef union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
} drm_wait_vblank_t;

#define DRM_IOCTL_BASE                  'd'
#define DRM_IOWR(nr,type)               _IOWR(DRM_IOCTL_BASE,nr,type)

#define DRM_IOCTL_WAIT_VBLANK           DRM_IOWR(0x3a, drm_wait_vblank_t)

static int drmWaitVBlank(int fd, drm_wait_vblank_t *vbl)
{
    int ret;

    do {
       ret = ioctl(fd, DRM_IOCTL_WAIT_VBLANK, vbl);
       vbl->request.type &= ~DRM_VBLANK_RELATIVE;
    } while (ret && errno == EINTR);

    return ret;
}

char *DRMVideoSync::sm_dri_dev = "/dev/dri/card0";

DRMVideoSync::DRMVideoSync(int fr, int ri, bool intl) : VideoSync(fr, ri, intl)
{
    m_dri_fd = -1;
}
DRMVideoSync::~DRMVideoSync()
{
    if (m_dri_fd >= 0)
        close(m_dri_fd);
    m_dri_fd = -1;
}

bool DRMVideoSync::TryInit()
{
    drm_wait_vblank_t blank;

    m_dri_fd = open(sm_dri_dev, O_RDWR);
    if (m_dri_fd < 0)
    {
        VERBOSE(VB_PLAYBACK, QString("DRMVideoSync: Could not open device"
                " %1, %2").arg(sm_dri_dev).arg(strerror(errno)));
        return false; // couldn't open device
    }
    
    blank.request.type = DRM_VBLANK_RELATIVE;
    blank.request.sequence = 1;
    if (drmWaitVBlank(m_dri_fd, &blank))
    {
        VERBOSE(VB_PLAYBACK, QString("DRMVideoSync: VBlank ioctl did not work,"
                " unimplemented in this driver?"));
        return false; // VBLANK ioctl didn't worko
    }

    return true;
}

void DRMVideoSync::Start()
{
    // Wait for a refresh so we start out synched
    drm_wait_vblank_t blank;
    blank.request.type = DRM_VBLANK_RELATIVE;
    blank.request.sequence = 1;
    drmWaitVBlank(m_dri_fd, &blank);
    VideoSync::Start(); 
}

void DRMVideoSync::WaitForFrame(int sync_delay)
{
    // Offset for externally-provided A/V sync delay
    OffsetTimeval(m_nexttrigger, sync_delay);
    
    m_delay = CalcDelay();
    //cerr << "WaitForFrame at : " << m_delay;

    // Always sync to the next retrace execpt when we are very late.
    if (m_delay > -(m_refresh_interval/2)) 
    {
        drm_wait_vblank_t blank;
        blank.request.type = DRM_VBLANK_RELATIVE;
        blank.request.sequence = 1;
        drmWaitVBlank(m_dri_fd, &blank);
        m_delay = CalcDelay();
        // cerr << "\tDelay at sync: " << m_delay;
    }
    //cerr  << endl;

    if (m_delay > 0)
    {
        // Wait for any remaining retrace intervals in one pass.
        int n = m_delay / m_refresh_interval + 1;

        drm_wait_vblank_t blank;
        blank.request.type = DRM_VBLANK_RELATIVE;
        blank.request.sequence = n;
        drmWaitVBlank(m_dri_fd, &blank);
        m_delay = CalcDelay();
        //cerr << "Wait " << n << " intervals. Count " << blank.request.sequence;
        //cerr  << " Delay " << m_delay << endl;
    }
}

void DRMVideoSync::AdvanceTrigger()
{
    KeepPhase();
    UpdateNexttrigger();
}

char *nVidiaVideoSync::sm_nvidia_dev = "/dev/nvidia0";

nVidiaVideoSync::nVidiaVideoSync(int fi, int ri, bool intr) : 
    VideoSync(fi, ri, intr) {}

nVidiaVideoSync::~nVidiaVideoSync()
{
    if (m_nvidia_fd >= 0)
        close(m_nvidia_fd);
}

bool nVidiaVideoSync::dopoll() const
{
    int ret;
    struct pollfd polldata;
    polldata.fd = m_nvidia_fd;
    polldata.events = 0xff;
    polldata.revents = 0;
    ret = poll( &polldata, 1, 100 );
    if (!ret)
        return false;
    if (ret < 0) 
    {
        perror("nVidiaVideoSync::");
        return false;
    }
    return true;
}

bool nVidiaVideoSync::TryInit()
{
    m_nvidia_fd = open(sm_nvidia_dev, O_RDONLY);
    if (m_nvidia_fd < 0)
    {
        VERBOSE(VB_PLAYBACK, QString("nVidiaVideoSync: Could not open device"
                " %1, %2").arg(sm_nvidia_dev).arg(strerror(errno)));
        return false;
    }

    if (!dopoll())
    {
        VERBOSE(VB_PLAYBACK, QString("nVidiaVideoSync: VBlank ioctl did not work,"
                " unimplemented in this driver?"));
        close(m_nvidia_fd);
        return false;
    }
    return true;
}

void nVidiaVideoSync::Start()
{
    dopoll();
    VideoSync::Start();
}

void nVidiaVideoSync::WaitForFrame(int sync_delay)
{
    OffsetTimeval(m_nexttrigger, sync_delay);
    
    m_delay = CalcDelay();

    // Always sync to the next retrace execpt when we are very late.
    if (m_delay > -(m_refresh_interval/2))
    {
        dopoll();
        m_delay = CalcDelay();
    }

    // Wait for any remaining retrace intervals.
    while (m_delay > 0)
    {
        dopoll();
        m_delay = CalcDelay();
    }
}

void nVidiaVideoSync::AdvanceTrigger()
{
    KeepPhase();
    UpdateNexttrigger();
}

#ifdef USING_OPENGL_VSYNC
/* Try to create an OpenGL surface so we can use glXWaitVideoSyncSGI:
 * http://osgcvs.no-ip.com/osgarchiver/archives/June2002/0022.html
 * http://www.ac3.edu.au/SGI_Developer/books/OpenGLonSGI/sgi_html/ch10.html#id37188
 * http://www.inb.mu-luebeck.de/~boehme/xvideo_sync.html
 */
OpenGLVideoSync::OpenGLVideoSync(int fr, int ri, bool intl) : 
    VideoSync(fr, ri, intl)
{
    m_display = NULL;
    m_drawable = 0;
    m_context = 0;
}

OpenGLVideoSync::~OpenGLVideoSync()
{
    if (m_display != NULL)
        X11S(XCloseDisplay(m_display));
}

bool OpenGLVideoSync::TryInit()
{
    X11S(m_display = XOpenDisplay(NULL));
    if (m_display == NULL)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: Could not"
                " open X Window Display.")/*arg(XErrorMessage)*/;
        return false;
    }

    /* Look for GLX at all */
    int ndummy;
    int ret;
    X11S(ret = glXQueryExtension(m_display, &ndummy, &ndummy));
    if (!ret)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: OpenGL extension not present.");
        return false;
    }

    /* Look for video sync extension */
    const char *xt = NULL;
    X11S(xt = glXQueryExtensionsString(m_display, DefaultScreen(m_display)));
    VERBOSE(VB_PLAYBACK, QString("OpenGLVideoSync: GLX extensions: %1").arg(xt));
    if (strstr(xt, "GLX_SGI_video_sync") == NULL)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: GLX Video Sync"
                " extension not present.");
        return false;
    }

    int attribList[] = {GLX_RGBA, 
                        GLX_RED_SIZE, 1,
                        GLX_GREEN_SIZE, 1,
                        GLX_BLUE_SIZE, 1,
                        None};
    XVisualInfo *vis;
    XSetWindowAttributes swa;
    Window w;
    
    X11S(vis = glXChooseVisual(m_display, DefaultScreen(m_display), attribList));
    if (vis == NULL) 
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: No appropriate visual found");
        return false;
    }
    X11S(swa.colormap = XCreateColormap(m_display,
                                        RootWindow(m_display, vis->screen),
                                        vis->visual, AllocNone));
    if (swa.colormap == 0)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: Failed to create colormap");
        return false;
    }
    X11S(w = XCreateWindow(m_display, RootWindow(m_display, vis->screen),
                           0, 0, 1, 1, 0, vis->depth, InputOutput,
                           vis->visual, CWColormap, &swa));
    if (w == 0)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: Failed to create dummy window");
        return false;
    }
    X11S(m_context = glXCreateContext(m_display, vis, None, GL_TRUE));
    if (m_context == NULL)
    {
        VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: Failed to create GLX context");
        return false;
    }
    m_drawable = w;
    X11S(ret = glXMakeCurrent(m_display, m_drawable, m_context));
    if (ret != False)
    {
        unsigned int count;
        X11S(ret = glXGetVideoSyncSGI(&count));
        if (GLX_BAD_CONTEXT == ret)
            VERBOSE(VB_PLAYBACK, "OpenGLVideoSync: Bad Context for VSync");
        return GLX_BAD_CONTEXT != ret;
    }
    else
    {
        VERBOSE(VB_PLAYBACK,
                "OpenGLVideoSync: Failed to make GLX context current context");
        return false;
    }
}

void OpenGLVideoSync::Start()
{
    // Wait for a refresh so we start out synched
    unsigned int count;
    int r;

    X11L;

    r = glXMakeCurrent(m_display, m_drawable, m_context);
    r = glXGetVideoSyncSGI(&count);
    r = glXWaitVideoSyncSGI(2, (count+1)%2 ,&count);

    X11U;

    VideoSync::Start();
}

void OpenGLVideoSync::WaitForFrame(int sync_delay)
{
    unsigned int count, n;
    int r;

    OffsetTimeval(m_nexttrigger, sync_delay);

    m_delay = CalcDelay();
    //cerr << "WaitForFrame at : " << m_delay;

    // Always sync to the next retrace execpt when we are very late.
    if (m_delay > -(m_refresh_interval/2)) 
    {

        X11L;
        r = glXMakeCurrent(m_display, m_drawable, m_context);
        r = glXGetVideoSyncSGI(&count);
        X11U;

        // POSSIBLY UNSAFE -- POSSIBLY UNSAFE
        r = glXWaitVideoSyncSGI(2, (count+1)%2 ,&count);
        // POSSIBLY UNSAFE -- POSSIBLY UNSAFE


        m_delay = CalcDelay();
        //cerr << "\tDelay at sync: " << m_delay;
    }
    //cerr << endl;

    if (m_delay > 0)
    {
        // Wait for any remaining retrace intervals in one pass.
        n = m_delay / m_refresh_interval + 1;

        // POSSIBLY UNSAFE -- POSSIBLY UNSAFE
        r = glXWaitVideoSyncSGI((n+1), (count+n)%(n+1), &count);
        // POSSIBLY UNSAFE -- POSSIBLY UNSAFE

        m_delay = CalcDelay();
        //cerr << "Wait " << n << " intervals. Count " << count;
        //cerr  << " Delay " << m_delay << endl;
    }
}

void OpenGLVideoSync::AdvanceTrigger()
{
    KeepPhase();
    UpdateNexttrigger();
}

/** \fn OpenGLVideoSync::Stop()
 *  \brief Stops VSync; must be called from main thread.
 *
 *   This method should be called from the MAIN THREAD to work around
 *   a bug in some OpenGL implementations, where if the thread that
 *   has the current GL context goes away, no other thread can ever
 *   have it.
 */
void OpenGLVideoSync::Stop()
{
    X11S(glXMakeCurrent(m_display, 0, 0));
}
#endif /* USING_OPENGL_VSYNC */

#ifdef __linux__
#define RTCRATE 1024
RTCVideoSync::RTCVideoSync(int fi, int ri, bool intr) :
    VideoSync(fi, ri, intr)
{
    m_rtcfd = -1;
}

RTCVideoSync::~RTCVideoSync()
{
    if (m_rtcfd >= 0)
        close(m_rtcfd);
}

bool RTCVideoSync::TryInit()
{
    m_rtcfd = open("/dev/rtc", O_RDONLY);
    if (m_rtcfd < 0)
    {
        VERBOSE(VB_PLAYBACK, QString("RTCVideoSync: Could not"
                " open /dev/rtc, %1.").arg(strerror(errno)));
        return false;
    }

    // FIXME, does it make sense to tie RTCRATE to the desired framerate?
    if ((ioctl(m_rtcfd, RTC_IRQP_SET, RTCRATE) < 0))
    {
        VERBOSE(VB_PLAYBACK, QString("RTCVideoSync: Could not"
                " set RTC frequency, %1.").arg(strerror(errno)));
        return false;
    }

    if (ioctl(m_rtcfd, RTC_PIE_ON, 0) < 0)
    {
        VERBOSE(VB_PLAYBACK, QString("RTCVideoSync: Could not enable periodic "
                "timer interrupts, %1.").arg(strerror(errno)));
        return false;
    }
    
    return true;
}

void RTCVideoSync::WaitForFrame(int sync_delay)
{
    OffsetTimeval(m_nexttrigger, sync_delay);

    m_delay = CalcDelay();

    unsigned long rtcdata;
    while (m_delay > 0)
    {
        read(m_rtcfd, &rtcdata, sizeof(rtcdata));
        m_delay = CalcDelay();
    }
}

void RTCVideoSync::AdvanceTrigger()
{
    UpdateNexttrigger();
}
#endif /* __linux__ */

BusyWaitVideoSync::BusyWaitVideoSync(int fr, int ri, bool intl) : 
    VideoSync(fr, ri, intl) 
{
    m_cheat = 5000;
    m_fudge = 0;
}

BusyWaitVideoSync::~BusyWaitVideoSync()
{
}

bool BusyWaitVideoSync::TryInit()
{
    return true;
}

void BusyWaitVideoSync::WaitForFrame(int sync_delay)
{
    // Offset for externally-provided A/V sync delay
    OffsetTimeval(m_nexttrigger, sync_delay);

    m_delay = CalcDelay();

    if (m_delay > 0)
    {
        int cnt = 0;
        m_cheat += 100;
        // The usleep() is shortened by "cheat" so that this process gets
        // the CPU early for about half the frames.
        if (m_delay > (m_cheat - m_fudge))
            usleep(m_delay - (m_cheat - m_fudge));
        
        // If late, draw the frame ASAP.  If early, hold the CPU until
        // half as late as the previous frame (fudge).
        m_delay = CalcDelay();
        while (m_delay + m_fudge > 0)
        {
            m_delay = CalcDelay();
            cnt++;
        }
        m_fudge = abs(m_delay / 2);
        if (cnt > 1)
            m_cheat -= 200;
    }
}

void BusyWaitVideoSync::AdvanceTrigger()
{
    UpdateNexttrigger();
}

USleepVideoSync::USleepVideoSync(int fr, int ri, bool intl) : 
    VideoSync(fr, ri, intl) 
{
}

USleepVideoSync::~USleepVideoSync()
{
}

bool USleepVideoSync::TryInit()
{
    return true;
}

void USleepVideoSync::WaitForFrame(int sync_delay)
{
    // Offset for externally-provided A/V sync delay
    OffsetTimeval(m_nexttrigger, sync_delay);
    
    m_delay = CalcDelay();
    if (m_delay > 0)
        usleep(m_delay);
}

void USleepVideoSync::AdvanceTrigger()
{
    UpdateNexttrigger();
}
