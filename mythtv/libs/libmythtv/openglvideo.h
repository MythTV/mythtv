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

// Std
#include <vector>
#include <map>
using std::vector;
using std::map;

class OpenGLFilter;

class OpenGLVideo : public QObject
{
    Q_OBJECT

    enum OpenGLFilterType
    {
        // Conversion filters
        kGLFilterYUV2RGB,
        kGLFilterYV12RGB,
        // Frame scaling/resizing filters
        kGLFilterResize
    };
    typedef map<OpenGLFilterType,OpenGLFilter*> glfilt_map_t;

  public:
    enum FrameType
    {
        kGLGPU,   // Frame is already in GPU memory
        kGLUYVY,  // CPU conversion to UYVY422 format - 16bpp
        kGLYV12,  // All processing on GPU - 12bpp
        kGLHQUYV, // High quality interlaced CPU conversion to UYV - 32bpp
        kGLRGBA   // fallback software YV12 to RGB - 32bpp
    };

    static FrameType StringToType(const QString &Type);
    static QString   TypeToString(FrameType Type);

    OpenGLVideo();
   ~OpenGLVideo();

    bool    Init(MythRenderOpenGL *Render, VideoColourSpace *ColourSpace,
                 QSize VideoDim, QSize VideoDispDim, QRect DisplayVisibleRect,
                 QRect DisplayVideoRect, QRect videoRect,
                 bool ViewportControl,  FrameType Type);

    MythGLTexture* GetInputTexture(void) const;
    void    UpdateInputFrame(const VideoFrame *Frame);
    bool    AddDeinterlacer(const QString &Deinterlacer);
    void    SetDeinterlacing(bool Deinterlacing);
    QString GetDeinterlacer(void) const;
    void    SetSoftwareDeinterlacer(const QString &Filter);
    void    PrepareFrame(bool TopFieldFirst, FrameScanType Scan, StereoscopicMode Stereo, bool DrawBorder = false);
    void    SetMasterViewport(QSize Size);
    void    SetVideoRect(const QRect &DisplayVideoRect, const QRect &VideoRect);
    QSize   GetVideoSize(void) const;
    FrameType GetType() const;

  public slots:
    void    UpdateColourSpace(void);
    void    UpdateShaderParameters(void);

  private:
    void    Teardown(void);
    bool    AddFilter(OpenGLFilterType Filter);
    bool    RemoveFilter(OpenGLFilterType Filter);
    void    CheckResize(bool Deinterlacing);
    bool    OptimiseFilters(void);
    QOpenGLShaderProgram* AddFragmentProgram(OpenGLFilterType Name,
                            QString Deinterlacer = QString(),
                            FrameScanType Field = kScan_Progressive);
    MythGLTexture* CreateVideoTexture(QSize Size, QSize &ActualTextureSize);
    void    SetFiltering(void);
    void    RotateTextures(void);
    void    SetTextureFilters(vector<MythGLTexture*>  *Textures,
                              QOpenGLTexture::Filter   Filter,
                              QOpenGLTexture::WrapMode Wrap);
    void    DeleteTextures(vector<MythGLTexture*> *Textures);
    void    TearDownDeinterlacer(void);

    FrameType      m_frameType;
    MythRenderOpenGL *m_render;
    QSize          m_videoDispDim;        ///< Useful video frame size e.g. 1920x1080
    QSize          m_videoDim;            ///< Total video frame size e.g. 1920x1088
    QSize          m_masterViewportSize;  ///< Current viewport into which OpenGL is rendered
    QRect          m_displayVisibleRect;  ///< Total useful, visible rectangle
    QRect          m_displayVideoRect;    ///< Sub-rect of display_visible_rect for video
    QRect          m_videoRect;           ///< Sub-rect of video_disp_dim to display (after zoom adjustments etc)
    QString        m_softwareDeiterlacer;
    QString        m_hardwareDeinterlacer;
    bool           m_hardwareDeinterlacing; ///< OpenGL deinterlacing is enabled
    VideoColourSpace *m_videoColourSpace;
    bool           m_viewportControl;     ///< Video has control over view port
    vector<MythGLTexture*> m_referenceTextures; ///< Up to 3 reference textures for filters
    vector<MythGLTexture*> m_inputTextures;     ///< Textures with raw video data
    QSize          m_inputTextureSize;    ///< Actual size of input texture(s)
    glfilt_map_t   m_filters;             ///< Filter stages to be applied to frame
    int            m_referenceTexturesNeeded; ///< Number of reference textures still required
    QOpenGLFunctions::OpenGLFeatures m_features; ///< Default features available from Qt
    uint           m_extraFeatures;       ///< OR'd list of extra, Myth specific features
    MythAVCopy     m_copyCtx;             ///< Conversion context for YV12 to UYVY
    bool           m_forceResize;         ///< Global setting to force a resize stage
    bool           m_discardFrameBuffers; ///< Use GL_EXT_discard_framebuffer
};
#endif // _OPENGL_VIDEO_H__
