#ifndef MYTH_OPENGL_VIDEO_H_
#define MYTH_OPENGL_VIDEO_H_

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "mythvideoout.h"
#include "mythvideogpu.h"
#include "videoouttypes.h"
#include "opengl/mythrenderopengl.h"
#include "mythavutil.h"
#include "mythopenglinterop.h"

// Std
#include <vector>
#include <map>
using std::vector;
using std::map;

class MythOpenGLTonemap;

class MythOpenGLVideo : public MythVideoGPU
{
    Q_OBJECT

  public:
    enum VideoShaderType
    {
        Default       = 0, // Plain blit
        Progressive   = 1, // Progressive video frame
        InterlacedTop = 2, // Deinterlace with top field first
        InterlacedBot = 3, // Deinterlace with bottom field first
        ShaderCount   = 4
    };

    static QString        TypeToProfile(VideoFrameType Type);

    MythOpenGLVideo(MythRenderOpenGL* Render, MythVideoColourSpace* ColourSpace,
                    MythVideoBounds* Bounds, const QString &Profile);
    ~MythOpenGLVideo() override;

    void    StartFrame       () override {}
    void    PrepareFrame     (VideoFrame* Frame, FrameScanType Scan = kScan_Progressive) override;
    void    RenderFrame      (VideoFrame* Frame, bool TopFieldFirst, FrameScanType Scan,
                              StereoscopicMode StereoOverride, bool DrawBorder = false) override;
    void    EndFrame         () override {}
    QString GetProfile       () const override;
    void    ResetFrameFormat () override;
    void    ResetTextures    () override;

  public slots:
    void    UpdateShaderParameters();

  protected:
    void    ColourSpaceUpdate(bool PrimariesChanged) override;

  private:
    bool    SetupFrameFormat (VideoFrameType InputType, VideoFrameType OutputType,
                              QSize Size, GLenum TextureTarget);
    bool    CreateVideoShader(VideoShaderType Type, MythDeintType Deint = DEINT_NONE);
    void    BindTextures     (bool Deinterlacing, vector<MythVideoTexture*>& Current,
                              vector<MythGLTexture*>& Textures);
    bool    AddDeinterlacer  (const VideoFrame* Frame,  FrameScanType Scan,
                              MythDeintType Filter = DEINT_SHADER, bool CreateReferences = true);
    QOpenGLFramebufferObject* CreateVideoFrameBuffer(VideoFrameType OutputType, QSize Size);
    void    CleanupDeinterlacers();

    MythRenderOpenGL* m_openglRender               { nullptr };
    int            m_gles                          { 0 };
    MythDeintType  m_fallbackDeinterlacer          { MythDeintType::DEINT_NONE };
    std::array<QOpenGLShaderProgram*,ShaderCount> m_shaders { nullptr };
    std::array<int,ShaderCount> m_shaderCost       { 1 };
    vector<MythVideoTexture*> m_inputTextures;
    vector<MythVideoTexture*> m_prevTextures;
    vector<MythVideoTexture*> m_nextTextures;
    QOpenGLFramebufferObject* m_frameBuffer        { nullptr };
    MythVideoTexture*         m_frameBufferTexture { nullptr };
    QOpenGLFunctions::OpenGLFeatures m_features;
    int            m_extraFeatures                 { 0 };
    GLenum         m_textureTarget                 { QOpenGLTexture::Target2D };
    bool           m_chromaUpsamplingFilter        { false };
    MythOpenGLTonemap* m_toneMap                   { nullptr };
};
#endif
