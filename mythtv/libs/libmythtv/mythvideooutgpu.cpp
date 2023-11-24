// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythpaintergpu.h"

#include "mythplayer.h"
#include "mythvideogpu.h"
#include "mythvideooutgpu.h"

#ifdef _WIN32
#include "libmythui/mythpainter_d3d9.h"
#include "videoout_d3d.h"
#endif
#ifdef USING_OPENGL
#include "libmythui/opengl/mythpainteropengl.h"
#include "opengl/mythvideooutopengl.h"
#endif
#ifdef USING_VULKAN
#include "libmythui/vulkan/mythpaintervulkan.h"
#include "vulkan/mythvideooutputvulkan.h"
#endif

#define LOC QString("VidOutGPU: ")

void MythVideoOutputGPU::GetRenderOptions(RenderOptions& Options, MythRender* Render)
{
#ifdef USING_OPENGL
    if (dynamic_cast<MythRenderOpenGL*>(Render) != nullptr)
        MythVideoOutputOpenGL::GetRenderOptions(Options);
#endif

#ifdef USING_VULKAN
    if (dynamic_cast<MythRenderVulkan*>(Render) != nullptr)
        MythVideoOutputVulkan::GetRenderOptions(Options);
#endif
}

MythVideoOutputGPU *MythVideoOutputGPU::Create(MythMainWindow* MainWindow, MythRender* Render,
                                               MythPainter* Painter, MythDisplay* Display,
                                               const QString& Decoder,
                                               MythCodecID CodecID,       const QSize VideoDim,
                                               const QSize VideoDispDim,  float VideoAspect,
                                               float FrameRate,           uint  PlayerFlags,
                                               const QString& Codec,      int ReferenceFrames,
                                               const VideoFrameTypes*& RenderFormats)
{
    if (!(MainWindow && Render && Painter && Display))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
        return nullptr;
    }

    if (PlayerFlags & kVideoIsNull)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot create null video output here");
        return nullptr;
    }

    QStringList renderers;

#ifdef _WIN32
//    auto * d3drender = dynamic_cast<MythRenderD3D9*>(Render);
//    auto * d3dpainter = dynamic_cast<MythD3D9Painter*>(Painter);
//    if (Render->Type() == kRenderDirect3D9)
//        renderers += VideoOutputD3D::GetAllowedRenderers(CodecID, VideoDispDim);
#endif

#ifdef USING_OPENGL
    auto * openglrender = dynamic_cast<MythRenderOpenGL*>(Render);
    auto * openglpainter = dynamic_cast<MythOpenGLPainter*>(Painter);
    if (openglrender && openglpainter && (Render->Type() == kRenderOpenGL))
        renderers += MythVideoOutputOpenGL::GetAllowedRenderers(openglrender, CodecID, VideoDispDim);
#endif

#ifdef USING_VULKAN
    auto * vulkanrender = dynamic_cast<MythRenderVulkan*>(Render);
    auto * vulkanpainter = dynamic_cast<MythPainterVulkan*>(Painter);
    if (vulkanrender && vulkanpainter && (Render->Type() == kRenderVulkan))
        renderers += MythVideoOutputVulkan::GetAllowedRenderers(CodecID);
#endif

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers for %1 %2 (Decoder: %3): '%4'")
        .arg(get_encoding_type(CodecID),
             get_decoder_name(CodecID),
             Decoder,
             renderers.join(",")));
    renderers = MythVideoProfile::GetFilteredRenderers(Decoder, renderers);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Allowed renderers (filt: %1): %2")
        .arg(Decoder, renderers.join(",")));

    QString renderer;

    auto videoprofile = std::make_shared<MythVideoProfile>();

    if (!renderers.empty())
    {
        videoprofile->SetInput(VideoDispDim, FrameRate, Codec);
        QString tmp = videoprofile->GetVideoRenderer();
        if (videoprofile->IsDecoderCompatible(Decoder) && renderers.contains(tmp))
        {
            renderer = tmp;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Preferred renderer: " + renderer);
        }
        else
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("No preferred renderer for decoder '%1' - profile renderer: '%2'")
                .arg(Decoder, tmp));
        }
    }

    if (renderer.isEmpty())
        renderer = MythVideoProfile::GetBestVideoRenderer(renderers);

    if (renderer.isEmpty())
    {
        QString fallback;
#ifdef USING_OPENGL
        if (Render->Type() == kRenderOpenGL)
            fallback = "opengl";
#endif
#ifdef USING_VULKAN
        if (Render->Type() == kRenderVulkan)
            fallback = VULKAN_RENDERER;
#endif
        LOG(VB_GENERAL, LOG_WARNING, LOC + "No renderer found. This should not happen!.");
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Falling back to '%1'").arg(fallback));
        renderer = fallback;
    }

    while (!renderers.empty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Trying video renderer: '%1'").arg(renderer));
        int index = renderers.indexOf(renderer);
        if (index >= 0)
            renderers.removeAt(index);
        else
            break;

        MythVideoOutputGPU* video = nullptr;

#ifdef _WIN32
//        if (renderer == "direct3d")
//            video = new VideoOutputD3D(MainWindow, d3drender,
//                                       d3dpainter, Display,
//                                       videoprofile, renderer);
#endif
#ifdef USING_OPENGL
        // cppcheck-suppress knownConditionTrueFalse
        if (!video && renderer.contains("opengl") && openglrender)
        {
            video = new MythVideoOutputOpenGL(MainWindow, openglrender,
                                              openglpainter, Display,
                                              videoprofile, renderer);
        }
#endif
#ifdef USING_VULKAN
        if (!video && renderer.contains(VULKAN_RENDERER))
        {
            video = new MythVideoOutputVulkan(MainWindow, vulkanrender,
                                              vulkanpainter, Display,
                                              videoprofile, renderer);
        }
#endif

        if (video)
        {
            video->SetVideoFrameRate(FrameRate);
            video->SetReferenceFrames(ReferenceFrames);
            if (video->Init(VideoDim, VideoDispDim, VideoAspect, MainWindow->GetUIScreenRect(), CodecID))
            {
                video->SetVideoScalingAllowed(true);
                RenderFormats = video->m_renderFormats;
                return video;
            }
            delete video;
            video = nullptr;
        }
        renderer = MythVideoProfile::GetBestVideoRenderer(renderers);
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Not compiled with any useable video output method.");
    return nullptr;
}

VideoFrameType MythVideoOutputGPU::FrameTypeForCodec(MythCodecID CodecId)
{
    if (codec_is_vaapi(CodecId)) return FMT_VAAPI;
    if (codec_is_vdpau(CodecId)) return FMT_VDPAU;
    if (codec_is_nvdec(CodecId)) return FMT_NVDEC;
    if (codec_is_vtb(CodecId))   return FMT_VTB;
    if (codec_is_mmal(CodecId))  return FMT_MMAL;
    if (codec_is_v4l2(CodecId) || codec_is_drmprime(CodecId)) return FMT_DRMPRIME;
    if (codec_is_mediacodec(CodecId)) return FMT_MEDIACODEC;
    return FMT_NONE;
}

/*! \class MythVideoOutputGPU
 * \brief Common code shared between GPU accelerated sub-classes (e.g. OpenGL)
 *
 * MythVideoOutputGPU is a pure virtual class that contains code shared between
 * the differing hardware accelerated MythVideoOutput subclasses.
 *
 * \note This should be considered a work-in-progress and it is likely to change.
 *
 * \sa MythVideoOutput
 * \sa MythVideoOutputOpenGL
 * \sa MythVideoOutputVulkan
 */
MythVideoOutputGPU::MythVideoOutputGPU(MythMainWindow* MainWindow, MythRender* Render,
                                       MythPainterGPU* Painter, MythDisplay* Display,
                                       MythVideoProfilePtr VideoProfile, QString& Profile)
  : m_mainWindow(MainWindow),
    m_render(Render),
    m_painter(Painter),
    m_profile(std::move(Profile))
{
    if (!(m_mainWindow && m_render && m_painter && Display))
    {
        LOG(VB_GENERAL, LOG_ERR, "Fatal error");
        return;
    }

    m_videoProfile = std::move(VideoProfile);
    m_render->IncrRef();
    SetDisplay(Display);
    m_painter->SetViewControl(MythPainterGPU::None);

    // If our rendering context is overlaid on top of a video plane, we need transparency
    // and we need to ensure we are clearing the entire framebuffer.
    // Note: an alpha of zero is probably safe to use everywhere. tbc.
    if (m_display->IsPlanar())
    {
        m_clearAlpha = 0;
        m_needFullClear = true;
    }

    m_hdrTracker = MythHDRTracker::Create(Display);

    connect(this, &MythVideoOutputGPU::RefreshState,   this, &MythVideoOutputGPU::DoRefreshState);
    connect(this, &MythVideoOutputGPU::DoRefreshState, this, &MythVideoOutputGPU::RefreshVideoBoundsState);
    connect(this, &MythVideoOutputGPU::DoRefreshState,
            &m_videoColourSpace, &MythVideoColourSpace::RefreshState);
    connect(this, &MythVideoOutputGPU::ChangePictureAttribute,
            &m_videoColourSpace, &MythVideoColourSpace::ChangePictureAttribute);
    connect(&m_videoColourSpace, &MythVideoColourSpace::PictureAttributeChanged,
            this, &MythVideoOutputGPU::PictureAttributeChanged);
    connect(&m_videoColourSpace, &MythVideoColourSpace::SupportedAttributesChanged,
            this, &MythVideoOutputGPU::SupportedAttributesChanged);
    connect(&m_videoColourSpace, &MythVideoColourSpace::PictureAttributesUpdated,
            this, &MythVideoOutputGPU::PictureAttributesUpdated);
}

MythVideoOutputGPU::~MythVideoOutputGPU()
{
    m_hdrTracker = nullptr;

    MythVideoOutputGPU::DestroyBuffers();
    delete m_video;
    if (m_painter)
        m_painter->SetViewControl(MythPainterGPU::Viewport | MythPainterGPU::Framebuffer);
    if (m_render)
        m_render->DecrRef();
}

QRect MythVideoOutputGPU::GetDisplayVisibleRectAdj()
{
    return GetDisplayVisibleRect();
}

void MythVideoOutputGPU::InitPictureAttributes()
{
    m_videoColourSpace.SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
}

void MythVideoOutputGPU::WindowResized(const QSize Size)
{
    SetWindowSize(Size);
    InitDisplayMeasurements();
}

void MythVideoOutputGPU::SetVideoFrameRate(float NewRate)
{
    if (!m_videoProfile)
        return;

    if (qFuzzyCompare(m_videoProfile->GetOutput() + 1.0F, NewRate + 1.0F))
        return;

    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Video frame rate changed: %1->%2")
        .arg(static_cast<double>(m_videoProfile->GetOutput())).arg(static_cast<double>(NewRate)));
    m_videoProfile->SetOutput(NewRate);
    m_newFrameRate = true;
}

bool MythVideoOutputGPU::Init(const QSize VideoDim, const QSize VideoDispDim,
                              float Aspect, const QRect DisplayVisibleRect, MythCodecID CodecId)
{
    // if we are the main video player then free up as much video memory
    // as possible at startup
    if ((kCodec_NONE == m_newCodecId) && m_painter)
        m_painter->FreeResources();

    // Default initialisation - mainly MythVideoBounds
    if (!MythVideoOutput::Init(VideoDim, VideoDispDim, Aspect, DisplayVisibleRect, CodecId))
        return false;

    // Ensure any new profile preferences are handled after a stream change
    if (m_videoProfile)
        m_video->SetProfile(m_videoProfile->GetVideoRenderer());

    // Set default support for picture attributes
    InitPictureAttributes();

    // Setup display
    QSize size = GetVideoDim();

    // Set the display mode if required
    if (m_display && m_display->UsingVideoModes() && !IsEmbedding())
        ResizeForVideo(size);
    InitDisplayMeasurements();

    // Create buffers
    m_buffersCreated = CreateBuffers(CodecId, GetVideoDim());
    if (!m_buffersCreated)
        return false;

    // Adjust visible rect for embedding
    QRect dvr = GetDisplayVisibleRectAdj();
    if (m_videoCodecID == kCodec_NONE)
    {
        m_render->SetViewPort(QRect(QPoint(), dvr.size()));
        return true;
    }

    // Reset OpenGLVideo
    if (m_video->IsValid())
        m_video->ResetFrameFormat();

    return true;
}

/*! \brief Discard video frames
 *
 * If Flushed is true, the decoder will probably reset the hardware decoder in
 * use and we need to release any hardware pause frames so the decoder is released
 * before a new one is created.
*/
void MythVideoOutputGPU::DiscardFrames(bool KeyFrame, bool Flushed)
{
    if (Flushed)
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("(%1): %2").arg(KeyFrame).arg(m_videoBuffers.GetStatus()));
        m_videoBuffers.DiscardPauseFrames();
    }
    MythVideoOutput::DiscardFrames(KeyFrame, Flushed);
}

/*! \brief Release a video frame back into the decoder pool.
 *
 * Software frames do not need a pause frame as the MythVideo subclass
 * holds a copy of the last frame in its input textures. So
 * just release the frame.
 *
 * Hardware frames hold the underlying interop class and
 * hence access to the video texture. We cannot access them
 * without a frame so retain the most recent frame by removing
 * it from the 'used' queue and adding it to the 'pause' queue.
*/
void MythVideoOutputGPU::DoneDisplayingFrame(MythVideoFrame* Frame)
{
    if (!Frame)
        return;

    auto retain = MythVideoFrame::HardwareFormat(Frame->m_type);
    QVector<MythVideoFrame*> release;

    m_videoBuffers.BeginLock(kVideoBuffer_pause);
    while (m_videoBuffers.Size(kVideoBuffer_pause))
    {
        auto * frame = m_videoBuffers.Dequeue(kVideoBuffer_pause);
        if (!retain || (frame != Frame))
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

bool MythVideoOutputGPU::CreateBuffers(MythCodecID CodecID, QSize Size)
{
    if (m_buffersCreated)
        return true;

    if (codec_is_copyback(CodecID))
    {
        m_videoBuffers.Init(VideoBuffers::GetNumBuffers(FMT_NONE), 1, 4, 2);
        return m_videoBuffers.CreateBuffers(FMT_YV12, Size.width(), Size.height(), m_renderFormats);
    }

    if (codec_is_mediacodec(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_MEDIACODEC, m_renderFormats, Size, 1, 2, 2);
    if (codec_is_vaapi(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VAAPI, m_renderFormats, Size, 2, 1, 4, m_maxReferenceFrames);
    if (codec_is_vtb(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VTB, m_renderFormats, Size, 1, 4, 2);
    if (codec_is_vdpau(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_VDPAU, m_renderFormats, Size, 2, 1, 4, m_maxReferenceFrames);
    if (codec_is_nvdec(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_NVDEC, m_renderFormats, Size, 2, 1, 4);
    if (codec_is_mmal(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_MMAL, m_renderFormats, Size, 2, 1, 4);
    if (codec_is_v4l2(CodecID) || codec_is_drmprime(CodecID))
        return m_videoBuffers.CreateBuffers(FMT_DRMPRIME, m_renderFormats, Size, 2, 1, 4);

    return m_videoBuffers.CreateBuffers(FMT_YV12, m_renderFormats, Size, 1, 8, 4, m_maxReferenceFrames);
}

void MythVideoOutputGPU::DestroyBuffers()
{
    MythVideoOutputGPU::DiscardFrames(true, true);
    m_videoBuffers.Reset();
    m_buffersCreated = false;
}

void MythVideoOutputGPU::SetReferenceFrames(int ReferenceFrames)
{
    m_maxReferenceFrames = ReferenceFrames;
}

bool MythVideoOutputGPU::InputChanged(QSize VideoDim, QSize VideoDispDim,
                                      float VideoAspect, MythCodecID CodecId,
                                      bool& AspectOnly, int ReferenceFrames,
                                      bool ForceChange)
{
    QSize currentvideodim     = GetVideoDim();
    QSize currentvideodispdim = GetVideoDispDim();
    MythCodecID currentcodec  = m_videoCodecID;
    float currentaspect       = GetVideoAspect();

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
        .arg(toString(CodecId)).arg(static_cast<double>(VideoAspect))
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
    m_newAspect = VideoAspect;
    return true;
}

bool MythVideoOutputGPU::ProcessInputChange()
{
    if (m_newCodecId != kCodec_NONE)
    {
        // Ensure we don't lose embedding through program changes.
        bool wasembedding = IsEmbedding();
        QRect oldrect;
        if (wasembedding)
        {
            oldrect = GetEmbeddingRect();
            EmbedPlayback(false, {});
        }

        // Note - we don't call the default VideoOutput::InputChanged method as
        // the OpenGL implementation is asynchronous.
        // So we need to update the video display profile here. It is a little
        // circular as we need to set the video dimensions first which are then
        // reset in Init.
        // All told needs a cleanup - not least because the use of codecName appears
        // to be inconsistent.
        SourceChanged(m_newVideoDim, m_newVideoDispDim, m_newAspect);
        AVCodecID avCodecId = myth2av_codecid(m_newCodecId);
        const AVCodec* codec = avcodec_find_decoder(avCodecId);
        QString codecName;
        if (codec)
            codecName = codec->name;
        if (m_videoProfile)
            m_videoProfile->SetInput(GetVideoDispDim(), 0 , codecName);
        bool ok = Init(m_newVideoDim, m_newVideoDispDim, m_newAspect, GetRawWindowRect(), m_newCodecId);
        m_newCodecId = kCodec_NONE;
        m_newVideoDim = QSize();
        m_newVideoDispDim = QSize();
        m_newAspect = 0.0F;
        m_newFrameRate = false;

        if (wasembedding && ok)
            EmbedPlayback(true, oldrect);

        // Update deinterlacers for any input change
        SetDeinterlacing(m_deinterlacing, m_deinterlacing2X, m_forcedDeinterlacer);

        if (!ok)
            return false;
    }
    else if (m_newFrameRate)
    {
        // If we are switching mode purely for a refresh rate change, then there
        // is no need to recreate buffers etc etc
        ResizeForVideo();
        m_newFrameRate = false;
    }

    return true;
}

/*! \brief Initialise display measurement
 *
 * The sole intent here is to ensure that MythVideoBounds has the correct aspect
 * ratio when it calculates the video display rectangle.
*/
void MythVideoOutputGPU::InitDisplayMeasurements()
{
    if (!m_display)
        return;

    // Retrieve the display aspect ratio.
    // This will be, in priority order:-
    // - aspect ratio override when using resolution/mode switching (if not 'Default')
    // - aspect ratio override for setups where detection does not work/is broken (multiscreen, broken EDID etc)
    // - aspect ratio based on detected physical size (this should be the common/default value)
    // - aspect ratio fallback using screen resolution
    // - 16:9
    QString source;
    double displayaspect = m_display->GetAspectRatio(source);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Display aspect ratio: %1 (%2)")
        .arg(displayaspect).arg(source));

    // Get the window and screen resolutions
    QSize window = GetRawWindowRect().size();
    QSize screen = m_display->GetResolution();

    // If not running fullscreen, adjust for window size and ignore any video
    // mode overrides as they do not apply when in a window
    if (!window.isEmpty() && !screen.isEmpty() && window != screen)
    {
        displayaspect = m_display->GetAspectRatio(source, true);
        double screenaspect = screen.width() / static_cast<double>(screen.height());
        double windowaspect = window.width() / static_cast<double>(window.height());
        displayaspect = displayaspect * (1.0 / screenaspect) * windowaspect;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Window aspect ratio: %1").arg(displayaspect));
    }

    SetDisplayAspect(static_cast<float>(displayaspect));
}

void MythVideoOutputGPU::PrepareFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    // Process input changes
    if (!ProcessInputChange())
        return;

    if (Frame)
    {
        SetRotation(Frame->m_rotation);

        if (m_hdrTracker)
            m_hdrTracker->Update(Frame);

        if (MythVideoFrame::HardwareFormat(Frame->m_type) || Frame->m_dummy)
            return;

        // software deinterlacing
        m_deinterlacer.Filter(Frame, Scan, m_videoProfile.get());

        // update software textures
        if (m_video)
            m_video->PrepareFrame(Frame, Scan);
    }
}

void MythVideoOutputGPU::RenderFrame(MythVideoFrame *Frame, FrameScanType Scan)
{
    bool dummy = false;
    bool topfieldfirst = false;
    if (Frame)
    {
        m_framesPlayed = Frame->m_frameNumber + 1;
        topfieldfirst = Frame->m_interlacedReverse ? !Frame->m_topFieldFirst : Frame->m_topFieldFirst;
        dummy = Frame->m_dummy;
    }
    else
    {
        // see DoneDisplayingFrame
        // we only retain pause frames for hardware formats
        if (m_videoBuffers.Size(kVideoBuffer_pause))
            Frame = m_videoBuffers.Tail(kVideoBuffer_pause);
    }

    // Main UI when embedded
    if (m_painter && IsEmbedding())
    {
        // If we are using high dpi, the painter needs to set the appropriate
        // viewport and enable scaling of its images
        m_painter->SetViewControl(MythPainterGPU::Viewport);

        if (m_mainWindow->GetPaintWindow())
        {
            m_mainWindow->GetPaintWindow()->clearMask();
            m_mainWindow->Draw(m_painter);
        }
        m_painter->SetViewControl(MythPainterGPU::None);
    }

    // Video
    // N.B. dummy streams need the viewport updated in case we have resized the window (i.e. LiveTV)
    if (m_video && !dummy)
        m_video->RenderFrame(Frame, topfieldfirst, Scan, GetStereoOverride());
    else if (dummy)
        m_render->SetViewPort(GetWindowRect());
}

void MythVideoOutputGPU::UpdatePauseFrame(std::chrono::milliseconds& DisplayTimecode, FrameScanType Scan)
{
    MythVideoFrame* release = nullptr;
    m_videoBuffers.BeginLock(kVideoBuffer_used);
    MythVideoFrame* used = m_videoBuffers.Head(kVideoBuffer_used);
    if (used)
    {
        if (MythVideoFrame::HardwareFormat(used->m_type))
        {
            release = m_videoBuffers.Dequeue(kVideoBuffer_used);
        }
        else
        {
            Scan = (is_interlaced(Scan) && !used->m_alreadyDeinterlaced) ? kScan_Interlaced : kScan_Progressive;
            m_deinterlacer.Filter(used, Scan, m_videoProfile.get(), true);
            if (m_video)
                m_video->PrepareFrame(used, Scan);
        }
        DisplayTimecode = used->m_displayTimecode;
    }
    else
    {
        LOG(VB_PLAYBACK, LOG_WARNING, LOC + "Could not update pause frame");
    }
    m_videoBuffers.EndLock();

    if (release)
        DoneDisplayingFrame(release);
}

void MythVideoOutputGPU::EndFrame()
{
    m_video->EndFrame();
}

void MythVideoOutputGPU::ClearAfterSeek()
{
    // Clear reference frames for GPU deinterlacing
    if (m_video)
        m_video->ResetTextures();
    // Clear decoded frames
    MythVideoOutput::ClearAfterSeek();
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
void MythVideoOutputGPU::ResizeForVideo(QSize Size)
{
    if (!m_display)
        return;
    if (!m_display->UsingVideoModes())
        return;

    if (Size.isEmpty())
    {
        Size = GetVideoDispDim();
        if (Size.isEmpty())
            return;
    }

    float rate = m_videoProfile ? m_videoProfile->GetOutput() : 0.0F;

    bool hide = m_display->NextModeIsLarger(Size);
    if (hide)
        m_mainWindow->hide();

    if (m_display->SwitchToVideo(Size, static_cast<double>(rate)))
    {
        // Switching to custom display resolution succeeded
        // Make a note of the new size
        QString source;
        double aspect = m_display->GetAspectRatio(source);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Aspect ratio: %1 (%2)")
            .arg(aspect).arg(source));
        SetDisplayAspect(static_cast<float>(aspect));
        SetWindowSize(m_display->GetResolution());

        bool fullscreen = !UsingGuiSize();

        // if width && height are zero users expect fullscreen playback
        if (!fullscreen)
        {
            int gui_width = 0;
            int gui_height = 0;
            gCoreContext->GetResolutionSetting("Gui", gui_width, gui_height);
            fullscreen |= (0 == gui_width && 0 == gui_height);
        }

        if (fullscreen)
        {
            QSize size = m_display->GetResolution();
            QRect display_visible_rect = QRect(m_mainWindow->geometry().topLeft(), size);
            if (hide)
            {
                m_mainWindow->Show();
                hide = false;
            }
            m_mainWindow->MoveResize(display_visible_rect);
        }
    }
    if (hide)
        m_mainWindow->Show();
}
