// C/C++
#include <utility>

// MythTV
#include "mythcontext.h"
#include "mythmainwindow.h"
#include "mythplayer.h"
#include "videodisplayprofile.h"
#include "osd.h"
#include "mythuihelper.h"
#include "opengl/mythopenglperf.h"
#include "opengl/mythrenderopengl.h"
#include "opengl/mythpainteropengl.h"
#include "mythcodeccontext.h"
#include "mythopenglinterop.h"
#include "mythvideooutopengl.h"

#define LOC QString("VidOutGL: ")

/*! \brief Generate the list of available OpenGL profiles
 *
 * \note This could be improved by eliminating unsupported profiles at run time -
 * but it is currently called statically and hence options would be fixed and unable
 * to reflect changes in UI render device.
*/
void MythVideoOutputOpenGL::GetRenderOptions(RenderOptions &Options)
{
    QStringList safe;
    safe << "opengl" << "opengl-yv12";

    // all profiles can handle all software frames
    (*Options.safe_renderers)["dummy"].append(safe);
    (*Options.safe_renderers)["nuppel"].append(safe);
    if (Options.decoders->contains("ffmpeg"))
        (*Options.safe_renderers)["ffmpeg"].append(safe);
    if (Options.decoders->contains("mediacodec-dec"))
        (*Options.safe_renderers)["mediacodec-dec"].append(safe);
    if (Options.decoders->contains("vaapi-dec"))
        (*Options.safe_renderers)["vaapi-dec"].append(safe);
    if (Options.decoders->contains("vdpau-dec"))
        (*Options.safe_renderers)["vdpau-dec"].append(safe);
    if (Options.decoders->contains("nvdec-dec"))
        (*Options.safe_renderers)["nvdec-dec"].append(safe);
    if (Options.decoders->contains("vtb-dec"))
        (*Options.safe_renderers)["vtb-dec"].append(safe);
    if (Options.decoders->contains("v4l2-dec"))
        (*Options.safe_renderers)["v4l2-dec"].append(safe);
    if (Options.decoders->contains("mmal-dec"))
        (*Options.safe_renderers)["mmal-dec"].append(safe);

    // OpenGL UYVY
    Options.renderers->append("opengl");
    Options.priorities->insert("opengl", 65);

    // OpenGL YV12
    Options.renderers->append("opengl-yv12");
    Options.priorities->insert("opengl-yv12", 65);

#if defined(USING_VAAPI) || defined (USING_VTB) || defined (USING_MEDIACODEC) || defined (USING_VDPAU) || defined (USING_NVDEC) || defined (USING_MMAL) || defined (USING_V4L2PRIME) || defined (USING_EGL)
    Options.renderers->append("opengl-hw");
    (*Options.safe_renderers)["dummy"].append("opengl-hw");
    (*Options.safe_renderers)["nuppel"].append("opengl-hw");
    Options.priorities->insert("opengl-hw", 110);
#endif
#ifdef USING_VAAPI
    if (Options.decoders->contains("vaapi"))
        (*Options.safe_renderers)["vaapi"].append("opengl-hw");
#endif
#ifdef USING_VTB
    if (Options.decoders->contains("vtb"))
        (*Options.safe_renderers)["vtb"].append("opengl-hw");
#endif
#ifdef USING_MEDIACODEC
    if (Options.decoders->contains("mediacodec"))
        (*Options.safe_renderers)["mediacodec"].append("opengl-hw");
#endif
#ifdef USING_VDPAU
    if (Options.decoders->contains("vdpau"))
        (*Options.safe_renderers)["vdpau"].append("opengl-hw");
#endif
#ifdef USING_NVDEC
    if (Options.decoders->contains("nvdec"))
        (*Options.safe_renderers)["nvdec"].append("opengl-hw");
#endif
#ifdef USING_MMAL
    if (Options.decoders->contains("mmal"))
        (*Options.safe_renderers)["mmal"].append("opengl-hw");
#endif
#ifdef USING_V4L2PRIME
    if (Options.decoders->contains("v4l2"))
        (*Options.safe_renderers)["v4l2"].append("opengl-hw");
#endif
#ifdef USING_EGL
    if (Options.decoders->contains("drmprime"))
        (*Options.safe_renderers)["drmprime"].append("opengl-hw");
#endif
}

MythVideoOutputOpenGL::MythVideoOutputOpenGL(QString Profile)
  : m_videoProfile(std::move(Profile))
{
    // Retrieve render context
    m_render = MythRenderOpenGL::GetOpenGLRender();
    if (!m_render)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve OpenGL context");
        return;
    }

    // Retain and lock
    m_render->IncrRef();
    OpenGLLocker locker(m_render);

    // enable performance monitoring if requested
    // will report the execution time for the key GL code blocks
    // N.B. 'Upload' should always be zero when using hardware decoding and direct
    // rendering. Any copy cost for direct rendering will be included within 'Render'
    if (VERBOSE_LEVEL_CHECK(VB_GPUVIDEO, LOG_INFO))
    {
        m_openGLPerf = new MythOpenGLPerf("GLVidPerf: ", { "Upload:", "Clear:", "Render:", "Flush:", "Swap:" });
        if (!m_openGLPerf->isCreated())
        {
            delete m_openGLPerf;
            m_openGLPerf = nullptr;
        }
    }

    // Disallow unsupported video texturing on GLES2/GL1.X
    if (m_render->GetExtraFeatures() & kGLLegacyTextures)
        m_textureFormats = LegacyFormats;

    // Retrieve OpenGL painter
    MythMainWindow *win = MythMainWindow::getMainWindow();
    m_openGLPainter = dynamic_cast<MythOpenGLPainter*>(win->GetCurrentPainter());
    if (!m_openGLPainter)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get painter");
        return;
    }

    // we need to control buffer swapping
    m_openGLPainter->SetViewControl(MythOpenGLPainter::None);

    // Create OpenGLVideo
    QRect dvr = GetDisplayVisibleRect();
    m_openGLVideo = new MythOpenGLVideo(m_render, &m_videoColourSpace, m_window.GetVideoDim(),
                                        m_window.GetVideoDispDim(), dvr, m_window.GetDisplayVideoRect(),
                                        m_window.GetVideoRect(), true, m_videoProfile);

    // Connect VideoOutWindow to OpenGLVideo
    QObject::connect(&m_window, &VideoOutWindow::VideoSizeChanged,  m_openGLVideo, &MythOpenGLVideo::SetVideoDimensions);
    QObject::connect(&m_window, &VideoOutWindow::VideoRectsChanged, m_openGLVideo, &MythOpenGLVideo::SetVideoRects);
    QObject::connect(&m_window, &VideoOutWindow::WindowRectChanged, m_openGLVideo, &MythOpenGLVideo::SetViewportRect);
    QObject::connect(m_openGLVideo, &MythOpenGLVideo::OutputChanged, &m_window,    &VideoOutWindow::InputChanged);
}

MythVideoOutputOpenGL::~MythVideoOutputOpenGL()
{
    MythVideoOutputOpenGL::DestroyBuffers();
    while (!m_openGLVideoPiPs.empty())
    {
        delete *m_openGLVideoPiPs.begin();
        m_openGLVideoPiPs.erase(m_openGLVideoPiPs.begin());
    }
    m_openGLVideoPiPsReady.clear();
    if (m_openGLPainter)
        m_openGLPainter->SetViewControl(MythOpenGLPainter::Viewport | MythOpenGLPainter::Framebuffer);
    delete m_openGLVideo;
    if (m_render)
    {
        m_render->makeCurrent();
        delete m_openGLPerf;
        m_render->doneCurrent();
        m_render->DecrRef();
    }
    m_render = nullptr;
}

void MythVideoOutputOpenGL::DestroyBuffers(void)
{
    MythVideoOutputOpenGL::DiscardFrames(true, true);
    m_videoBuffers.DeleteBuffers();
    m_videoBuffers.Reset();
    m_buffersCreated = false;
}

bool MythVideoOutputOpenGL::Init(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect,
                                 MythDisplay *Display, const QRect &DisplayVisibleRect, MythCodecID CodecId)
{
    if (!m_render || !m_openGLPainter || !m_openGLVideo)
        return false;

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot initialise OpenGL video from this thread");
        return false;
    }

    OpenGLLocker ctx_lock(m_render);

    // if we are the main video player then free up as much video memory
    // as possible at startup
    PIPState pip = m_window.GetPIPState();
    if ((kCodec_NONE == m_newCodecId) && ((kPIPOff == pip) || (kPBPLeft == pip)))
        m_openGLPainter->FreeResources();

    // Default initialisation - mainly VideoOutWindow
    if (!MythVideoOutput::Init(VideoDim, VideoDispDim, Aspect, Display, DisplayVisibleRect, CodecId))
        return false;

    // Ensure any new profile preferences are handled after a stream change
    if (m_dbDisplayProfile)
        m_openGLVideo->SetProfile(m_dbDisplayProfile->GetVideoRenderer());

    // Set default support for picture attributes
    InitPictureAttributes();

    // Setup display
    QSize size = m_window.GetVideoDim();

    // Set the display mode if required
    if (m_display->UsingVideoModes() && !m_window.IsEmbedding())
        ResizeForVideo(size);
    InitDisplayMeasurements();

    // Create buffers
    if (!CreateBuffers(CodecId, m_window.GetVideoDim()))
        return false;

    // Adjust visible rect for embedding
    QRect dvr = GetDisplayVisibleRect();
    if (m_videoCodecID == kCodec_NONE)
    {
        m_render->SetViewPort(QRect(QPoint(), dvr.size()));
        return true;
    }

    if (m_window.GetPIPState() >= kPIPStandAlone)
    {
        QRect tmprect = QRect(QPoint(0,0), dvr.size());
        ResizeDisplayWindow(tmprect, true);
    }

    // Reset OpenGLVideo
    if (m_openGLVideo->IsValid())
        m_openGLVideo->ResetFrameFormat();

    // This works around an issue with VDPAU direct rendering using legacy drivers
    m_render->Flush();

    return true;
}

void MythVideoOutputOpenGL::SetVideoFrameRate(float NewRate)
{
    if (!m_dbDisplayProfile)
        return;

    if (qFuzzyCompare(m_dbDisplayProfile->GetOutput() + 1.0F, NewRate + 1.0F))
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video frame rate changed: %1->%2")
        .arg(static_cast<double>(m_dbDisplayProfile->GetOutput())).arg(static_cast<double>(NewRate)));
    m_dbDisplayProfile->SetOutput(NewRate);
    m_newFrameRate = true;
}

bool MythVideoOutputOpenGL::InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                                         float Aspect, MythCodecID CodecId, bool &AspectOnly,
                                         MythMultiLocker* /*Locks*/, int ReferenceFrames,
                                         bool ForceChange)
{
    QSize currentvideodim     = m_window.GetVideoDim();
    QSize currentvideodispdim = m_window.GetVideoDispDim();
    MythCodecID currentcodec  = m_videoCodecID;
    float currentaspect       = m_window.GetVideoAspect();

    if (m_newCodecId != kCodec_NONE)
    {
        // InputChanged has been called twice in quick succession without a call to ProcessFrame
        currentvideodim = m_newVideoDim;
        currentvideodispdim = m_newVideoDispDim;
        currentcodec = m_newCodecId;
        currentaspect = m_newAspect;
    }

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video changed: %1x%2 (%3x%4) '%5' (Aspect %6 Refs %13)"
                                             "-> %7x%8 (%9x%10) '%11' (Aspect %12 Refs %14)")
        .arg(currentvideodispdim.width()).arg(currentvideodispdim.height())
        .arg(currentvideodim.width()).arg(currentvideodim.height())
        .arg(toString(currentcodec)).arg(static_cast<double>(currentaspect))
        .arg(VideoDispDim.width()).arg(VideoDispDim.height())
        .arg(VideoDim.width()).arg(VideoDim.height())
        .arg(toString(CodecId)).arg(static_cast<double>(Aspect))
        .arg(m_maxReferenceFrames).arg(ReferenceFrames));

    bool cidchanged = (CodecId != currentcodec);
    bool reschanged = (VideoDispDim != currentvideodispdim);
    bool refschanged = m_maxReferenceFrames != ReferenceFrames;

    // aspect ratio changes are a no-op as changes are handled at display time
    if (!(cidchanged || reschanged || refschanged || ForceChange))
    {
        AspectOnly = true;
        return true;
    }

    // N.B. We no longer check for interop support for the new codec as it is a
    // poor substitute for a full check of decoder capabilities etc. Better to let
    // hardware decoding fail if necessary - which should at least fallback to
    // software decoding rather than bailing out here.

    // delete and recreate the buffers and flag that the input has changed
    m_maxReferenceFrames = ReferenceFrames;
    m_buffersCreated = m_videoBuffers.DiscardAndRecreate(CodecId, VideoDim, m_maxReferenceFrames);
    if (!m_buffersCreated)
        return false;

    m_newCodecId= CodecId;
    m_newVideoDim = VideoDim;
    m_newVideoDispDim = VideoDispDim;
    m_newAspect = Aspect;
    return true;
}

QRect MythVideoOutputOpenGL::GetDisplayVisibleRect(void)
{
    QRect dvr = m_window.GetDisplayVisibleRect();

    MythMainWindow *mainwin = GetMythMainWindow();
    if (!mainwin)
        return dvr;
    QSize size = mainwin->size();

    // may be called before m_window is initialised fully
    if (dvr.isEmpty())
        dvr = QRect(QPoint(0, 0), size);

    // If the Video screen mode has vertically less pixels
    // than the GUI screen mode - OpenGL coordinate adjustments
    // must be made to put the video at the top of the display
    // area instead of at the bottom.
    if (dvr.height() < size.height())
        dvr.setTop(dvr.top() - size.height() + dvr.height());

    // If the Video screen mode has horizontally less pixels
    // than the GUI screen mode - OpenGL width must be set
    // as the higher GUI width so that the Program Guide
    // invoked from playback is not cut off.
    if (dvr.width() < size.width())
        dvr.setWidth(size.width());

    return dvr;
}

bool MythVideoOutputOpenGL::CreateBuffers(MythCodecID CodecID, QSize Size)
{
    if (m_buffersCreated)
        return true;

    if (codec_is_copyback(CodecID))
    {
        m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_NONE), false, 1, 4, 2);
        return m_videoBuffers.CreateBuffers(FMT_YV12, Size.width(), Size.height());
    }

    if (codec_is_mediacodec(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_MEDIACODEC, Size, false, 1, 2, 2);
    if (codec_is_vaapi(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VAAPI, Size, false, 2, 1, 4, m_maxReferenceFrames);
    if (codec_is_vtb(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VTB, Size, false, 1, 4, 2);
    if (codec_is_vdpau(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VDPAU, Size, false, 2, 1, 4, m_maxReferenceFrames);
    if (codec_is_nvdec(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_NVDEC, Size, false, 2, 1, 4);
    if (codec_is_mmal(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_MMAL, Size, false, 2, 1, 4);
    if (codec_is_v4l2(CodecID) || codec_is_drmprime(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_DRMPRIME, Size, false, 2, 1, 4);

    return m_videoBuffers.CreateBuffers(FMT_YV12, Size, false, 1, 8, 4, m_maxReferenceFrames);
}

void MythVideoOutputOpenGL::ProcessFrame(VideoFrame *Frame, OSD */*osd*/,
                                         const PIPMap &PiPPlayers,
                                         FrameScanType Scan)
{
    if (!m_render)
        return;

    OpenGLLocker ctx_lock(m_render);

    // start the first timer
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    // Process input changes
    if (m_newCodecId != kCodec_NONE)
    {
        // Ensure we don't lose embedding through program changes.
        bool wasembedding = m_window.IsEmbedding();
        QRect oldrect;
        if (wasembedding)
        {
            oldrect = m_window.GetEmbeddingRect();
            StopEmbedding();
        }

        // Note - we don't call the default VideoOutput::InputChanged method as
        // the OpenGL implementation is asynchronous.
        // So we need to update the video display profile here. It is a little
        // circular as we need to set the video dimensions first which are then
        // reset in Init.
        // All told needs a cleanup - not least because the use of codecName appears
        // to be inconsistent.
        m_window.InputChanged(m_newVideoDim, m_newVideoDispDim, m_newAspect);
        AVCodecID avCodecId = myth2av_codecid(m_newCodecId);
        AVCodec *codec = avcodec_find_decoder(avCodecId);
        QString codecName;
        if (codec)
            codecName = codec->name;
        if (m_dbDisplayProfile)
            m_dbDisplayProfile->SetInput(m_window.GetVideoDispDim(), 0 , codecName);

        bool ok = Init(m_newVideoDim, m_newVideoDispDim, m_newAspect,
                       m_display, m_window.GetRawWindowRect(), m_newCodecId);
        m_newCodecId = kCodec_NONE;
        m_newVideoDim = QSize();
        m_newVideoDispDim = QSize();
        m_newAspect = 0.0F;
        m_newFrameRate = false;

        if (wasembedding && ok)
            EmbedInWidget(oldrect);

        // Update deinterlacers for any input change
        SetDeinterlacing(m_deinterlacing, m_deinterlacing2X, m_forcedDeinterlacer);

        if (!ok)
            return;
    }
    else if (m_newFrameRate)
    {
        // If we are switching mode purely for a refresh rate change, then there
        // is no need to recreate buffers etc etc
        ResizeForVideo();
        m_newFrameRate = false;
    }

    if (Frame)
        m_window.SetRotation(Frame->rotation);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PROCESS_FRAME_START");

    bool swframe = Frame ? !format_is_hw(Frame->codec) : false;
    bool dummy   = Frame ? Frame->dummy : false;

    // software deinterlacing
    if (!dummy && swframe)
        m_deinterlacer.Filter(Frame, Scan, m_dbDisplayProfile);

    if (!m_window.IsEmbedding())
    {
        m_openGLVideoPiPActive = nullptr;
        ShowPIPs(Frame, PiPPlayers);
    }

    if (m_openGLVideo && swframe && !dummy)
        m_openGLVideo->ProcessFrame(Frame, Scan);

    // time texture update
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PROCESS_FRAME_END");
}

void MythVideoOutputOpenGL::PrepareFrame(VideoFrame *Frame, FrameScanType Scan, OSD *Osd)
{
    if (!m_render)
        return;

    if (m_newCodecId != kCodec_NONE)
        return; // input changes need to be handled in ProcessFrame

    OpenGLLocker ctx_lock(m_render);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREPARE_FRAME_START");

    bool dummy = false;
    int topfieldfirst = 0;
    if (Frame)
    {
        m_framesPlayed = Frame->frameNumber + 1;
        topfieldfirst = Frame->interlaced_reversed ? !Frame->top_field_first : Frame->top_field_first;
        dummy = Frame->dummy;
    }
    else
    {
        // see VideoOutputOpenGL::DoneDisplayingFrame
        // we only retain pause frames for hardware formats
        if (m_videoBuffers.Size(kVideoBuffer_pause))
            Frame = m_videoBuffers.Tail(kVideoBuffer_pause);
    }

    // if process frame has not been called (double rate hardware deint), then
    // we need to start the first 2 performance timers here
    if (m_openGLPerf)
    {
        if (!m_openGLPerf->GetTimersRunning())
        {
            m_openGLPerf->RecordSample();
            m_openGLPerf->RecordSample();
        }
    }

    m_render->BindFramebuffer(nullptr);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "CLEAR_START");

    int gray = m_dbLetterboxColour == kLetterBoxColour_Gray25 ? 64 : 0;
    bool useclear = !Frame || dummy || m_render->GetExtraFeatures() & kGLTiled;
#if QT_VERSION < QT_VERSION_CHECK(5, 8, 0)
    // Qt < 5.8 uses a different QRegion API. Just clear and remove this code
    // when 5.8 is standard
    useclear = true;
#endif

    if (useclear)
    {
        m_render->SetBackground(gray, gray, gray, 255);
        m_render->ClearFramebuffer();
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    // avoid clearing the framebuffer if it will be entirely overwritten by video
    else if (!m_window.VideoIsFullScreen())
    {
        if (m_window.IsEmbedding())
        {
            // use MythRenderOpenGL rendering as it will clear to the appropriate 'black level'
            m_render->ClearRect(nullptr, m_window.GetWindowRect(), gray);
        }
        else
        {
            // in the vast majority of cases it is significantly quicker to just
            // clear the unused portions of the screen
            QRegion toclear = m_window.GetBoundingRegion();
            foreach (auto rect, toclear)
                m_render->ClearRect(nullptr, rect, gray);
        }
    }
#endif

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "CLEAR_END");

    // time framebuffer clearing
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    // stereoscopic views
    QRect main   = m_render->GetViewPort();
    QRect first  = main;
    QRect second = main;
    bool twopass = (m_stereo == kStereoscopicModeSideBySide) || (m_stereo == kStereoscopicModeTopAndBottom);

    if (kStereoscopicModeSideBySide == m_stereo)
    {
        first  = QRect(main.left() / 2,  main.top(), main.width() / 2, main.height());
        second = first.translated(main.width() / 2, 0);
    }
    else if (kStereoscopicModeTopAndBottom == m_stereo)
    {
        first  = QRect(main.left(),  main.top() / 2, main.width(), main.height() / 2);
        second = first.translated(0, main.height() / 2);
    }

    // main UI when embedded
    if (m_window.IsEmbedding())
    {
        // If we are using high dpi, the painter needs to set the appropriate
        // viewport and enable scaling of its images
        m_openGLPainter->SetViewControl(MythOpenGLPainter::Viewport);
        MythMainWindow *win = GetMythMainWindow();
        if (win && win->GetPaintWindow())
        {
            if (twopass)
                m_render->SetViewPort(first, true);
            win->GetPaintWindow()->clearMask();
            win->draw(m_openGLPainter);
            if (twopass)
            {
                m_render->SetViewPort(second, true);
                win->GetPaintWindow()->clearMask();
                win->draw(m_openGLPainter);
                m_render->SetViewPort(main, true);
            }
        }
        m_openGLPainter->SetViewControl(MythOpenGLPainter::None);
    }

    // video
    if (m_openGLVideo && !dummy)
    {
        m_openGLVideo->PrepareFrame(Frame, topfieldfirst, Scan, m_stereo);
    // dummy streams need the viewport updated in case we have resized the window
    }
    else if (dummy)
    {
        m_render->SetViewPort(m_window.GetWindowRect());
    }

    // PiPs/PBPs
    if (!m_openGLVideoPiPs.empty() && !m_window.IsEmbedding())
    {
        for (auto it = m_openGLVideoPiPs.begin(); it != m_openGLVideoPiPs.end(); ++it)
        {
            if (m_openGLVideoPiPsReady[it.key()])
            {
                bool active = m_openGLVideoPiPActive == *it;
                if (twopass)
                    m_render->SetViewPort(first, true);
                (*it)->PrepareFrame(nullptr, topfieldfirst, Scan, kStereoscopicModeNone, active);
                if (twopass)
                {
                    m_render->SetViewPort(second, true);
                    (*it)->PrepareFrame(nullptr, topfieldfirst, Scan, kStereoscopicModeNone, active);
                    m_render->SetViewPort(main);
                }
            }
        }
    }

    // visualisation
    if (m_visual && m_openGLPainter && !m_window.IsEmbedding())
    {
        if (twopass)
            m_render->SetViewPort(first, true);
        m_visual->Draw(GetTotalOSDBounds(), m_openGLPainter, nullptr);
        if (twopass)
        {
            m_render->SetViewPort(second, true);
            m_visual->Draw(GetTotalOSDBounds(), m_openGLPainter, nullptr);
            m_render->SetViewPort(main);
        }
    }

    // OSD
    if (Osd && m_openGLPainter && !m_window.IsEmbedding())
    {
        if (twopass)
            m_render->SetViewPort(first, true);
        Osd->Draw(m_openGLPainter, GetTotalOSDBounds().size(), true);
        if (twopass)
        {
            m_render->SetViewPort(second, true);
            Osd->Draw(m_openGLPainter, GetTotalOSDBounds().size(), true);
            m_render->SetViewPort(main);
        }
    }

    // time rendering
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    m_render->Flush();

    // time flush
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_render->logDebugMarker(LOC + "PREPARE_FRAME_END");
}

/*! \brief Release a video frame back into the decoder pool.
 *
 * Software frames do not need a pause frame as OpenGLVideo
 * holds a copy of the last frame in its input textures. So
 * just release the frame.
 *
 * Hardware frames hold the underlying interop class and
 * hence access to the video texture. We cannot access them
 * without a frame so retain the most recent frame by removing
 * it from the 'used' queue and adding it to the 'pause' queue.
*/
void MythVideoOutputOpenGL::DoneDisplayingFrame(VideoFrame *Frame)
{
    if (!Frame)
        return;

    bool retain = format_is_hw(Frame->codec);
    QVector<VideoFrame*> release;

    m_videoBuffers.BeginLock(kVideoBuffer_pause);
    while (m_videoBuffers.Size(kVideoBuffer_pause))
    {
        VideoFrame* frame = m_videoBuffers.Dequeue(kVideoBuffer_pause);
        if (!retain || (retain && (frame != Frame)))
            release.append(frame);
    }

    if (retain)
    {
        m_videoBuffers.Enqueue(kVideoBuffer_pause, Frame);
        if (m_videoBuffers.Contains(kVideoBuffer_used, Frame))
            m_videoBuffers.Remove(kVideoBuffer_used, Frame);
    }
    else
    {
        release.append(Frame);
    }
    m_videoBuffers.EndLock();

    for (auto * frame : release)
        m_videoBuffers.DoneDisplayingFrame(frame);
}

/*! \brief Discard video frames
 *
 * If Flushed is true, the decoder will probably reset the hardware decoder in
 * use and we need to release any hardware pause frames so the decoder is released
 * before a new one is created.
*/
void MythVideoOutputOpenGL::DiscardFrames(bool KeyFrame, bool Flushed)
{
    if (Flushed)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("(%1): %2").arg(KeyFrame).arg(m_videoBuffers.GetStatus()));
        m_videoBuffers.DiscardPauseFrames();
    }
    MythVideoOutput::DiscardFrames(KeyFrame, Flushed);
}

VideoFrameType* MythVideoOutputOpenGL::DirectRenderFormats(void)
{
    // Complete list of formats supported for OpenGL 2.0 and higher and OpenGL ES3.X
    static VideoFrameType s_AllFormats[] =
        { FMT_YV12,     FMT_NV12,      FMT_YUV422P,   FMT_YUV444P,
          FMT_YUV420P9, FMT_YUV420P10, FMT_YUV420P12, FMT_YUV420P14, FMT_YUV420P16,
          FMT_YUV422P9, FMT_YUV422P10, FMT_YUV422P12, FMT_YUV422P14, FMT_YUV422P16,
          FMT_YUV444P9, FMT_YUV444P10, FMT_YUV444P12, FMT_YUV444P14, FMT_YUV444P16,
          FMT_P010, FMT_P016,
          FMT_NONE };

    // OpenGL ES 2.0 and OpenGL1.X only allow luminance textures
    static VideoFrameType s_LegacyFormats[] =
        { FMT_YV12, FMT_YUV422P, FMT_YUV444P, FMT_NONE };

    static VideoFrameType* s_formats[2] = { s_AllFormats, s_LegacyFormats };
    return s_formats[m_textureFormats];
}

void MythVideoOutputOpenGL::WindowResized(const QSize &Size)
{
    m_window.SetWindowSize(Size);
    InitDisplayMeasurements();
}

void MythVideoOutputOpenGL::Show(FrameScanType /*scan*/)
{
    if (m_render && !IsErrored())
    {
        m_render->makeCurrent();
        if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
            m_render->logDebugMarker(LOC + "SHOW");
        m_render->swapBuffers();
        if (m_openGLPerf)
        {
            // time buffer swap and log
            // Results will normally be available on the next pass - and we will typically
            // test every other frame as a result to avoid blocking in the driver.
            // With the default of averaging over 30 samples this should give a 30 sample
            // average over 60 frames
            m_openGLPerf->RecordSample();
            m_openGLPerf->LogSamples();
        }
        m_render->doneCurrent();
    }
}

void MythVideoOutputOpenGL::ClearAfterSeek(void)
{
    if (m_openGLVideo)
        m_openGLVideo->ResetTextures();
    MythVideoOutput::ClearAfterSeek();
}

/*! \brief Generate a list of supported OpenGL profiles.
 *
 * \note This list could be filtered based upon current feature support. This
 * would however assume an OpenGL render device (not currently a given) but more
 * importantly, filtering out a selected profile encourages the display profile
 * code to use a higher priority, non-OpenGL renderer (such as VDPAU). By not
 * filtering, we allow the OpenGL video code to fallback to a supported, reasonable
 * alternative.
*/
QStringList MythVideoOutputOpenGL::GetAllowedRenderers(MythCodecID CodecId, const QSize& /*VideoDim*/)
{
    QStringList allowed;
    if (getenv("NO_OPENGL"))
        return allowed;

    if (codec_sw_copy(CodecId))
    {
        allowed << "opengl" << "opengl-yv12";
        return allowed;
    }

    VideoFrameType format = FMT_NONE;
    if (codec_is_vaapi(CodecId))
        format = FMT_VAAPI;
    else if (codec_is_vdpau(CodecId))
        format = FMT_VDPAU;
    else if (codec_is_nvdec(CodecId))
        format = FMT_NVDEC;
    else if (codec_is_vtb(CodecId))
        format = FMT_VTB;
    else if (codec_is_mmal(CodecId))
        format = FMT_MMAL;
    else if (codec_is_v4l2(CodecId) || codec_is_drmprime(CodecId))
        format = FMT_DRMPRIME;
    else if (codec_is_mediacodec(CodecId))
        format = FMT_MEDIACODEC;

    if (FMT_NONE == format)
        return allowed;

    allowed += MythOpenGLInterop::GetAllowedRenderers(format);
    return allowed;
}

void MythVideoOutputOpenGL::UpdatePauseFrame(int64_t &DisplayTimecode, FrameScanType Scan)
{
    VideoFrame* release = nullptr;
    m_videoBuffers.BeginLock(kVideoBuffer_used);
    VideoFrame *used = m_videoBuffers.Head(kVideoBuffer_used);
    if (used)
    {
        if (format_is_hw(used->codec))
        {
            release = m_videoBuffers.Dequeue(kVideoBuffer_used);
        }
        else
        {
            Scan = (is_interlaced(Scan) && !used->already_deinterlaced) ? kScan_Interlaced : kScan_Progressive;
            m_deinterlacer.Filter(used, Scan, m_dbDisplayProfile, true);
            m_openGLVideo->ProcessFrame(used, Scan);
        }
        DisplayTimecode = used->disp_timecode;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Could not update pause frame");
    }
    m_videoBuffers.EndLock();

    if (release)
        DoneDisplayingFrame(release);
}

void MythVideoOutputOpenGL::InitPictureAttributes(void)
{
    m_videoColourSpace.SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
}

void MythVideoOutputOpenGL::ShowPIP(VideoFrame* /*Frame*/, MythPlayer *PiPPlayer, PIPLocation Location)
{
    if (!PiPPlayer)
        return;

    int pipw = 0;
    int piph = 0;
    VideoFrame *pipimage     = PiPPlayer->GetCurrentFrame(pipw, piph);
    const QSize pipvideodim  = PiPPlayer->GetVideoBufferSize();
    QRect       pipvideorect = QRect(QPoint(0, 0), pipvideodim);

    if ((PiPPlayer->GetVideoAspect() <= 0.0F) || !pipimage || !pipimage->buf ||
        (pipimage->codec != FMT_YV12) || !PiPPlayer->IsPIPVisible())
    {
        PiPPlayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(Location, PiPPlayer);
    QRect dvr = m_window.GetDisplayVisibleRect();

    m_openGLVideoPiPsReady[PiPPlayer] = false;
    MythOpenGLVideo *gl_pipchain = m_openGLVideoPiPs[PiPPlayer];
    if (!gl_pipchain)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Initialise PiP");
        auto *colourspace = new VideoColourSpace(&m_videoColourSpace);
        m_openGLVideoPiPs[PiPPlayer] = gl_pipchain = new MythOpenGLVideo(m_render, colourspace,
                                                                pipvideodim, pipvideodim,
                                                                dvr, position, pipvideorect,
                                                                false, m_videoProfile);

        colourspace->DecrRef();
        if (!gl_pipchain->IsValid())
        {
            PiPPlayer->ReleaseCurrentFrame(pipimage);
            return;
        }
        gl_pipchain->SetMasterViewport(dvr.size());
    }

    if (gl_pipchain->GetVideoSize() != pipvideodim)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Re-initialise PiP.");
        delete gl_pipchain;
        auto *colourspace = new VideoColourSpace(&m_videoColourSpace);
        m_openGLVideoPiPs[PiPPlayer] = gl_pipchain = new MythOpenGLVideo(m_render, colourspace,
                                                                pipvideodim, pipvideodim,
                                                                dvr, position, pipvideorect,
                                                                false, m_videoProfile);
        colourspace->DecrRef();
        if (!gl_pipchain->IsValid())
        {
            PiPPlayer->ReleaseCurrentFrame(pipimage);
            return;
        }
        gl_pipchain->SetMasterViewport(dvr.size());
    }

    if (gl_pipchain->IsValid())
    {
        gl_pipchain->SetVideoRects(position, pipvideorect);
        gl_pipchain->ProcessFrame(pipimage);
    }

    m_openGLVideoPiPsReady[PiPPlayer] = true;
    if (PiPPlayer->IsPIPActive())
        m_openGLVideoPiPActive = gl_pipchain;
    PiPPlayer->ReleaseCurrentFrame(pipimage);
}

void MythVideoOutputOpenGL::RemovePIP(MythPlayer *PiPPlayer)
{
    if (m_openGLVideoPiPs.contains(PiPPlayer))
    {
        m_render->makeCurrent();
        delete m_openGLVideoPiPs.take(PiPPlayer);
        m_openGLVideoPiPsReady.remove(PiPPlayer);
        m_openGLVideoPiPs.remove(PiPPlayer);
        m_render->doneCurrent();
    }
}

QStringList MythVideoOutputOpenGL::GetVisualiserList(void)
{
    if (m_render)
        return VideoVisual::GetVisualiserList(m_render->Type());
    return MythVideoOutput::GetVisualiserList();
}

MythPainter *MythVideoOutputOpenGL::GetOSDPainter(void)
{
    return m_openGLPainter;
}

bool MythVideoOutputOpenGL::CanVisualise(AudioPlayer *Audio, MythRender* /*Render*/)
{
    return MythVideoOutput::CanVisualise(Audio, m_render);
}

bool MythVideoOutputOpenGL::SetupVisualisation(AudioPlayer *Audio, MythRender* /*Render*/, const QString &Name)
{
    return MythVideoOutput::SetupVisualisation(Audio, m_render, Name);
}
