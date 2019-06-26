#ifndef _OPENGL_VIDEO_H__
#define _OPENGL_VIDEO_H__

// Qt
#include <QRect>
#include <QObject>

// MythTV
#include "videooutbase.h"
#include "videoouttypes.h"
#include "mythrender_opengl.h"
#include "mythavutil.h"
#include "mythopenglinterop.h"

// Std
#include <vector>
#include <map>
using std::vector;
using std::map;

class OpenGLVideo : public QObject
{
    Q_OBJECT

  public:
    enum VideoShaderType
    {
        Default       = 0, // Plain blit
        Progressive   = 1, // Progressive video frame
        InterlacedTop = 2, // Deinterlace with top field first
        InterlacedBot = 3, // Deinterlace with bottom field first
        Interlaced    = 4, // Interlaced but do not deinterlace
        ShaderCount   = 5
    };

    static QString        TypeToProfile(VideoFrameType Type);

    OpenGLVideo(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                QRect DisplayVideoRect, QRect videoRect,
                bool ViewportControl, QString Profile);
   ~OpenGLVideo();

    bool    IsValid(void) const;
    void    ProcessFrame(const VideoFrame *Frame, FrameScanType Scan = kScan_Progressive);
    void    PrepareFrame(VideoFrame *Frame, bool TopFieldFirst, FrameScanType Scan,
                         StereoscopicMode Stereo, bool DrawBorder = false);
    void    SetMasterViewport(QSize Size);
    QSize   GetVideoSize(void) const;
    QString GetProfile() const;
    void    SetProfile(const QString &Profile);
    void    ResetFrameFormat(void);
    void    ResetTextures(void);

  public slots:
    void    SetVideoDimensions(const QSize &VideoDim, const QSize &VideoDispDim);
    void    SetVideoRects(const QRect &DisplayVideoRect, const QRect &VideoRect);
    void    SetViewportRect(const QRect &DisplayVisibleRect);
    void    UpdateColourSpace(void);
    void    UpdateShaderParameters(void);

  private:
    bool    SetupFrameFormat(VideoFrameType InputType, VideoFrameType OutputType,
                             QSize Size, GLenum TextureTarget);
    bool    CreateVideoShader(VideoShaderType Type, MythDeintType Deint = DEINT_NONE);
    void    LoadTextures(bool Deinterlacing, vector<MythVideoTexture*> &Current,
                         MythGLTexture** Textures, uint &TextureCount);
    bool    AddDeinterlacer(const VideoFrame *Frame,  FrameScanType Scan,
                            MythDeintType Filter = DEINT_SHADER, bool CreateReferences = true);

    bool           m_valid;
    QString        m_profile;
    VideoFrameType m_inputType;           ///< Usually YV12 for software, VDPAU etc for hardware
    VideoFrameType m_outputType;          ///< Set by profile for software or decoder for hardware
    MythRenderOpenGL *m_render;
    QSize          m_videoDispDim;        ///< Useful video frame size e.g. 1920x1080
    QSize          m_videoDim;            ///< Total video frame size e.g. 1920x1088
    QSize          m_masterViewportSize;  ///< Current viewport into which OpenGL is rendered
    QRect          m_displayVisibleRect;  ///< Total useful, visible rectangle
    QRect          m_displayVideoRect;    ///< Sub-rect of display_visible_rect for video
    QRect          m_videoRect;           ///< Sub-rect of video_disp_dim to display (after zoom adjustments etc)
    MythDeintType  m_deinterlacer;
    bool           m_deinterlacer2x;
    VideoColourSpace *m_videoColourSpace;
    bool           m_viewportControl;     ///< Video has control over view port
    QOpenGLShaderProgram* m_shaders[ShaderCount] { nullptr };
    int            m_shaderCost[ShaderCount]     { 1 };
    vector<MythVideoTexture*> m_inputTextures; ///< Current textures with raw video data
    vector<MythVideoTexture*> m_prevTextures;  ///< Previous textures with raw video data
    vector<MythVideoTexture*> m_nextTextures;  ///< Next textures with raw video data
    QOpenGLFramebufferObject* m_frameBuffer;
    MythVideoTexture*         m_frameBufferTexture;
    QSize          m_inputTextureSize;    ///< Actual size of input texture(s)
    QOpenGLFunctions::OpenGLFeatures m_features; ///< Default features available from Qt
    int            m_extraFeatures;       ///< OR'd list of extra, Myth specific features
    bool           m_resizing;
    GLenum         m_textureTarget;       ///< Some interops require custom texture targets
    long long      m_discontinuityCounter; ///< Check when to release reference frames after a skip
};
#endif // _OPENGL_VIDEO_H__
