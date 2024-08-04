#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV
#include "libmythui/opengl/mythrenderopengl.h"
#include "mythvideooutgpu.h"

class MythRenderOpenGL;
class MythOpenGLPainter;
class MythOpenGLPerf;

class MythVideoOutputOpenGL : public MythVideoOutputGPU
{
    Q_OBJECT

  public:
    enum TextureFormats : std::uint8_t
    {
        AllFormats    = 0,
        LegacyFormats = 1
    };

    MythVideoOutputOpenGL(MythMainWindow* MainWindow, MythRenderOpenGL* Render,
                          MythOpenGLPainter* Painter, MythDisplay* Display,
                          const MythVideoProfilePtr& VideoProfile, QString& Profile);
    ~MythVideoOutputOpenGL() override;

    static void GetRenderOptions (RenderOptions& Options);
    static QStringList GetAllowedRenderers(MythRenderOpenGL* Render, MythCodecID CodecId, QSize VideoDim);

    bool          Init             (QSize VideoDim, QSize VideoDispDim,
                                    float Aspect, QRect DisplayVisibleRect, MythCodecID CodecId) override;
    void          PrepareFrame     (MythVideoFrame* Frame, FrameScanType Scan) override;
    void          RenderFrame      (MythVideoFrame* Frame, FrameScanType Scan) override;
    void          RenderEnd        () override;
    void          EndFrame         () override;

  protected:
    QRect         GetDisplayVisibleRectAdj() override;

  private:
    MythRenderOpenGL* m_openglRender   { nullptr };
    MythOpenGLPerf*   m_openGLPerf     { nullptr };
};

#endif
