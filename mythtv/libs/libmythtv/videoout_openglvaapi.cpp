#include "videoout_openglvaapi.h"
#include "mythrender_opengl.h"
#include "openglvideo.h"
#include "vaapicontext.h"
#include "mythpainter.h"
#include "mythmainwindow.h"
#include <QGuiApplication>

#define LOC QString("VidOutGLVAAPI: ")
#define ERR QString("VidOutGLVAAPI Error: ")

void VideoOutputOpenGLVAAPI::GetRenderOptions(render_opts &opts)
{
    opts.renderers->append("openglvaapi");

    (*opts.deints)["openglvaapi"].append("vaapionefield");
    (*opts.deints)["openglvaapi"].append("vaapibobdeint");
    (*opts.deints)["openglvaapi"].append("none");
    (*opts.osds)["openglvaapi"].append("opengl2");

    if (opts.decoders->contains("vaapi"))
        (*opts.safe_renderers)["vaapi"].append("openglvaapi");

    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("openglvaapi");

    (*opts.safe_renderers)["dummy"].append("openglvaapi");
    (*opts.safe_renderers)["nuppel"].append("openglvaapi");

    opts.priorities->insert("openglvaapi", 110);
}

VideoOutputOpenGLVAAPI::VideoOutputOpenGLVAAPI()
  : VideoOutputOpenGL(),
    m_vaapiInterop()
{
}

VideoOutputOpenGLVAAPI::~VideoOutputOpenGLVAAPI()
{
}

bool VideoOutputOpenGLVAAPI::InputChanged(const QSize &video_dim_buf,
                                          const QSize &video_dim_disp,
                                          float aspect,
                                          MythCodecID  av_codec_id,
                                          bool &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("InputChanged(%1,%2,%3) %4->%5")
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect)
            .arg(toString(video_codec_id)).arg(toString(av_codec_id)));

    if (!codec_is_vaapi(av_codec_id))
        return VideoOutputOpenGL::InputChanged(video_dim_buf, video_dim_disp,
                                               aspect, av_codec_id, aspect_only);

    bool wasembedding = window.IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = window.GetEmbeddingRect();
        StopEmbedding();
    }

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != window.GetVideoDim();
    bool asp_changed = aspect      != window.GetVideoAspect();

    if (!res_changed && !cid_changed)
    {
        if (asp_changed)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
            if (wasembedding)
                EmbedInWidget(oldrect);
        }
        return true;
    }

    if (gCoreContext->IsUIThread())
        TearDown();
    else
        DestroyCPUResources();

    QRect disp = window.GetDisplayVisibleRect();
    if (Init(video_dim_buf, video_dim_disp,
             aspect, m_window, disp, av_codec_id))
    {
        if (wasembedding)
            EmbedInWidget(oldrect);
        if (gCoreContext->IsUIThread())
            BestDeint();
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to re-initialise video output.");
    errorState = kError_Unknown;

    return false;
}

bool VideoOutputOpenGLVAAPI::Init(const QSize &video_dim_buf,
                                  const QSize &video_dim_disp,
                                  float aspect,
                                  WId winid, const QRect &win_rect,
                                  MythCodecID codec_id)
{
    bool ok = VideoOutputOpenGL::Init(video_dim_buf, video_dim_disp,
                                      aspect, winid,
                                      win_rect, codec_id);

    if (ok && codec_is_vaapi(video_codec_id))
        for (int i = 0; i < NUM_VAAPI_BUFFERS; i++)
            ok &= vbuffers.CreateBuffer(video_dim_disp.width(), video_dim_disp.height(), i, nullptr, FMT_VAAPI);
    return ok;
}

bool VideoOutputOpenGLVAAPI::CreateBuffers(void)
{
    if (codec_is_vaapi(video_codec_id))
    {
        vbuffers.Init(NUM_VAAPI_BUFFERS, false, 2, 1, 4, 1);
        return true;
    }
    return VideoOutputOpenGL::CreateBuffers();
}

void VideoOutputOpenGLVAAPI::SetProfile(void)
{
    if (db_vdisp_profile)
        db_vdisp_profile->SetVideoRenderer("openglvaapi");
}

bool VideoOutputOpenGLVAAPI::ApproveDeintFilter(const QString &filtername) const
{
    return filtername.contains("vaapi");
}

bool VideoOutputOpenGLVAAPI::SetDeinterlacingEnabled(bool enable)
{
    m_deinterlacing = enable;
    SetupDeinterlace(enable);
    return m_deinterlacing;
}

bool VideoOutputOpenGLVAAPI::SetupDeinterlace(bool interlaced, const QString& overridefilter)
{
    if (db_vdisp_profile)
        m_deintfiltername = db_vdisp_profile->GetFilteredDeint(overridefilter);
    m_deinterlacing = m_deintfiltername.contains("vaapi") ? interlaced : false;
    return m_deinterlacing;
}

void VideoOutputOpenGLVAAPI::InitPictureAttributes(void)
{
    VideoOutputOpenGL::InitPictureAttributes();
}

int VideoOutputOpenGLVAAPI::SetPictureAttribute(PictureAttribute attribute, int newValue)
{
    int val = newValue;
    if (codec_is_vaapi(video_codec_id))
        val = m_vaapiInterop.SetPictureAttribute(attribute, newValue);
    if (val > -1)
        return VideoOutputOpenGL::SetPictureAttribute(attribute, val);
    return -1;
}

void VideoOutputOpenGLVAAPI::UpdatePauseFrame(int64_t &disp_timecode)
{
    if (codec_is_std(video_codec_id))
    {
        VideoOutputOpenGL::UpdatePauseFrame(disp_timecode);
        return;
    }

    vbuffers.begin_lock(kVideoBuffer_used);
    if (vbuffers.Size(kVideoBuffer_used))
    {
        VideoFrame *frame = vbuffers.Head(kVideoBuffer_used);
        disp_timecode = frame->disp_timecode;
    }
    else
        LOG(VB_PLAYBACK, LOG_WARNING, LOC +
            "Could not update pause frame - no used frames.");

    vbuffers.end_lock();
}

void VideoOutputOpenGLVAAPI::PrepareFrame(VideoFrame *frame, FrameScanType scan, OSD *osd)
{
    if (codec_is_vaapi(video_codec_id) && m_openGLVideo)
    {
        // TODO pause frame
        MythGLTexture *texture = m_openGLVideo->GetInputTexture();
        GLuint textureid = texture->m_texture ? texture->m_texture->textureId() : texture->m_textureId;
        m_vaapiInterop.CopySurfaceToTexture(m_render, &videoColourSpace,
                                            frame ? frame : nullptr /*av_pause_frame*/,
                                            textureid, GL_TEXTURE_2D, scan);
    }

    VideoOutputOpenGL::PrepareFrame(frame, scan, osd);
}

QStringList VideoOutputOpenGLVAAPI::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;
    QStringList list;
    if ((codec_is_std(myth_codec_id) || (codec_is_vaapi(myth_codec_id))) &&
         !getenv("NO_VAAPI") && AllowVAAPIDisplay())
    {
        list += "openglvaapi";
    }
    return list;
}

// We currently (v30) only support rendering to a GLX surface.
// Disallow OpenGL ES (crashes hard) and EGL (fails to initialise) with Intel.
// This needs extending when EGL etc support is added and also generalising
// for other video output classes.
bool VideoOutputOpenGLVAAPI::AllowVAAPIDisplay()
{
    if (qApp->platformName().contains("egl", Qt::CaseInsensitive))
    {
        LOG(VB_GENERAL, LOG_INFO, "Disabling VAAPI display with EGL");
        return false;
    }

    MythMainWindow* win = MythMainWindow::getMainWindow();
    if (win)
    {
        MythRenderOpenGL *render = static_cast<MythRenderOpenGL*>(win->GetRenderDevice());
        if (!render || (render && render->isOpenGLES()))
        {
            LOG(VB_GENERAL, LOG_INFO, "Disabling VAAPI - incompatible with current render device");
            return false;
        }
    }
    return true;
}
