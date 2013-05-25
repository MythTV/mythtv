#include <cmath>
#include <cstdlib>

#include <QDesktopWidget>

#include "osd.h"
#include "mythplayer.h"
#include "videodisplayprofile.h"
#include "decoderbase.h"

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythxdisplay.h"
#include "mythpainter_yuva.h"
#include "util-osd.h"

#ifdef USING_XV
#include "videoout_xv.h"
#endif

#ifdef USING_MINGW
#include "videoout_d3d.h"
#endif

#ifdef USING_QUARTZ_VIDEO
#include "videoout_quartz.h"
#endif

#ifdef USING_OPENGL_VIDEO
#include "videoout_opengl.h"
#endif

#ifdef USING_VDPAU
#include "videoout_vdpau.h"
#include "videoout_nullvdpau.h"
#endif

#ifdef USING_VAAPI
#include "videoout_nullvaapi.h"
#endif
#ifdef USING_GLVAAPI
#include "videoout_openglvaapi.h"
#endif
#include "videoout_null.h"
#include "dithertable.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

#include "filtermanager.h"

#include "videooutbase.h"

#define LOC QString("VideoOutput: ")

static QString to_comma_list(const QStringList &list);

void VideoOutput::GetRenderOptions(render_opts &opts)
{
    QStringList cpudeints;
    cpudeints += "onefield";
    cpudeints += "linearblend";
    cpudeints += "kerneldeint";
    cpudeints += "kerneldoubleprocessdeint";
    cpudeints += "greedyhdeint";
    cpudeints += "greedyhdoubleprocessdeint";
    cpudeints += "yadifdeint";
    cpudeints += "yadifdoubleprocessdeint";
    cpudeints += "fieldorderdoubleprocessdeint";
    cpudeints += "none";

    VideoOutputNull::GetRenderOptions(opts, cpudeints);

#ifdef USING_MINGW
    VideoOutputD3D::GetRenderOptions(opts, cpudeints);
#endif

#ifdef USING_XV
    VideoOutputXv::GetRenderOptions(opts, cpudeints);
#endif // USING_XV

#ifdef USING_QUARTZ_VIDEO
    VideoOutputQuartz::GetRenderOptions(opts, cpudeints);
#endif // USING_QUARTZ_VIDEO

#ifdef USING_OPENGL_VIDEO
    VideoOutputOpenGL::GetRenderOptions(opts, cpudeints);
#endif // USING_OPENGL_VIDEO

#ifdef USING_VDPAU
    VideoOutputVDPAU::GetRenderOptions(opts);
    VideoOutputNullVDPAU::GetRenderOptions(opts);
#endif // USING_VDPAU

#ifdef USING_VAAPI
    VideoOutputNullVAAPI::GetRenderOptions(opts);
#endif // USING_VAAPI
#ifdef USING_GLVAAPI
    VideoOutputOpenGLVAAPI::GetRenderOptions(opts);
#endif // USING_GLVAAPI
}

/**
 * \brief  Depending on compile-time configure settings and run-time
 *         renderer settings, create a relevant VideoOutput subclass.
 * \return instance of VideoOutput if successful, NULL otherwise.
 */
VideoOutput *VideoOutput::Create(
    const QString &decoder, MythCodecID  codec_id,     void *codec_priv,
    PIPState pipState,      const QSize &video_dim_buf,
    const QSize &video_dim_disp, float video_aspect,
    QWidget *parentwidget,  const QRect &embed_rect, float video_prate,
    uint playerFlags)
{
    (void) codec_priv;
    QStringList renderers;
#ifdef USING_XV
    QStringList xvlist;
#endif
#ifdef USING_QUARTZ_VIDEO
    QStringList osxlist;
#endif

    // select the best available output
    if (playerFlags & kVideoIsNull)
    {
        // plain null output
        renderers += "null";

        if (playerFlags & kDecodeAllowGPU)
        {
#ifdef USING_VDPAU
            renderers += VideoOutputNullVDPAU::GetAllowedRenderers(codec_id);
#endif // USING_VDPAU
#ifdef USING_VAAPI
            renderers += VideoOutputNullVAAPI::GetAllowedRenderers(codec_id);
#endif
        }
    }
    else
    {
#ifdef USING_MINGW
        renderers += VideoOutputD3D::
            GetAllowedRenderers(codec_id, video_dim_disp);
#endif

#ifdef USING_XV
        xvlist = VideoOutputXv::
            GetAllowedRenderers(codec_id, video_dim_disp);
        renderers += xvlist;
#endif // USING_XV

#ifdef USING_QUARTZ_VIDEO
        osxlist = VideoOutputQuartz::
            GetAllowedRenderers(codec_id, video_dim_disp);
        renderers += osxlist;
#endif // Q_OS_MACX

#ifdef USING_OPENGL_VIDEO
        renderers += VideoOutputOpenGL::
            GetAllowedRenderers(codec_id, video_dim_disp);
#endif // USING_OPENGL_VIDEO

#ifdef USING_VDPAU
        renderers += VideoOutputVDPAU::
            GetAllowedRenderers(codec_id, video_dim_disp);
#endif // USING_VDPAU

#ifdef USING_GLVAAPI
        renderers += VideoOutputOpenGLVAAPI::
            GetAllowedRenderers(codec_id, video_dim_disp);
#endif // USING_GLVAAPI
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Allowed renderers: " +
            to_comma_list(renderers));

    renderers = VideoDisplayProfile::GetFilteredRenderers(decoder, renderers);

    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Allowed renderers (filt: " + decoder +
            "): " + to_comma_list(renderers));

    QString renderer = QString::null;
    if (renderers.size() > 0)
    {
        VideoDisplayProfile vprof;
        vprof.SetInput(video_dim_disp);

        QString tmp = vprof.GetVideoRenderer();
        if (vprof.IsDecoderCompatible(decoder) && renderers.contains(tmp))
        {
            renderer = tmp;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preferred renderer: " + renderer);
        }
    }

    if (renderer.isEmpty())
        renderer = VideoDisplayProfile::GetBestVideoRenderer(renderers);

    while (!renderers.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC +
            QString("Trying video renderer: '%1'").arg(renderer));
        int index = renderers.indexOf(renderer);
        if (index >= 0)
            renderers.removeAt(index);
        else
            break;

        VideoOutput *vo = NULL;

        /* these cases are mutually exlusive */
        if (renderer == "null")
            vo = new VideoOutputNull();

#ifdef USING_MINGW
        else if (renderer == "direct3d")
            vo = new VideoOutputD3D();
#endif // USING_MINGW

#ifdef USING_QUARTZ_VIDEO
        else if (osxlist.contains(renderer))
            vo = new VideoOutputQuartz();
#endif // Q_OS_MACX

#ifdef USING_OPENGL_VIDEO
        else if (renderer.contains("opengl") && (renderer != "openglvaapi"))
            vo = new VideoOutputOpenGL(renderer);
#endif // USING_OPENGL_VIDEO

#ifdef USING_VDPAU
        else if (renderer == "vdpau")
            vo = new VideoOutputVDPAU();
        else if (renderer == "nullvdpau")
            vo = new VideoOutputNullVDPAU();
#endif // USING_VDPAU

#ifdef USING_VAAPI
        else if (renderer == "nullvaapi")
            vo = new VideoOutputNullVAAPI();
#endif // USING_VAAPI

#ifdef USING_GLVAAPI
        else if (renderer == "openglvaapi")
            vo = new VideoOutputOpenGLVAAPI();
#endif // USING_GLVAAPI

#ifdef USING_XV
        else if (xvlist.contains(renderer))
            vo = new VideoOutputXv();
#endif // USING_XV

        if (vo && !(playerFlags & kVideoIsNull))
        {
            // ensure we have a window to display into
            QWidget *widget = parentwidget;
            MythMainWindow *window = GetMythMainWindow();
            if (!widget && window)
                widget = window->findChild<QWidget*>("video playback window");

            if (!widget)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No window for video output.");
                delete vo;
                vo = NULL;
                return NULL;
            }

            if (!widget->winId())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No window for video output.");
                delete vo;
                vo = NULL;
                return NULL;
            }

            // determine the display rectangle
            QRect display_rect = QRect(0, 0, widget->width(), widget->height());
            if (pipState == kPIPStandAlone)
                display_rect = embed_rect;

            vo->SetPIPState(pipState);
            vo->SetVideoFrameRate(video_prate);
            if (vo->Init(
                    video_dim_buf, video_dim_disp, video_aspect,
                    widget->winId(), display_rect, codec_id))
            {
                vo->SetVideoScalingAllowed(true);
                return vo;
            }

            delete vo;
            vo = NULL;
        }
        else if (vo && (playerFlags & kVideoIsNull))
        {
            if (vo->Init(video_dim_buf, video_dim_disp,
                         video_aspect, 0, QRect(), codec_id))
            {
                return vo;
            }

            delete vo;
            vo = NULL;
        }

        renderer = VideoDisplayProfile::GetBestVideoRenderer(renderers);
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "Not compiled with any useable video output method.");

    return NULL;
}

/**
 * \class VideoOutput
 * \brief This class serves as the base class for all video output methods.
 *
 * The basic use is:
 * \code
 * VideoOutputType type = kVideoOutput_Default;
 * vo = VideoOutput::InitVideoOut(type);
 * vo->Init(width, height, aspect ...);
 *
 * // Then create two threads.
 * // In the decoding thread
 * while (decoding)
 * {
 *     if (vo->WaitForAvailable(1000)
 *     {
 *         frame = vo->GetNextFreeFrame(); // remove frame from "available"
 *         av_lib_process(frame);   // do something to fill it.
 *         // call DrawSlice()      // if you need piecemeal processing
 *                                  // by VideoOutput use DrawSlice
 *         vo->ReleaseFrame(frame); // enqueues frame in "used" queue
 *     }
 * }
 *
 * // In the displaying thread
 * while (playing)
 * {
 *     if (vo->EnoughPrebufferedFrames())
 *     {
 *         // Sets "Last Shown Frame" to head of "used" queue
 *         vo->StartDisplayingFrame();
 *         // Get pointer to "Last Shown Frame"
 *         frame = vo->GetLastShownFrame();
 *         // add OSD, do any filtering, etc.
 *         vo->ProcessFrame(frame, osd, filters, pict-in-pict);
 *         // tells show what frame to be show, do other last minute stuff
 *         vo->PrepareFrame(frame, scan);
 *         // here you wait until it's time to show the frame
 *         // Show blits the last prepared frame to the screen
 *         // as quickly as possible.
 *         vo->Show(scan);
 *         // remove frame from the head of "used",
 *         // vo must get it into "available" eventually.
 *         vo->DoneDisplayingFrame();
 *     }
 * }
 * delete vo;
 * \endcode
 *
 *  Note: Show() may be called multiple times between PrepareFrame() and
 *        DoneDisplayingFrame(). But if a frame is ever removed from
 *        available via GetNextFreeFrame(), you must either call
 *        DoneDisplayFrame() or call DiscardFrame(VideoFrame*) on it.
 *
 *  Note: ProcessFrame() may be called multiple times on a frame, to
 *        update an OSD for example.
 *
 *  The VideoBuffers class handles the buffer tracking,
 *  see it for more details on the states a buffer can
 *  take before it becomes available for reuse.
 *
 * \see VideoBuffers, MythPlayer
 */

/**
 * \fn VideoOutput::VideoOutput()
 * \brief This constructor for VideoOutput must be followed by an
 *        Init(int,int,float,WId,int,int,int,int,WId) call.
 */
VideoOutput::VideoOutput() :
    // DB Settings
    db_display_dim(0,0),
    db_aspectoverride(kAspect_Off), db_adjustfill(kAdjustFill_Off),
    db_letterbox_colour(kLetterBoxColour_Black),
    db_deint_filtername(QString::null),

    // Video parameters
    video_codec_id(kCodec_NONE),        db_vdisp_profile(NULL),

    // Picture-in-Picture stuff
    pip_desired_display_size(160,128),  pip_display_size(0,0),
    pip_video_size(0,0),
    pip_tmp_buf(NULL),                  pip_tmp_buf2(NULL),
    pip_scaling_context(NULL),

    // Video resizing (for ITV)
    vsz_enabled(false),
    vsz_desired_display_rect(0,0,0,0),  vsz_display_size(0,0),
    vsz_video_size(0,0),
    vsz_tmp_buf(NULL),                  vsz_scale_context(NULL),

    // Deinterlacing
    m_deinterlacing(false),             m_deintfiltername("linearblend"),
    m_deintFiltMan(NULL),               m_deintFilter(NULL),
    m_deinterlaceBeforeOSD(true),

    // Various state variables
    errorState(kError_None),            framesPlayed(0),

    // Custom display resolutions
    display_res(NULL),

    // Physical display
    monitor_sz(640,480),                monitor_dim(400,300),

    // OSD
    osd_painter(NULL),                  osd_image(NULL),

    // Visualisation
    m_visual(NULL),

    // 3D TV
    m_stereo(kStereoscopicModeNone)
{
    memset(&pip_tmp_image, 0, sizeof(pip_tmp_image));
    db_display_dim = QSize(gCoreContext->GetNumSetting("DisplaySizeWidth",  0),
                           gCoreContext->GetNumSetting("DisplaySizeHeight", 0));

    db_aspectoverride = (AspectOverrideMode)
        gCoreContext->GetNumSetting("AspectOverride",      0);
    db_adjustfill = (AdjustFillMode)
        gCoreContext->GetNumSetting("AdjustFill",          0);
    db_letterbox_colour = (LetterBoxColour)
        gCoreContext->GetNumSetting("LetterboxColour",     0);

    if (!gCoreContext->IsDatabaseIgnored())
        db_vdisp_profile = new VideoDisplayProfile();
}

/**
 * \fn VideoOutput::~VideoOutput()
 * \brief Shuts down video output.
 */
VideoOutput::~VideoOutput()
{
    if (osd_image)
        osd_image->DecrRef();
    if (osd_painter)
        delete osd_painter;

    ShutdownPipResize();

    ShutdownVideoResize();

    if (m_deintFilter)
        delete m_deintFilter;
    if (m_deintFiltMan)
        delete m_deintFiltMan;
    if (db_vdisp_profile)
        delete db_vdisp_profile;

    ResizeForGui();
    if (display_res)
        display_res->Unlock();
}

/**
 * \fn VideoOutput::Init(int,int,float,WId,int,int,int,int,WId)
 * \brief Performs most of the initialization for VideoOutput.
 * \return true if successful, false otherwise.
 */
bool VideoOutput::Init(const QSize &video_dim_buf,
                       const QSize &video_dim_disp,
                       float aspect, WId winid,
                       const QRect &win_rect, MythCodecID codec_id)
{
    (void)winid;

    video_codec_id = codec_id;
    bool wasembedding = window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = window.GetEmbeddingRect();
        StopEmbedding();
    }

    bool mainSuccess = window.Init(video_dim_buf, video_dim_disp,
                                   aspect, win_rect,
                                   db_aspectoverride, db_adjustfill);

    if (db_vdisp_profile)
        db_vdisp_profile->SetInput(window.GetVideoDim());

    if (wasembedding)
        EmbedInWidget(oldrect);

    VideoAspectRatioChanged(aspect); // apply aspect ratio and letterbox mode

    return mainSuccess;
}

void VideoOutput::InitOSD(OSD *osd)
{
    if (db_vdisp_profile && !db_vdisp_profile->IsOSDFadeEnabled())
        osd->DisableFade();
}

QString VideoOutput::GetFilters(void) const
{
    if (db_vdisp_profile)
        return db_vdisp_profile->GetFilters();
    return QString::null;
}

bool VideoOutput::IsPreferredRenderer(QSize video_size)
{
    if (!db_vdisp_profile || (video_size == window.GetVideoDispDim()))
        return true;

    VideoDisplayProfile vdisp;
    vdisp.SetInput(video_size);
    QString new_rend = vdisp.GetVideoRenderer();
    if (new_rend.isEmpty())
        return true;

    return db_vdisp_profile->CheckVideoRendererGroup(new_rend);
}

void VideoOutput::SetVideoFrameRate(float playback_fps)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetOutput(playback_fps);
}

/**
 * \fn VideoOutput::SetDeinterlacingEnabled(bool)
 * \brief Attempts to enable/disable deinterlacing using
 *        existing deinterlace method when enabling.
 */
bool VideoOutput::SetDeinterlacingEnabled(bool enable)
{
    if (enable && m_deinterlacing)
        return m_deinterlacing;

    // if enable and no deinterlacer allocated, attempt allocate one
    if (enable && (!m_deintFiltMan || !m_deintFilter))
        return SetupDeinterlace(enable);

    m_deinterlacing = enable;
    return m_deinterlacing;
}

/**
 * \fn VideoOutput::SetupDeinterlace(bool,const QString&)
 * \brief Attempts to enable or disable deinterlacing.
 * \return true if successful, false otherwise.
 * \param overridefilter optional, explicitly use this nondefault deint filter
 */
bool VideoOutput::SetupDeinterlace(bool interlaced,
                                   const QString& overridefilter)
{
    PIPState pip_state = window.GetPIPState();

    if (pip_state > kPIPOff && pip_state < kPBPLeft)
        return false;

    if (m_deinterlacing == interlaced)
        return m_deinterlacing;

    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    m_deinterlacing = interlaced;

    if (m_deinterlacing)
    {
        m_deinterlaceBeforeOSD = true;

        VideoFrameType itmp = FMT_YV12;
        VideoFrameType otmp = FMT_YV12;
        int btmp;

        if (db_vdisp_profile)
            m_deintfiltername =
                db_vdisp_profile->GetFilteredDeint(overridefilter);
        else
            m_deintfiltername = "";

        m_deintFiltMan = new FilterManager;
        m_deintFilter = NULL;

        if (!m_deintfiltername.isEmpty())
        {
            if (!ApproveDeintFilter(m_deintfiltername))
            {
                LOG(VB_GENERAL, LOG_ERR,
                    QString("Failed to approve '%1' deinterlacer "
                            "as a software deinterlacer")
                        .arg(m_deintfiltername));
                m_deintfiltername = QString::null;
            }
            else
            {
                int threads = db_vdisp_profile ?
                                db_vdisp_profile->GetMaxCPUs() : 1;
                const QSize video_dim = window.GetVideoDim();
                int width  = video_dim.width();
                int height = video_dim.height();
                m_deintFilter = m_deintFiltMan->LoadFilters(
                    m_deintfiltername, itmp, otmp,
                    width, height, btmp, threads);
                window.SetVideoDim(QSize(width, height));
            }
        }

        if (m_deintFilter == NULL)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                QString("Couldn't load deinterlace filter %1")
                    .arg(m_deintfiltername));
            m_deinterlacing = false;
            m_deintfiltername = "";
        }

        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Using deinterlace method %1")
                                   .arg(m_deintfiltername));

        if (m_deintfiltername == "bobdeint")
            m_deinterlaceBeforeOSD = false;
    }

    return m_deinterlacing;
}

/** \fn VideoOutput::FallbackDeint(void)
 *  \brief Fallback to non-frame-rate-doubling deinterlacing method.
 */
void VideoOutput::FallbackDeint(void)
{
    SetupDeinterlace(false);
    if (db_vdisp_profile)
        SetupDeinterlace(true, db_vdisp_profile->GetFallbackDeinterlacer());
}

/** \fn VideoOutput::BestDeint(void)
 *  \brief Change to the best deinterlacing method.
 */
void VideoOutput::BestDeint(void)
{
    SetupDeinterlace(false);
    SetupDeinterlace(true);
}

/** \fn VideoOutput::IsExtraProcessingRequired(void) const
 *  \brief Should Prepare() and Show() and ProcessFrame be called
 *         twice for every Frameloop().
 *
 *   All adaptive full framerate deinterlacers require an extra
 *   ProcessFrame() call.
 *
 *  \return true if deint name contains doubleprocess
 */
bool VideoOutput::IsExtraProcessingRequired(void) const
{
    return (m_deintfiltername.contains("doubleprocess")) && m_deinterlacing;
}
/**
 * \fn VideoOutput::NeedsDoubleFramerate() const
 * \brief Should Prepare() and Show() be called twice for every ProcessFrame().
 *
 * \return m_deintfiltername == "bobdeint" && m_deinterlacing
 */
bool VideoOutput::NeedsDoubleFramerate() const
{
    // Bob deinterlace requires doubling framerate
    return ((m_deintfiltername.contains("bobdeint") ||
             m_deintfiltername.contains("doublerate") ||
             m_deintfiltername.contains("doubleprocess")) &&
             m_deinterlacing);
}

bool VideoOutput::IsBobDeint(void) const
{
    return (m_deinterlacing && m_deintfiltername == "bobdeint");
}

/**
 * \fn VideoOutput::ApproveDeintFilter(const QString& filtername) const
 * \brief Approves all deinterlace filters, except ones which
 *        must be supported by a specific video output class.
 */
bool VideoOutput::ApproveDeintFilter(const QString& filtername) const
{
    // Default to not supporting bob deinterlace
    return (!filtername.contains("bobdeint") &&
            !filtername.contains("doublerate") &&
            !filtername.contains("opengl") &&
            !filtername.contains("vdpau"));
}

void VideoOutput::GetDeinterlacers(QStringList &deinterlacers)
{
    if (!db_vdisp_profile)
        return;
    QString rend = db_vdisp_profile->GetActualVideoRenderer();
    deinterlacers = db_vdisp_profile->GetDeinterlacers(rend);
}

QString VideoOutput::GetDeinterlacer(void)
{
    QString res = m_deintfiltername;
    res.detach();
    return res;
}

/**
 * \fn VideoOutput::VideoAspectRatioChanged(float aspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutput::VideoAspectRatioChanged(float aspect)
{
    window.VideoAspectRatioChanged(aspect);
}

/**
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool VideoOutput::InputChanged(const QSize &video_dim_buf,
                               const QSize &video_dim_disp,
                               float        aspect,
                               MythCodecID  myth_codec_id,
                               void        *codec_private,
                               bool        &aspect_only)
{
    window.InputChanged(video_dim_buf, video_dim_disp,
                        aspect, myth_codec_id, codec_private);

    if (db_vdisp_profile)
        db_vdisp_profile->SetInput(window.GetVideoDim());
    video_codec_id = myth_codec_id;
    BestDeint();

    DiscardFrames(true);

    return true;
}
/**
* \brief Resize Display Window
*/
void VideoOutput::ResizeDisplayWindow(const QRect &rect, bool save_visible_rect)
{
    window.ResizeDisplayWindow(rect, save_visible_rect);
}

/**
 * \brief Tells video output to embed video in an existing window.
 * \sa StopEmbedding()
 */
void VideoOutput::EmbedInWidget(const QRect &rect)
{
    window.EmbedInWidget(rect);
}

/**
 * \fn VideoOutput::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(const QRect&)
 */
void VideoOutput::StopEmbedding(void)
{
    window.StopEmbedding();
}

/**
 * \fn VideoOutput::DrawSlice(VideoFrame*, int, int, int, int)
 * \brief Informs video output of new data for frame,
 *        used for hardware accelerated decoding.
 */
void VideoOutput::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

void VideoOutput::GetOSDBounds(QRect &total, QRect &visible,
                               float &visible_aspect,
                               float &font_scaling,
                               float themeaspect) const
{
    total = GetTotalOSDBounds();
    visible = GetVisibleOSDBounds(visible_aspect, font_scaling, themeaspect);
}

/**
 * \fn VideoOutput::GetVisibleOSDBounds(float&,float&,float) const
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 */
QRect VideoOutput::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling, float themeaspect) const
{
    if (!hasFullScreenOSD())
    {
        return window.GetVisibleOSDBounds(
            visible_aspect, font_scaling, themeaspect);
    }

    QRect dvr = window.GetDisplayVisibleRect();

    // This rounding works for I420 video...
    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    float dispPixelAdj = 1.0f;
    if (dvr2.height() && dvr2.width())
        dispPixelAdj = (GetDisplayAspect() * dvr2.height()) / dvr2.width();
    visible_aspect = themeaspect / dispPixelAdj;
    font_scaling   = 1.0f;
    return QRect(QPoint(0,0), dvr2);
}

/**
 * \fn VideoOutput::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutput::GetTotalOSDBounds(void) const
{
    if (!hasFullScreenOSD())
        return window.GetTotalOSDBounds();

    QRect dvr = window.GetDisplayVisibleRect();
    QSize dvr2 = QSize(dvr.width()  & ~0x3,
                       dvr.height() & ~0x1);

    return QRect(QPoint(0,0), dvr2);
}

QRect VideoOutput::GetMHEGBounds(void)
{
    if (!hasFullScreenOSD())
        return window.GetTotalOSDBounds();

    QRect dvr = window.GetDisplayVideoRect();
    return QRect(QPoint(dvr.left() & ~0x1, dvr.top()  & ~0x1),
                 QSize(dvr.width() & ~0x1, dvr.height() & ~0x1));
}

bool VideoOutput::AllowPreviewEPG(void) const
{
    return window.IsPreviewEPGAllowed();
}

/**
 * \fn VideoOutput::MoveResize(void)
 * \brief performs all the calculations for video framing and any resizing.
 *
 * First we apply playback over/underscanning and offsetting,
 * then we letterbox settings, and finally we apply manual
 * scale & move properties for "Zoom Mode".
 *
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(int)
 */
void VideoOutput::MoveResize(void)
{
    window.MoveResize();
}

/**
 * \fn VideoOutput::Zoom(ZoomDirection)
 * \brief Sets up zooming into to different parts of the video, the zoom
 *        is actually applied in MoveResize().
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutput::Zoom(ZoomDirection direction)
{
    window.Zoom(direction);
}

/**
 * \fn VideoOutput::ToggleAspectOverride(AspectOverrideMode)
 * \brief Enforce different aspect ration than detected,
 *        then calls VideoAspectRatioChanged(float)
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutput::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    window.ToggleAspectOverride(aspectMode);
}

/**
 * \fn VideoOutput::ToggleAdjustFill(AdjustFillMode)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls MoveResize()
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void VideoOutput::ToggleAdjustFill(AdjustFillMode adjustFill)
{
    window.ToggleAdjustFill(adjustFill);
}

int VideoOutput::ChangePictureAttribute(
    PictureAttribute attributeType, bool direction)
{
    int curVal = GetPictureAttribute(attributeType);
    if (curVal < 0)
        return -1;

    int newVal = curVal + ((direction) ? +1 : -1);

    if (kPictureAttribute_Hue == attributeType)
        newVal = newVal % 100;

    if ((kPictureAttribute_StudioLevels == attributeType) && newVal > 1)
        newVal = 1;

    newVal = min(max(newVal, 0), 100);

    return SetPictureAttribute(attributeType, newVal);
}

/**
 * \fn VideoOutput::SetPictureAttribute(PictureAttribute, int)
 * \brief Sets a specified picture attribute.
 * \param attribute Picture attribute to set.
 * \param newValue  Value to set attribute to.
 * \return Set value if it succeeds, -1 if it does not.
 */
int VideoOutput::SetPictureAttribute(PictureAttribute attribute, int newValue)
{
    return videoColourSpace.SetPictureAttribute(attribute, newValue);
}

int VideoOutput::GetPictureAttribute(PictureAttribute attributeType)
{
    return videoColourSpace.GetPictureAttribute(attributeType);
}

/**
\ brief return OSD renderer type for this videoOutput
*/
QString VideoOutput::GetOSDRenderer(void) const
{
    return db_vdisp_profile->GetOSDRenderer();
}

/*
 * \brief Determines PIP Window size and Position.
 */
QRect VideoOutput::GetPIPRect(
    PIPLocation location, MythPlayer *pipplayer, bool do_pixel_adj) const
{
    return window.GetPIPRect(location, pipplayer, do_pixel_adj);
}

/**
 * \fn VideoOutput::DoPipResize(int,int)
 * \brief Sets up Picture in Picture image resampler.
 * \param pipwidth  input width
 * \param pipheight input height
 * \sa ShutdownPipResize(), ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::DoPipResize(int pipwidth, int pipheight)
{
    QSize vid_size = QSize(pipwidth, pipheight);
    if (vid_size == pip_desired_display_size)
        return;

    ShutdownPipResize();

    pip_video_size   = vid_size;
    pip_display_size = pip_desired_display_size;

    int sz = pip_display_size.height() * pip_display_size.width() * 3 / 2;
    pip_tmp_buf = new unsigned char[sz];
    pip_tmp_buf2 = new unsigned char[sz];

    pip_scaling_context = sws_getCachedContext(pip_scaling_context,
                              pip_video_size.width(), pip_video_size.height(),
                              PIX_FMT_YUV420P,
                              pip_display_size.width(),
                              pip_display_size.height(),
                              PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                              NULL, NULL, NULL);
}

/**
 * \fn VideoOutput::ShutdownPipResize()
 * \brief Shuts down Picture in Picture image resampler.
 * \sa VideoOutput::DoPipResize(int,int),
 *     ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::ShutdownPipResize(void)
{
    if (pip_tmp_buf)
    {
        delete [] pip_tmp_buf;
        pip_tmp_buf   = NULL;
    }

    if (pip_tmp_buf2)
    {
        delete [] pip_tmp_buf2;
        pip_tmp_buf2 = NULL;
    }

    if (pip_scaling_context)
    {
        sws_freeContext(pip_scaling_context);
        pip_scaling_context = NULL;
    }

    pip_video_size   = QSize(0,0);
    pip_display_size = QSize(0,0);
}

void VideoOutput::ShowPIPs(VideoFrame *frame, const PIPMap &pipPlayers)
{
    PIPMap::const_iterator it = pipPlayers.begin();
    for (; it != pipPlayers.end(); ++it)
        ShowPIP(frame, it.key(), *it);
}

/**
 * \fn VideoOutput::ShowPIP(VideoFrame*,MythPlayer*,PIPLocation)
 * \brief Composites PiP image onto a video frame.
 *
 *  Note: This only works with memory backed VideoFrames.
 *
 * \param frame     Frame to composite PiP onto.
 * \param pipplayer Picture-in-Picture Player.
 * \param loc       Location of this PiP on the frame.
 */
void VideoOutput::ShowPIP(VideoFrame  *frame,
                          MythPlayer  *pipplayer,
                          PIPLocation  loc)
{
    if (!pipplayer)
        return;

    const float video_aspect           = window.GetVideoAspect();
//     const QRect display_video_rect     = window.GetDisplayVideoRect();
//     const QRect video_rect             = window.GetVideoRect();
//     const QRect display_visible_rect   = window.GetDisplayVisibleRect();
//     const QSize video_disp_dim         = window.GetVideoDispDim();

    int pipw, piph;
    VideoFrame *pipimage       = pipplayer->GetCurrentFrame(pipw, piph);
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const float pipVideoAspect = pipplayer->GetVideoAspect();
//     const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();

    // If PiP is not initialized to values we like, silently ignore the frame.
    if ((video_aspect <= 0) || (pipVideoAspect <= 0) ||
        (frame->height <= 0) || (frame->width <= 0) ||
        !pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    if (!pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    pip_desired_display_size = position.size();

    // Scale the image if we have to...
    unsigned char *pipbuf = pipimage->buf;
    if (pipw != pip_desired_display_size.width() ||
        piph != pip_desired_display_size.height())
    {
        DoPipResize(pipw, piph);

        memset(&pip_tmp_image, 0, sizeof(pip_tmp_image));

        if (pip_tmp_buf && pip_scaling_context)
        {
            AVPicture img_in, img_out;

            avpicture_fill(
                &img_out, (uint8_t *)pip_tmp_buf, PIX_FMT_YUV420P,
                pip_display_size.width(), pip_display_size.height());

            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            sws_scale(pip_scaling_context, img_in.data, img_in.linesize, 0,
                      piph, img_out.data, img_out.linesize);

            if (pipActive)
            {
                AVPicture img_padded;
                avpicture_fill(
                    &img_padded, (uint8_t *)pip_tmp_buf2, PIX_FMT_YUV420P,
                    pip_display_size.width(), pip_display_size.height());

                int color[3] = { 20, 0, 200 }; //deep red YUV format
                av_picture_pad(&img_padded, &img_out,
                               pip_display_size.height(),
                               pip_display_size.width(),
                               PIX_FMT_YUV420P, 10, 10, 10, 10, color);

                pipbuf = pip_tmp_buf2;
            }
            else
            {
                pipbuf = pip_tmp_buf;
            }

            pipw = pip_display_size.width();
            piph = pip_display_size.height();

            init(&pip_tmp_image, FMT_YV12, pipbuf, pipw, piph, sizeof(*pipbuf));
        }
    }

    if ((position.left() >= 0) && (position.top() >= 0))
    {
        int xoff = position.left();
        int yoff = position.top();
        int xoff2[3] = { xoff, xoff>>1, xoff>>1 };
        int yoff2[3] = { yoff, yoff>>1, yoff>>1 };

        int pip_height = pip_tmp_image.height;
        int height[3] = { pip_height, pip_height>>1, pip_height>>1 };

        for (int p = 0; p < 3; p++)
        {
            for (int h = 2; h < height[p]; h++)
            {
                memcpy((frame->buf + frame->offsets[p]) + (h + yoff2[p]) *
                       frame->pitches[p] + xoff2[p],
                       (pip_tmp_image.buf + pip_tmp_image.offsets[p]) + h *
                       pip_tmp_image.pitches[p], pip_tmp_image.pitches[p]);
            }
        }
    }

    // we're done with the frame, release it
    pipplayer->ReleaseCurrentFrame(pipimage);
}

/**
 * \brief Sets up Picture in Picture image resampler.
 * \param inDim  input width and height
 * \param outDim output width and height
 * \sa ShutdownPipResize(), ShowPIPs(VideoFrame*,const PIPMap&)
 */
void VideoOutput::DoVideoResize(const QSize &inDim, const QSize &outDim)
{
    if ((inDim == vsz_video_size) && (outDim == vsz_display_size))
        return;

    ShutdownVideoResize();

    vsz_enabled      = true;
    vsz_video_size   = inDim;
    vsz_display_size = outDim;

    int sz = vsz_display_size.height() * vsz_display_size.width() * 3 / 2;
    vsz_tmp_buf = new unsigned char[sz];

    vsz_scale_context = sws_getCachedContext(vsz_scale_context,
                              vsz_video_size.width(), vsz_video_size.height(),
                              PIX_FMT_YUV420P,
                              vsz_display_size.width(),
                              vsz_display_size.height(),
                              PIX_FMT_YUV420P, SWS_FAST_BILINEAR,
                              NULL, NULL, NULL);
}

void VideoOutput::ResizeVideo(VideoFrame *frame)
{
    if (vsz_desired_display_rect.isNull() || frame->codec !=  FMT_YV12)
        return;

    QRect resize = vsz_desired_display_rect;
    QSize frameDim(frame->width, frame->height);

    // if resize is outside existing frame, abort
    bool abort =
        (resize.right() > frame->width || resize.bottom() > frame->height ||
         resize.width() > frame->width || resize.height() > frame->height);
    // if resize == existing frame, no need to carry on
    abort |= !resize.left() && !resize.top() && (resize.size() == frameDim);

    if (abort)
    {
        ShutdownVideoResize();
        vsz_desired_display_rect = QRect();
        return;
    }

    DoVideoResize(frameDim, resize.size());
    if (!vsz_tmp_buf)
    {
        ShutdownVideoResize();
        vsz_desired_display_rect = QRect();
        return;
    }

    if (vsz_tmp_buf && vsz_scale_context)
    {
        AVPicture img_in, img_out;

        avpicture_fill(&img_out, (uint8_t *)vsz_tmp_buf, PIX_FMT_YUV420P,
                       resize.width(), resize.height());
        avpicture_fill(&img_in, (uint8_t *)frame->buf, PIX_FMT_YUV420P,
                       frame->width, frame->height);
        sws_scale(vsz_scale_context, img_in.data, img_in.linesize, 0,
                      frame->height, img_out.data, img_out.linesize);
    }

    int xoff = resize.left();
    int yoff = resize.top();
    int resw = resize.width();

    // Copy Y (intensity values)
    for (int i = 0; i < resize.height(); i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               vsz_tmp_buf + i * resw, resw);
    }

    // Copy U & V (half plane chroma values)
    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *videouptr = vsz_tmp_buf + resw * resize.height();
    unsigned char *videovptr = vsz_tmp_buf + resw * resize.height() * 5 / 4;
    resw /= 2;
    for (int i = 0; i < resize.height() / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, videouptr + i * resw, resw);
        memcpy(vptr + (i + yoff) * vidw + xoff, videovptr + i * resw, resw);
    }
}

AspectOverrideMode VideoOutput::GetAspectOverride(void) const
{
    return window.GetAspectOverride();
}

AdjustFillMode VideoOutput::GetAdjustFill(void) const
{
    return window.GetAdjustFill();
}

float VideoOutput::GetDisplayAspect(void) const
{
    return window.GetDisplayAspect();
}

bool VideoOutput::IsVideoScalingAllowed(void) const
{
    return window.IsVideoScalingAllowed();
}

void VideoOutput::ShutdownVideoResize(void)
{
    if (vsz_tmp_buf)
    {
        delete [] vsz_tmp_buf;
        vsz_tmp_buf = NULL;
    }

    if (vsz_scale_context)
    {
        sws_freeContext(vsz_scale_context);
        vsz_scale_context = NULL;
    }

    vsz_video_size   = QSize(0,0);
    vsz_display_size = QSize(0,0);
    vsz_enabled      = false;
}

void VideoOutput::ClearDummyFrame(VideoFrame *frame)
{
    // used by render devices to ignore frame rendering
    if (frame)
        frame->dummy = 1;
    // will only clear frame in main memory
    clear(frame);
}

void VideoOutput::SetVideoResize(const QRect &videoRect)
{
    if (!videoRect.isValid()    ||
         videoRect.width()  < 1 || videoRect.height() < 1 ||
         videoRect.left()   < 0 || videoRect.top()    < 0)
    {
        ShutdownVideoResize();
        vsz_desired_display_rect = QRect();
    }
    else
    {
        vsz_enabled = true;
        vsz_desired_display_rect = videoRect;
    }
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutput::SetVideoScalingAllowed(bool change)
{
    window.SetVideoScalingAllowed(change);
}

/**
 * \fn VideoOutput::DisplayOSD(VideoFrame*,OSD *,int,int)
 * \brief If the OSD has changed, this will convert the OSD buffer
 *        to the OSDSurface's color format.
 *
 *  If the destination format is either IA44 or AI44 the osd is
 *  converted to greyscale.
 *
 * \return true if visible, false otherwise
 */
bool VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd)
{
    if (!osd || !frame)
        return false;

    if (vsz_enabled)
        ResizeVideo(frame);

    if (!osd_painter)
    {
        osd_painter = new MythYUVAPainter();
        if (!osd_painter)
            return false;
    }

    QSize osd_size = GetTotalOSDBounds().size();
    if (osd_image && (osd_image->size() != osd_size))
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("OSD size changed."));
        osd_image->DecrRef();
        osd_image = NULL;
    }

    if (!osd_image)
    {
        osd_image = osd_painter->GetFormatImage();
        if (osd_image)
        {
            QImage blank = QImage(osd_size,
                                  QImage::Format_ARGB32_Premultiplied);
            osd_image->Assign(blank);
            osd_image->ConvertToYUV();
            osd_painter->Clear(osd_image,
                               QRegion(QRect(QPoint(0,0), osd_size)));
            LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created YV12 OSD."));
        }
        else
            return false;
    }

    if (m_visual)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Visualiser not supported here");
        // Clear the audio buffer
        m_visual->Draw(QRect(), NULL, NULL);
    }

    QRegion dirty   = QRegion();
    QRegion visible = osd->Draw(osd_painter, osd_image, osd_size, dirty,
                                frame->codec == FMT_YV12 ? ALIGN_X_MMX : 0,
                                frame->codec == FMT_YV12 ? ALIGN_C : 0);
    bool changed    = !dirty.isEmpty();
    bool show       = !visible.isEmpty();

    if (!show)
        return show;

    if (!changed && frame->codec != FMT_YV12)
        return show;

    QSize video_dim = window.GetVideoDim();

    QVector<QRect> vis = visible.rects();
    for (int i = 0; i < vis.size(); i++)
    {
        int left   = min(vis[i].left(), osd_image->width());
        int top    = min(vis[i].top(), osd_image->height());
        int right  = min(left + vis[i].width(), osd_image->width());
        int bottom = min(top + vis[i].height(), osd_image->height());

        if (FMT_YV12 == frame->codec)
        {
            yuv888_to_yv12(frame, osd_image, left, top, right, bottom);
        }
        else if (FMT_AI44 == frame->codec)
        {
            memset(frame->buf, 0, video_dim.width() * video_dim.height());
            yuv888_to_i44(frame->buf, osd_image, video_dim,
                          left, top, right, bottom, true);
        }
        else if (FMT_IA44 == frame->codec)
        {
            memset(frame->buf, 0, video_dim.width() * video_dim.height());
            yuv888_to_i44(frame->buf, osd_image, video_dim,
                          left, top, right, bottom, false);
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, LOC +
                "Display OSD: Frame format not supported.");
        }
    }
    return show;
}

bool VideoOutput::EnableVisualisation(AudioPlayer *audio, bool enable,
                                      const QString &name)
{
    if (!enable)
    {
        DestroyVisualisation();
        return false;
    }
    return SetupVisualisation(audio, NULL, name);
}

bool VideoOutput::CanVisualise(AudioPlayer *audio, MythRender *render)
{
    return VideoVisual::CanVisualise(audio, render);
}

bool VideoOutput::SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                     const QString &name)
{
    DestroyVisualisation();
    m_visual = VideoVisual::Create(name, audio, render);
    return m_visual;
}

QString VideoOutput::GetVisualiserName(void)
{
    if (m_visual)
        return m_visual->Name();
    return QString("");
}

QStringList VideoOutput::GetVisualiserList(void)
{
    return QStringList();
}

void VideoOutput::DestroyVisualisation(void)
{
    delete m_visual;
    m_visual = NULL;
}

/**
 * \fn VideoOutput::CopyFrame(VideoFrame*, const VideoFrame*)
 * \brief Copies frame data from one VideoFrame to another.
 *
 *  Note: The frames must have the same width, height, and format.
 * \param to   The destination frame.
 * \param from The source frame
 */
void VideoOutput::CopyFrame(VideoFrame *to, const VideoFrame *from)
{
    if (to == NULL || from == NULL)
        return;

    to->frameNumber = from->frameNumber;
    to->disp_timecode = from->disp_timecode;

    // guaranteed to be correct sizes.
    if (from->size == to->size)
        memcpy(to->buf, from->buf, from->size);
    else if ((to->pitches[0] == from->pitches[0]) &&
             (to->pitches[1] == from->pitches[1]) &&
             (to->pitches[2] == from->pitches[2]))
    {
        memcpy(to->buf + to->offsets[0], from->buf + from->offsets[0],
               from->pitches[0] * from->height);
        memcpy(to->buf + to->offsets[1], from->buf + from->offsets[1],
               from->pitches[1] * (from->height>>1));
        memcpy(to->buf + to->offsets[2], from->buf + from->offsets[2],
               from->pitches[2] * (from->height>>1));
    }
    else if ((from->height >= 0) && (to->height >= 0))
    {
        int f[3] = { from->height,   from->height>>1, from->height>>1, };
        int t[3] = { to->height,     to->height>>1,   to->height>>1,   };
        int h[3] = { min(f[0],t[0]), min(f[1],t[1]),  min(f[2],t[2]),  };
        for (uint i = 0; i < 3; i++)
        {
            for (int j = 0; j < h[i]; j++)
            {
                memcpy(to->buf   + to->offsets[i]   + (j * to->pitches[i]),
                       from->buf + from->offsets[i] + (j * from->pitches[i]),
                       min(from->pitches[i], to->pitches[i]));
            }
        }
    }

/* XXX: Broken.
    if (from->qstride > 0 && from->qscale_table != NULL)
    {
        int tablesize = from->qstride * ((from->height + 15) / 16);

        if (to->qstride != from->qstride || to->qscale_table == NULL)
        {
            to->qstride = from->qstride;
            if (to->qscale_table)
                delete [] to->qscale_table;

            to->qscale_table = new unsigned char[tablesize];
        }

        memcpy(to->qscale_table, from->qscale_table, tablesize);
    }
*/
}

QRect VideoOutput::GetImageRect(const QRect &rect, QRect *display)
{
    float hscale, vscale, tmp;
    tmp = 0.0;
    QRect visible_osd  = GetVisibleOSDBounds(tmp, tmp, tmp);
    QSize video_size   = window.GetVideoDispDim();
    int image_height   = video_size.height();
    int image_width    = (image_height > 720) ? 1920 :
                         (image_height > 576) ? 1280 : 720;
    float image_aspect = (float)image_width / (float)image_height;
    float pixel_aspect = (float)video_size.width() /
                         (float)video_size.height();

    QRect rect1 = rect;
    if (display && display->isValid())
    {
        QMatrix m0;
        m0.scale((float)image_width  / (float)display->width(),
                 (float)image_height / (float)display->height());
        rect1 = m0.mapRect(rect1);
        rect1.translate(display->left(), display->top());
    }
    QRect result = rect1;

    if (hasFullScreenOSD())
    {
        QRect dvr_rec = window.GetDisplayVideoRect();
        QRect vid_rec = window.GetVideoRect();

        hscale = image_aspect / pixel_aspect;
        if (hscale < 0.99f || hscale > 1.01f)
        {
            vid_rec.setLeft((int)(((float)vid_rec.left() * hscale) + 0.5f));
            vid_rec.setWidth((int)(((float)vid_rec.width() * hscale) + 0.5f));
        }

        vscale = (float)dvr_rec.width() / (float)image_width;
        hscale = (float)dvr_rec.height() / (float)image_height;
        QMatrix m1;
        m1.translate(dvr_rec.left(), dvr_rec.top());
        m1.scale(vscale, hscale);

        vscale = (float)image_width / (float)vid_rec.width();
        hscale = (float)image_height / (float)vid_rec.height();
        QMatrix m2;
        m2.scale(vscale, hscale);
        m2.translate(-vid_rec.left(), -vid_rec.top());

        result = m2.mapRect(result);
        result = m1.mapRect(result);
        return result;
    }

    hscale = pixel_aspect / image_aspect;
    if (hscale < 0.99f || hscale > 1.01f)
    {
        result.setLeft((int)(((float)rect1.left() * hscale) + 0.5f));
        result.setWidth((int)(((float)rect1.width() * hscale) + 0.5f));
    }

    result.translate(-visible_osd.left(), -visible_osd.top());
    return result;
}

/**
 * \brief Returns a QRect describing an area of the screen on which it is
 *        'safe' to render the On Screen Display. For 'fullscreen' OSDs this
 *        will still translate to a subset of the video frame area to ensure
 *        consistency of presentation for subtitling etc.
 */
QRect VideoOutput::GetSafeRect(void)
{
    static const float safeMargin = 0.05f;
    float dummy;
    QRect result = GetVisibleOSDBounds(dummy, dummy, 1.0f);
    int safex = (int)((float)result.width()  * safeMargin);
    int safey = (int)((float)result.height() * safeMargin);
    return QRect(result.left() + safex, result.top() + safey,
                 result.width() - (2 * safex), result.height() - (2 * safey));
}

void VideoOutput::SetPIPState(PIPState setting)
{
    window.SetPIPState(setting);
}


static QString to_comma_list(const QStringList &list)
{
    QString ret = "";
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        ret += *it + ",";

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

bool VideoOutput::IsEmbedding(void)
{
    return window.IsEmbedding();
}

void VideoOutput::ExposeEvent(void)
{
    window.SetNeedRepaint(true);
}

/**
 * \fn VideoOutput::ResizeForGui(void)
 * If we are using DisplayRes support, return the screen size and
 * refresh rate to those used for the GUI.
 */
void VideoOutput::ResizeForGui(void)
{
    if (display_res)
        display_res->SwitchToGUI();
}

/**
 * \fn VideoOutput::ResizeForVideo(uint width, uint height)
 * Sets display parameters based on video resolution.
 *
 * If we are using DisplayRes support we use the video size to
 * determine the desired screen size and refresh rate.
 * If we are also not using "GuiSizeForTV" we also resize
 * the video output window.
 *
 * \param width,height Resolution of the video we will be playing
 */
void VideoOutput::ResizeForVideo(uint width, uint height)
{
    if (!display_res)
        return;

    if (!width || !height)
    {
        width  = window.GetVideoDispDim().width();
        height = window.GetVideoDispDim().height();
        if (!width || !height)
            return;
    }

#if 0
    // width and height should already be the properly cropped
    // versions, and therefore height should not need this manual
    // truncation.  Delete this code if no problems crop up.
    if ((width == 1920 || width == 1440) && height == 1088)
        height = 1080; // ATSC 1920x1080
#endif

    float rate = db_vdisp_profile ? db_vdisp_profile->GetOutput() : 0.0f;
    if (display_res && display_res->SwitchToVideo(width, height, rate))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        window.SetDisplayDim(QSize(display_res->GetPhysicalWidth(),
                                       display_res->GetPhysicalHeight()));
        window.SetDisplayAspect(display_res->GetAspectRatio());

        bool fullscreen = !window.UsingGuiSize();

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gCoreContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize sz(display_res->GetWidth(), display_res->GetHeight());
            const QRect display_visible_rect =
                    QRect(GetMythMainWindow()->geometry().topLeft(), sz);
            window.SetDisplayVisibleRect(display_visible_rect);
            MoveResize();
            // Resize X window to fill new resolution
            MoveResizeWindow(display_visible_rect);
        }
    }
}

/**
 * \brief Init display measurements based on database settings and
 *        actual screen parameters.
 */
void VideoOutput::InitDisplayMeasurements(uint width, uint height, bool resize)
{
    DisplayInfo disp = MythDisplay::GetDisplayInfo();
    QString     source = "Actual";

    // The very first Resize needs to be the maximum possible
    // desired res, because X will mask off anything outside
    // the initial dimensions
    QSize sz1 = disp.res;
    QSize sz2 = window.GetScreenGeometry().size();
    QSize max_size = sz1.expandedTo(sz2);

    if (window.UsingGuiSize())
        max_size = GetMythMainWindow()->geometry().size();

    if (display_res)
    {
        max_size.setWidth(display_res->GetMaxWidth());
        max_size.setHeight(display_res->GetMaxHeight());
    }

    if (resize)
    {
        MoveResizeWindow(QRect(GetMythMainWindow()->geometry().x(),
                               GetMythMainWindow()->geometry().y(),
                               max_size.width(), max_size.height()));
    }

    // get the physical dimensions (in mm) of the display. If using
    // DisplayRes, this will be overridden when we call ResizeForVideo
    if (db_display_dim.isEmpty())
    {
        window.SetDisplayDim(disp.size);
    }
    else
    {
        window.SetDisplayDim(db_display_dim);
        source = "Database";
    }

    // Set the display mode if required
    if (display_res)
        ResizeForVideo(width, height);

    // Determine window and screen dimensions in pixels
    QSize screen_size = window.GetScreenGeometry().size();
    QSize window_size = window.GetDisplayVisibleRect().size();

    float pixel_aspect = (float)screen_size.width() /
                         (float)screen_size.height();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(screen_size.width()).arg(screen_size.height())
            .arg(window_size.width()).arg(window_size.height()));

    // Check the display dimensions
    QSize disp_dim = window.GetDisplayDim();
    float disp_aspect;

    // If we are using Xinerama the display dimensions cannot be trusted.
    // We need to use the Xinerama monitor aspect ratio from the DB to set
    // the physical screen width. This assumes the height is correct, which
    // is more or less true in the typical side-by-side monitor setup.
    if (window.UsingXinerama())
    {
        source = "Xinerama";
        disp_aspect = gCoreContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gCoreContext->GetHostName(), pixel_aspect);
        if (disp_dim.height() <= 0)
            disp_dim.setHeight(300);
        disp_dim.setWidth((int) ((disp_dim.height() * disp_aspect) + 0.5));
    }

    if (disp_dim.isEmpty())
    {
        source = "Guessed!";
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Physical size of display unknown."
                "\n\t\t\tAssuming 17\" monitor with square pixels.");
        disp_dim = QSize((int) ((300 * pixel_aspect) + 0.5), 300);
    }

    disp_aspect = (float) disp_dim.width() / (float) disp_dim.height();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("%1 display dimensions: %2x%3 mm  Aspect: %4")
            .arg(source).arg(disp_dim.width()).arg(disp_dim.height())
            .arg(disp_aspect));

    // Save the unscaled size and dimensions for window resizing
    monitor_sz  = screen_size;
    monitor_dim = disp_dim;

    // We must now scale the display measurements to our window size and save
    // them. If we are running fullscreen this is a no-op.
    disp_dim = QSize((disp_dim.width()  * window_size.width()) /
                      screen_size.width(),
                     (disp_dim.height() * window_size.height()) /
                      screen_size.height());
    disp_aspect = (float) disp_dim.width() / (float) disp_dim.height();
    window.SetDisplayDim(disp_dim);
    window.SetDisplayAspect(disp_aspect);

    // If we are using XRandR, use the aspect ratio from it
    if (display_res)
        window.SetDisplayAspect(display_res->GetAspectRatio());

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(window.GetDisplayDim().width())
            .arg(window.GetDisplayDim().height())
            .arg(window.GetDisplayAspect()));
}

int VideoOutput::CalcHueBase(const QString &adaptor_name)
{
    int hue_adj = 50;

    // XVideo adjustments
    if ((adaptor_name == "ATI Radeon Video Overlay") ||
        (adaptor_name == "XA G3D Textured Video") || /* ATI in VMWare*/
        (adaptor_name == "Radeon Textured Video") || /* ATI */
        (adaptor_name == "AMD Radeon AVIVO Video") || /* ATI */
        (adaptor_name == "XV_SWOV" /* VIA 10K & 12K */) ||
        (adaptor_name == "Savage Streams Engine" /* S3 Prosavage DDR-K */) ||
        (adaptor_name == "SIS 300/315/330 series Video Overlay") ||
        adaptor_name.toLower().contains("xvba")) /* VAAPI */
    {
        hue_adj = 50;
    }
    else if (adaptor_name.startsWith("NV17")) /* nVidia */
    {
        hue_adj = 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("CalcHueBase(%1): Unknown adaptor, hue may be wrong.")
            .arg(adaptor_name));
        LOG(VB_GENERAL, LOG_INFO, LOC +
            "Please open a ticket if you need to adjust the hue.");
    }

    return hue_adj;
}

void VideoOutput::CropToDisplay(VideoFrame *frame)
{
    // TODO: support cropping in the horizontal dimension
    if (!frame)
        return;
    if (frame->pitches[1] != frame->pitches[2])
        return;
    int crop = window.GetVideoDim().height() -
        window.GetVideoDispDim().height();
    if (crop <= 0 || crop >= 16)
        return; // something may be amiss, so don't crop

    // assume input and output formats are FMT_YV12
    uint64_t *ybuf = (uint64_t*) (frame->buf + frame->offsets[0]);
    uint64_t *ubuf = (uint64_t*) (frame->buf + frame->offsets[1]);
    uint64_t *vbuf = (uint64_t*) (frame->buf + frame->offsets[2]);
    const uint64_t Y_black  = 0x0000000000000000LL; // 8 bytes
    const uint64_t UV_black = 0x8080808080808080LL; // 8 bytes
    int y;
    int sz = (frame->pitches[0] * frame->height) >> 3; // div 8 bytes
    // Luma bottom
    y = ((frame->height - crop) >> 4) * frame->pitches[0] << 1;
    y = y + (sz - y) * (16 - crop) / 16;
    for (; y < sz; y++)
    {
        ybuf[y] = Y_black;
    }
    // Chroma bottom
    sz = (frame->pitches[1] * (frame->height >> 1)) >> 3; // div 8 bytes
    y = ((frame->height - crop) >> 4) * frame->pitches[1];
    y = y + (sz - y) * (16 - crop) / 16;
    for (; y < sz; y++)
    {
        ubuf[y] = UV_black;
        vbuf[y] = UV_black;
    }
}
