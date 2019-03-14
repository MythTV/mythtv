// MythTV
#include "mythlogging.h"
#include "util-opengl.h"
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

MythVideoTexture::~MythVideoTexture()
{
}

void MythVideoTexture::DeleteTexture(MythRenderOpenGL *Context, MythVideoTexture *Texture)
{
    if (!Context || !Texture)
        return;

     OpenGLLocker locker(Context);
     if (Texture->m_copyContext)
         delete Texture->m_copyContext;
     if (Texture->m_texture)
         delete Texture->m_texture;
     if (Texture->m_data)
         delete [] Texture->m_data;
     if (Texture->m_vbo)
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
        Context->SetTextureFilters(reinterpret_cast<MythGLTexture*>(Textures[i]), Filter, Wrap);
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
                                                           QOpenGLTexture::Target Target)
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
*/
vector<MythVideoTexture*> MythVideoTexture::CreateHardwareTextures(MythRenderOpenGL *Context,
                                                                   VideoFrameType Type,
                                                                   VideoFrameType Format,
                                                                   vector<QSize> Sizes,
                                                                   QOpenGLTexture::Target Target)
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

        MythVideoTexture* texture = new MythVideoTexture(textureid);
        texture->m_frameType   = Type;
        texture->m_frameFormat = Format;
        texture->m_plane       = plane;
        texture->m_planeCount  = count;
        texture->m_target      = Target;
        texture->m_size        = Sizes[plane];
        texture->m_totalSize   = Context->GetTextureSize(Sizes[plane], texture->m_target != QOpenGLTexture::TargetRectangle);
        texture->m_vbo         = Context->CreateVBO(MythRenderOpenGL::kVertexSize);
        result.push_back(texture);
    }

    return result;
}

/*! \brief Create a set of OpenGL textures to represent the given Format.
 *
 * \todo Extend for frame types other than YV12.
*/
vector<MythVideoTexture*> MythVideoTexture::CreateSoftwareTextures(MythRenderOpenGL *Context,
                                                                   VideoFrameType Type,
                                                                   VideoFrameType Format,
                                                                   vector<QSize> Sizes,
                                                                   QOpenGLTexture::Target Target)
{
    vector<MythVideoTexture*> result;

    uint count = planes(Format);
    if (count < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Invalid software frame format");
        return result;
    }

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
                                        QOpenGLTexture::UInt8,  QOpenGLTexture::Luminance,
                                        QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge,
                                        QOpenGLTexture::LuminanceFormat);
                break;
            case FMT_YUY2:
                size.setWidth(size.width() >> 1);
                texture = CreateTexture(Context, size, Target);
                break;
            case FMT_YUYVHQ:
                texture = CreateTexture(Context, size, Target);
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
 *
 * \todo Extend for format other than YV12.
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
                    case FMT_YUY2:   YV12ToYUYV(Context, Frame, texture);    break;
                    case FMT_YUYVHQ: YV12ToYUYVHQ(Context, Frame, texture);  break;
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
                                                  QOpenGLTexture::Target Target,
                                                  QOpenGLTexture::PixelType PixelType,
                                                  QOpenGLTexture::PixelFormat PixelFormat,
                                                  QOpenGLTexture::Filter Filter,
                                                  QOpenGLTexture::WrapMode Wrap,
                                                  QOpenGLTexture::TextureFormat Format)
{
    if (!Context)
        return nullptr;

    OpenGLLocker locker(Context);

    int datasize = Context->GetBufferSize(Size, PixelFormat, PixelType);
    if (!datasize)
        return nullptr;

    QOpenGLTexture *texture = new QOpenGLTexture(Target);
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
        if (Context->isOpenGLES() && Context->format().majorVersion() < 3)
            Format = QOpenGLTexture::RGBAFormat;
        else
            Format = QOpenGLTexture::RGBA8_UNorm;
    }
    texture->setFormat(Format);
    texture->allocateStorage(PixelFormat, PixelType);
    texture->setMinMagFilters(Filter, Filter);
    texture->setWrapMode(Wrap);

    MythVideoTexture *result = new MythVideoTexture(texture);
    result->m_target      = Target;
    result->m_pixelFormat = PixelFormat;
    result->m_pixelType   = PixelType;
    result->m_vbo         = Context->CreateVBO(MythRenderOpenGL::kVertexSize);
    result->m_totalSize   = Context->GetTextureSize(Size, result->m_target != QOpenGLTexture::TargetRectangle);
    result->m_bufferSize  = datasize;
    result->m_size        = Size;
    return result;
}

/// \brief Copy YV12 frame data to 'YV12' textures.
void MythVideoTexture::YV12ToYV12(MythRenderOpenGL *Context, const VideoFrame *Frame,
                                  MythVideoTexture *Texture, int Plane)
{
    OpenGLLocker locker(Context);

    void *buffer = nullptr;
    int offset   = 0;
    int width    = Texture->m_size.width();

    // Direct copy
    if ((Frame->codec == FMT_YV12) && (Frame->pitches[Plane] == width))
    {
        buffer = Frame->buf;
        offset = Frame->offsets[Plane];
    }
    else
    {
        // Create a buffer
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        buffer = Texture->m_data;

        // Refresh
        copyplane(static_cast<uint8_t*>(buffer), width, Frame->buf + Frame->offsets[Plane],
                  Frame->pitches[Plane], width, Texture->m_size.height());
    }

    // Update
    Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType,
                                static_cast<uint8_t*>(buffer) + offset);
    Texture->m_valid = true;
}

/// \brief Copy YV12 frame data to a YUYV texture.
void MythVideoTexture::YV12ToYUYV(MythRenderOpenGL *Context, const VideoFrame *Frame,
                                  MythVideoTexture *Texture)
{
    OpenGLLocker locker(Context);

    void * buffer = nullptr;

    // Direct copy
    if ((Frame->codec == FMT_YUY2) && (Frame->pitches[0] == Texture->m_size.width()))
    {
        buffer = Frame->buf;
    }
    else
    {
        // Create a buffer
        if (!Texture->m_data)
            if (!CreateBuffer(Texture, Texture->m_bufferSize))
                return;
        buffer = Texture->m_data;

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
    }

    // Update
    Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType, buffer);
    Texture->m_valid = true;
}

/// \brief Copy YV12 frame data to a YUYV texture with high quality interlaced chroma sampling.
void MythVideoTexture::YV12ToYUYVHQ(MythRenderOpenGL *Context, const VideoFrame *Frame,
                                    MythVideoTexture *Texture)
{
    OpenGLLocker locker(Context);

    // Create a buffer
    if (!Texture->m_data)
        if (!CreateBuffer(Texture, Texture->m_bufferSize))
            return;

    // Convert
    if (Frame->interlaced_frame)
    {
        pack_yv12interlaced(Frame->buf, static_cast<unsigned char*>(Texture->m_data),
                            Frame->offsets, Frame->pitches, Texture->m_size);
    }
    else
    {
        pack_yv12progressive(Frame->buf, static_cast<unsigned char*>(Texture->m_data),
                             Frame->offsets, Frame->pitches, Texture->m_size);
    }

    // Update
    Texture->m_texture->setData(Texture->m_pixelFormat, Texture->m_pixelType, Texture->m_data);
    Texture->m_valid = true;
}

/// \brief Create a data buffer for holding CPU side texture data.
bool MythVideoTexture::CreateBuffer(MythVideoTexture *Texture, int Size)
{
    if (!Texture || Size < 1)
        return false;

    unsigned char *scratch = new unsigned char[Size];
    if (scratch)
    {
        memset(scratch, 0, static_cast<size_t>(Size));
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
                                              QOpenGLTexture::Linear,
                                              QOpenGLTexture::Repeat,
                                              QOpenGLTexture::RGBA16_UNorm);

    float *buf = nullptr;
    buf = new float[texture->m_bufferSize];
    float *ref = buf;

    for (int i = 0; i < width; i++)
    {
        float x = (i + 0.5f) / static_cast<float>(width);
        StoreBicubicWeights(x, ref);
        ref += 4;
    }
    StoreBicubicWeights(0, buf);
    StoreBicubicWeights(1, &buf[(width - 1) << 2]);

    Context->EnableTextures();
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
