// C/C++
#include <utility>

// MythTV
#include "libmyth/mythcontext.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"
#include "libmythui/opengl/mythopenglperf.h"
#include "libmythui/opengl/mythpainteropengl.h"
#include "libmythui/opengl/mythrenderopengl.h"

#include "decoders/mythcodeccontext.h"
#include "mythplayer.h"
#include "mythvideoprofile.h"
#include "opengl/mythopenglinterop.h"
#include "opengl/mythopenglvideo.h"
#include "opengl/mythvideooutopengl.h"
#include "osd.h"

#define LOC QString("VidOutGL: ")

/*! \brief Generate the list of available OpenGL profiles
 *
 * \note This could be improved by eliminating unsupported profiles at run time -
 * but it is currently called statically and hence options would be fixed and unable
 * to reflect changes in UI render device.
*/
void MythVideoOutputOpenGL::GetRenderOptions(RenderOptions& Options)
{
    QStringList safe;
    safe << "opengl" << "opengl-yv12";

    // all profiles can handle all software frames
    (*Options.safe_renderers)["dummy"].append(safe);
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

MythVideoOutputOpenGL::MythVideoOutputOpenGL(MythMainWindow* MainWindow,
                                             MythRenderOpenGL* Render,
                                             MythOpenGLPainter* Painter,
                                             MythDisplay *Display,
                                             const MythVideoProfilePtr& VideoProfile,
                                             QString &Profile)
  : MythVideoOutputGPU(MainWindow, Render, Painter, Display, VideoProfile, Profile),
    m_openglRender(Render)
{
    // Complete list of formats supported for OpenGL 2.0 and higher and OpenGL ES3.X
    static VideoFrameTypes s_openglRenderFormats =
    {
        FMT_YV12,     FMT_NV12,      FMT_YUV422P,   FMT_YUV444P,
        FMT_YUV420P9, FMT_YUV420P10, FMT_YUV420P12, FMT_YUV420P14, FMT_YUV420P16,
        FMT_YUV422P9, FMT_YUV422P10, FMT_YUV422P12, FMT_YUV422P14, FMT_YUV422P16,
        FMT_YUV444P9, FMT_YUV444P10, FMT_YUV444P12, FMT_YUV444P14, FMT_YUV444P16,
        FMT_P010,     FMT_P016
    };

    // OpenGL ES 2.0 and OpenGL1.X only allow luminance textures
    static VideoFrameTypes s_openglRenderFormatsLegacy =
    {
        FMT_YV12, FMT_YUV422P, FMT_YUV444P
    };

    // Retrieve render context
    if (!m_openglRender)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No OpenGL context");
        return;
    }

    OpenGLLocker locker(m_openglRender);

    // Enable performance monitoring if requested
    // This will report the execution time for the key GL code blocks
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
    if (m_openglRender->GetExtraFeatures() & kGLLegacyTextures)
        m_renderFormats = &s_openglRenderFormatsLegacy;
    else
        m_renderFormats = &s_openglRenderFormats;

    if (!qobject_cast<MythOpenGLPainter*>(m_painter))
        LOG(VB_GENERAL, LOG_ERR, LOC + "No OpenGL painter");

    // Create OpenGLVideo
    m_video = new MythOpenGLVideo(m_openglRender, &m_videoColourSpace, this, m_videoProfile, m_profile);
    if (m_video)
    {
        m_video->SetViewportRect(MythVideoOutputOpenGL::GetDisplayVisibleRectAdj());
        if (!m_video->IsValid())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create valid OpenGL video");
            delete m_video;
            m_video = nullptr;
        }
    }
}

MythVideoOutputOpenGL::~MythVideoOutputOpenGL()
{
    if (m_openglRender)
    {
        m_openglRender->makeCurrent();
        delete m_openGLPerf;
        m_openglRender->doneCurrent();
    }
}

bool MythVideoOutputOpenGL::Init(const QSize VideoDim, const QSize VideoDispDim,
                                 float Aspect, const QRect DisplayVisibleRect, MythCodecID CodecId)
{
    if (!(m_openglRender && m_painter && m_video))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Fatal error");
        return false;
    }

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Cannot initialise OpenGL video from this thread");
        return false;
    }

    OpenGLLocker ctx_lock(m_openglRender);

    if (!MythVideoOutputGPU::Init(VideoDim, VideoDispDim, Aspect, DisplayVisibleRect, CodecId))
        return false;

    // This works around an issue with VDPAU direct rendering using legacy drivers
    m_openglRender->Flush();

    return true;
}

/// \brief Adjust the display rectangle for OpenGL coordinates in some cases.
QRect MythVideoOutputOpenGL::GetDisplayVisibleRectAdj()
{
    QRect dvr = GetDisplayVisibleRect();
    if (!m_mainWindow)
        return dvr;

    QSize size = m_mainWindow->size();

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

void MythVideoOutputOpenGL::PrepareFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    if (!m_openglRender)
        return;

    // Lock
    OpenGLLocker ctx_lock(m_openglRender);

    // Start the first timer
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "PROCESS_FRAME_START");

    // Update software frames
    MythVideoOutputGPU::PrepareFrame(Frame, Scan);

    // Time texture update
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "PROCESS_FRAME_END");
}

void MythVideoOutputOpenGL::RenderFrame(MythVideoFrame* Frame, FrameScanType Scan)
{
    if (!m_openglRender)
        return;

    // Input changes need to be handled in ProcessFrame
    if (m_newCodecId != kCodec_NONE)
        return;

    // Lock
    OpenGLLocker ctx_lock(m_openglRender);

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "RENDER_FRAME_START");

    // If process frame has not been called (double rate hardware deint), then
    // we need to start the first 2 performance timers here
    if (m_openGLPerf)
    {
        if (!m_openGLPerf->GetTimersRunning())
        {
            m_openGLPerf->RecordSample();
            m_openGLPerf->RecordSample();
        }
    }

    m_openglRender->BindFramebuffer(nullptr);

    // Clear framebuffer
    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "CLEAR_START");

    if (!Frame || Frame->m_dummy || m_needFullClear || ((m_openglRender->GetExtraFeatures() & kGLTiled) != 0))
    {
        m_openglRender->SetBackground(m_clearColor, m_clearColor, m_clearColor, m_clearAlpha);
        m_openglRender->ClearFramebuffer();
    }
    // Avoid clearing the framebuffer if it will be entirely overwritten by video
    else if (!VideoIsFullScreen())
    {
        if (IsEmbedding())
        {
            // use MythRenderOpenGL rendering as it will clear to the appropriate 'black level'
            m_openglRender->ClearRect(nullptr, GetWindowRect(), m_clearColor, m_clearAlpha);
        }
        else
        {
            // in the vast majority of cases it is significantly quicker to just
            // clear the unused portions of the screen
            QRegion toclear = GetBoundingRegion();
            for (const auto& rect : std::as_const(toclear))
                m_openglRender->ClearRect(nullptr, rect, m_clearColor, m_clearAlpha);
        }
    }

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "CLEAR_END");

    // Time framebuffer clearing
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    // Render
    MythVideoOutputGPU::RenderFrame(Frame, Scan);

    // Time rendering
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "RENDER_FRAME_END");
}

void MythVideoOutputOpenGL::RenderEnd()
{
    // Flush and time
    m_openglRender->Flush();
    if (m_openGLPerf)
        m_openGLPerf->RecordSample();
}

void MythVideoOutputOpenGL::EndFrame()
{
    if (!m_openglRender || IsErrored())
        return;

    m_openglRender->makeCurrent();

    if (VERBOSE_LEVEL_CHECK(VB_GPU, LOG_INFO))
        m_openglRender->logDebugMarker(LOC + "SHOW");

    m_openglRender->swapBuffers();

    if (m_openGLPerf)
    {
        // Time buffer swap and log
        // Results will normally be available on the next pass - and we will typically
        // test every other frame as a result to avoid blocking in the driver.
        // With the default of averaging over 30 samples this should give a 30 sample
        // average over 60 frames
        m_openGLPerf->RecordSample();
        m_openGLPerf->LogSamples();
    }

    m_openglRender->doneCurrent();
}

/*! \brief Generate a list of supported OpenGL profiles.
*/
QStringList MythVideoOutputOpenGL::GetAllowedRenderers(MythRenderOpenGL *Render, MythCodecID CodecId, QSize /*VideoDim*/)
{
    QStringList allowed;
    if (!Render)
        return allowed;

    if (codec_sw_copy(CodecId))
    {
        allowed << "opengl" << "opengl-yv12";
        return allowed;
    }

    if (auto format = FrameTypeForCodec(CodecId); FMT_NONE != format)
    {
        MythInteropGPU::InteropMap supported;
        if (MythOpenGLInterop::GetTypes(Render, supported); supported.find(format) != supported.cend())
            allowed << "opengl-hw";
    }
    return allowed;
}
