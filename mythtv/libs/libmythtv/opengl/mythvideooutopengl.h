#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV
#include "opengl/mythrenderopengl.h"
#include "mythvideooutgpu.h"

class MythRenderOpenGL;
class MythOpenGLPainter;
class MythOpenGLPerf;

class MythVideoOutputOpenGL : public MythVideoOutputGPU
{
  public:
    enum TextureFormats
    {
        AllFormats    = 0,
        LegacyFormats = 1
    };

    explicit MythVideoOutputOpenGL(QString Profile = QString());
    ~MythVideoOutputOpenGL() override;

    static void   GetRenderOptions (RenderOptions& Options);
    static QStringList GetAllowedRenderers(MythCodecID CodecId, const QSize& VideoDim);

    bool          Init             (const QSize& VideoDim, const QSize& VideoDispDim,
                                    float Aspect, MythDisplay* Display,
                                    const QRect& DisplayVisibleRect, MythCodecID CodecId) override;
    void          PrepareFrame     (VideoFrame* Frame, FrameScanType Scan, OSD* Osd) override;
    void          ProcessFrame     (VideoFrame* Frame, OSD* Osd,
                                    const PIPMap& PiPPlayers, FrameScanType Scan) override;
    void          Show             (FrameScanType Scan) override;
    VideoFrameVec DirectRenderFormats() override;

  protected:
    QRect         GetDisplayVisibleRect() override;

  private:
    MythVideoGPU* CreateSecondaryVideo(const QSize VideoDim,
                                       const QSize VideoDispDim,
                                       const QRect DisplayVisibleRect,
                                       const QRect DisplayVideoRect,
                                       const QRect VideoRect) override;

    MythRenderOpenGL* m_openglRender   { nullptr };
    TextureFormats    m_textureFormats { AllFormats };
    MythOpenGLPerf*   m_openGLPerf     { nullptr };
};

#endif
