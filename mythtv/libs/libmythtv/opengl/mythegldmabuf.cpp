// MythTV
#include "libmythui/opengl/mythrenderopengl.h"

#include "fourcc.h"
#include "mythavutil.h"
#include "opengl/mythvideotextureopengl.h"
#include "opengl/mythegldmabuf.h"
#include "opengl/mythegldefs.h"

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
    }
}

bool MythEGLDMABUF::HaveDMABuf(MythRenderOpenGL *Context)
{
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
        {
            LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("  Plane %1: Index %2 Offset %3 Pitch %4")
                .arg(j).arg(Desc->layers[i].planes[j].object_index)
                .arg(Desc->layers[i].planes[j].offset).arg(Desc->layers[i].planes[j].pitch));
        }
    }
    for (int i = 0; i < Desc->nb_objects; ++i)
        LOG(VB_PLAYBACK, LOG_DEBUG, LOC + QString("Object: %1 FD %2 Mods 0x%3")
            .arg(i).arg(Desc->objects[i].fd).arg(Desc->objects[i].format_modifier, 0 , 16));
}

/*! \brief Create a single RGBA32 texture using the provided AVDRMFramDescriptor.
 *
 * \note This assumes one layer with multiple planes, typically where the layer
 * is a YUV format.
*/
inline std::vector<MythVideoTextureOpenGL*> MythEGLDMABUF::CreateComposed(AVDRMFrameDescriptor* Desc,
                                                                          MythRenderOpenGL *Context,
                                                                           MythVideoFrame *Frame, FrameScanType Scan) const
{
    Frame->m_alreadyDeinterlaced = true;
    std::vector<MythVideoTextureOpenGL*> result;
    for (int i = 0; i < (Scan == kScan_Progressive ? 1 : 2); ++i)
    {
        std::vector<QSize> sizes;
        int frameheight = Scan == kScan_Progressive ? Frame->m_height : Frame->m_height >> 1;
        sizes.emplace_back(Frame->m_width, frameheight);
        std::vector<MythVideoTextureOpenGL*> textures =
            MythVideoTextureOpenGL::CreateTextures(Context, Frame->m_type, FMT_RGBA32, sizes,
                                                   GL_TEXTURE_EXTERNAL_OES);
        if (textures.empty())
        {
            ClearDMATextures(Context, result);
            return result;
        }

        textures[0]->m_allowGLSLDeint = false;

        EGLint colourspace = EGL_ITU_REC709_EXT;
        switch (Frame->m_colorspace)
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
                if (Frame->m_width < 1280)
                    colourspace = EGL_ITU_REC601_EXT;
                break;
        }

        static constexpr std::array<EGLint,4> kPlaneFd
            { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
              EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT };
        static constexpr std::array<EGLint,4> kPlaneOffset
            { EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
              EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT };
        static constexpr std::array<EGLint,4> kPlanePitch
            { EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
              EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT };
        static constexpr std::array<EGLint,4> kPlaneModlo
            { EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT };
        static constexpr std::array<EGLint,4> kPlaneModhi
            { EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
              EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT };

        AVDRMLayerDescriptor* layer = &Desc->layers[0];

        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT,      static_cast<EGLint>(layer->format),
            EGL_WIDTH,                     Frame->m_width,
            EGL_HEIGHT,                    frameheight,
            EGL_YUV_COLOR_SPACE_HINT_EXT,  colourspace,
            EGL_SAMPLE_RANGE_HINT_EXT,     Frame->m_colorrange == AVCOL_RANGE_JPEG ? EGL_YUV_FULL_RANGE_EXT : EGL_YUV_NARROW_RANGE_EXT,
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
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage '%1'").arg(Context->GetEGLError()));
            // Ensure we release anything already created and return nothing
            ClearDMATextures(Context, result);
            return result;
        }

        MythVideoTextureOpenGL *texture = textures[0];
        Context->glBindTexture(texture->m_target, texture->m_textureId);
        Context->eglImageTargetTexture2DOES(texture->m_target, image);
        Context->glBindTexture(texture->m_target, 0);
        texture->m_data = static_cast<unsigned char *>(image);
        result.push_back(texture);
    }

    return result;
}

/*! \brief Create multiple textures that represent the planes for the given AVDRMFrameDescriptor
 *
 * \note This assumes multiple layers each with one plane.
*/
inline std::vector<MythVideoTextureOpenGL*> MythEGLDMABUF::CreateSeparate(AVDRMFrameDescriptor* Desc,
                                                                          MythRenderOpenGL *Context,
                                                                          MythVideoFrame *Frame) const
{
    // N.B. this works for YV12/NV12/P010 etc but will probably break for YUV422 etc
    std::vector<QSize> sizes;
    for (int plane = 0 ; plane < Desc->nb_layers; ++plane)
    {
        int width = Frame->m_width >> ((plane > 0) ? 1 : 0);
        int height = Frame->m_height >> ((plane > 0) ? 1 : 0);
        sizes.emplace_back(width, height);
    }

    VideoFrameType format = MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_swPixFmt));
    std::vector<MythVideoTextureOpenGL*> result =
            MythVideoTextureOpenGL::CreateTextures(Context, Frame->m_type, format, sizes,
                                             QOpenGLTexture::Target2D);
    if (result.empty())
        return result;

    for (uint plane = 0; plane < result.size(); ++plane)
    {
        result[plane]->m_allowGLSLDeint = true;
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
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(Context->GetEGLError()));
            ClearDMATextures(Context, result);
            return result;
        }

        Context->glBindTexture(result[plane]->m_target, result[plane]->m_textureId);
        Context->eglImageTargetTexture2DOES(result[plane]->m_target, image);
        Context->glBindTexture(result[plane]->m_target, 0);
        result[plane]->m_data = static_cast<unsigned char *>(image);
    }

    return result;
}

#ifndef DRM_FORMAT_R8
static constexpr uint32_t MKTAG2(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{ return ((static_cast<uint32_t>(a)      ) |
          (static_cast<uint32_t>(b) <<  8) |
          (static_cast<uint32_t>(c) << 16) |
          (static_cast<uint32_t>(d) << 24)); }
static constexpr uint32_t DRM_FORMAT_R8     { MKTAG2('R', '8', ' ', ' ') };
static constexpr uint32_t DRM_FORMAT_GR88   { MKTAG2('G', 'R', '8', '8') };
static constexpr uint32_t DRM_FORMAT_R16    { MKTAG2('R', '1', '6', ' ') };
static constexpr uint32_t DRM_FORMAT_GR32   { MKTAG2('G', 'R', '3', '2') };
static constexpr uint32_t DRM_FORMAT_NV12   { MKTAG2('N', 'V', '1', '2') };
static constexpr uint32_t DRM_FORMAT_NV21   { MKTAG2('N', 'V', '2', '1') };
static constexpr uint32_t DRM_FORMAT_YUV420 { MKTAG2('Y', 'U', '1', '2') };
static constexpr uint32_t DRM_FORMAT_YVU420 { MKTAG2('Y', 'V', '1', '2') };
static constexpr uint32_t DRM_FORMAT_P010   { MKTAG2('P', '0', '1', '0') };
static_assert(DRM_FORMAT_GR88   == 0x38385247);
static_assert(DRM_FORMAT_YUV420 == 0x32315559);
static_assert(DRM_FORMAT_YVU420 == 0x32315659);
#endif

/*! \brief Create multiple textures that represent the planes for the given AVDRMFrameDescriptor
 *
 * \note This assumes one layer with multiple planes that represent a YUV format.
 *
 * It is used where the OpenGL DMA BUF implementation does not support composing YUV formats.
 * It offers better feature support (as we can enable colour controls, shader
 * deinterlacing etc) but may not be as fast on low end hardware; it might not
 * use hardware accelerated paths and shader deinterlacing may not be as fast as the
 * simple EGL based onefield/bob deinterlacer. It is essentially the same as
 * CreateSeparate but the DRM descriptor uses a different layout and we have
 * to 'guess' the correct DRM_FORMATs for each plane.
 *
 * \todo Add support for simple onefield/bob with YUV textures.
*/
inline std::vector<MythVideoTextureOpenGL*> MythEGLDMABUF::CreateSeparate2(AVDRMFrameDescriptor* Desc,
                                                                           MythRenderOpenGL *Context,
                                                                           MythVideoFrame *Frame) const
{
    // As for CreateSeparate - may not work for some formats
    AVDRMLayerDescriptor* layer = &Desc->layers[0];
    std::vector<QSize> sizes;
    for (int plane = 0 ; plane < layer->nb_planes; ++plane)
    {
        int width = Frame->m_width >> ((plane > 0) ? 1 : 0);
        int height = Frame->m_height >> ((plane > 0) ? 1 : 0);
        sizes.emplace_back(width, height);
    }

    // TODO - the v4l2_m2m decoder is not setting the correct sw_fmt - so we
    // need to deduce the frame format from the fourcc
    VideoFrameType format = FMT_YV12;
    EGLint fourcc1 = DRM_FORMAT_R8;
    EGLint fourcc2 = DRM_FORMAT_R8;
    if (layer->format == DRM_FORMAT_NV12 || layer->format == DRM_FORMAT_NV21)
    {
        format  = FMT_NV12;
        fourcc2 = DRM_FORMAT_GR88;
    }
    else if (layer->format == DRM_FORMAT_P010)
    {
        format  = FMT_P010;
        fourcc1 = DRM_FORMAT_R16;
        fourcc2 = DRM_FORMAT_GR32;
    }

    std::vector<MythVideoTextureOpenGL*> result =
            MythVideoTextureOpenGL::CreateTextures(Context, Frame->m_type, format, sizes,
                                             QOpenGLTexture::Target2D);
    if (result.empty())
        return result;

    for (uint plane = 0; plane < result.size(); ++plane)
    {
        result[plane]->m_allowGLSLDeint = true;
        EGLint fourcc = fourcc1;
        if (plane > 0)
            fourcc = fourcc2;
        AVDRMPlaneDescriptor* drmplane = &layer->planes[plane];
        QVector<EGLint> attribs = {
            EGL_LINUX_DRM_FOURCC_EXT, fourcc,
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
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + QString("No EGLImage for plane %1 %2")
                .arg(plane).arg(Context->GetEGLError()));
            ClearDMATextures(Context, result);
            return result;
        }

        Context->glBindTexture(result[plane]->m_target, result[plane]->m_textureId);
        Context->eglImageTargetTexture2DOES(result[plane]->m_target, image);
        Context->glBindTexture(result[plane]->m_target, 0);
        result[plane]->m_data = static_cast<unsigned char *>(image);
    }

    return result;
}

void MythEGLDMABUF::ClearDMATextures(MythRenderOpenGL* Context,
                                     std::vector<MythVideoTextureOpenGL *> &Textures)
{
    for (auto & texture : Textures)
    {
        if (texture->m_data)
            Context->eglDestroyImageKHR(Context->GetEGLDisplay(), texture->m_data);
        texture->m_data = nullptr;
        if (texture->m_textureId)
            Context->glDeleteTextures(1, &texture->m_textureId);
        MythVideoTextureOpenGL::DeleteTexture(Context, texture);
    }
    Textures.clear();
}

std::vector<MythVideoTextureOpenGL*> MythEGLDMABUF::CreateTextures(AVDRMFrameDescriptor* Desc,
                                                                   MythRenderOpenGL *Context,
                                                                   MythVideoFrame *Frame,
                                                                   bool UseSeparate,
                                                                   FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
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
    {
        if (UseSeparate)
            return CreateSeparate2(Desc, Context, Frame);
        return CreateComposed(Desc, Context, Frame, Scan);
    }
    // X layers with one plane each
    return CreateSeparate(Desc, Context, Frame);
}
