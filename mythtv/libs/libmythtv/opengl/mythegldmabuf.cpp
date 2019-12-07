// MythTV
#include "fourcc.h"
#include "opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "mythvideotexture.h"
#include "mythegldmabuf.h"
#include "mythegldefs.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_drm.h"
}

#define LOC QString("EGLDMABUF: ")

MythEGLDMABUF::MythEGLDMABUF(MythRenderOpenGL *Context)
{
    if (Context)
    {
        OpenGLLocker locker(Context);
        m_useModifiers = Context->IsEGL() && Context->HasEGLExtension("EGL_EXT_image_dma_buf_import_modifiers");
        QSurfaceFormat fmt = Context->format();
    }
}

bool MythEGLDMABUF::HaveDMABuf(MythRenderOpenGL *Context)
{
    if (!Context)
        Context = MythRenderOpenGL::GetOpenGLRender();
    if (!Context)
        return false;
    OpenGLLocker locker(Context);
    return (Context->IsEGL() && Context->hasExtension("GL_OES_EGL_image") &&
            Context->HasEGLExtension("EGL_EXT_image_dma_buf_import"));
}

static void inline DebugDRMFrame(AVDRMFrameDescriptor* Desc)
{
    LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("DRM frame: Layers %1 Objects %2")
        .arg(Desc->nb_layers).arg(Desc->nb_objects));
    for (int i = 0; i < Desc->nb_layers; ++i)
    {
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Layer %1: Format %2 Planes %3")
            .arg(i).arg(fourcc_str(static_cast<int>(Desc->layers[i].format)))
            .arg(Desc->layers[i].nb_planes));
        for (int j = 0; j < Desc->layers[i].nb_planes; ++j)
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("  Plane %1: Index %2 Offset %3 Pitch %4")
                .arg(j).arg(Desc->layers[i].planes[j].object_index)
                .arg(Desc->layers[i].planes[j].offset).arg(Desc->layers[i].planes[j].pitch));
    }
    for (int i = 0; i < Desc->nb_objects; ++i)
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Object: %1 FD %2 Mods 0x%3")
            .arg(i).arg(Desc->objects[i].fd).arg(Desc->objects[i].format_modifier, 0 , 16));
}

inline vector<MythVideoTexture*> MythEGLDMABUF::CreateComposed(AVDRMFrameDescriptor* Desc,
                                                               MythRenderOpenGL *Context,
                                                               VideoFrame *Frame, FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    for (int i = 0; i < (Scan == kScan_Progressive ? 1 : 2); ++i)
    {
        vector<QSize> sizes;
        int frameheight = Scan == kScan_Progressive ? Frame->height : Frame->height >> 1;
        sizes.emplace_back(QSize(Frame->width, frameheight));
        vector<MythVideoTexture*> textures =
                MythVideoTexture::CreateTextures(Context, Frame->codec, FMT_RGBA32, sizes,
                                                 GL_TEXTURE_EXTERNAL_OES);
        for (uint j = 0; j < textures.size(); ++j)
            textures[j]->m_allowGLSLDeint = false;

        EGLint colourspace = EGL_ITU_REC709_EXT;
        switch (Frame->colorspace)
        {
            case AVCOL_SPC_BT470BG:
            case AVCOL_SPC_SMPTE170M:
            case AVCOL_SPC_SMPTE240M:
                colourspace = EGL_ITU_REC601_EXT;
                break;
            case AVCOL_SPC_BT2020_CL:
            case AVCOL_SPC_BT2020_NCL:
                colourspace = EGL_ITU_REC2020_EXT;
                break;
            default:
                if (Frame->width < 1280)
                    colourspace = EGL_ITU_REC601_EXT;
                break;
        }

        static constexpr EGLint kPlaneFd[4] =
            { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
              EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
        static constexpr EGLint kPlaneOffset[4] =
            { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
              EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
        static constexpr EGLint kPlanePitch[4] =
            { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
              EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
        static constexpr EGLint kPlaneModlo[4] =
            { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
        static constexpr EGLint kPlaneModhi[4] =
            { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

        AVDRMLayerDescriptor* layer = &Desc->layers[0];

        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(layer->format),
            EGL_WIDTH,                     Frame->width,
            EGL_HEIGHT,                    frameheight,
            EGL_YUV_COLOR_SPACE_HINT_EXT,  colourspace,
            EGL_SAMPLE_RANGE_HINT_EXT,     Frame->colorrange == AVCOL_RANGE_JPEG ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
            EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT,   EGL_YUV_CHROMA_SITING_0_EXT,
            EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT, EGL_YUV_CHROMA_SITING_0_EXT
        };

        for (int plane = 0; plane < layer->nb_planes; ++plane)
        {
            AVDRMPlaneDescriptor* drmplane = &layer->planes[plane];
            ptrdiff_t pitch  = drmplane->pitch;
            ptrdiff_t offset = drmplane->offset;
            if (Scan != kScan_Progressive)
            {
                if (i > 0)
                    offset += pitch;
                pitch = pitch << 1;
            }
            attribs << kPlaneFd[plane]     << Desc->objects[drmplane->object_index].fd
                    << kPlaneOffset[plane] << static_cast<EGLint>(offset)
                    << kPlanePitch[plane]  << static_cast<EGLint>(pitch);
            if (m_useModifiers && (Desc->objects[drmplane->object_index].format_modifier != 0 /* DRM_FORMAT_MOD_NONE*/))
            {
                attribs << kPlaneModlo[plane]
                        << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier & 0xffffffff)
                        << kPlaneModhi[plane]
                        << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier >> 32);
            }
        }
        attribs << EGL_NONE;

        EGLImageKHR image = Context->eglCreateImageKHR(Context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                       EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage '%1'").arg(Context->GetEGLError()));
        MythVideoTexture *texture = textures[0];
        Context->glBindTexture(texture->m_target, texture->m_textureId);
        Context->eglImageTargetTexture2DOES(texture->m_target, image);
        Context->glBindTexture(texture->m_target, 0);
        texture->m_data = static_cast<unsigned char *>(image);
        result.push_back(texture);
    }

    return result;
}

inline vector<MythVideoTexture*> MythEGLDMABUF::CreateSeparate(AVDRMFrameDescriptor* Desc,
                                                               MythRenderOpenGL *Context,
                                                               VideoFrame *Frame)
{
    // N.B. this works for YV12/NV12/P010 etc but will probably break for YUV422 etc
    vector<QSize> sizes;
    for (int plane = 0 ; plane < Desc->nb_layers; ++plane)
    {
        int width = Frame->width >> ((plane > 0) ? 1 : 0);
        int height = Frame->height >> ((plane > 0) ? 1 : 0);
        sizes.emplace_back(QSize(width, height));
    }

    VideoFrameType format = PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));
    vector<MythVideoTexture*> result =
            MythVideoTexture::CreateTextures(Context, Frame->codec, format, sizes,
                                             QOpenGLTexture::Target2D);
    for (uint i = 0; i < result.size(); ++i)
        result[i]->m_allowGLSLDeint = true;

    for (uint plane = 0; plane < result.size(); ++plane)
    {
        AVDRMLayerDescriptor* layer    = &Desc->layers[plane];
        AVDRMPlaneDescriptor* drmplane = &layer->planes[0];
        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(layer->format),
            EGL_WIDTH,  result[plane]->m_size.width(),
            EGL_HEIGHT, result[plane]->m_size.height(),
            EGL_DMA_BUF_PLANE0_FD_EXT,     Desc->objects[drmplane->object_index].fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(drmplane->offset),
            EGL_DMA_BUF_PLANE0_PITCH_EXT,  static_cast<EGLint>(drmplane->pitch)
        };

        if (m_useModifiers && (Desc->objects[drmplane->object_index].format_modifier != 0 /* DRM_FORMAT_MOD_NONE*/))
        {
            attribs << EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
                    << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier & 0xffffffff)
                    << EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT
                    << static_cast<EGLint>(Desc->objects[drmplane->object_index].format_modifier >> 32);
        }

        attribs << EGL_NONE;

        EGLImageKHR image = Context->eglCreateImageKHR(Context->GetEGLDisplay(), EGL_NO_CONTEXT,
                                                         EGL_LINUX_DMA_BUF_EXT, nullptr, attribs.data());
        if (!image)
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(Context->GetEGLError()));

        Context->glBindTexture(result[plane]->m_target, result[plane]->m_textureId);
        Context->eglImageTargetTexture2DOES(result[plane]->m_target, image);
        Context->glBindTexture(result[plane]->m_target, 0);
        result[plane]->m_data = static_cast<unsigned char *>(image);
    }

    return result;
}

vector<MythVideoTexture*> MythEGLDMABUF::CreateTextures(AVDRMFrameDescriptor* Desc,
                                                        MythRenderOpenGL *Context,
                                                        VideoFrame *Frame,
                                                        FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Desc || !Context || !Frame)
        return result;

    if (VERBOSE_LEVEL_CHECK(VB_PLAYBACK, LOG_DEBUG))
        DebugDRMFrame(Desc);

    uint numlayers = static_cast<uint>(Desc->nb_layers);
    if (numlayers < 1)
        return result;

    OpenGLLocker locker(Context);

    // N.B. If the descriptor has a single layer (RGB, packed YUV), it shouldn't
    // matter which path is taken here - the resulting calls are identical (with the
    // exception of the colourspace hints for composed - which should be ignored per the spec for RGB).
    // This MAY breakdown however for packed YUV formats when the frame format
    // is not set correctly and/or the 'returned' format does not match
    // our expectations.

    // For multiplanar formats (ie. YUV), this essentially assumes the implementation
    // will supply a descriptor that matches the expectation of the
    // EGL_EXT_image_dma_buf_import implementation (i.e. if it can cope with
    // separate layers, it will supply a suitable descriptor).

    // One layer with X planes
    if (numlayers == 1)
        return CreateComposed(Desc, Context, Frame, Scan);
    // X layers with one plane each
    return CreateSeparate(Desc, Context, Frame);
}
