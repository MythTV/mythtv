// MythTV
#include "libmythbase/mythlogging.h"
#include "libmythtv/opengl/mythvideotextureopengl.h"

#define LOC QString("MythVidTex: ")

MythVideoTextureOpenGL::MythVideoTextureOpenGL(QOpenGLTexture* Texture)
  : MythGLTexture(Texture)
{
}

MythVideoTextureOpenGL::MythVideoTextureOpenGL(GLuint Texture)
  : MythGLTexture(Texture)
{
}

void MythVideoTextureOpenGL::DeleteTexture(MythRenderOpenGL *Context, MythVideoTextureOpenGL *Texture)
{
    if (!Context || !Texture)
        return;

     OpenGLLocker locker(Context);
     delete Texture->m_copyContext;
     delete Texture->m_texture;
     delete [] Texture->m_data;
     delete Texture->m_vbo;

     delete Texture;
}

void MythVideoTextureOpenGL::DeleteTextures(MythRenderOpenGL *Context, std::vector<MythVideoTextureOpenGL *> &Textures)
{
    if (!Context || Textures.empty())
        return;
    for (auto & Texture : Textures)
        DeleteTexture(Context, Texture);
    Textures.clear();
}

void MythVideoTextureOpenGL::SetTextureFilters(MythRenderOpenGL *Context,
                                               const std::vector<MythVideoTextureOpenGL *> &Textures,
                                               QOpenGLTexture::Filter Filter,
                                               QOpenGLTexture::WrapMode Wrap)
{
    for (uint i = 0; (i < Textures.size()) && Context; i++)
    {
        Textures[i]->m_filter = Filter;
        Context->SetTextureFilters(reinterpret_cast<MythGLTexture*>(Textures[i]), Filter, Wrap);
    }
}

/*! \brief Create a set of textures suitable for the given Type and Format.
 *
 * \param Type   Source video frame type.
 * \param Format Output frame type.
*/
std::vector<MythVideoTextureOpenGL*> MythVideoTextureOpenGL::CreateTextures(MythRenderOpenGL *Context,
                                                                            VideoFrameType Type,
                                                                            VideoFrameType Format,
                                                                            std::vector<QSize> Sizes,
                                                                            GLenum Target)
{
    std::vector<MythVideoTextureOpenGL*> result;
    if (!Context || Sizes.empty())
        return result;
    if (Sizes[0].isEmpty())
        return result;

    OpenGLLocker locker(Context);

    // Hardware frames
    if (MythVideoFrame::HardwareFormat(Type))
        return CreateHardwareTextures(Context, Type, Format, Sizes, Target);

    return CreateSoftwareTextures(Context, Type, Format, Sizes, Target);
}

/*! \brief Create a set of hardware textures suitable for the given format.
 *
 * \note This is a simple wrapper that deliberately does nothing more than create
 * a texture and sets suitable defaults. In most instances, hardware textures require
 * specific handling.
 * \note Linear filtering is always set (except MediaCodec/MMAL which use OES)
*/
std::vector<MythVideoTextureOpenGL*> MythVideoTextureOpenGL::CreateHardwareTextures(MythRenderOpenGL *Context,
                                                                                    VideoFrameType Type,
                                                                                    VideoFrameType Format,
                                                                                    std::vector<QSize> Sizes,
                                                                                    GLenum Target)
{
    std::vector<MythVideoTextureOpenGL*> result;

    uint count = MythVideoFrame::GetNumPlanes(Format);
    if (count < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid hardware frame format");
        return result;
    }

    if (count != Sizes.size())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent plane count");
        return result;
    }

    for (uint plane = 0; plane < count; ++plane)
    {
        GLuint textureid = 0;
        Context->glGenTextures(1, &textureid);
        if (!textureid)
            continue;

        auto* texture = new MythVideoTextureOpenGL(textureid);
        texture->m_frameType   = Type;
        texture->m_frameFormat = Format;
        texture->m_plane       = plane;
        texture->m_planeCount  = count;
        texture->m_target      = Target;
        texture->m_size        = Sizes[plane];
        texture->m_totalSize   = Context->GetTextureSize(Sizes[plane], texture->m_target != QOpenGLTexture::TargetRectangle);
        texture->m_vbo         = Context->CreateVBO(static_cast<int>(MythRenderOpenGL::kVertexSize));
        result.push_back(texture);
    }

    SetTextureFilters(Context, result, QOpenGLTexture::Linear);
    return result;
}

/*! \brief Create a set of OpenGL textures to represent the given Format.
 *
 * \note All textures are created with CreateTexture which defaults to Linear filtering.
*/
std::vector<MythVideoTextureOpenGL*> MythVideoTextureOpenGL::CreateSoftwareTextures(MythRenderOpenGL *Context,
                                                                                    VideoFrameType Type,
                                                                                    VideoFrameType Format,
                                                                                    std::vector<QSize> Sizes,
                                                                                    GLenum Target)
{
    std::vector<MythVideoTextureOpenGL*> result;

    uint count = MythVideoFrame::GetNumPlanes(Format);
    if (count < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame format");
        return result;
    }

    // OpenGL ES 2.0 has very limited texture format support
    bool legacy = (Context->GetExtraFeatures() & kGLLegacyTextures) != 0;
    // GLES3 supports GL_RED etc and 16bit formats but there are no unsigned,
    // normalised 16bit integer formats. So we must use unsigned integer formats
    // for 10bit video textures which force:-
    //  - only nearest texture filtering - so we must use the resize stage
    //  - no GLSL mix method for uint/uvec4
    //  - must use unsigned samplers in the shaders - and convert uint to normalised float as necessary
    //  - need to use GLSL ES 3.00 - which means new/updated shaders
    bool gles3  = Context->isOpenGLES() && Context->format().majorVersion() > 2;
    QOpenGLTexture::PixelFormat   r8pixelfmtnorm  = QOpenGLTexture::Red;
    QOpenGLTexture::PixelFormat   r8pixelfmtuint  = QOpenGLTexture::Red;
    QOpenGLTexture::PixelFormat   rg8pixelfmtnorm = QOpenGLTexture::RG;
    QOpenGLTexture::PixelFormat   rg8pixelfmtuint = QOpenGLTexture::RG;
    QOpenGLTexture::TextureFormat r8internalfmt   = QOpenGLTexture::R8_UNorm;
    QOpenGLTexture::TextureFormat r16internalfmt  = QOpenGLTexture::R16_UNorm;
    QOpenGLTexture::TextureFormat rg8internalfmt  = QOpenGLTexture::RG8_UNorm;
    QOpenGLTexture::TextureFormat rg16internalfmt = QOpenGLTexture::RG16_UNorm;

    if (gles3)
    {
        r8pixelfmtuint  = QOpenGLTexture::Red_Integer;
        rg8pixelfmtuint = QOpenGLTexture::RG_Integer;
        r16internalfmt  = QOpenGLTexture::R16U;
        rg16internalfmt = QOpenGLTexture::RG16U;
    }
    else if (legacy)
    {
        r8pixelfmtnorm = QOpenGLTexture::Luminance;
        r8internalfmt  = QOpenGLTexture::LuminanceFormat;
    }

    for (uint plane = 0; plane < count; ++plane)
    {
        QSize size = Sizes[0];
        MythVideoTextureOpenGL* texture = nullptr;
        switch (Format)
        {
            case FMT_YV12:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height() >> 1);
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, r8pixelfmtnorm, r8internalfmt);
                break;
            case FMT_YUV420P9:
            case FMT_YUV420P10:
            case FMT_YUV420P12:
            case FMT_YUV420P14:
            case FMT_YUV420P16:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height() >> 1);
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, r8pixelfmtuint, r16internalfmt);
                break;
            case FMT_YUY2:
                size.setWidth(size.width() >> 1);
                texture = CreateTexture(Context, size, Target);
                break;
            case FMT_NV12:
                if (plane == 0)
                {
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt8, r8pixelfmtnorm, r8internalfmt);
                }
                else
                {
                    size = QSize(size.width() >> 1, size.height() >> 1);
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt8, rg8pixelfmtnorm, rg8internalfmt);
                }
                break;
            case FMT_P010:
            case FMT_P016:
                if (plane == 0)
                {
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt16, r8pixelfmtuint, r16internalfmt);
                }
                else
                {
                    size = QSize(size.width() >> 1, size.height() >> 1);
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt16, rg8pixelfmtuint, rg16internalfmt);
                }
                break;
            case FMT_YUV422P:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height());
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, r8pixelfmtnorm, r8internalfmt);
                break;
            case FMT_YUV422P9:
            case FMT_YUV422P10:
            case FMT_YUV422P12:
            case FMT_YUV422P14:
            case FMT_YUV422P16:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height());
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, r8pixelfmtuint, r16internalfmt);
                break;
            case FMT_YUV444P:
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, r8pixelfmtnorm, r8internalfmt);
                break;
            case FMT_YUV444P9:
            case FMT_YUV444P10:
            case FMT_YUV444P12:
            case FMT_YUV444P14:
            case FMT_YUV444P16:
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, r8pixelfmtuint, r16internalfmt);
                break;
            default: break;
        }
        if (texture)
        {
            texture->m_plane       = plane;
            texture->m_planeCount  = count;
            texture->m_frameType   = Type;
            texture->m_frameFormat = Format;
            result.push_back(texture);
        }
    }

    if (result.size() != count)
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create all textures: %1 < %2")
            .arg(result.size()).arg(count));
    return result;
}

/*! \brief Update the contents of the given Textures for data held in Frame.
*/
void MythVideoTextureOpenGL::UpdateTextures(MythRenderOpenGL *Context,
                                            const MythVideoFrame *Frame,
                                            const std::vector<MythVideoTextureOpenGL*> &Textures)
{
    if (!Context || !Frame || Textures.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Texture error: Context %1 Frame %2 Textures %3")
            .arg(Context != nullptr).arg(Frame != nullptr).arg(Textures.size()));
        return;
    }

    if (Frame->m_type != Textures[0]->m_frameType)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent video and texture frame types");
        return;
    }

    uint count = MythVideoFrame::GetNumPlanes(Textures[0]->m_frameFormat);
    if (!count || (count != Textures.size()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame type");
        return;
    }

    OpenGLLocker locker(Context);

    for (uint i = 0; i < count; ++i)
    {
        MythVideoTextureOpenGL *texture = Textures[i];
        if (!texture)
            continue;
        if (!texture->m_texture)
            continue;

        if (i != texture->m_plane)
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent plane numbering");
            continue;
        }

        switch (texture->m_frameType)
        {
            case FMT_YV12:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YV12:   YV12ToYV12(Context, Frame, texture, i); break;
                    case FMT_YUY2:   YV12ToYUYV(Frame, texture);    break;
                    default: break;
                }
                break;
            }
            case FMT_YUV420P9:
            case FMT_YUV420P10:
            case FMT_YUV420P12:
            case FMT_YUV420P14:
            case FMT_YUV420P16:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YUV420P9:
                    case FMT_YUV420P10:
                    case FMT_YUV420P12:
                    case FMT_YUV420P14:
                    case FMT_YUV420P16: YV12ToYV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_NV12:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_NV12:   NV12ToNV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_P010:
            case FMT_P016:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_P010:
                    case FMT_P016: NV12ToNV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_YUV422P:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YUV422P: YV12ToYV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_YUV422P9:
            case FMT_YUV422P10:
            case FMT_YUV422P12:
            case FMT_YUV422P14:
            case FMT_YUV422P16:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YUV422P9:
                    case FMT_YUV422P10:
                    case FMT_YUV422P12:
                    case FMT_YUV422P14:
                    case FMT_YUV422P16: YV12ToYV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_YUV444P:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YUV444P: YV12ToYV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            case FMT_YUV444P9:
            case FMT_YUV444P10:
            case FMT_YUV444P12:
            case FMT_YUV444P14:
            case FMT_YUV444P16:
            {
                switch (texture->m_frameFormat)
                {
                    case FMT_YUV444P9:
                    case FMT_YUV444P10:
                    case FMT_YUV444P12:
                    case FMT_YUV444P14:
                    case FMT_YUV444P16: YV12ToYV12(Context, Frame, texture, i); break;
                    default: break;
                }
                break;
            }
            default: break;
        }
    }
}

/*! \brief Create and initialise a MythVideoTexture that is backed by a QOpenGLTexture.
 *
 * QOpenGLTexture abstracts much of the detail of handling textures but is not compatible
 * with a number of hardware decoder requirements.
*/
MythVideoTextureOpenGL* MythVideoTextureOpenGL::CreateTexture(MythRenderOpenGL *Context,
                                                              QSize Size,
                                                              GLenum Target,
                                                              QOpenGLTexture::PixelType PixelType,
                                                              QOpenGLTexture::PixelFormat PixelFormat,
                                                              QOpenGLTexture::TextureFormat Format,
                                                              QOpenGLTexture::Filter Filter,
                                                              QOpenGLTexture::WrapMode Wrap)
{
    if (!Context)
        return nullptr;

    OpenGLLocker locker(Context);

    int datasize = MythRenderOpenGL::GetBufferSize(Size, PixelFormat, PixelType);
    if (!datasize)
        return nullptr;

    auto *texture = new QOpenGLTexture(static_cast<QOpenGLTexture::Target>(Target));
    texture->setAutoMipMapGenerationEnabled(false);
    texture->setMipLevels(1);
    texture->setSize(Size.width(), Size.height()); // this triggers creation
    if (!texture->textureId())
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "Failed to create texture");
        delete texture;
        return nullptr;
    }

    if (Format == QOpenGLTexture::NoFormat)
    {
        if (Context->GetExtraFeatures() & kGLLegacyTextures)
            Format = QOpenGLTexture::RGBAFormat;
        else
            Format = QOpenGLTexture::RGBA8_UNorm;
    }
    texture->setFormat(Format);
    texture->allocateStorage(PixelFormat, PixelType);
    texture->setMinMagFilters(Filter, Filter);
    texture->setWrapMode(Wrap);

    auto *result = new MythVideoTextureOpenGL(texture);
    result->m_target      = Target;
    result->m_pixelFormat = PixelFormat;
    result->m_pixelType   = PixelType;
    result->m_vbo         = Context->CreateVBO(static_cast<int>(MythRenderOpenGL::kVertexSize));
    result->m_totalSize   = Context->GetTextureSize(Size, result->m_target != QOpenGLTexture::TargetRectangle);
    result->m_bufferSize  = datasize;
    result->m_size        = Size;
    return result;
}

/// \brief Copy YV12 frame data to 'YV12' textures.
inline void MythVideoTextureOpenGL::YV12ToYV12(MythRenderOpenGL *Context, const MythVideoFrame *Frame,
                                               MythVideoTextureOpenGL *Texture, uint Plane)
{
    if (Context->GetExtraFeatures() & kGLExtSubimage)
    {
        int pitch = (Frame->m_type == FMT_YV12 || Frame->m_type == FMT_YUV422P || Frame->m_type == FMT_YUV444P) ?
                     Frame->m_pitches[Plane] : Frame->m_pitches[Plane] >> 1;
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
                                    static_cast<const uint8_t*>(Frame->m_buffer) + Frame->m_offsets[Plane]);
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    else
    {
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        int pitch = (Frame->m_type == FMT_YV12 || Frame->m_type == FMT_YUV422P || Frame->m_type == FMT_YUV444P) ?
                     Texture->m_size.width() : Texture->m_size.width() << 1;
        MythVideoFrame::CopyPlane(Texture->m_data, pitch, Frame->m_buffer + Frame->m_offsets[Plane],
                                  Frame->m_pitches[Plane], pitch, Texture->m_size.height());
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
				    static_cast<const uint8_t *>(Texture->m_data));
    }
    Texture->m_valid = true;
}

/// \brief Copy YV12 frame data to a YUYV texture.
inline void MythVideoTextureOpenGL::YV12ToYUYV(const MythVideoFrame *Frame, MythVideoTextureOpenGL *Texture)
{
    // Create a buffer
    if (!Texture->m_data)
        if (!CreateBuffer(Texture, Texture->m_bufferSize))
            return;
   void* buffer = Texture->m_data;

    // Create a copy context
    if (!Texture->m_copyContext)
    {
        Texture->m_copyContext = new MythAVCopy();
        if (!Texture->m_copyContext)
            return;
    }

    // Convert
    AVFrame out;
    Texture->m_copyContext->Copy(&out, Frame, static_cast<unsigned char*>(buffer), AV_PIX_FMT_UYVY422);

    // Update
    Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
				static_cast<const uint8_t *>(buffer));
    Texture->m_valid = true;
}

/// \brief Copy NV12 video frame data to 'NV12' textures.
inline void MythVideoTextureOpenGL::NV12ToNV12(MythRenderOpenGL *Context, const MythVideoFrame *Frame,
                                               MythVideoTextureOpenGL *Texture, uint Plane)
{
    if (Context->GetExtraFeatures() & kGLExtSubimage)
    {
        bool hdr = Texture->m_frameFormat != FMT_NV12;
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, Plane ? Frame->m_pitches[Plane] >> (hdr ? 2 : 1) :
                                                             Frame->m_pitches[Plane] >> (hdr ? 1 : 0));
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
                                    static_cast<const uint8_t*>(Frame->m_buffer) + Frame->m_offsets[Plane]);
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    else
    {
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        MythVideoFrame::CopyPlane(Texture->m_data, Frame->m_pitches[Plane], Frame->m_buffer + Frame->m_offsets[Plane],
                                  Frame->m_pitches[Plane], Frame->m_pitches[Plane], Texture->m_size.height());
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
				    static_cast<const uint8_t *>(Texture->m_data));
    }
    Texture->m_valid = true;
}

/// \brief Create a data buffer for holding CPU side texture data.
inline bool MythVideoTextureOpenGL::CreateBuffer(MythVideoTextureOpenGL *Texture, int Size)
{
    if (!Texture || Size < 1)
        return false;

    unsigned char *scratch = MythVideoFrame::GetAlignedBuffer(static_cast<size_t>(Size));
    if (scratch)
    {
        memset(scratch, 0, static_cast<size_t>(Size));
        Texture->m_data = scratch;
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate texture buffer");
    return false;
}

VideoFramebuffer MythVideoTextureOpenGL::CreateVideoFrameBuffer(MythRenderOpenGL* Context, VideoFrameType OutputType,
                                                                QSize Size, bool HighPrecision)
{
    // Use a 16bit float framebuffer if requested/required and available (not GLES2) to maintain precision.
    // The depth check will pick up all software formats as well as NVDEC, VideoToolBox and VAAPI DRM.
    // VAAPI GLXPixmap and GLXCopy are currently not 10/12bit aware and VDPAU has no 10bit support -
    // and all return RGB formats anyway. The MediaCodec texture format is an unknown but resizing will
    // never be enabled as it returns an RGB frame - so if MediaCodec uses a 16bit texture, precision
    // will be preserved.
    bool sixteenbitfb  = Context->GetExtraFeatures() & kGL16BitFBO;
    bool sixteenbitvid = HighPrecision ? true : MythVideoFrame::ColorDepth(OutputType) > 8;
    if (sixteenbitfb && sixteenbitvid)
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Requesting 16bit framebuffer texture");
    auto * framebuffer = Context->CreateFramebuffer(Size, sixteenbitfb && sixteenbitvid);
    if (framebuffer == nullptr)
        return { nullptr, nullptr };
    auto * texture = new MythVideoTextureOpenGL(framebuffer->texture());
    texture->m_size = texture->m_totalSize = framebuffer->size();
    texture->m_vbo  = Context->CreateVBO(static_cast<int>(MythRenderOpenGL::kVertexSize));
    texture->m_flip = false;
    texture->m_planeCount = 1;
    texture->m_frameType = FMT_RGBA32;
    texture->m_frameFormat = FMT_RGBA32;
    texture->m_valid = true;
    return { framebuffer, texture };
}
