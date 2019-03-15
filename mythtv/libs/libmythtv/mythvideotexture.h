#ifndef MYTHVIDEOTEXTURE_H
#define MYTHVIDEOTEXTURE_H

// MythTV
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "mythframe.h"

// FFmpeg
extern "C" {
#include "libavutil/pixfmt.h"
}

// Std
#include <vector>

using namespace std;
class QMatrix4x4;

class MythVideoTexture : public MythGLTexture
{
  public:
    static vector<MythVideoTexture*> CreateTextures(MythRenderOpenGL* Context,
                                                    VideoFrameType Type,
                                                    VideoFrameType Format,
                                                    vector<QSize> Sizes,
                                                    QOpenGLTexture::Target Target = QOpenGLTexture::Target2D);
    static MythVideoTexture* CreateHelperTexture(MythRenderOpenGL *Context);
    static void UpdateTextures(MythRenderOpenGL* Context, const VideoFrame *Frame,
                               const vector<MythVideoTexture*> &Textures);
    static void DeleteTexture (MythRenderOpenGL* Context, MythVideoTexture *Texture);
    static void DeleteTextures(MythRenderOpenGL* Context, vector<MythVideoTexture*> &Textures);
    static void SetTextureFilters(MythRenderOpenGL* Context, const vector<MythVideoTexture*> &Textures,
                                  QOpenGLTexture::Filter Filter, QOpenGLTexture::WrapMode Wrap);
    static MythVideoTexture* CreateTexture(MythRenderOpenGL *Context, QSize Size,
                                           QOpenGLTexture::Target Target = QOpenGLTexture::Target2D,
                                           QOpenGLTexture::PixelType PixelType = QOpenGLTexture::UInt8,
                                           QOpenGLTexture::PixelFormat PixelFormat = QOpenGLTexture::RGBA,
                                           QOpenGLTexture::Filter Filter = QOpenGLTexture::Linear,
                                           QOpenGLTexture::WrapMode Wrap = QOpenGLTexture::ClampToEdge,
                                           QOpenGLTexture::TextureFormat Format = QOpenGLTexture::NoFormat);
    static vector<MythVideoTexture*> CreateHardwareTextures(MythRenderOpenGL* Context,
                                                            VideoFrameType Type, VideoFrameType Format,
                                                            vector<QSize> Sizes, QOpenGLTexture::Target Target);
    static vector<MythVideoTexture*> CreateSoftwareTextures(MythRenderOpenGL* Context,
                                                            VideoFrameType Type, VideoFrameType Format,
                                                            vector<QSize> Sizes, QOpenGLTexture::Target Target);

   ~MythVideoTexture();

  public:
    bool           m_valid       { false };
    VideoFrameType m_frameType   { FMT_NONE };
    VideoFrameType m_frameFormat { FMT_NONE };
    uint           m_plane       { 0 };
    uint           m_planeCount  { 0 };
    QMatrix4x4    *m_transform   { nullptr };
    MythAVCopy    *m_copyContext { nullptr };

  protected:
    explicit MythVideoTexture(QOpenGLTexture* Texture);
    explicit MythVideoTexture(GLuint Texture);

  private:
    Q_DISABLE_COPY(MythVideoTexture)
    static void YV12ToYV12   (MythRenderOpenGL *Context, const VideoFrame *Frame, MythVideoTexture* Texture, int Plane);
    static void YV12ToYUYV   (const VideoFrame *Frame, MythVideoTexture* Texture);
    static void YV12ToYUYVHQ (const VideoFrame *Frame, MythVideoTexture* Texture);
    static void NV12ToNV12   (MythRenderOpenGL *Context, const VideoFrame *Frame, MythVideoTexture* Texture, int Plane);
    static bool CreateBuffer (MythVideoTexture* Texture, int Size);
    static void StoreBicubicWeights(float X, float *Dest);
};

#endif // MYTHVIDEOTEXTURE_H
