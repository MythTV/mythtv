#include <cmath>
#include <cstdlib>

#include <QRunnable>

#include "osd.h"
#include "mythplayer.h"
#include "videodisplayprofile.h"
#include "decoderbase.h"

#include "mythcorecontext.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythuihelper.h"
#include "mythxdisplay.h"
#include "mythavutil.h"
#include "mthreadpool.h"
#include "mythcodeccontext.h"

#ifdef _WIN32
#include "videoout_d3d.h"
#endif

#ifdef USING_OPENGL_VIDEO
#include "videoout_opengl.h"
#endif

#include "videoout_null.h"
#include "videooutbase.h"

#define LOC QString("VideoOutput: ")

void VideoOutput::GetRenderOptions(render_opts &opts)
{
    QStringList cpudeints;
    cpudeints += "none";

    VideoOutputNull::GetRenderOptions(opts, cpudeints);

#ifdef _WIN32
    VideoOutputD3D::GetRenderOptions(opts, cpudeints);
#endif

#ifdef USING_OPENGL_VIDEO
    VideoOutputOpenGL::GetRenderOptions(opts, cpudeints);
#endif // USING_OPENGL_VIDEO
}

/**
 * \brief  Depending on compile-time configure settings and run-time
 *         renderer settings, create a relevant VideoOutput subclass.
 * \return instance of VideoOutput if successful, nullptr otherwise.
 */
VideoOutput *VideoOutput::Create(
    const QString &decoder, MythCodecID  codec_id,
    PIPState pipState,      const QSize &video_dim_buf,
    const QSize &video_dim_disp, float video_aspect,
    QWidget *parentwidget,  const QRect &embed_rect, float video_prate,
    uint playerFlags, QString &codecName, int ReferenceFrames)
{
    QStringList renderers;

    // select the best available output
    if (playerFlags & kVideoIsNull)
    {
        // plain null output
        renderers += "null";
    }
    else
    {
#ifdef _WIN32
        renderers += VideoOutputD3D::GetAllowedRenderers(codec_id, video_dim_disp);
#endif

#ifdef USING_OPENGL_VIDEO
        renderers += VideoOutputOpenGL::GetAllowedRenderers(codec_id, video_dim_disp);
#endif // USING_OPENGL_VIDEO
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers: %1").arg(renderers.join(",")));
    renderers = VideoDisplayProfile::GetFilteredRenderers(decoder, renderers);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers (filt: %1): %2")
        .arg(decoder).arg(renderers.join(",")));

    QString renderer;

    VideoDisplayProfile *vprof = new VideoDisplayProfile();

    if (!renderers.empty())
    {
        vprof->SetInput(video_dim_disp, video_prate, codecName);
        QString tmp = vprof->GetVideoRenderer();
        if (vprof->IsDecoderCompatible(decoder) && renderers.contains(tmp))
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

        VideoOutput *vo = nullptr;

        /* these cases are mutually exlusive */
        if (renderer == "null")
            vo = new VideoOutputNull();

#ifdef _WIN32
        else if (renderer == "direct3d")
            vo = new VideoOutputD3D();
#endif // _WIN32

#ifdef USING_OPENGL_VIDEO
        else if (renderer.contains("opengl"))
            vo = new VideoOutputOpenGL(renderer);
#endif // USING_OPENGL_VIDEO

        if (vo)
            vo->m_dbDisplayProfile = vprof;

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
                vo = nullptr;
                return nullptr;
            }

            if (!widget->winId())
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "No window for video output.");
                delete vo;
                vo = nullptr;
                return nullptr;
            }

            // determine the display rectangle
            QRect display_rect = QRect(0, 0, widget->width(), widget->height());
            if (pipState == kPIPStandAlone)
                display_rect = embed_rect;

            vo->SetPIPState(pipState);
            vo->SetVideoFrameRate(video_prate);
            vo->SetReferenceFrames(ReferenceFrames);
            if (vo->Init(
                    video_dim_buf, video_dim_disp, video_aspect,
                    widget->winId(), display_rect, codec_id))
            {
                vo->SetVideoScalingAllowed(true);
                return vo;
            }

            vo->m_dbDisplayProfile = nullptr;
            delete vo;
            vo = nullptr;
        }
        else if (vo && (playerFlags & kVideoIsNull))
        {
            if (vo->Init(video_dim_buf, video_dim_disp,
                         video_aspect, 0, QRect(), codec_id))
            {
                return vo;
            }

            vo->m_dbDisplayProfile = nullptr;
            delete vo;
            vo = nullptr;
        }

        renderer = VideoDisplayProfile::GetBestVideoRenderer(renderers);
    }

    LOG(VB_GENERAL, LOG_ERR, LOC +
        "Not compiled with any useable video output method.");
    delete vprof;

    return nullptr;
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
 *         vo->ProcessFrame(frame, osd, filters, pict-in-pict, scan);
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
    m_dbDisplayDimensions(0,0),
    m_dbAspectOverride(kAspect_Off), m_dbAdjustFill(kAdjustFill_Off),
    m_dbLetterboxColour(kLetterBoxColour_Black),

    // Video parameters
    m_videoCodecID(kCodec_NONE),
    m_maxReferenceFrames(16),
    m_dbDisplayProfile(nullptr),

    // Various state variables
    m_errorState(kError_None),            m_framesPlayed(0),

    // Custom display resolutions
    m_displayRes(nullptr),

    // Physical display
    m_monitorSize(640,480),                m_monitorDimensions(400,300),

    // Visualisation
    m_visual(nullptr),

    // 3D TV
    m_stereo(kStereoscopicModeNone),

    m_deinterlacer()
{
    m_dbDisplayDimensions = QSize(gCoreContext->GetNumSetting("DisplaySizeWidth",  0),
                           gCoreContext->GetNumSetting("DisplaySizeHeight", 0));

    m_dbAspectOverride = static_cast<AspectOverrideMode>(gCoreContext->GetNumSetting("AspectOverride", 0));
    m_dbAdjustFill = static_cast<AdjustFillMode>(gCoreContext->GetNumSetting("AdjustFill", 0));
    m_dbLetterboxColour = static_cast<LetterBoxColour>(gCoreContext->GetNumSetting("LetterboxColour", 0));

    m_dbDisplayProfile = nullptr;
}

/**
 * \fn VideoOutput::~VideoOutput()
 * \brief Shuts down video output.
 */
VideoOutput::~VideoOutput()
{
    delete m_dbDisplayProfile;

    ResizeForGui();
    if (m_displayRes)
        DisplayRes::Unlock();
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

    m_videoCodecID = codec_id;
    bool wasembedding = m_window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = m_window.GetEmbeddingRect();
        StopEmbedding();
    }

    bool mainSuccess = m_window.Init(video_dim_buf, video_dim_disp,
                                   aspect, win_rect,
                                   m_dbAspectOverride, m_dbAdjustFill);

    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetInput(m_window.GetVideoDispDim());

    if (wasembedding)
        EmbedInWidget(oldrect);

    VideoAspectRatioChanged(aspect); // apply aspect ratio and letterbox mode

    return mainSuccess;
}

void VideoOutput::InitOSD(OSD *osd)
{
    if (m_dbDisplayProfile && !m_dbDisplayProfile->IsOSDFadeEnabled())
        osd->DisableFade();
}

QString VideoOutput::GetFilters(void) const
{
    if (m_dbDisplayProfile)
        return m_dbDisplayProfile->GetFilters();
    return QString();
}

void VideoOutput::SetVideoFrameRate(float playback_fps)
{
    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetOutput(playback_fps);
}

void VideoOutput::SetReferenceFrames(int ReferenceFrames)
{
    m_maxReferenceFrames = ReferenceFrames;
}

void VideoOutput::SetDeinterlacing(bool Enable, bool DoubleRate)
{
    if (!Enable)
    {
        m_videoBuffers.SetDeinterlacing(DEINT_NONE, DEINT_NONE, m_videoCodecID);
        return;
    }

    MythDeintType singlerate = DEINT_HIGH | DEINT_CPU | DEINT_SHADER | DEINT_DRIVER;
    MythDeintType doublerate = DoubleRate ? DEINT_HIGH | DEINT_CPU | DEINT_SHADER | DEINT_DRIVER : DEINT_NONE;
    //if (db_vdisp_profile)
    //{
    //    singlerate = db_vdisp_profile->GetFilteredDeint();
    //    doublerate = DoubleRate ? db_vdisp_profile->GetFilteredDeint(true) : DEINT_NONE;
    //}
    LOG(VB_GENERAL, LOG_INFO, LOC + QString("SetDeinterlacing: %1 DoubleRate %2")
        .arg(DeinterlacerPref(singlerate)).arg(DeinterlacerPref(doublerate)));
    m_videoBuffers.SetDeinterlacing(singlerate, doublerate, m_videoCodecID);
}

/**
 * \fn VideoOutput::VideoAspectRatioChanged(float aspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutput::VideoAspectRatioChanged(float aspect)
{
    m_window.VideoAspectRatioChanged(aspect);
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
                               bool        &/*aspect_only*/,
                               MythMultiLocker*,
                               int          ReferenceFrames)
{
    m_window.InputChanged(video_dim_buf, video_dim_disp, aspect);
    m_maxReferenceFrames = ReferenceFrames;
    AVCodecID avCodecId = myth2av_codecid(myth_codec_id);
    AVCodec *codec = avcodec_find_decoder(avCodecId);
    QString codecName;
    if (codec)
        codecName = codec->name;
    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetInput(m_window.GetVideoDispDim(),0,codecName);
    m_videoCodecID = myth_codec_id;
    DiscardFrames(true);
    return true;
}
/**
* \brief Resize Display Window
*/
void VideoOutput::ResizeDisplayWindow(const QRect &rect, bool save_visible_rect)
{
    m_window.ResizeDisplayWindow(rect, save_visible_rect);
}

/**
 * \brief Tells video output to embed video in an existing window.
 * \sa StopEmbedding()
 */
void VideoOutput::EmbedInWidget(const QRect &rect)
{
    m_window.EmbedInWidget(rect);
}

/**
 * \fn VideoOutput::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(const QRect&)
 */
void VideoOutput::StopEmbedding(void)
{
    m_window.StopEmbedding();
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
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 * \param themeaspect    aspect ration of the theme
 */
QRect VideoOutput::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling, float themeaspect) const
{
    QRect dvr = m_window.GetDisplayVisibleRect();
    float dispPixelAdj = 1.0F;
    if (dvr.height() && dvr.width())
        dispPixelAdj = (GetDisplayAspect() * dvr.height()) / dvr.width();

    float ova = m_window.GetOverridenVideoAspect();
    QRect vr = m_window.GetVideoRect();
    float vs = vr.height() ? static_cast<float>(vr.width()) / vr.height() : 1.0F;
    visible_aspect = themeaspect * (ova > 0.0f ? vs / ova : 1.F) * dispPixelAdj;

    font_scaling   = 1.0F;
    return { QPoint(0,0), dvr.size() };
}

/**
 * \fn VideoOutput::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutput::GetTotalOSDBounds(void) const
{
    return m_window.GetDisplayVisibleRect();
}

QRect VideoOutput::GetMHEGBounds(void)
{
    return m_window.GetDisplayVideoRect();
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
    m_window.MoveResize();
}

/**
 * \fn VideoOutput::Zoom(ZoomDirection)
 * \brief Sets up zooming into to different parts of the video, the zoom
 *        is actually applied in MoveResize().
 * \sa ToggleAdjustFill(AdjustFillMode)
 */
void VideoOutput::Zoom(ZoomDirection direction)
{
    m_window.Zoom(direction);
}

/**
 * \fn VideoOutput::ToogleMoveBottomLine(void)
 * \brief Toggle "zooming" the bottomline (sports tickers) off the screen.
 *        Applied in MoveResize().
 */
void VideoOutput::ToggleMoveBottomLine(void)
{
    m_window.ToggleMoveBottomLine();
}

/**
 * \fn VideoOutput::SaveBottomLine(void)
 * \brief Save current Manual Zoom settings as BottomLine adjustment.
 */
void VideoOutput::SaveBottomLine(void)
{
    m_window.SaveBottomLine();
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
    m_window.ToggleAspectOverride(aspectMode);
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
    m_window.ToggleAdjustFill(adjustFill);
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

    if ((kPictureAttribute_Range == attributeType) && newVal > 1)
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
    return m_videoColourSpace.SetPictureAttribute(attribute, newValue);
}

int VideoOutput::GetPictureAttribute(PictureAttribute attributeType)
{
    return m_videoColourSpace.GetPictureAttribute(attributeType);
}

/**
\ brief return OSD renderer type for this videoOutput
*/
QString VideoOutput::GetOSDRenderer(void) const
{
    return m_dbDisplayProfile->GetOSDRenderer();
}

/*
 * \brief Determines PIP Window size and Position.
 */
QRect VideoOutput::GetPIPRect(
    PIPLocation location, MythPlayer *pipplayer, bool do_pixel_adj) const
{
    return m_window.GetPIPRect(location, pipplayer, do_pixel_adj);
}

void VideoOutput::ShowPIPs(VideoFrame *frame, const PIPMap &pipPlayers)
{
    PIPMap::const_iterator it = pipPlayers.begin();
    for (; it != pipPlayers.end(); ++it)
        ShowPIP(frame, it.key(), *it);
}

AspectOverrideMode VideoOutput::GetAspectOverride(void) const
{
    return m_window.GetAspectOverride();
}

AdjustFillMode VideoOutput::GetAdjustFill(void) const
{
    return m_window.GetAdjustFill();
}

float VideoOutput::GetDisplayAspect(void) const
{
    return m_window.GetDisplayAspect();
}

void VideoOutput::ClearDummyFrame(VideoFrame *frame)
{
    // used by render devices to ignore frame rendering
    if (frame)
        frame->dummy = 1;
    // will only clear frame in main memory
    clear(frame);
}

void VideoOutput::SetVideoResize(const QRect &VideoRect)
{
    m_window.SetITVResize(VideoRect);
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutput::SetVideoScalingAllowed(bool change)
{
    m_window.SetVideoScalingAllowed(change);
}

bool VideoOutput::EnableVisualisation(AudioPlayer *audio, bool enable,
                                      const QString &name)
{
    if (!enable)
    {
        DestroyVisualisation();
        return false;
    }
    return SetupVisualisation(audio, nullptr, name);
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
    m_visual = nullptr;
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
    if (to == nullptr || from == nullptr)
        return;

    to->frameNumber = from->frameNumber;
    to->disp_timecode = from->disp_timecode;

    copy(to, from);
}

QRect VideoOutput::GetImageRect(const QRect &rect, QRect *display)
{
    qreal hscale = 0.0;
    QSize video_size   = m_window.GetVideoDispDim();
    int image_height   = video_size.height();
    int image_width    = (image_height > 720) ? 1920 :
                         (image_height > 576) ? 1280 : 720;
    qreal image_aspect = static_cast<qreal>(image_width) / image_height;
    qreal pixel_aspect = static_cast<qreal>(video_size.width()) / video_size.height();

    QRect rect1 = rect;
    if (display && display->isValid())
    {
        QMatrix m0;
        m0.scale(static_cast<qreal>(image_width)  / display->width(),
                 static_cast<qreal>(image_height) / display->height());
        rect1 = m0.mapRect(rect1);
        rect1.translate(display->left(), display->top());
    }
    QRect result = rect1;

    QRect dvr_rec = m_window.GetDisplayVideoRect();
    QRect vid_rec = m_window.GetVideoRect();

    hscale = image_aspect / pixel_aspect;
    if (hscale < 0.99 || hscale > 1.01)
    {
        vid_rec.setLeft(static_cast<int>(lround(static_cast<qreal>(vid_rec.left()) * hscale)));
        vid_rec.setWidth(static_cast<int>(lround(static_cast<qreal>(vid_rec.width()) * hscale)));
    }

    qreal vscale = static_cast<qreal>(dvr_rec.width()) / image_width;
    hscale = static_cast<qreal>(dvr_rec.height()) / image_height;
    QMatrix m1;
    m1.translate(dvr_rec.left(), dvr_rec.top());
    m1.scale(vscale, hscale);

    vscale = static_cast<qreal>(image_width) / vid_rec.width();
    hscale = static_cast<qreal>(image_height) / vid_rec.height();
    QMatrix m2;
    m2.scale(vscale, hscale);
    m2.translate(-vid_rec.left(), -vid_rec.top());

    result = m2.mapRect(result);
    result = m1.mapRect(result);
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
    static const float safeMargin = 0.05F;
    float dummy;
    QRect result = GetVisibleOSDBounds(dummy, dummy, 1.0F);
    int safex = static_cast<int>(static_cast<float>(result.width())  * safeMargin);
    int safey = static_cast<int>(static_cast<float>(result.height()) * safeMargin);
    return { result.left() + safex, result.top() + safey,
             result.width() - (2 * safex), result.height() - (2 * safey) };
}

void VideoOutput::SetPIPState(PIPState setting)
{
    m_window.SetPIPState(setting);
}

VideoFrameType* VideoOutput::DirectRenderFormats(void)
{
    static VideoFrameType defaultformats[] = { FMT_YV12, FMT_NONE };
    return &defaultformats[0];
}

bool VideoOutput::ReAllocateFrame(VideoFrame *Frame, VideoFrameType Type)
{
    return m_videoBuffers.ReinitBuffer(Frame, Type, m_videoCodecID);
}

bool VideoOutput::IsEmbedding(void)
{
    return m_window.IsEmbedding();
}

/**
 * \fn VideoOutput::ResizeForGui(void)
 * If we are using DisplayRes support, return the screen size and
 * refresh rate to those used for the GUI.
 */
void VideoOutput::ResizeForGui(void)
{
    if (m_displayRes)
        m_displayRes->SwitchToGUI();
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
void VideoOutput::ResizeForVideo(int width, int height)
{
    if (!m_displayRes)
        return;

    if (!width || !height)
    {
        width  = m_window.GetVideoDispDim().width();
        height = m_window.GetVideoDispDim().height();
        if (!width || !height)
            return;
    }

    float rate = m_dbDisplayProfile ? m_dbDisplayProfile->GetOutput() : 0.0f;
    if (m_displayRes && m_displayRes->SwitchToVideo(width, height, static_cast<double>(rate)))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        m_window.SetDisplayDim(QSize(m_displayRes->GetPhysicalWidth(),
                                       m_displayRes->GetPhysicalHeight()));
        m_window.SetDisplayAspect(static_cast<float>(m_displayRes->GetAspectRatio()));

        bool fullscreen = !m_window.UsingGuiSize();

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0, gui_height = 0;
            gCoreContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize sz(m_displayRes->GetWidth(), m_displayRes->GetHeight());
            const QRect display_visible_rect =
                    QRect(GetMythMainWindow()->geometry().topLeft(), sz);
            m_window.SetDisplayVisibleRect(display_visible_rect);
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
void VideoOutput::InitDisplayMeasurements(int width, int height, bool resize)
{
    DisplayInfo disp = MythDisplay::GetDisplayInfo();
    QString     source = "Actual";

    // The very first Resize needs to be the maximum possible
    // desired res, because X will mask off anything outside
    // the initial dimensions
    QSize sz1 = disp.m_res;
    QSize sz2 = m_window.GetScreenGeometry().size();
    QSize max_size = sz1.expandedTo(sz2);

    if (m_window.UsingGuiSize())
        max_size = GetMythMainWindow()->geometry().size();

    if (m_displayRes)
    {
        max_size.setWidth(m_displayRes->GetMaxWidth());
        max_size.setHeight(m_displayRes->GetMaxHeight());
    }

    if (resize)
    {
        MoveResizeWindow(QRect(GetMythMainWindow()->geometry().x(),
                               GetMythMainWindow()->geometry().y(),
                               max_size.width(), max_size.height()));
    }

    // get the physical dimensions (in mm) of the display. If using
    // DisplayRes, this will be overridden when we call ResizeForVideo
    if (m_dbDisplayDimensions.isEmpty())
    {
        m_window.SetDisplayDim(disp.m_size);
    }
    else
    {
        m_window.SetDisplayDim(m_dbDisplayDimensions);
        source = "Database";
    }

    // Set the display mode if required
    if (m_displayRes)
        ResizeForVideo(width, height);

    // Determine window and screen dimensions in pixels
    QSize screen_size = disp.m_res;
    QSize window_size = m_window.GetDisplayVisibleRect().size();

    float pixel_aspect = static_cast<float>(screen_size.width()) / screen_size.height();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(screen_size.width()).arg(screen_size.height())
            .arg(window_size.width()).arg(window_size.height()));

    // Check the display dimensions
    QSize disp_dim = m_window.GetDisplayDim();
    float disp_aspect;

    // If we are using Xinerama the display dimensions cannot be trusted.
    // We need to use the Xinerama monitor aspect ratio from the DB to set
    // the physical screen width. This assumes the height is correct, which
    // is more or less true in the typical side-by-side monitor setup.
    if (m_window.UsingXinerama())
    {
        source = "Xinerama";
        disp_aspect = static_cast<float>(gCoreContext->GetFloatSettingOnHost(
            "XineramaMonitorAspectRatio",
            gCoreContext->GetHostName(), static_cast<double>(pixel_aspect)));
        if (disp_dim.height() <= 0)
            disp_dim.setHeight(300);
        disp_dim.setWidth(static_cast<int>(lroundf(disp_dim.height() * disp_aspect)));
    }

    if (disp_dim.isEmpty())
    {
        source = "Guessed!";
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Physical size of display unknown."
                "\n\t\t\tAssuming 17\" monitor with square pixels.");
        disp_dim = QSize(static_cast<int>(lroundf(300 * pixel_aspect)), 300);
    }

    disp_aspect = static_cast<float>(disp_dim.width()) / disp_dim.height();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("%1 display dimensions: %2x%3 mm  Aspect: %4")
            .arg(source).arg(disp_dim.width()).arg(disp_dim.height())
            .arg(static_cast<double>(disp_aspect)));

    // Save the unscaled size and dimensions for window resizing
    m_monitorSize  = screen_size;
    m_monitorDimensions = disp_dim;

    // We must now scale the display measurements to our window size and save
    // them. If we are running fullscreen this is a no-op.
    disp_dim = QSize((disp_dim.width()  * window_size.width()) /
                      screen_size.width(),
                     (disp_dim.height() * window_size.height()) /
                      screen_size.height());
    disp_aspect = static_cast<float>(disp_dim.width()) / disp_dim.height();
    m_window.SetDisplayDim(disp_dim);
    m_window.SetDisplayAspect(disp_aspect);

    // If we are using XRandR, use the aspect ratio from it
    if (m_displayRes)
        m_window.SetDisplayAspect(static_cast<float>(m_displayRes->GetAspectRatio()));

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Estimated window dimensions: %1x%2 mm  Aspect: %3")
            .arg(m_window.GetDisplayDim().width())
            .arg(m_window.GetDisplayDim().height())
            .arg(static_cast<double>(m_window.GetDisplayAspect())));
}

int VideoOutput::CalcHueBase(const QString &adaptor_name)
{
    int hue_adj = 50;

    QString lower = adaptor_name.toLower();
    // Hue base for different adaptors
    // This can probably now be removed as it is only relevant to VAAPI
    // which always uses 50
    if (lower.contains("radeon") ||
        lower.contains("g3d") ||
        lower.contains("xvba") /* VAAPI */ ||
        lower.startsWith("intel") ||
        lower.contains("splitted"))
    {
        hue_adj = 50;
    }
    else if (lower.startsWith("nv17")) /* nVidia */
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
