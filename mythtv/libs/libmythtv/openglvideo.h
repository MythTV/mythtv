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
#include "util-opengl.h"
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

    enum FrameType
    {
        kGLInterop, // Frame is already in GPU memory
        kGLUYVY,    // CPU conversion to UYVY422 format - 16bpp
        kGLYV12,    // All processing on GPU - 12bpp
        kGLHQUYV,   // High quality interlaced CPU conversion to UYV - 32bpp
        kGLNV12     // Currently used for VAAPI interop only
    };

    static FrameType StringToType(const QString &Type);
    static QString   TypeToString(FrameType Type);

    OpenGLVideo(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                QRect DisplayVideoRect, QRect videoRect,
                bool ViewportControl,  FrameType Type);
   ~OpenGLVideo();

    bool    IsValid(void) const;
    MythGLTexture* GetInputTexture(void) const;
    void    UpdateInputFrame(const VideoFrame *Frame);
    bool    AddDeinterlacer(QString &Deinterlacer);
    void    SetDeinterlacing(bool Deinterlacing);
    QString GetDeinterlacer(void) const;
    void    PrepareFrame(VideoFrame *Frame, bool TopFieldFirst, FrameScanType Scan,
                         StereoscopicMode Stereo, bool DrawBorder = false);
    void    SetMasterViewport(QSize Size);
    QSize   GetVideoSize(void) const;
    FrameType GetType() const;

  public slots:
    void    SetVideoRects(const QRect &DisplayVideoRect, const QRect &VideoRect);
    void    UpdateColourSpace(void);
    void    UpdateShaderParameters(void);

  private:
    bool    CreateVideoShader(VideoShaderType Type, QString Deinterlacer = QString());
    MythGLTexture* CreateVideoTexture(QSize Size, QSize &ActualTextureSize);
    void    RotateTextures(void);
    void    SetTextureFilters(vector<MythGLTexture*>  *Textures,
                              QOpenGLTexture::Filter   Filter,
                              QOpenGLTexture::WrapMode Wrap);
    void    DeleteTextures(vector<MythGLTexture*> *Textures);
    void    TearDownDeinterlacer(void);

    bool           m_valid;
    FrameType      m_frameType;
    FrameType      m_interopFrameType;    ///< Interop can return RGB, YV12 or NV12
    MythRenderOpenGL *m_render;
    QSize          m_videoDispDim;        ///< Useful video frame size e.g. 1920x1080
    QSize          m_videoDim;            ///< Total video frame size e.g. 1920x1088
    QSize          m_masterViewportSize;  ///< Current viewport into which OpenGL is rendered
    QRect          m_displayVisibleRect;  ///< Total useful, visible rectangle
    QRect          m_displayVideoRect;    ///< Sub-rect of display_visible_rect for video
    QRect          m_videoRect;           ///< Sub-rect of video_disp_dim to display (after zoom adjustments etc)
    QString        m_hardwareDeinterlacer;
    bool           m_hardwareDeinterlacing; ///< OpenGL deinterlacing is enabled
    VideoColourSpace *m_videoColourSpace;
    bool           m_viewportControl;     ///< Video has control over view port
    QOpenGLShaderProgram* m_shaders[ShaderCount];
    int            m_shaderCost[ShaderCount];
    vector<MythGLTexture*> m_referenceTextures; ///< Up to 3 reference textures for filters
    vector<MythGLTexture*> m_inputTextures;     ///< Textures with raw video data
    QOpenGLFramebufferObject* m_frameBuffer;
    MythGLTexture*            m_frameBufferTexture;
    QSize          m_inputTextureSize;    ///< Actual size of input texture(s)
    uint           m_referenceTexturesNeeded; ///< Number of reference textures still required
    QOpenGLFunctions::OpenGLFeatures m_features; ///< Default features available from Qt
    int            m_extraFeatures;       ///< OR'd list of extra, Myth specific features
    MythAVCopy     m_copyCtx;             ///< Conversion context for YV12 to UYVY
    bool           m_resizing;
    bool           m_forceResize;         ///< Global setting to force a resize stage
    GLenum         m_textureTarget;       ///< Some interops require custom texture targets
};
#endif // _OPENGL_VIDEO_H__
