#ifndef MYTHVIDEOTEXTURE_H
#define MYTHVIDEOTEXTURE_H

// MythTV
#include "opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
}

// Std
#include <vector>

class QMatrix4x4;

class MythVideoTexture : public MythGLTexture
{
  public:
    explicit MythVideoTexture(GLuint Texture);
    static std::vector<MythVideoTexture*> CreateTextures(MythRenderOpenGL* Context,
                                                    VideoFrameType Type,
                                                    VideoFrameType Format,
                                                    std::vector<QSize> Sizes,
                                                    GLenum Target = QOpenGLTexture::Target2D);
    static MythVideoTexture* CreateHelperTexture(MythRenderOpenGL *Context);
    static void UpdateTextures(MythRenderOpenGL* Context, const VideoFrame *Frame,
                               const std::vector<MythVideoTexture*> &Textures);
    static void DeleteTexture (MythRenderOpenGL* Context, MythVideoTexture *Texture);
    static void DeleteTextures(MythRenderOpenGL* Context, std::vector<MythVideoTexture*> &Textures);
    static void SetTextureFilters(MythRenderOpenGL* Context, const std::vector<MythVideoTexture*> &Textures,
                                  QOpenGLTexture::Filter Filter,
                                  QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
    static MythVideoTexture* CreateTexture(MythRenderOpenGL *Context, QSize Size,
                                           GLenum Target = QOpenGLTexture::Target2D,
                                           QOpenGLTexture::PixelType PixelType = QOpenGLTexture::UInt8,
                                           QOpenGLTexture::PixelFormat PixelFormat = QOpenGLTexture::RGBA,
                                           QOpenGLTexture::TextureFormat Format = QOpenGLTexture::NoFormat,
                                           QOpenGLTexture::Filter Filter = QOpenGLTexture::Linear,
                                           QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
   ~MythVideoTexture() = default;

  public:
    bool           m_valid          { false };
    QOpenGLTexture::Filter m_filter { QOpenGLTexture::Linear };
    VideoFrameType m_frameType      { FMT_NONE };
    VideoFrameType m_frameFormat    { FMT_NONE };
    uint           m_plane          { 0 };
    uint           m_planeCount     { 0 };
    bool           m_allowGLSLDeint { false };
#ifdef USING_MEDIACODEC
    QMatrix4x4    *m_transform      { nullptr };
#endif
    MythAVCopy    *m_copyContext    { nullptr };

  protected:
    explicit MythVideoTexture(QOpenGLTexture* Texture);

  private:
    static std::vector<MythVideoTexture*> CreateHardwareTextures(MythRenderOpenGL* Context,
                                                            VideoFrameType Type, VideoFrameType Format,
                                                            std::vector<QSize> Sizes, GLenum Target);
    static std::vector<MythVideoTexture*> CreateSoftwareTextures(MythRenderOpenGL* Context,
                                                            VideoFrameType Type, VideoFrameType Format,
                                                            std::vector<QSize> Sizes, GLenum Target);

    Q_DISABLE_COPY(MythVideoTexture)
    static void YV12ToYV12   (MythRenderOpenGL *Context, const VideoFrame *Frame, MythVideoTexture* Texture, uint Plane);
    static void YV12ToYUYV   (const VideoFrame *Frame, MythVideoTexture* Texture);
    static void NV12ToNV12   (MythRenderOpenGL *Context, const VideoFrame *Frame, MythVideoTexture* Texture, uint Plane);
    static bool CreateBuffer (MythVideoTexture* Texture, int Size);
    static void StoreBicubicWeights(float X, float *Dest);
};

#endif // MYTHVIDEOTEXTURE_H
