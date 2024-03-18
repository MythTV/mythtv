#ifndef MYTH_OPENGL_VIDEO_H_
#define MYTH_OPENGL_VIDEO_H_

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "libmythui/opengl/mythrenderopengl.h"
#include "mythvideoout.h"
#include "mythvideogpu.h"
#include "videoouttypes.h"
#include "mythavutil.h"
#include "opengl/mythopenglinterop.h"

// Std
#include <vector>
#include <map>

class MythOpenGLTonemap;

class MythOpenGLVideo : public MythVideoGPU
{
    Q_OBJECT

  public:
    enum VideoShaderType : std::uint8_t
    {
        Default       = 0, // Plain blit
        Progressive   = 1, // Progressive video frame
        InterlacedTop = 2, // Deinterlace with top field first
        InterlacedBot = 3, // Deinterlace with bottom field first
        BicubicUpsize = 4,
        ShaderCount   = 5
    };

    static QString        TypeToProfile(VideoFrameType Type);

    MythOpenGLVideo(MythRenderOpenGL* Render, MythVideoColourSpace* ColourSpace,
                    MythVideoBounds* Bounds, const MythVideoProfilePtr& VideoProfile, const QString &Profile);
    ~MythOpenGLVideo() override;

    void    StartFrame       () override {}
    void    PrepareFrame     (MythVideoFrame* Frame, FrameScanType Scan = kScan_Progressive) override;
    void    RenderFrame      (MythVideoFrame* Frame, bool TopFieldFirst, FrameScanType Scan,
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
    void    BindTextures     (bool Deinterlacing, std::vector<MythVideoTextureOpenGL*>& Current,
                              std::vector<MythGLTexture*>& Textures);
    bool    AddDeinterlacer  (const MythVideoFrame* Frame,  FrameScanType Scan,
                              MythDeintType Filter = DEINT_SHADER, bool CreateReferences = true);
    void    CleanupDeinterlacers();
    void    SetupBicubic(VideoResizing& Resize);

    MythRenderOpenGL* m_openglRender               { nullptr };
    int            m_gles                          { 0 };
    MythDeintType  m_fallbackDeinterlacer          { MythDeintType::DEINT_NONE };
    std::array<QOpenGLShaderProgram*,ShaderCount> m_shaders { nullptr };
    std::array<int,ShaderCount> m_shaderCost       { 1 };
    std::vector<MythVideoTextureOpenGL*> m_inputTextures;
    std::vector<MythVideoTextureOpenGL*> m_prevTextures;
    std::vector<MythVideoTextureOpenGL*> m_nextTextures;
    QOpenGLFramebufferObject* m_frameBuffer        { nullptr };
    MythVideoTextureOpenGL*   m_frameBufferTexture { nullptr };
    QOpenGLFunctions::OpenGLFeatures m_features;
    int            m_extraFeatures                 { 0 };
    GLenum         m_textureTarget                 { QOpenGLTexture::Target2D };
    bool           m_chromaUpsamplingFilter        { false };
    MythOpenGLTonemap* m_toneMap                   { nullptr };
};
#endif
