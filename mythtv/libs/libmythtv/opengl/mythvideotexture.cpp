// MythTV
#include "mythlogging.h"
#include "mythvideotexture.h"

#define LOC QString("MythVidTex: ")

MythVideoTexture::MythVideoTexture(QOpenGLTexture* Texture)
  : MythGLTexture(Texture)
{
}

MythVideoTexture::MythVideoTexture(GLuint Texture)
  : MythGLTexture(Texture)
{
}

void MythVideoTexture::DeleteTexture(MythRenderOpenGL *Context, MythVideoTexture *Texture)
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

void MythVideoTexture::DeleteTextures(MythRenderOpenGL *Context, vector<MythVideoTexture *> &Textures)
{
    if (!Context || Textures.empty())
        return;
    for (uint i = 0; i < Textures.size(); ++i)
        DeleteTexture(Context, Textures[i]);
    Textures.clear();
}

void MythVideoTexture::SetTextureFilters(MythRenderOpenGL *Context,
                                         const vector<MythVideoTexture *> &Textures,
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
vector<MythVideoTexture*> MythVideoTexture::CreateTextures(MythRenderOpenGL *Context,
                                                           VideoFrameType Type,
                                                           VideoFrameType Format,
                                                           vector<QSize> Sizes,
                                                           GLenum Target)
{
    vector<MythVideoTexture*> result;
    if (!Context || Sizes.empty())
        return result;
    if (Sizes[0].isEmpty())
        return result;

    OpenGLLocker locker(Context);

    // Hardware frames
    if (format_is_hw(Type))
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
vector<MythVideoTexture*> MythVideoTexture::CreateHardwareTextures(MythRenderOpenGL *Context,
                                                                   VideoFrameType Type,
                                                                   VideoFrameType Format,
                                                                   vector<QSize> Sizes,
                                                                   GLenum Target)
{
    vector<MythVideoTexture*> result;

    uint count = planes(Format);
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
        GLuint textureid;
        Context->glGenTextures(1, &textureid);
        if (!textureid)
            continue;

        auto* texture = new MythVideoTexture(textureid);
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
vector<MythVideoTexture*> MythVideoTexture::CreateSoftwareTextures(MythRenderOpenGL *Context,
                                                                   VideoFrameType Type,
                                                                   VideoFrameType Format,
                                                                   vector<QSize> Sizes,
                                                                   GLenum Target)
{
    vector<MythVideoTexture*> result;

    uint count = planes(Format);
    if (count < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame format");
        return result;
    }

    // Strict OpenGL ES 2.0 has no GL_RED or GL_R8 so use Luminance alternatives which give the same result
    // There is no obvious alternative solution for GL_RG/8 for higher bit depths. These will fail on some
    // implementations.
    bool legacy = Context->GetExtraFeatures() & kGLLegacyTextures;
    QOpenGLTexture::PixelFormat bytepixfmt   = legacy ? QOpenGLTexture::Luminance       : QOpenGLTexture::Red;
    QOpenGLTexture::TextureFormat bytetexfmt = legacy ? QOpenGLTexture::LuminanceFormat : QOpenGLTexture::R8_UNorm;
    for (uint plane = 0; plane < count; ++plane)
    {
        QSize size = Sizes[0];
        MythVideoTexture* texture = nullptr;
        switch (Format)
        {
            case FMT_YV12:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height() >> 1);
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, bytepixfmt, bytetexfmt);
                break;
            case FMT_YUV420P9:
            case FMT_YUV420P10:
            case FMT_YUV420P12:
            case FMT_YUV420P14:
            case FMT_YUV420P16:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height() >> 1);
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, bytepixfmt, QOpenGLTexture::R16_UNorm);
                break;
            case FMT_YUY2:
                size.setWidth(size.width() >> 1);
                texture = CreateTexture(Context, size, Target);
                break;
            case FMT_NV12:
                if (plane == 0)
                {
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt8, bytepixfmt, bytetexfmt);
                }
                else
                {
                    size = QSize(size.width() >> 1, size.height() >> 1);
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt8, QOpenGLTexture::RG, QOpenGLTexture::RG8_UNorm);
                }
                break;
            case FMT_P010:
            case FMT_P016:
                if (plane == 0)
                {
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt16, bytepixfmt, QOpenGLTexture::R16_UNorm);
                }
                else
                {
                    size = QSize(size.width() >> 1, size.height() >> 1);
                    texture = CreateTexture(Context, size, Target,
                                  QOpenGLTexture::UInt16, QOpenGLTexture::RG, QOpenGLTexture::RG16_UNorm);
                }
                break;
            case FMT_YUV422P:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height());
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, bytepixfmt, bytetexfmt);
                break;
            case FMT_YUV422P9:
            case FMT_YUV422P10:
            case FMT_YUV422P12:
            case FMT_YUV422P14:
            case FMT_YUV422P16:
                if (plane > 0)
                    size = QSize(size.width() >> 1, size.height());
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, bytepixfmt, QOpenGLTexture::R16_UNorm);
                break;
            case FMT_YUV444P:
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt8, bytepixfmt, bytetexfmt);
                break;
            case FMT_YUV444P9:
            case FMT_YUV444P10:
            case FMT_YUV444P12:
            case FMT_YUV444P14:
            case FMT_YUV444P16:
                texture = CreateTexture(Context, size, Target,
                              QOpenGLTexture::UInt16, bytepixfmt, QOpenGLTexture::R16_UNorm);
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
void MythVideoTexture::UpdateTextures(MythRenderOpenGL *Context,
                                      const VideoFrame *Frame,
                                      const vector<MythVideoTexture*> &Textures)
{
    if (!Context || !Frame || Textures.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Error");
        return;
    }

    if (Frame->codec != Textures[0]->m_frameType)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Inconsistent video and texture frame types");
        return;
    }

    uint count = planes(Textures[0]->m_frameFormat);
    if (!count || (count != Textures.size()))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame type");
        return;
    }

    OpenGLLocker locker(Context);

    for (uint i = 0; i < count; ++i)
    {
        MythVideoTexture *texture = Textures[i];
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
MythVideoTexture* MythVideoTexture::CreateTexture(MythRenderOpenGL *Context,
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

    auto *result = new MythVideoTexture(texture);
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
inline void MythVideoTexture::YV12ToYV12(MythRenderOpenGL *Context, const VideoFrame *Frame,
                                         MythVideoTexture *Texture, uint Plane)
{
    if (Context->GetExtraFeatures() & kGLExtSubimage)
    {
        int pitch = (Frame->codec == FMT_YV12 || Frame->codec == FMT_YUV422P || Frame->codec == FMT_YUV444P) ?
                     Frame->pitches[Plane] : Frame->pitches[Plane] >> 1;
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch);
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
                                    static_cast<uint8_t*>(Frame->buf) + Frame->offsets[Plane]);
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    else
    {
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        int pitch = (Frame->codec == FMT_YV12 || Frame->codec == FMT_YUV422P || Frame->codec == FMT_YUV444P) ?
                     Texture->m_size.width() : Texture->m_size.width() << 1;
        copyplane(Texture->m_data, pitch, Frame->buf + Frame->offsets[Plane],
                  Frame->pitches[Plane], pitch, Texture->m_size.height());
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType, Texture->m_data);
    }
    Texture->m_valid = true;
}

/// \brief Copy YV12 frame data to a YUYV texture.
inline void MythVideoTexture::YV12ToYUYV(const VideoFrame *Frame, MythVideoTexture *Texture)
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
    Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType, buffer);
    Texture->m_valid = true;
}

/// \brief Copy NV12 video frame data to 'NV12' textures.
inline void MythVideoTexture::NV12ToNV12(MythRenderOpenGL *Context, const VideoFrame *Frame,
                                         MythVideoTexture *Texture, uint Plane)
{
    if (Context->GetExtraFeatures() & kGLExtSubimage)
    {
        bool hdr = Texture->m_frameFormat != FMT_NV12;
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, Plane ? Frame->pitches[Plane] >> (hdr ? 2 : 1) :
                                                             Frame->pitches[Plane] >> (hdr ? 1 : 0));
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
                                    static_cast<uint8_t*>(Frame->buf) + Frame->offsets[Plane]);
        Context->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    else
    {
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        copyplane(Texture->m_data, Frame->pitches[Plane], Frame->buf + Frame->offsets[Plane],
                  Frame->pitches[Plane], Frame->pitches[Plane], Texture->m_size.height());
        Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType, Texture->m_data);
    }
    Texture->m_valid = true;
}

/// \brief Create a data buffer for holding CPU side texture data.
inline bool MythVideoTexture::CreateBuffer(MythVideoTexture *Texture, int Size)
{
    if (!Texture || Size < 1)
        return false;

    unsigned char *scratch = GetAlignedBufferZero(static_cast<size_t>(Size));
    if (scratch)
    {
        Texture->m_data = scratch;
        return true;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to allocate texture buffer");
    return false;
}

MythVideoTexture* MythVideoTexture::CreateHelperTexture(MythRenderOpenGL *Context)
{
    if (!Context)
        return nullptr;

    OpenGLLocker locker(Context);

    int width = Context->GetMaxTextureSize();
    MythVideoTexture *texture = CreateTexture(Context, QSize(width, 1),
                                              QOpenGLTexture::Target2D,
                                              QOpenGLTexture::Float32,
                                              QOpenGLTexture::RGBA,
                                              QOpenGLTexture::RGBA16_UNorm,
                                              QOpenGLTexture::Linear,
                                              QOpenGLTexture::Repeat);

    float *buf = nullptr;
    buf = new float[texture->m_bufferSize];
    float *ref = buf;

    for (int i = 0; i < width; i++)
    {
        float x = (i + 0.5F) / static_cast<float>(width);
        StoreBicubicWeights(x, ref);
        ref += 4;
    }
    StoreBicubicWeights(0, buf);
    StoreBicubicWeights(1, &buf[(width - 1) << 2]);

    texture->m_texture->bind();
    texture->m_texture->setData(texture->m_pixelFormat, texture->m_pixelType, buf);
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Created bicubic helper texture (%1 samples)") .arg(width));
    delete [] buf;
    return texture;
}

void MythVideoTexture::StoreBicubicWeights(float X, float *Dest)
{
    float w0 = (((-1 * X + 3) * X - 3) * X + 1) / 6;
    float w1 = ((( 3 * X - 6) * X + 0) * X + 4) / 6;
    float w2 = (((-3 * X + 3) * X + 3) * X + 1) / 6;
    float w3 = ((( 1 * X + 0) * X + 0) * X + 0) / 6;
    *Dest++ = 1 + X - w1 / (w0 + w1);
    *Dest++ = 1 - X + w3 / (w2 + w3);
    *Dest++ = w0 + w1;
    *Dest++ = 0;
}
