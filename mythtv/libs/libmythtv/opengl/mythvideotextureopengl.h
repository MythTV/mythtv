#ifndef MYTHVIDEOTEXTURE_H
#define MYTHVIDEOTEXTURE_H

// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythui/opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
}

// Std
#include <vector>

class QMatrix4x4;
class MythVideoTextureOpenGL;
using VideoFramebuffer = std::pair<QOpenGLFramebufferObject*, MythVideoTextureOpenGL*>;

class MythVideoTextureOpenGL : public MythGLTexture
{
  public:
    explicit MythVideoTextureOpenGL(GLuint Texture);
    static std::vector<MythVideoTextureOpenGL*> CreateTextures(MythRenderOpenGL* Context,
                                                               VideoFrameType Type,
                                                               VideoFrameType Format,
                                                               std::vector<QSize> Sizes,
                                                               GLenum Target = QOpenGLTexture::Target2D);
    static void UpdateTextures(MythRenderOpenGL* Context, const MythVideoFrame *Frame,
                               const std::vector<MythVideoTextureOpenGL*> &Textures);
    static void DeleteTexture (MythRenderOpenGL* Context, MythVideoTextureOpenGL *Texture);
    static void DeleteTextures(MythRenderOpenGL* Context, std::vector<MythVideoTextureOpenGL*> &Textures);
    static void SetTextureFilters(MythRenderOpenGL* Context, const std::vector<MythVideoTextureOpenGL*> &Textures,
                                  QOpenGLTexture::Filter Filter,
                                  QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
    static MythVideoTextureOpenGL* CreateTexture(MythRenderOpenGL *Context, QSize Size,
                                                 GLenum Target = QOpenGLTexture::Target2D,
                                                 QOpenGLTexture::PixelType PixelType = QOpenGLTexture::UInt8,
                                                 QOpenGLTexture::PixelFormat PixelFormat = QOpenGLTexture::RGBA,
                                                 QOpenGLTexture::TextureFormat Format = QOpenGLTexture::NoFormat,
                                                 QOpenGLTexture::Filter Filter = QOpenGLTexture::Linear,
                                                 QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge);
    static VideoFramebuffer CreateVideoFrameBuffer(MythRenderOpenGL* Context, VideoFrameType OutputType,
                                                   QSize Size, bool HighPrecision = true);
   ~MythVideoTextureOpenGL() override = default;

  public:
    bool           m_valid          { false };
    QOpenGLTexture::Filter m_filter { QOpenGLTexture::Linear };
    VideoFrameType m_frameType      { FMT_NONE };
    VideoFrameType m_frameFormat    { FMT_NONE };
    uint           m_plane          { 0 };
    uint           m_planeCount     { 0 };
    bool           m_allowGLSLDeint { false };
#if CONFIG_MEDIACODEC
    QMatrix4x4    *m_transform      { nullptr };
#endif
    MythAVCopy    *m_copyContext    { nullptr };

  protected:
    explicit MythVideoTextureOpenGL(QOpenGLTexture* Texture);

  private:
    static std::vector<MythVideoTextureOpenGL*> CreateHardwareTextures(MythRenderOpenGL* Context,
                                                                       VideoFrameType Type, VideoFrameType Format,
                                                                       std::vector<QSize> Sizes, GLenum Target);
    static std::vector<MythVideoTextureOpenGL*> CreateSoftwareTextures(MythRenderOpenGL* Context,
                                                                       VideoFrameType Type, VideoFrameType Format,
                                                                       std::vector<QSize> Sizes, GLenum Target);

    Q_DISABLE_COPY(MythVideoTextureOpenGL)
    static void YV12ToYV12   (MythRenderOpenGL *Context, const MythVideoFrame *Frame,
                              MythVideoTextureOpenGL* Texture, uint Plane);
    static void YV12ToYUYV   (const MythVideoFrame *Frame, MythVideoTextureOpenGL* Texture);
    static void NV12ToNV12   (MythRenderOpenGL *Context, const MythVideoFrame *Frame,
                              MythVideoTextureOpenGL* Texture, uint Plane);
    static bool CreateBuffer (MythVideoTextureOpenGL* Texture, int Size);
};

#endif
