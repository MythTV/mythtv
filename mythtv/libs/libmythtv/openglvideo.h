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
        kGLFilterNone = 0,

        // Conversion filters
        kGLFilterYUV2RGB,
        kGLFilterYV12RGB,

        // Frame scaling/resizing filters
        kGLFilterResize
    };
    typedef map<OpenGLFilterType,OpenGLFilter*> glfilt_map_t;

  public:
    enum VideoType
    {
        kGLGPU,   // Frame is already in GPU memory
        kGLUYVY,  // CPU conversion to UYVY422 format - 16bpp
        kGLYV12,  // All processing on GPU - 12bpp
        kGLHQUYV, // High quality interlaced CPU conversion to UYV - 32bpp
        kGLRGBA   // fallback software YV12 to RGB - 32bpp
    };

    OpenGLVideo();
   ~OpenGLVideo();

    bool Init(MythRenderOpenGL *glcontext, VideoColourSpace *colourspace,
              QSize videoDim, QSize videoDispDim, QRect displayVisibleRect,
              QRect displayVideoRect, QRect videoRect,
              bool viewport_control,  VideoType type);

    MythGLTexture* GetInputTexture(void) const;
    uint GetTextureType(void) const;
    void UpdateInputFrame(const VideoFrame *frame);

    /// \brief Public interface to AddFilter(OpenGLFilterType filter)
    bool AddFilter(const QString &filter)
         { return AddFilter(StringToFilter(filter)); }
    bool RemoveFilter(const QString &filter)
         { return RemoveFilter(StringToFilter(filter)); }

    bool AddDeinterlacer(const QString &deinterlacer);
    void SetDeinterlacing(bool deinterlacing);
    QString GetDeinterlacer(void) const
         { return hardwareDeinterlacer; }
    void SetSoftwareDeinterlacer(const QString &filter);

    void PrepareFrame(bool topfieldfirst, FrameScanType scan,
                      long long frame, StereoscopicMode stereo,
                      bool draw_border = false);

    void  SetMasterViewport(QSize size)   { masterViewportSize = size; }
    QSize GetViewPort(void)         const { return viewportSize; }
    void  SetVideoRect(const QRect &dispvidrect, const QRect &vidrect);
    QSize GetVideoSize(void)        const { return video_dim;}
    static VideoType StringToType(const QString &Type);
    static QString TypeToString(VideoType Type);
    VideoType GetType() { return videoType; }

  public slots:
    void UpdateColourSpace(void);
    void UpdateShaderParameters(void);

  private:
    void Teardown(void);
    void SetViewPort(const QSize &new_viewport_size);
    bool AddFilter(OpenGLFilterType filter);
    bool RemoveFilter(OpenGLFilterType filter);
    void CheckResize(bool deinterlacing);
    bool OptimiseFilters(void);
    QOpenGLShaderProgram* AddFragmentProgram(OpenGLFilterType name,
                            QString deint = QString(),
                            FrameScanType field = kScan_Progressive);
    MythGLTexture* CreateVideoTexture(QSize size, QSize &tex_size);
    void GetProgramStrings(QString &vertex, QString &fragment,
                           OpenGLFilterType filter,
                           QString deint = QString(),
                           FrameScanType field = kScan_Progressive);
    static QString FilterToString(OpenGLFilterType filter);
    static OpenGLFilterType StringToFilter(const QString &filter);
    void SetFiltering(void);

    void RotateTextures(void);
    void SetTextureFilters(vector<MythGLTexture*>  *Textures,
                           QOpenGLTexture::Filter   Filter,
                           QOpenGLTexture::WrapMode Wrap);
    void DeleteTextures(vector<MythGLTexture*> *Textures);
    void TearDownDeinterlacer(void);

    VideoType      videoType;
    MythRenderOpenGL *gl_context;
    QSize          video_disp_dim;       ///< Useful video frame size e.g. 1920x1080
    QSize          video_dim;            ///< Total video frame size e.g. 1920x1088
    QSize          viewportSize;         ///< Can be removed
    QSize          masterViewportSize;   ///< Current viewport into which OpenGL is rendered
    QRect          display_visible_rect; ///< Total useful, visible rectangle
    QRect          display_video_rect;   ///< Sub-rect of display_visible_rect for video
    QRect          video_rect;           ///< Sub-rect of video_disp_dim to display (after zoom adjustments etc)
    QRect          frameBufferRect;      ///< Rect version of video_disp_dim
    QString        softwareDeinterlacer;
    QString        hardwareDeinterlacer;
    bool           hardwareDeinterlacing;///< OpenGL deinterlacing is enabled
    VideoColourSpace *colourSpace;
    bool           viewportControl;      ///< Video has control over view port
    vector<MythGLTexture*> referenceTextures; ///< Up to 3 reference textures for filters
    vector<MythGLTexture*> inputTextures;     ///< Textures with raw video data
    QSize          inputTextureSize;     ///< Actual size of input texture(s)
    glfilt_map_t   filters;              ///< Filter stages to be applied to frame
    int            refsNeeded;           ///< Number of reference textures expected
    uint           textureType;          ///< Texture type (e.g. GL_TEXTURE_2D or GL_TEXTURE_RECT)
    QOpenGLFunctions::OpenGLFeatures m_features; ///< Default features available from Qt
    uint           m_extraFeatures;      ///< OR'd list of extra, Myth specific features
    MythAVCopy     m_copyCtx;            ///< Conversion context for YV12 to UYVY
    bool           forceResize;          ///< Global setting to force a resize stage
    bool           discardFramebuffers;  ///< Use GL_EXT_discard_framebuffer
    bool           enablePBOs;           ///< Allow use of Pixel Buffer Objects
};
#endif // _OPENGL_VIDEO_H__
