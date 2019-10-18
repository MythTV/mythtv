// Qt
#include <QRunnable>

// MythTV
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

// std
#include <cmath>
#include <cstdlib>

#define LOC QString("VideoOutput: ")

void VideoOutput::GetRenderOptions(RenderOptions &Options)
{
    VideoOutputNull::GetRenderOptions(Options);

#ifdef _WIN32
    VideoOutputD3D::GetRenderOptions(Options);
#endif

#ifdef USING_OPENGL_VIDEO
    VideoOutputOpenGL::GetRenderOptions(Options);
#endif // USING_OPENGL_VIDEO
}

/**
 * \brief  Depending on compile-time configure settings and run-time
 *         renderer settings, create a relevant VideoOutput subclass.
 * \return instance of VideoOutput if successful, nullptr otherwise.
 */
VideoOutput *VideoOutput::Create(const QString &Decoder,    MythCodecID CodecID,
                                 PIPState PiPState,         const QSize &VideoDim,
                                 const QSize &VideoDispDim, float VideoAspect,
                                 QWidget *ParentWidget,     const QRect &EmbedRect,
                                 float FrameRate,           uint  PlayerFlags,
                                 QString &Codec,            int ReferenceFrames)
{
    QStringList renderers;

    // select the best available output
    if (PlayerFlags & kVideoIsNull)
    {
        // plain null output
        renderers += "null";
    }
    else
    {
#ifdef _WIN32
        renderers += VideoOutputD3D::GetAllowedRenderers(CodecID, VideoDispDim);
#endif

#ifdef USING_OPENGL_VIDEO
        renderers += VideoOutputOpenGL::GetAllowedRenderers(CodecID, VideoDispDim);
#endif // USING_OPENGL_VIDEO
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers: %1").arg(renderers.join(",")));
    renderers = VideoDisplayProfile::GetFilteredRenderers(Decoder, renderers);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers (filt: %1): %2")
        .arg(Decoder).arg(renderers.join(",")));

    QString renderer;

    VideoDisplayProfile *vprof = new VideoDisplayProfile();

    if (!renderers.empty())
    {
        vprof->SetInput(VideoDispDim, FrameRate, Codec);
        QString tmp = vprof->GetVideoRenderer();
        if (vprof->IsDecoderCompatible(Decoder) && renderers.contains(tmp))
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

        if (vo && !(PlayerFlags & kVideoIsNull))
        {
            // ensure we have a window to display into
            QWidget *widget = ParentWidget;
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
            if (PiPState == kPIPStandAlone)
                display_rect = EmbedRect;

            vo->SetPIPState(PiPState);
            vo->SetVideoFrameRate(FrameRate);
            vo->SetReferenceFrames(ReferenceFrames);
            if (vo->Init(VideoDim, VideoDispDim, VideoAspect,
                         widget->winId(), display_rect, CodecID))
            {
                vo->SetVideoScalingAllowed(true);
                return vo;
            }

            vo->m_dbDisplayProfile = nullptr;
            delete vo;
            vo = nullptr;
        }
        else if (vo && (PlayerFlags & kVideoIsNull))
        {
            if (vo->Init(VideoDim, VideoDispDim, VideoAspect, 0, QRect(), CodecID))
                return vo;

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
VideoOutput::VideoOutput()
  : m_dbDisplayDimensionsMM(0,0),
    m_dbAspectOverride(kAspect_Off),
    m_dbAdjustFill(kAdjustFill_Off),
    m_dbLetterboxColour(kLetterBoxColour_Black),
    m_videoCodecID(kCodec_NONE),
    m_maxReferenceFrames(16),
    m_dbDisplayProfile(nullptr),
    m_errorState(kError_None),
    m_framesPlayed(0),
    m_displayRes(nullptr),
    m_monitorSize(640,480),
    m_monitorDimensions(400,300),
    m_visual(nullptr),
    m_stereo(kStereoscopicModeNone),
    m_deinterlacer()
{
    m_dbDisplayDimensionsMM = QSize(gCoreContext->GetNumSetting("DisplaySizeWidth",  0),
                                    gCoreContext->GetNumSetting("DisplaySizeHeight", 0));
    m_dbAspectOverride  = static_cast<AspectOverrideMode>(gCoreContext->GetNumSetting("AspectOverride", 0));
    m_dbAdjustFill      = static_cast<AdjustFillMode>(gCoreContext->GetNumSetting("AdjustFill", 0));
    m_dbLetterboxColour = static_cast<LetterBoxColour>(gCoreContext->GetNumSetting("LetterboxColour", 0));
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
bool VideoOutput::Init(const QSize &VideoDim, const QSize &VideoDispDim,
                       float VideoAspect, WId WinID,
                       const QRect &WindowRect, MythCodecID CodecID)
{
    (void)WinID;

    m_videoCodecID = CodecID;
    bool wasembedding = m_window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = m_window.GetEmbeddingRect();
        StopEmbedding();
    }

    bool mainSuccess = m_window.Init(VideoDim, VideoDispDim,
                                     VideoAspect, WindowRect,
                                     m_dbAspectOverride, m_dbAdjustFill);

    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetInput(m_window.GetVideoDispDim());

    if (wasembedding)
        EmbedInWidget(oldrect);

    VideoAspectRatioChanged(VideoAspect); // apply aspect ratio and letterbox mode

    return mainSuccess;
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

MythDeintType VideoOutput::ParseDeinterlacer(const QString &Deinterlacer)
{
    MythDeintType result = DEINT_NONE;

    if (Deinterlacer.contains(DEINT_QUALITY_HIGH))
        result = DEINT_HIGH;
    else if (Deinterlacer.contains(DEINT_QUALITY_MEDIUM))
        result = DEINT_MEDIUM;
    else if (Deinterlacer.contains(DEINT_QUALITY_LOW))
        result = DEINT_BASIC;

    if (result != DEINT_NONE)
    {
        result = result | DEINT_CPU; // NB always assumed
        if (Deinterlacer.contains(DEINT_QUALITY_SHADER))
            result = result | DEINT_SHADER;
        if (Deinterlacer.contains(DEINT_QUALITY_DRIVER))
            result = result | DEINT_DRIVER;
    }

    return result;
}

void VideoOutput::SetDeinterlacing(bool Enable, bool DoubleRate)
{
    if (!Enable)
    {
        m_videoBuffers.SetDeinterlacing(DEINT_NONE, DEINT_NONE, m_videoCodecID);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled all deinterlacing");
        return;
    }

    MythDeintType singlerate = DEINT_NONE;
    MythDeintType doublerate = DEINT_NONE;
    if (m_dbDisplayProfile)
    {
        singlerate = ParseDeinterlacer(m_dbDisplayProfile->GetSingleRatePreferences());
        if (DoubleRate)
            doublerate = ParseDeinterlacer(m_dbDisplayProfile->GetDoubleRatePreferences());
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("SetDeinterlacing (Doublerate %1): Single %2 Double %3")
        .arg(DoubleRate).arg(DeinterlacerPref(singlerate)).arg(DeinterlacerPref(doublerate)));
    m_videoBuffers.SetDeinterlacing(singlerate, doublerate, m_videoCodecID);
}

/**
 * \fn VideoOutput::VideoAspectRatioChanged(float VideoAspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutput::VideoAspectRatioChanged(float VideoAspect)
{
    m_window.VideoAspectRatioChanged(VideoAspect);
}

/**
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool VideoOutput::InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                               float VideoAspect, MythCodecID  CodecID,
                               bool  &/*AspectOnly*/, MythMultiLocker*,
                               int   ReferenceFrames)
{
    m_window.InputChanged(VideoDim, VideoDispDim, VideoAspect);
    m_maxReferenceFrames = ReferenceFrames;
    AVCodecID avCodecId = myth2av_codecid(CodecID);
    AVCodec *codec = avcodec_find_decoder(avCodecId);
    QString codecName;
    if (codec)
        codecName = codec->name;
    if (m_dbDisplayProfile)
        m_dbDisplayProfile->SetInput(m_window.GetVideoDispDim(), 0 ,codecName);
    m_videoCodecID = CodecID;
    DiscardFrames(true, true);
    return true;
}
/**
* \brief Resize Display Window
*/
void VideoOutput::ResizeDisplayWindow(const QRect &Rect, bool SaveVisible)
{
    m_window.ResizeDisplayWindow(Rect, SaveVisible);
}

/**
 * \brief Tells video output to embed video in an existing window.
 * \sa StopEmbedding()
 */
void VideoOutput::EmbedInWidget(const QRect &EmbedRect)
{
    m_window.EmbedInWidget(EmbedRect);
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

void VideoOutput::GetOSDBounds(QRect &Total, QRect &Visible,
                               float &VisibleAspect,
                               float &FontScaling,
                               float ThemeAspect) const
{
    Total = GetTotalOSDBounds();
    Visible = GetVisibleOSDBounds(VisibleAspect, FontScaling, ThemeAspect);
}

/**
 * \brief Returns visible portions of total OSD bounds
 * \param VisibleAspect physical aspect ratio of bounds returned
 * \param FontScaling   scaling to apply to fonts
 * \param ThemeAspect   aspect ration of the theme
 */
QRect VideoOutput::GetVisibleOSDBounds(float &VisibleAspect,
                                       float &FontScaling,
                                       float ThemeAspect) const
{
    QRect dvr = m_window.GetDisplayVisibleRect();
    float dispPixelAdj = 1.0F;
    if (dvr.height() && dvr.width())
        dispPixelAdj = (GetDisplayAspect() * dvr.height()) / dvr.width();

    float ova = m_window.GetOverridenVideoAspect();
    QRect vr = m_window.GetVideoRect();
    float vs = vr.height() ? static_cast<float>(vr.width()) / vr.height() : 1.0F;
    VisibleAspect = ThemeAspect * (ova > 0.0f ? vs / ova : 1.F) * dispPixelAdj;

    FontScaling   = 1.0F;
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
void VideoOutput::Zoom(ZoomDirection Direction)
{
    m_window.Zoom(Direction);
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
void VideoOutput::ToggleAspectOverride(AspectOverrideMode AspectMode)
{
    m_window.ToggleAspectOverride(AspectMode);
}

/**
 * \fn VideoOutput::ToggleAdjustFill(AdjustFillMode)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls MoveResize()
 *        to apply them.
 * \sa Zoom(ZoomDirection), ToggleAspectOverride(AspectOverrideMode)
 */
void VideoOutput::ToggleAdjustFill(AdjustFillMode FillMode)
{
    m_window.ToggleAdjustFill(FillMode);
}

QString VideoOutput::GetZoomString(void) const
{
    return m_window.GetZoomString();
}

PictureAttributeSupported VideoOutput::GetSupportedPictureAttributes(void)
{
    return m_videoColourSpace.SupportedAttributes();
}

int VideoOutput::ChangePictureAttribute(PictureAttribute AttributeType, bool Direction)
{
    int curVal = GetPictureAttribute(AttributeType);
    if (curVal < 0)
        return -1;

    int newVal = curVal + ((Direction) ? +1 : -1);

    if (kPictureAttribute_Hue == AttributeType)
        newVal = newVal % 100;

    if ((kPictureAttribute_Range == AttributeType) && newVal > 1)
        newVal = 1;

    newVal = min(max(newVal, 0), 100);

    return SetPictureAttribute(AttributeType, newVal);
}

/**
 * \fn VideoOutput::SetPictureAttribute(PictureAttribute, int)
 * \brief Sets a specified picture attribute.
 * \param attribute Picture attribute to set.
 * \param newValue  Value to set attribute to.
 * \return Set value if it succeeds, -1 if it does not.
 */
int VideoOutput::SetPictureAttribute(PictureAttribute Attribute, int NewValue)
{
    return m_videoColourSpace.SetPictureAttribute(Attribute, NewValue);
}

int VideoOutput::GetPictureAttribute(PictureAttribute AttributeType)
{
    return m_videoColourSpace.GetPictureAttribute(AttributeType);
}

void VideoOutput::SetFramesPlayed(long long FramesPlayed)
{
    m_framesPlayed = FramesPlayed;
}

long long VideoOutput::GetFramesPlayed(void)
{
    return m_framesPlayed;
}

bool VideoOutput::IsErrored(void) const
{
    return m_errorState != kError_None;
}

VideoErrorState VideoOutput::GetError(void) const
{
    return m_errorState;
}

/// \brief Sets whether to use a normal number of buffers or fewer buffers.
void VideoOutput::SetPrebuffering(bool Normal)
{
    m_videoBuffers.SetPrebuffering(Normal);
}

/// \brief Tells video output to toss decoded buffers due to a seek
void VideoOutput::ClearAfterSeek(void)
{
    m_videoBuffers.ClearAfterSeek();
}

/// \brief Returns number of frames that are fully decoded.
int VideoOutput::ValidVideoFrames(void) const
{
    return static_cast<int>(m_videoBuffers.ValidVideoFrames());
}

/// \brief Returns number of frames available for decoding onto
int VideoOutput::FreeVideoFrames(void)
{
    return static_cast<int>(m_videoBuffers.FreeVideoFrames());
}

/// \brief Returns true iff enough frames are available to decode onto.
bool VideoOutput::EnoughFreeFrames(void)
{
    return m_videoBuffers.EnoughFreeFrames();
}

/// \brief Returns true iff there are plenty of decoded frames ready
///        for display.
bool VideoOutput::EnoughDecodedFrames(void)
{
    return m_videoBuffers.EnoughDecodedFrames();
}

/// \brief Returns true iff we have at least the minimum number of
///        decoded frames ready for display.
bool VideoOutput::EnoughPrebufferedFrames(void)
{
    return m_videoBuffers.EnoughPrebufferedFrames();
}

/// \brief returns QRect of PIP based on PIPLocation
QRect VideoOutput::GetPIPRect(PIPLocation Location, MythPlayer *PiPPlayer, bool DoPixelAdj) const
{
    return m_window.GetPIPRect(Location, PiPPlayer, DoPixelAdj);
}

void VideoOutput::ShowPIPs(VideoFrame *Frame, const PIPMap &PiPPlayers)
{
    PIPMap::const_iterator it = PiPPlayers.begin();
    for (; it != PiPPlayers.end(); ++it)
        ShowPIP(Frame, it.key(), *it);
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

/// \bug not implemented correctly. vpos is not updated.
VideoFrame* VideoOutput::GetLastDecodedFrame(void)
{
    return m_videoBuffers.GetLastDecodedFrame();
}

/// \brief Returns string with status of each frame for debugging.
QString VideoOutput::GetFrameStatus(void) const
{
    return m_videoBuffers.GetStatus();
}

/// \brief Returns frame from the head of the ready to be displayed queue,
///        if StartDisplayingFrame has been called.
VideoFrame* VideoOutput::GetLastShownFrame(void)
{
    return m_videoBuffers.GetLastShownFrame();
}

/// \brief Tells the player to resize the video frame (used for ITV)
void VideoOutput::SetVideoResize(const QRect &VideoRect)
{
    m_window.SetITVResize(VideoRect);
}

/**
 * \brief Disable or enable underscan/overscan
 */
void VideoOutput::SetVideoScalingAllowed(bool Allow)
{
    m_window.SetVideoScalingAllowed(Allow);
}

bool VideoOutput::EnableVisualisation(AudioPlayer *Audio, bool Enable,
                                      const QString &Name)
{
    if (!Enable)
    {
        DestroyVisualisation();
        return false;
    }
    return SetupVisualisation(Audio, nullptr, Name);
}

bool VideoOutput::CanVisualise(AudioPlayer *Audio, MythRender *Render)
{
    return VideoVisual::CanVisualise(Audio, Render);
}

bool VideoOutput::SetupVisualisation(AudioPlayer *Audio, MythRender *Render,
                                     const QString &Name)
{
    DestroyVisualisation();
    m_visual = VideoVisual::Create(Name, Audio, Render);
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
 * \param To   The destination frame.
 * \param From The source frame
 */
void VideoOutput::CopyFrame(VideoFrame *To, const VideoFrame *From)
{
    if (To == nullptr || From == nullptr)
        return;

    To->frameNumber = From->frameNumber;
    To->disp_timecode = From->disp_timecode;
    To->frameCounter = From->frameCounter;

    copy(To, From);
}

/// \brief translates caption/dvd button rectangle into 'screen' space
QRect VideoOutput::GetImageRect(const QRect &Rect, QRect *DisplayRect)
{
    qreal hscale = 0.0;
    QSize video_size   = m_window.GetVideoDispDim();
    int image_height   = video_size.height();
    int image_width    = (image_height > 720) ? 1920 :
                         (image_height > 576) ? 1280 : 720;
    qreal image_aspect = static_cast<qreal>(image_width) / image_height;
    qreal pixel_aspect = static_cast<qreal>(video_size.width()) / video_size.height();

    QRect rect1 = Rect;
    if (DisplayRect && DisplayRect->isValid())
    {
        QMatrix m0;
        m0.scale(static_cast<qreal>(image_width)  / DisplayRect->width(),
                 static_cast<qreal>(image_height) / DisplayRect->height());
        rect1 = m0.mapRect(rect1);
        rect1.translate(DisplayRect->left(), DisplayRect->top());
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

void VideoOutput::SetPIPState(PIPState Setting)
{
    m_window.SetPIPState(Setting);
}

VideoFrameType* VideoOutput::DirectRenderFormats(void)
{
    static VideoFrameType defaultformats[] = { FMT_YV12, FMT_NONE };
    return &defaultformats[0];
}

/**
 * \brief Blocks until it is possible to return a frame for decoding onto.
 */
VideoFrame* VideoOutput::GetNextFreeFrame(void)
{
    return m_videoBuffers.GetNextFreeFrame();
}

/// \brief Releases a frame from the ready for decoding queue onto the
///        queue of frames ready for display.
void VideoOutput::ReleaseFrame(VideoFrame *Frame)
{
    m_videoBuffers.ReleaseFrame(Frame);
}

/// \brief Releases a frame for reuse if it is in limbo.
void VideoOutput::DeLimboFrame(VideoFrame *Frame)
{
    m_videoBuffers.DeLimboFrame(Frame);
}

/// \brief Returns if videooutput is embedding
bool VideoOutput::IsEmbedding(void)
{
    return m_window.IsEmbedding();
}

/// \brief Tell GetLastShownFrame() to return the next frame from the head
///        of the queue of frames to display.
void VideoOutput::StartDisplayingFrame(void)
{
    m_videoBuffers.StartDisplayingFrame();
}

/// \brief Releases frame returned from GetLastShownFrame() onto the
///        queue of frames ready for decoding onto.
void VideoOutput::DoneDisplayingFrame(VideoFrame *Frame)
{
    m_videoBuffers.DoneDisplayingFrame(Frame);
}

/// \brief Releases frame from any queue onto the
///        queue of frames ready for decoding onto.
void VideoOutput::DiscardFrame(VideoFrame *Frame)
{
    m_videoBuffers.DiscardFrame(Frame);
}

/// \brief Releases all frames not being actively displayed from any queue
///        onto the queue of frames ready for decoding onto.
void VideoOutput::DiscardFrames(bool KeyFrame, bool)
{
    m_videoBuffers.DiscardFrames(KeyFrame);
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
void VideoOutput::ResizeForVideo(int Width, int Height)
{
    if (!m_displayRes)
        return;

    if (!Width || !Height)
    {
        Width  = m_window.GetVideoDispDim().width();
        Height = m_window.GetVideoDispDim().height();
        if (!Width || !Height)
            return;
    }

    float rate = m_dbDisplayProfile ? m_dbDisplayProfile->GetOutput() : 0.0f;
    if (m_displayRes && m_displayRes->SwitchToVideo(Width, Height, static_cast<double>(rate)))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        m_window.SetDisplayProperties(QSize(m_displayRes->GetPhysicalWidth(),
                                            m_displayRes->GetPhysicalHeight()),
                                      static_cast<float>(m_displayRes->GetAspectRatio()));
        m_window.SetWindowSize(QSize(m_displayRes->GetWidth(), m_displayRes->GetHeight()));

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
            QRect display_visible_rect = QRect(GetMythMainWindow()->geometry().topLeft(), sz);
            if (HasMythMainWindow())
                GetMythMainWindow()->MoveResize(display_visible_rect);
        }
    }
}

/**
 * \brief Init display measurements based on database settings and
 *        actual screen parameters.
 */
void VideoOutput::InitDisplayMeasurements(int Width, int Height)
{
    DisplayInfo disp = MythDisplay::GetDisplayInfo();
    QString     source = "Actual";

    // get the physical dimensions (in mm) of the display. If using
    // DisplayRes, this will be overridden when we call ResizeForVideo
    float disp_aspect = m_window.GetDisplayAspect();
    QSize disp_dim = m_dbDisplayDimensionsMM;
    if (disp_dim.isEmpty())
        disp_dim = disp.m_size;
    else
        source = "Database";
    m_window.SetDisplayProperties(disp_dim, disp_aspect);

    // Set the display mode if required
    if (m_displayRes)
    {
        ResizeForVideo(Width, Height);
        disp = MythDisplay::GetDisplayInfo();
    }

    // Determine window and screen dimensions in pixels
    QSize screen_size = disp.m_res;
    QSize window_size = m_window.GetWindowRect().size();

    if (screen_size.isEmpty())
        screen_size = window_size.isEmpty() ? QSize(1920, 1080): window_size;
    if (window_size.isEmpty())
        window_size = screen_size;

    float pixel_aspect = static_cast<float>(screen_size.width()) / screen_size.height();

    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Pixel dimensions: Screen %1x%2, window %3x%4")
            .arg(screen_size.width()).arg(screen_size.height())
            .arg(window_size.width()).arg(window_size.height()));

    // Check the display dimensions
    disp_aspect = m_window.GetDisplayAspect();
    disp_dim = m_window.GetDisplayDim();

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
        LOG(VB_GENERAL, LOG_WARNING, LOC +
            "Physical size of display unknown. Assuming 17\" monitor with square pixels.");
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

    // If we are using XRandR, use the aspect ratio from it
    if (m_displayRes)
        disp_aspect = static_cast<float>(m_displayRes->GetAspectRatio());

    m_window.SetDisplayProperties(disp_dim, disp_aspect);
}

///\note Probably no longer required
int VideoOutput::CalcHueBase(const QString &AdaptorName)
{
    int defaulthue = 50;

    QString lower = AdaptorName.toLower();
    // Hue base for different adaptors
    // This can probably now be removed as it is only relevant to VAAPI
    // which always uses 50
    if (lower.contains("radeon") ||
        lower.contains("g3d") ||
        lower.contains("xvba") /* VAAPI */ ||
        lower.startsWith("intel") ||
        lower.contains("splitted"))
    {
        defaulthue = 50;
    }
    else if (lower.startsWith("nv17")) /* nVidia */
    {
        defaulthue = 0;
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC +
            QString("CalcHueBase(%1): Unknown adaptor, hue may be wrong.")
            .arg(AdaptorName));
    }

    return defaulthue;
}
