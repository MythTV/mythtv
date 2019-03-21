
#include "videoout_omx.h"

#ifdef OSD_EGL /* includes QJson with enum value named Bool, must go before EGL/egl.h */
# include "mythpainter_ogl.h"
# include "mythpainter_qimage.h"
#endif //def OSD_EGL

/* must go before X11/X.h due to #define None 0L */
#include "privatedecoder_omx.h" // For PrivateDecoderOMX::s_name

#include <cstddef>
#include <cassert>
#include <algorithm> // max/min
#include <vector>

#include <QTransform>

#include <OMX_Core.h>
#include <OMX_Video.h>
#ifdef USING_BROADCOM
#include <OMX_Broadcom.h>
#include <bcm_host.h>
#endif

#ifdef OSD_EGL
#include <EGL/egl.h>
#include <QtGlobal>
#endif

// MythTV
#ifdef OSD_EGL
# define LOC QString("EGL: ")
# include "mythrender_opengl2es.h"
# undef LOC
# if 0 /* moved to top so it goes before X11/Xlib.h which is included via EGL/egl.h on Raspbian */
#  include "mythpainter_ogl.h"
# endif
#endif //def OSD_EGL

#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythcorecontext.h"

#include "filtermanager.h"
#include "videodisplayprofile.h"
#include "videobuffers.h"

#include "omxcontext.h"
using namespace omxcontext;


/*
 * Macros
 */
#define LOC QString("VideoOutputOMX: ")

// Roundup a value: y = ROUNDUP(x,4)
#define ROUNDUP( _x,_z) ((_x) + ((-(_x)) & ((_z) -1)) )

// VideoFrame <> OMX_BUFFERHEADERTYPE
#define FRAMESETHDR(f,h) ((f)->priv[3] = reinterpret_cast<unsigned char* >(h))
#define FRAMESETHDRNONE(f) ((f)->priv[3] = nullptr)
#define FRAME2HDR(f) ((OMX_BUFFERHEADERTYPE*)((f)->priv[3]))
#define HDR2FRAME(h) ((VideoFrame*)((h)->pAppPrivate))

// Component names
#ifdef USING_BELLAGIO
# define VIDEO_RENDER "xvideosink"
# define IMAGE_FX "" // Not implemented
#else
# define VIDEO_RENDER "video_render"
# define IMAGE_FX "image_fx"
#endif


/*
 * Types
 */
#ifdef OSD_EGL
class MythRenderEGL : public MythRenderOpenGL2ES
{
    // No copying
    MythRenderEGL(MythRenderEGL&);
    MythRenderEGL& operator =(MythRenderEGL &rhs);

  public:
    MythRenderEGL();

    void makeCurrent() override; // MythRenderOpenGL
    void doneCurrent() override; // MythRenderOpenGL
#ifdef USE_OPENGL_QT5
    void swapBuffers() override; // MythRenderOpenGL
#else
    void swapBuffers() const override; // QGLContext
    bool create(const QGLContext * = nullptr) override // QGLContext
        { return isValid(); }
#endif

  protected:
    virtual ~MythRenderEGL(); // Use MythRenderOpenGL2ES::DecrRef to delete

    EGLNativeWindowType createNativeWindow();
    void destroyNativeWindow();

    EGLDisplay m_display;
    EGLContext m_context;
    EGLNativeWindowType m_window;
    EGLSurface m_surface;

#ifdef USING_BROADCOM
private:
    EGL_DISPMANX_WINDOW_T gNativewindow;
    DISPMANX_DISPLAY_HANDLE_T m_dispman_display;
#endif
};

class GlOsdThread : public MThread
{
    public:
        GlOsdThread() :
            MThread("GlOsdThread")
        {
            isRunning = true;
            m_osdImage = nullptr;
            m_EGLRender = nullptr;
            m_Painter = nullptr;
            rectsChanged = false;
            m_lock.lock();
        }
        void run() override // MThread
        {
            RunProlog();
            m_EGLRender = new MythRenderEGL();
            if (m_EGLRender->create())
            {
                m_EGLRender->Init();
                m_Painter = new MythOpenGLPainter(m_EGLRender);
                m_Painter->SetSwapControl(false);
                LOG(VB_GENERAL, LOG_INFO, LOC + __func__ +
                ": OSD display uses threaded opengl");

            }
            else
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + __func__ +
                    ": failed to create MythRenderEGL for OSD");
                m_EGLRender = nullptr;
                m_Painter = nullptr;
                isRunning = false;
            }
            m_lock.unlock();
            while (isRunning)
            {
                m_lock.lock();
                m_wait.wait(&m_lock);
                if (!isRunning) {
                    m_lock.unlock();
                    break;
                }

                if (rectsChanged)
                {
                    m_EGLRender->SetViewPort(m_displayRect);
                    m_EGLRender->SetViewPort(m_glRect,true);
                    rectsChanged = false;
                }
                m_EGLRender->makeCurrent();
                m_EGLRender->BindFramebuffer(0);
                m_Painter->DrawImage(m_bounds, m_osdImage,
                                  m_bounds, 255);
                m_EGLRender->swapBuffers();
                m_EGLRender->SetBackground(0, 0, 0, 0);
                m_EGLRender->ClearFramebuffer();
                m_Painter->DeleteTextures();
                m_EGLRender->doneCurrent();

                m_lock.unlock();
            }
            if (m_osdImage)
                m_osdImage->DecrRef(), m_osdImage = nullptr;
            if (m_Painter)
                delete m_Painter, m_Painter = nullptr;
            if (m_EGLRender)
                m_EGLRender->DecrRef(), m_EGLRender = nullptr;
            RunEpilog();
        }
        // All of the below methods are called from another thread
        void shutdown()
        {
            isRunning=false;
            m_lock.tryLock(2000);
            m_wait.wakeAll();
            m_lock.unlock();
            wait(2000);
        }
        bool isValid() {
            return isRunning;
        }
        void setRects(const QRect &displayRect,const QRect &glRect)
        {
            m_displayRect = displayRect;
            m_glRect = glRect;
            rectsChanged = true;
        }
    private:
        MythRenderEGL *m_EGLRender;
        MythOpenGLPainter *m_Painter;
        bool isRunning;
        QRect m_displayRect;
        QRect m_glRect;
        bool rectsChanged;
    public:
        QMutex  m_lock;
        QWaitCondition m_wait;
        MythImage *m_osdImage;
        QRect m_bounds;
};

#endif //def OSD_EGL

/*
 * Constants
 */
const int kNumBuffers = 11; // +1 if extra_for_pause
const int kMinBuffers = 5;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 1;
const int kPrebufferFramesSmall = 1;
const int kKeepPrebuffer = 1;

QString const VideoOutputOMX::kName ="openmax";


/*
 * Functions
 */
// static
void VideoOutputOMX::GetRenderOptions(render_opts &opts,
                                      QStringList &cpudeints)
{
    opts.renderers->append(kName);

    opts.deints->insert(kName, cpudeints);
#ifdef USING_BROADCOM
    (*opts.deints)[kName].append(kName + "advanced");
    (*opts.deints)[kName].append(kName + "fast");
    (*opts.deints)[kName].append(kName + "linedouble");
#endif
#ifdef OSD_EGL
    (*opts.osds)[kName].append("opengl");
    (*opts.osds)[kName].append("threaded");
#endif
    (*opts.osds)[kName].append("softblend");

    (*opts.safe_renderers)["dummy"].append(kName);
    (*opts.safe_renderers)["nuppel"].append(kName);
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append(kName);
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append(kName);
    if (opts.decoders->contains(PrivateDecoderOMX::s_name))
        (*opts.safe_renderers)[PrivateDecoderOMX::s_name].append(kName);

    opts.priorities->insert(kName, 70);
}

// static
QStringList VideoOutputOMX::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize & /*video_dim*/)
{
    QStringList list;
    if (codec_is_std(myth_codec_id))
        list += kName;

    return list;
}

VideoOutputOMX::VideoOutputOMX() :
    m_render(gCoreContext->GetSetting("OMXVideoRender", VIDEO_RENDER), *this),
    m_imagefx(gCoreContext->GetSetting("OMXVideoFilter", IMAGE_FX), *this),
    m_backgroundscreen(nullptr), m_videoPaused(false)
{
#ifdef OSD_EGL
    m_context = nullptr;
    m_osdpainter = nullptr;
    m_threaded_osdpainter = nullptr;
    m_glOsdThread = nullptr;
    m_changed = false;
#endif
    init(&av_pause_frame, FMT_YV12, nullptr, 0, 0, 0);

    if (gCoreContext->GetBoolSetting("UseVideoModes", false))
        display_res = DisplayRes::GetDisplayRes(true);

    if (OMX_ErrorNone != m_render.Init(OMX_IndexParamVideoInit))
        return;

    if (!m_render.IsValid())
        return;

    // Show default port definitions and video formats supported
    for (unsigned port = 0; port < m_render.Ports(); ++port)
    {
        m_render.ShowPortDef(port, LOG_DEBUG);
#if 0
        m_render.ShowFormats(port, LOG_DEBUG);
#endif
    }

    if (OMX_ErrorNone != m_imagefx.Init(OMX_IndexParamImageInit))
        return;

    if (!m_imagefx.IsValid())
        return;

    // Show default port definitions and formats supported
    for (unsigned port = 0; port < m_imagefx.Ports(); ++port)
    {
        m_imagefx.ShowPortDef(port, LOG_DEBUG);
#if 0
        m_imagefx.ShowFormats(port, LOG_DEBUG);
#endif
    }
}

// virtual
VideoOutputOMX::~VideoOutputOMX()
{
    // Must shutdown the OMX components now before our state becomes invalid.
    // When the component's dtor is called our state has already been destroyed.
    m_imagefx.Shutdown();
    m_render.Shutdown();

    DeleteBuffers();

#ifdef OSD_EGL
    if (m_osdpainter)
        delete m_osdpainter, m_osdpainter = nullptr;
    if (m_context)
        m_context->DecrRef(), m_context = nullptr;
    if (m_glOsdThread)
    {
        m_glOsdThread->shutdown();
        delete m_glOsdThread;
        m_glOsdThread = nullptr;
    }
    if (m_threaded_osdpainter)
        delete m_threaded_osdpainter, m_threaded_osdpainter = nullptr;
#endif

    if (m_backgroundscreen)
    {
        m_backgroundscreen->Close();
        m_backgroundscreen = nullptr;
        GetMythUI()->RemoveCurrentLocation();
    }
}

// virtual
bool VideoOutputOMX::Init(          // Return true if successful
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp,    // video display size
    float aspect,                   // w/h of presented video
    WId winid,                      // "video playback window" widget winId()
    const QRect &win_rect,          // playback rect on "video playback window"
    MythCodecID codec_id )          // video codec
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ +
        QString(" vbuf=%1x%2 vdisp=%3x%4 aspect=%5 win=%6,%7,%8x%9 codec=%10")
            .arg(video_dim_buf.width()).arg(video_dim_buf.height())
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect).arg(win_rect.x()).arg(win_rect.y())
            .arg(win_rect.width()).arg(win_rect.height())
            .arg(toString(codec_id)) );

    if (!codec_is_std(codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("Cannot create VideoOutput for codec %1")
            .arg(toString(codec_id)));
        errorState = kError_Unknown;
        return false;
    }

    if (getenv("NO_OPENMAX"))
    {
        LOG(VB_PLAYBACK, LOG_NOTICE, LOC + __func__ + " OpenMAX disabled");
        errorState = kError_Unknown;
        return false;
    }

    if (!m_render.IsValid())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No video render");
        errorState = kError_Unknown;
        return false;
    }
    if (!m_imagefx.IsValid())
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Hardware deinterlace unavailable");

    window.SetAllowPreviewEPG(true);

    if (!VideoOutput::Init(video_dim_buf, video_dim_disp,
                      aspect, winid, win_rect, codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + ": VideoOutput::Init failed");
        return false;
    }

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer(kName);

    // Set resolution/measurements
    InitDisplayMeasurements(video_dim_disp.width(), video_dim_disp.height(), false);

    if (OMX_ErrorNone != SetImageFilter(OMX_ImageFilterNone))
        return false;

    // Setup video buffers
    static const int kBuffers = std::max(
        gCoreContext->GetNumSetting("OmxVideoBuffers", kNumBuffers), kMinBuffers);
    vbuffers.Init(std::max(kBuffers, int(m_render.PortDef().nBufferCountMin)),
                  true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);

    // Allocate video buffers
    if (!CreateBuffers(video_dim_buf, video_dim_disp))
        return false;

    bool osdIsSet = false;
#ifdef OSD_EGL
    if (GetOSDRenderer() == "opengl")
    {
        MythRenderEGL *render = new MythRenderEGL();
        if (render->create())
        {
            render->Init();
            m_context = render;
            MythOpenGLPainter *p = new MythOpenGLPainter(m_context);
            p->SetSwapControl(false);
            m_osdpainter = p;
            LOG(VB_GENERAL, LOG_INFO, LOC + __func__ +
                ": OSD display uses opengl");
            osdIsSet = true;
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + __func__ +
                ": failed to create MythRenderEGL");
            render->DecrRef();
            m_context = nullptr;
            m_osdpainter = nullptr;
        }
    }
    if (GetOSDRenderer() == "threaded")
    {
        m_glOsdThread = new GlOsdThread();
        m_glOsdThread->start();
        // Wait until set up
        m_glOsdThread->m_lock.lock();
        m_glOsdThread->m_lock.unlock();
        if (m_glOsdThread->isValid())
            osdIsSet = true;
    }
#endif

    if (!osdIsSet)
        LOG(VB_GENERAL, LOG_INFO, LOC + __func__ +
                ": OSD display uses softblend");

    MoveResize();

    m_disp_rect = m_vid_rect = QRect();
    if (!SetVideoRect(window.GetDisplayVideoRect(), window.GetVideoRect()))
        return false;

    if (!Start())
        return false;

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + " done");
    return true;
}

// virtual
bool VideoOutputOMX::InputChanged(  // Return true if successful
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp,    // video display size
    float        aspect,            // w/h of presented video
    MythCodecID  av_codec_id,       // video codec
    void        *codec_private,
    bool        &aspect_only )      // Out: true if aspect only changed
{
    QSize cursize = window.GetActualVideoDim();

    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ +
        QString(" from %1: %2x%3 aspect %4 to %5: %6x%7 aspect %9")
            .arg(toString(video_codec_id)).arg(cursize.width())
            .arg(cursize.height()).arg(window.GetVideoAspect())
            .arg(toString(av_codec_id)).arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(aspect));

    if (!codec_is_std(av_codec_id))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "New video codec is not supported.");
        errorState = kError_Unknown;
        return false;
    }

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != cursize;
    bool asp_changed = aspect      != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    VideoOutput::InputChanged(video_dim_buf, video_dim_disp,
                              aspect, av_codec_id, codec_private,
                              aspect_only);

    m_imagefx.Shutdown();
    m_render.Shutdown();

    DeleteBuffers();
    if (!CreateBuffers(video_dim_buf, video_dim_disp))
        return false;

    MoveResize();

    m_disp_rect = m_vid_rect = QRect();
    if (!SetVideoRect(window.GetDisplayVideoRect(), window.GetVideoRect()))
        return false;

    if (!Start())
        return false;

    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer(kName);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + " done");
    return true;
}

// virtual
bool VideoOutputOMX::ApproveDeintFilter(const QString& filtername) const
{
    if (filtername.contains(kName))
        return true;

    return VideoOutput::ApproveDeintFilter(filtername);
}

// virtual
bool VideoOutputOMX::SetDeinterlacingEnabled(bool interlaced)
{
    return SetupDeinterlace(interlaced);
}

// virtual
bool VideoOutputOMX::SetupDeinterlace(bool interlaced, const QString &overridefilter)
{
    if (!m_imagefx.IsValid())
        return VideoOutput::SetupDeinterlace(interlaced, overridefilter);

    QString deintfiltername;
    if (db_vdisp_profile)
        deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);

    if (!deintfiltername.contains(kName))
    {
        if (m_deinterlacing && m_deintfiltername.contains(kName))
            SetImageFilter(OMX_ImageFilterNone);
        return VideoOutput::SetupDeinterlace(interlaced, overridefilter);
    }

    if (m_deinterlacing == interlaced && deintfiltername == m_deintfiltername)
        return m_deinterlacing;

    m_deintfiltername = deintfiltername;
    m_deinterlacing = interlaced;

    // Remove non-openmax filters
    delete m_deintFiltMan, m_deintFiltMan = nullptr;
    delete m_deintFilter, m_deintFilter = nullptr;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ + " switching " +
        (interlaced ? "on" : "off") + " '" +  deintfiltername + "'");

    OMX_IMAGEFILTERTYPE type;
    if (!m_deinterlacing || m_deintfiltername.isEmpty())
        type = OMX_ImageFilterNone;
#ifdef USING_BROADCOM
    else if (m_deintfiltername.contains("advanced"))
        type = OMX_ImageFilterDeInterlaceAdvanced;
    else if (m_deintfiltername.contains("fast"))
        type = OMX_ImageFilterDeInterlaceFast;
    else if (m_deintfiltername.contains("linedouble"))
        type = OMX_ImageFilterDeInterlaceLineDouble;
#endif
    else
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ +  " Unknown type: '" +
            m_deintfiltername + "'");
#ifdef USING_BROADCOM
        type = OMX_ImageFilterDeInterlaceFast;
#else
        type = OMX_ImageFilterNone;
#endif
    }

    (void)SetImageFilter(type);

    return m_deinterlacing;
}

OMX_ERRORTYPE VideoOutputOMX::SetImageFilter(OMX_IMAGEFILTERTYPE type)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + __func__ + " " + Filter2String(type));

#ifdef USING_BROADCOM
    OMX_INDEXTYPE index = OMX_IndexConfigCommonImageFilterParameters;
    OMX_CONFIG_IMAGEFILTERPARAMSTYPE image_filter;
    OMX_DATA_INIT(image_filter);
    switch (type)
    {
      case OMX_ImageFilterDeInterlaceAdvanced:
        image_filter.nNumParams = 1;
        image_filter.nParams[0] = 3;
        break;

      case OMX_ImageFilterDeInterlaceFast:
      case OMX_ImageFilterDeInterlaceLineDouble:
      case OMX_ImageFilterNone:
        break;

      default:
        break;
    }

#else
    OMX_INDEXTYPE index = OMX_IndexConfigCommonImageFilter;
    OMX_CONFIG_IMAGEFILTERTYPE image_filter;
    OMX_DATA_INIT(image_filter);
#endif //def USING_BROADCOM

    if (!m_imagefx.IsValid())
        return (type != OMX_ImageFilterNone) ? OMX_ErrorInvalidState : OMX_ErrorNone;

    image_filter.nPortIndex = m_imagefx.Base() + 1;
    image_filter.eImageFilter = type;
    OMX_ERRORTYPE e = m_imagefx.SetConfig(index, &image_filter);
    if (e != OMX_ErrorNone)
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "SetConfig CommonImageFilter error %1")
            .arg(Error2String(e)));
    return e;
}

// virtual
void VideoOutputOMX::EmbedInWidget(const QRect &rect)
{
    if (!window.IsEmbedding())
        VideoOutput::EmbedInWidget(rect);
}

// virtual
void VideoOutputOMX::StopEmbedding(void)
{
    if (window.IsEmbedding())
        VideoOutput::StopEmbedding();
}

// virtual
void VideoOutputOMX::Zoom(ZoomDirection direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

// virtual
QRect VideoOutputOMX::GetPIPRect(
    PIPLocation location, MythPlayer *pipplayer, bool do_pixel_adj) const
{
    QRect r = VideoOutput::GetPIPRect(location, pipplayer, do_pixel_adj);

    const QRect display_video_rect     = window.GetDisplayVideoRect();
    const QRect video_rect             = window.GetVideoRect();
    const QRect display_visible_rect   = window.GetDisplayVisibleRect();

    // Transform PIP rect from DisplayVisibleRect to VideoRect
    qreal s = qreal(display_video_rect.width())  / display_visible_rect.width();
    s      /= qreal(display_video_rect.height()) / display_visible_rect.height();
    r = QTransform()
        .scale( (s * video_rect.width())  / display_video_rect.width(),
                (s * video_rect.height()) / display_video_rect.height() )
        .mapRect(r);

    r.setRect(r.x() & ~1, r.y() & ~1, r.width() & ~1, r.height() & ~1);
    return r;
}

void VideoOutputOMX::CreatePauseFrame(void)
{
    delete [] av_pause_frame.buf;
    av_pause_frame.buf = nullptr;

    VideoFrame *scratch = vbuffers.GetScratchFrame();

    init(&av_pause_frame, FMT_YV12, new unsigned char[scratch->size + 128],
         scratch->width, scratch->height, scratch->size);

    av_pause_frame.frameNumber = scratch->frameNumber;

    clear(&av_pause_frame);

    // Need two pause frames for OMX video_render.
    // 1st is held by EmptyBuffer until next frame is sent.
    // Need additional pause frame for advanced deinterlacer
    while (vbuffers.Size(kVideoBuffer_pause) < 3)
        vbuffers.Enqueue(kVideoBuffer_pause, vbuffers.Dequeue(kVideoBuffer_avail));
}

// pure virtual
void VideoOutputOMX::UpdatePauseFrame(int64_t &disp_timecode)
{
    if (!av_pause_frame.buf)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " - no buffers?");
        return;
    }

    // Try used frame first, then fall back to scratch frame.
    vbuffers.begin_lock(kVideoBuffer_used);

    VideoFrame *used_frame = (vbuffers.Size(kVideoBuffer_used) > 0) ?
                                vbuffers.Head(kVideoBuffer_used) : nullptr;
    if (used_frame)
        CopyFrame(&av_pause_frame, used_frame);

    vbuffers.end_lock();

    if (!used_frame)
    {
        used_frame = vbuffers.Tail(kVideoBuffer_pause);
        used_frame->frameNumber = framesPlayed - 1;
        CopyFrame(&av_pause_frame, used_frame);
    }

    // Suppress deinterlace while paused to prevent the jiggles.
    av_pause_frame.interlaced_frame = 0;
    av_pause_frame.top_field_first = 0;

    disp_timecode = av_pause_frame.disp_timecode;
}

/**
 * Draw OSD, apply filters and deinterlacing,
 */
// pure virtual
void VideoOutputOMX::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  const PIPMap &pipPlayers,
                                  FrameScanType scan)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    m_videoPaused = false;
    if (!frame)
    {
        // Rotate pause frames
        m_videoPaused = true;
        vbuffers.Enqueue(kVideoBuffer_pause, vbuffers.Dequeue(kVideoBuffer_pause));
        frame = vbuffers.GetScratchFrame();
        CopyFrame(frame, &av_pause_frame);
    }

    CropToDisplay(frame);

    if (filterList)
        filterList->ProcessFrame(frame);

    if (m_deinterlacing && m_deintFilter && m_deinterlaceBeforeOSD)
        m_deintFilter->ProcessFrame(frame, scan);

    ShowPIPs(frame, pipPlayers);
    if (osd && !window.IsEmbedding())
        DisplayOSD(frame, osd);

    if (m_deinterlacing && m_deintFilter && !m_deinterlaceBeforeOSD)
        m_deintFilter->ProcessFrame(frame, scan);
}

// tells show what frame to be show, do other last minute stuff
// pure virtual
void VideoOutputOMX::PrepareFrame(VideoFrame *buffer, FrameScanType /*scan*/, OSD */*osd*/)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    if (!buffer)
    {
        buffer = vbuffers.GetScratchFrame();
        vbuffers.SetLastShownFrameToScratch();
    }

    framesPlayed = buffer->frameNumber + 1;

    // PGB Set up a background window to prevent bleed through
    // of theme when playing a video smaller than the play area
    if (m_backgroundscreen == nullptr)
    {
        MythMainWindow *mainWindow = GetMythMainWindow();
        MythScreenStack *mainStack = mainWindow->GetMainStack();
        m_backgroundscreen = new MythScreenType(mainStack,"VideoBackground");

        if (XMLParseBase::CopyWindowFromBase("videobackground",
                                          m_backgroundscreen))
        {
            mainStack->AddScreen(m_backgroundscreen, false);
            GetMythUI()->AddCurrentLocation("Playback");
            if (mainWindow->GetPaintWindow())
                mainWindow->GetPaintWindow()->update();
            qApp->processEvents();
        }
    }

    SetVideoRect( (hasFullScreenOSD() && vsz_enabled && !IsEmbedding()) ?
            vsz_desired_display_rect : window.GetDisplayVideoRect(),
        window.GetVideoRect() );
}

// BLT the last prepared frame to the screen as quickly as possible.
// pure virtual
void VideoOutputOMX::Show(FrameScanType /*scan*/)
{
    if (IsErrored())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " IsErrored is true");
        return;
    }

    VideoFrame *frame = GetLastShownFrame();
    if (!frame)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " No LastShownFrame");
        return;
    }

    OMX_BUFFERHEADERTYPE *hdr = FRAME2HDR(frame);
    if (!hdr)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + __func__ + " Frame has no buffer header");
        return;
    }

    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    assert(hdr->pBuffer == frame->buf);

    if (hdr->nFilledLen)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + __func__ + " Frame in use");
        return; // Pending empty callback
    }

    assert(frame->codec == FMT_YV12);

    hdr->nFilledLen = frame->offsets[2] + (frame->offsets[1] >> 2);
    hdr->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
#ifdef OMX_BUFFERFLAG_INTERLACED
    if (frame->interlaced_frame)
        hdr->nFlags |= OMX_BUFFERFLAG_INTERLACED;
#endif
#ifdef OMX_BUFFERFLAG_TOP_FIELD_FIRST
    if (frame->top_field_first)
        hdr->nFlags |= OMX_BUFFERFLAG_TOP_FIELD_FIRST;
#endif
    OMXComponent &cmpnt = m_imagefx.IsValid() ? m_imagefx : m_render;
    // Paused - do not display anything unless softblend set
    if (m_videoPaused && GetOSDRenderer() != "softblend")
    {
        // fake out that the buffer was already emptied
        EmptyBufferDone(cmpnt, hdr);
        return;
    }
    OMX_ERRORTYPE e = OMX_EmptyThisBuffer(cmpnt.Handle(), hdr);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
            "OMX_EmptyThisBuffer error %1").arg(Error2String(e)) );
        hdr->nFlags = 0;
        hdr->nFilledLen = 0;
    }
}

// pure virtual
void VideoOutputOMX::MoveResizeWindow(QRect /*new_rect*/)
{
}

// virtual
bool VideoOutputOMX::hasFullScreenOSD(void) const
{
#ifdef OSD_EGL
    if (GetOSDRenderer() == "opengl"
        && m_context && m_osdpainter)
        return true;
    if (GetOSDRenderer() == "threaded"
        && m_glOsdThread && m_glOsdThread->isValid())
        return true;
#endif
    return VideoOutput::hasFullScreenOSD();
}

// virtual
MythPainter *VideoOutputOMX::GetOSDPainter(void)
{
#ifdef OSD_EGL
    if (GetOSDRenderer() == "opengl")
        return m_osdpainter;
    if (GetOSDRenderer() == "threaded")
        return m_threaded_osdpainter;
#endif
    return VideoOutput::GetOSDPainter();
}

// virtual
bool VideoOutputOMX::DisplayOSD(VideoFrame *frame, OSD *osd)
{
#ifdef OSD_EGL
    if (GetOSDRenderer() == "opengl"
        && m_context && m_osdpainter)
    {
        m_context->makeCurrent();
        m_context->BindFramebuffer(0);

        QRect bounds = GetTotalOSDBounds();
        bool redraw = false;
        if (m_visual)
        {
            m_visual->Draw(bounds, m_osdpainter, nullptr);
            redraw = true;
        }

        if (osd)
            redraw |= osd->DrawDirect(m_osdpainter, bounds.size(), redraw);

        if (redraw)
        {
            m_context->swapBuffers();
            m_context->SetBackground(0, 0, 0, 0);
            m_context->ClearFramebuffer();
        }

        m_context->doneCurrent();
        return true;
    }

    if (GetOSDRenderer() == "threaded"
        && m_glOsdThread && m_glOsdThread->isValid())
    {
        if (!m_threaded_osdpainter)
        {
            m_threaded_osdpainter = new MythQImagePainter();
            if (!m_threaded_osdpainter)
                return false;
        }
        QSize osd_size = GetTotalOSDBounds().size();
        if (m_glOsdThread->m_osdImage && (m_glOsdThread->m_osdImage->size() != osd_size))
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("OSD size changed."));
            m_glOsdThread->m_osdImage->DecrRef();
            m_glOsdThread->m_osdImage = nullptr;
        }
        if (!m_glOsdThread->m_osdImage)
        {
            m_glOsdThread->m_osdImage = m_threaded_osdpainter->GetFormatImage();
            if (m_glOsdThread->m_osdImage)
            {
                QImage blank = QImage(osd_size,
                    QImage::Format_ARGB32_Premultiplied);
                m_glOsdThread->m_osdImage->Assign(blank);
                m_threaded_osdpainter->Clear(m_glOsdThread->m_osdImage,
                                   QRegion(QRect(QPoint(0,0), osd_size)));
                LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created OSD QImage."));
            }
            else {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Unable to create OSD QImage."));
                return false;
            }
        }
        if (m_visual)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Visualiser not supported here");
            m_visual->Draw(QRect(), nullptr, nullptr);
        }

        if (m_glOsdThread->m_lock.tryLock(0)) {
            QRegion dirty   = QRegion();
            QRegion visible = osd->Draw(m_threaded_osdpainter,
                m_glOsdThread->m_osdImage, osd_size, dirty);
            m_changed = m_changed || !dirty.isEmpty();
            m_glOsdThread->m_osdImage->SetChanged(m_changed);
            m_glOsdThread->m_bounds = GetTotalOSDBounds();
            m_glOsdThread->m_lock.unlock();
            if (m_changed) {
                m_glOsdThread->m_wait.wakeAll();
                m_changed = false;
            }
        }
        return true;
    }
#endif
    return VideoOutput::DisplayOSD(frame, osd);
}

// virtual
void VideoOutputOMX::DrawUnusedRects(bool sync)
{
    (void)sync;
}

// virtual
bool VideoOutputOMX::CanVisualise(AudioPlayer *audio, MythRender *render)
{
#ifdef OSD_EGL
    return VideoOutput::CanVisualise(audio, m_context ? m_context : render);
#else
    return VideoOutput::CanVisualise(audio, render);
#endif
}

// virtual
bool VideoOutputOMX::SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                        const QString &name)
{
#ifdef OSD_EGL
    return VideoOutput::SetupVisualisation(audio,
        m_context ? m_context : render, name);
#else
    return VideoOutput::SetupVisualisation(audio, render, name);
#endif
}

// virtual
QStringList VideoOutputOMX::GetVisualiserList(void)
{
#ifdef OSD_EGL
    return m_context ?
        VideoVisual::GetVisualiserList(m_context->Type()) :
        VideoOutput::GetVisualiserList();
#else
    return VideoOutput::GetVisualiserList();
#endif
}

bool VideoOutputOMX::CreateBuffers(
    const QSize &video_dim_buf,     // video buffer size
    const QSize &video_dim_disp)    // video display size
{
    OMXComponent &cmpnt = m_imagefx.IsValid() ? m_imagefx : m_render;

    // Set the video dimensions
    OMX_S32 nStride = ROUNDUP(video_dim_buf.width(), 32);
    OMX_U32 nSliceHeight = ROUNDUP(video_dim_buf.height(), 16);
    OMX_PARAM_PORTDEFINITIONTYPE def = cmpnt.PortDef();
    assert(vbuffers.Size() >= def.nBufferCountMin);
    def.nBufferCountActual = vbuffers.Size();
    def.nBufferSize = 0;
    def.bBuffersContiguous = OMX_FALSE;
    def.nBufferAlignment = sizeof(int);
    def.eDomain = OMX_PortDomainVideo;
    def.format.video.cMIMEType = nullptr;
    def.format.video.pNativeRender = nullptr;
    def.format.video.nFrameWidth = video_dim_disp.width();
    def.format.video.nFrameHeight = video_dim_disp.height();
    def.format.video.nStride = nStride;
    def.format.video.nSliceHeight = nSliceHeight;
    def.format.video.nBitrate = 0;
    def.format.video.xFramerate = 0;
    def.format.video.bFlagErrorConcealment = OMX_FALSE;
    def.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    def.format.video.pNativeWindow = nullptr;
    OMX_ERRORTYPE e = cmpnt.SetParameter(OMX_IndexParamPortDefinition, &def);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "SetParameter PortDefinition error %1")
            .arg(Error2String(e)));
        return false;
    }

    // Update port status with revised buffer size etc
    if (OMX_ErrorNone != cmpnt.GetPortDef())
        return false;

    // Setup image_fx output
    if (m_imagefx.IsValid())
    {
        def = m_imagefx.PortDef(1);
        def.nBufferCountActual = 1 + std::max(def.nBufferCountMin,
                                            m_render.PortDef().nBufferCountMin);
        def.nBufferSize = 0;
        assert(def.eDomain == OMX_PortDomainImage);
        def.format.image.nFrameWidth = video_dim_disp.width();
        def.format.image.nFrameHeight = video_dim_disp.height();
        def.format.image.nStride = nStride;
        def.format.image.nSliceHeight = nSliceHeight;
        e = m_imagefx.SetParameter(OMX_IndexParamPortDefinition, &def);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                    "SetParameter image_fx output PortDefinition error %1")
                .arg(Error2String(e)));
            return false;
        }

        if (OMX_ErrorNone != m_imagefx.GetPortDef(1))
            return false;
    }

    int pitches[3], offsets[3];
    pitches[0] = nStride;
    pitches[1] = pitches[2] = nStride >> 1;
    offsets[0] = 0;
    offsets[1] = nStride * nSliceHeight;
    offsets[2] = offsets[1] + (offsets[1] >> 2);
    uint nBufferSize = buffersize(FMT_YV12, nStride, nSliceHeight);

    std::vector<unsigned char*> bufs;
    std::vector<YUVInfo> yuvinfo;
    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        yuvinfo.emplace_back(video_dim_disp.width(), video_dim_disp.height(),
                             nBufferSize, pitches, offsets);
        void *buf = av_malloc(nBufferSize + 64);
        if (!buf)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Out of memory");
            errorState = kError_Unknown;
            return false;
        }
        m_bufs.push_back(buf);
        bufs.push_back((unsigned char *)buf);
    }
    if (!vbuffers.CreateBuffers(FMT_YV12, video_dim_disp.width(),
                                video_dim_disp.height(), bufs, yuvinfo))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "CreateBuffers failed");
        errorState = kError_Unknown;
        return false;
    }

    CreatePauseFrame();

    return true;
}

void VideoOutputOMX::DeleteBuffers()
{
    delete [] av_pause_frame.buf;
    init(&av_pause_frame, FMT_YV12, nullptr, 0, 0, 0);

    vbuffers.DeleteBuffers();

    while (!m_bufs.empty())
    {
        av_free(m_bufs.back());
        m_bufs.pop_back();
    }
}

bool VideoOutputOMX::Start()
{
    if (m_imagefx.IsValid())
    {
        // Setup a tunnel between image_fx & video_render
        OMX_ERRORTYPE e;
        e = OMX_SetupTunnel(m_imagefx.Handle(), m_imagefx.Base() + 1,
            m_render.Handle(), m_render.Base());
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString("OMX_SetupTunnel error %1")
                .arg(Error2String(e)));
            return false;
        }

        // Disable image_fx input port.
        // A disabled port is not populated with buffers on a transition to IDLE
        if (m_imagefx.PortDisable(0, 500) != OMX_ErrorNone)
            return false;

        // Goto OMX_StateIdle
        if (m_imagefx.SetState(OMX_StateIdle, 500) != OMX_ErrorNone)
            return false;

        if (m_render.SetState(OMX_StateIdle, 500) != OMX_ErrorNone)
            return false;

        // Enable image_fx input port and populate buffers
        OMXComponentCB<VideoOutputOMX> cb(this, &VideoOutputOMX::UseBuffersCB);
        if (m_imagefx.PortEnable(0, 500, &cb) != OMX_ErrorNone)
            return false;

        // Goto OMX_StateExecuting
        if (m_imagefx.SetState(OMX_StateExecuting, 500) != OMX_ErrorNone)
            return false;
    }
    else
    {
        if (m_render.PortDisable(0, 500) != OMX_ErrorNone)
            return false;

        if (m_render.SetState(OMX_StateIdle, 500) != OMX_ErrorNone)
            return false;

        OMXComponentCB<VideoOutputOMX> cb(this, &VideoOutputOMX::UseBuffersCB);
        if (m_render.PortEnable(0, 500, &cb) != OMX_ErrorNone)
            return false;
    }

    return m_render.SetState(OMX_StateExecuting, 500) == OMX_ErrorNone;
}

bool VideoOutputOMX::SetVideoRect(const QRect &d_rect, const QRect &vid_rect)
{
    // Translate display rect to screen coordinates
    QRect disp_rect = d_rect.translated(GetMythMainWindow()->geometry().topLeft());

    if (disp_rect == m_disp_rect && vid_rect == m_vid_rect)
        return true;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + __func__ + QString(
            " display=%1,%2,%3x%4 (%9) video=%5,%6,%7x%8 (%10)")
        .arg(disp_rect.x()).arg(disp_rect.y())
        .arg(disp_rect.width()).arg(disp_rect.height())
        .arg(vid_rect.x()).arg(vid_rect.y())
        .arg(vid_rect.width()).arg(vid_rect.height())
        .arg(window.GetDisplayAspect()).arg(window.GetOverridenVideoAspect()) );

#ifdef USING_BROADCOM
    OMX_CONFIG_DISPLAYREGIONTYPE dregion;
    OMX_DATA_INIT(dregion);
    dregion.nPortIndex = m_render.Base();
    dregion.set = OMX_DISPLAYSETTYPE( OMX_DISPLAY_SET_FULLSCREEN |
                    OMX_DISPLAY_SET_TRANSFORM |
                    OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_SRC_RECT |
                    OMX_DISPLAY_SET_MODE |
                    OMX_DISPLAY_SET_PIXEL |
                    OMX_DISPLAY_SET_NOASPECT |
                    OMX_DISPLAY_SET_LAYER );
    dregion.fullscreen = OMX_FALSE;
    dregion.transform = OMX_DISPLAY_ROT0;
    dregion.dest_rect.x_offset = disp_rect.x();
    dregion.dest_rect.y_offset = disp_rect.y();
    dregion.dest_rect.width    = disp_rect.width();
    dregion.dest_rect.height   = disp_rect.height();
    dregion.src_rect.x_offset = vid_rect.x();
    dregion.src_rect.y_offset = vid_rect.y();
    dregion.src_rect.width    = vid_rect.width();
    dregion.src_rect.height   = vid_rect.height();
    dregion.noaspect = OMX_TRUE;
    dregion.mode = OMX_DISPLAY_MODE_FILL; //OMX_DISPLAY_MODE_LETTERBOX;
    dregion.pixel_x = dregion.pixel_y = 0;
    // NB Qt EGLFS uses layer 1 - See createDispmanxLayer() in
    // mkspecs/devices/linux-rasp-pi-g++/qeglfshooks_pi.cpp.
    // Therefore to view video must select layer >= 1
    dregion.layer = 2;

    OMX_ERRORTYPE e = m_render.SetConfig(OMX_IndexConfigDisplayRegion, &dregion);
    if (e != OMX_ErrorNone)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "SetConfig DisplayRegion error %1")
            .arg(Error2String(e)));
        return false;
    }
#endif // USING_BROADCOM

#ifdef OSD_EGL
    if (m_context
        || ( m_glOsdThread && m_glOsdThread->isValid() ) )
    {
        QRect displayRect = window.GetDisplayVisibleRect();
        QRect mainRect = GetMythMainWindow()->geometry();
        uint32_t maxwidth = 0, maxheight = 0;
        graphics_get_display_size(DISPMANX_ID_MAIN_LCD, &maxwidth, &maxheight);
        QRect glRect(mainRect.x(),        // left
            maxheight-mainRect.bottom(),  // top
            0,0);
        glRect.setRight(mainRect.right());
        glRect.setBottom(glRect.top()+mainRect.height());
        if (m_context)
        {
            m_context->SetViewPort(displayRect);
            m_context->SetViewPort(glRect,true);
        }
        if (m_glOsdThread && m_glOsdThread->isValid()) {
            m_glOsdThread->setRects(displayRect,glRect);
        }
    }
#endif

    m_disp_rect = disp_rect;
    m_vid_rect = vid_rect;

    return true;
}

// virtual
OMX_ERRORTYPE VideoOutputOMX::EmptyBufferDone(
    OMXComponent& /*cmpnt*/, OMX_BUFFERHEADERTYPE *hdr)
{
    assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
    assert(hdr->nVersion.nVersion == OMX_VERSION);
    hdr->nFilledLen = 0;
    hdr->nFlags = 0;
    return OMX_ErrorNone;
}

// Shutdown OMX_StateIdle -> OMX_StateLoaded callback
// virtual
void VideoOutputOMX::ReleaseBuffers(OMXComponent &cmpnt)
{
    OMXComponent &c = m_imagefx.IsValid() ? m_imagefx : m_render;
    if(cmpnt.Handle() == c.Handle())
        FreeBuffersCB();
}

// Use frame buffers
OMX_ERRORTYPE VideoOutputOMX::UseBuffersCB()
{
    OMXComponent &cmpnt = m_imagefx.IsValid() ? m_imagefx : m_render;
    const OMX_PARAM_PORTDEFINITIONTYPE &def = cmpnt.PortDef();
    assert(vbuffers.Size() >= def.nBufferCountActual);

    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Use %1 of %2 byte buffer(s)")
        .arg(def.nBufferCountActual).arg(def.nBufferSize));

    OMX_ERRORTYPE e = OMX_ErrorNone;

    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        VideoFrame *vf = vbuffers.At(i);
        assert(vf);
        assert(OMX_U32(vf->size) >= def.nBufferSize);
        assert(vf->buf);
        if (i >= def.nBufferCountActual)
            continue;

        OMX_BUFFERHEADERTYPE *hdr;
        e = OMX_UseBuffer(cmpnt.Handle(), &hdr, def.nPortIndex, vf,
                            def.nBufferSize, vf->buf);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                "OMX_UseBuffer error %1").arg(Error2String(e)) );
            break;
        }
        if (hdr->nSize != sizeof(OMX_BUFFERHEADERTYPE))
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer header mismatch");
            OMX_FreeBuffer(cmpnt.Handle(), def.nPortIndex, hdr);
            e = OMX_ErrorVersionMismatch;
            break;
        }
        if (hdr->nVersion.nVersion != OMX_VERSION)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + "OMX_UseBuffer version mismatch");
            OMX_FreeBuffer(cmpnt.Handle(), def.nPortIndex, hdr);
            e = OMX_ErrorVersionMismatch;
            break;
        }
        assert(vf == HDR2FRAME(hdr));
        FRAMESETHDR(vf, hdr);
        hdr->nFilledLen = 0;
        hdr->nOffset = 0;
    }

    return e;
}

// Free all OMX buffers
// OMX_CommandPortDisable callback
OMX_ERRORTYPE VideoOutputOMX::FreeBuffersCB()
{
    OMXComponent &cmpnt = m_imagefx.IsValid() ? m_imagefx : m_render;
#ifndef NDEBUG
    const OMX_PARAM_PORTDEFINITIONTYPE &def = cmpnt.PortDef();
    assert(vbuffers.Size() >= def.nBufferCountActual);
#endif

    for (uint i = 0; i < vbuffers.Size(); ++i)
    {
        VideoFrame *vf = vbuffers.At(i);
        assert(vf);
        OMX_BUFFERHEADERTYPE *hdr = FRAME2HDR(vf);
        if (!hdr)
            continue;
        assert(hdr->nSize == sizeof(OMX_BUFFERHEADERTYPE));
        assert(hdr->nVersion.nVersion == OMX_VERSION);
        assert(vf == HDR2FRAME(hdr));
        FRAMESETHDRNONE(vf);

        OMX_ERRORTYPE e = OMX_FreeBuffer(cmpnt.Handle(), cmpnt.Base(), hdr);
        if (e != OMX_ErrorNone)
        {
            LOG(VB_PLAYBACK, LOG_ERR, LOC + QString(
                    "OMX_FreeBuffer 0x%1 error %2")
                .arg(quintptr(hdr),0,16).arg(Error2String(e)));
        }
    }
    return OMX_ErrorNone;
}

#ifdef OSD_EGL
MythRenderEGL::MythRenderEGL() :
    MythRenderOpenGL2ES(MythRenderFormat()),
    m_display(EGL_NO_DISPLAY),
    m_context(EGL_NO_CONTEXT),
    m_window(nullptr),
    m_surface(EGL_NO_SURFACE)
{
    // Disable flush to get performance improvement
    m_flushEnabled = false;
    // get an EGL display connection
    m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_display == EGL_NO_DISPLAY)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglGetDisplay => EGL_NO_DISPLAY");
        return;
    }

    // initialize the EGL display connection
    EGLint major, minor;
    EGLBoolean b = eglInitialize(m_display, &major, &minor);
    if (!b)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglInitialize failed");
        return;
    }
    LOG(VB_PLAYBACK, LOG_INFO, QString("EGL runtime version %1.%2")
        .arg(major).arg(minor));

    // get an appropriate EGL frame buffer configuration
    static EGLint const attribute_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, // ES2 required for GLSL
        EGL_NONE
    };
    EGLConfig config;
    EGLint num_config;
    b = eglChooseConfig(m_display, attribute_list, &config, 1, &num_config);
    if (!b)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglChooseConfig failed");
        return;
    }

    // create an EGL rendering context
    static EGLint const ctx_attribute_list[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, // ES2 required for GLSL
        EGL_NONE
    };
    m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, ctx_attribute_list);
    if (m_context == EGL_NO_CONTEXT)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglCreateContext failed");
        return;
    }

#ifdef USE_OPENGL_QT5
    QVariant v;
    v.setValue(QEGLNativeContext(m_context, m_display));
    setNativeHandle(v);
#endif

    m_window = createNativeWindow();

    m_surface = eglCreateWindowSurface(m_display, config, m_window, nullptr);
    if (m_context == EGL_NO_SURFACE)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglCreateWindowSurface failed");
        return;
    }

    // connect the context to the surface
    b = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
    if (!b)
    {
        LOG(VB_GENERAL, LOG_ERR, "eglMakeCurrent failed");
        return;
    }

    // clear the color buffer
    glClearColor(0, 0, 0, 0); // RGBA
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    b = eglSwapBuffers(m_display, m_surface);
    if (!b)
        LOG(VB_GENERAL, LOG_ERR, "eglSwapBuffers failed");

    b = eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (!b)
        LOG(VB_GENERAL, LOG_ERR, "eglMakeCurrent EGL_NO_SURFACE failed");

#ifndef USE_OPENGL_QT5
    setValid(true);
#endif
}

MythRenderEGL::~MythRenderEGL()
{
#ifndef USE_OPENGL_QT5
    setValid(false);
#endif

    if (m_display == EGL_NO_DISPLAY)
        return;

    EGLBoolean b;
    if (m_context != EGL_NO_CONTEXT)
    {
        b = eglMakeCurrent(m_display, m_surface, m_surface, m_context);
        assert(b == EGL_TRUE);
    }

    if (m_surface != EGL_NO_SURFACE)
    {
        b = eglDestroySurface(m_display, m_surface);
        assert(b == EGL_TRUE);
        m_surface = EGL_NO_SURFACE;
    }

    destroyNativeWindow();

    if (m_context != EGL_NO_CONTEXT)
    {
        b = eglDestroyContext(m_display, m_context);
        assert(b == EGL_TRUE);
        m_context = EGL_NO_CONTEXT;
    }

#if 0 // This causes Qt to throw a segv
    b = eglTerminate(m_display);
    assert(b == EGL_TRUE);
#endif
    m_display = EGL_NO_DISPLAY;
    DeleteOpenGLResources();

#ifdef NDEBUG
    Q_UNUSED(b);
#endif
}

EGLNativeWindowType MythRenderEGL::createNativeWindow()
{
#ifdef USING_BROADCOM
    uint32_t width = 0, height = 0;
    int32_t ret = graphics_get_display_size(DISPMANX_ID_MAIN_LCD, &width, &height);
    assert(ret >= 0);

    m_dispman_display = vc_dispmanx_display_open(DISPMANX_ID_MAIN_LCD);
    assert(m_dispman_display != DISPMANX_NO_HANDLE);

    DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
    assert(update != DISPMANX_NO_HANDLE);

    VC_RECT_T dst_rect;
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = width;
    dst_rect.height = height;

    VC_RECT_T src_rect;
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = width << 16;
    src_rect.height = height << 16;

    VC_DISPMANX_ALPHA_T alpha = {
        DISPMANX_FLAGS_ALPHA_T(
            DISPMANX_FLAGS_ALPHA_FROM_SOURCE |
            (0&DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS) |
            (0&DISPMANX_FLAGS_ALPHA_FIXED_NON_ZERO) |
            (0&DISPMANX_FLAGS_ALPHA_FIXED_EXCEED_0X07) |
            (~0&DISPMANX_FLAGS_ALPHA_PREMULT) |
            (0&DISPMANX_FLAGS_ALPHA_MIX)
        ),  // flags
        0,  // opacity(alpha) 0->255
        0   // DISPMANX_RESOURCE_HANDLE_T mask
    };

    DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(
        update, m_dispman_display, 3/*layer*/, &dst_rect,
        DISPMANX_RESOURCE_HANDLE_T(0) /*src*/, &src_rect,
        DISPMANX_PROTECTION_NONE, &alpha, nullptr /*clamp*/, DISPMANX_NO_ROTATE);
    assert(dispman_element != DISPMANX_NO_HANDLE);

    gNativewindow.element = dispman_element;
    gNativewindow.width = width;
    gNativewindow.height = height;

    vc_dispmanx_update_submit_sync(update);
    return &gNativewindow;
#ifdef NDEBUG
    Q_UNUSED(ret);
#endif
#else
    return 0;
#endif
}

void MythRenderEGL::destroyNativeWindow()
{
#ifdef USING_BROADCOM
    if (m_dispman_display != DISPMANX_NO_HANDLE)
    {
        int ret;
        if (DISPMANX_NO_HANDLE != gNativewindow.element)
        {
            DISPMANX_UPDATE_HANDLE_T update = vc_dispmanx_update_start(0);
            assert(update != DISPMANX_NO_HANDLE);

            ret = vc_dispmanx_element_remove(update, gNativewindow.element);
            assert(ret >= 0);

            ret = vc_dispmanx_update_submit_sync(update);
            assert(ret >= 0);

            gNativewindow.element = DISPMANX_NO_HANDLE;
        }

        ret = vc_dispmanx_display_close(m_dispman_display);
        assert(ret >= 0);
        m_dispman_display = DISPMANX_NO_HANDLE;
#ifdef NDEBUG
        Q_UNUSED(ret);
#endif
    }
#endif //def USING_BROADCOM
}

// virtual
void MythRenderEGL::makeCurrent()
{
    assert(m_lock_level >= 0);
    if (++m_lock_level == 1)
        eglMakeCurrent(m_display, m_surface, m_surface, m_context);
}

// virtual
void MythRenderEGL::doneCurrent()
{
    assert(m_lock_level > 0);
    if (--m_lock_level == 0)
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

// virtual
#ifdef USE_OPENGL_QT5
void MythRenderEGL::swapBuffers()
#else
void MythRenderEGL::swapBuffers() const
#endif
{
    eglSwapBuffers(m_display, m_surface);
}
#endif //def OSD_EGL
// EOF
