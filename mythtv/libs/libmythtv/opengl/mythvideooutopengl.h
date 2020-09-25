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

    explicit MythVideoOutputOpenGL(QString& Profile);
    ~MythVideoOutputOpenGL() override;

    static void   GetRenderOptions (RenderOptions& Options);
    static QStringList GetAllowedRenderers(MythCodecID CodecId, const QSize& VideoDim);
    static VideoFrameTypeVec s_openglFrameTypes;
    static VideoFrameTypeVec s_openglFrameTypesLegacy;

    bool          Init             (const QSize& VideoDim, const QSize& VideoDispDim,
                                    float Aspect, const QRect& DisplayVisibleRect, MythCodecID CodecId) override;
    void          PrepareFrame     (VideoFrame* Frame, FrameScanType Scan) override;
    void          RenderFrame      (VideoFrame* Frame, FrameScanType Scan, OSD* Osd) override;
    void          EndFrame         () override;

  protected:
    QRect         GetDisplayVisibleRectAdj() override;

  private:
    MythRenderOpenGL* m_openglRender   { nullptr };
    MythOpenGLPerf*   m_openGLPerf     { nullptr };
};

#endif
