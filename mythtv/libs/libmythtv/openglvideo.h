#ifndef _OPENGL_VIDEO_H__
#define _OPENGL_VIDEO_H__

#include <vector>
#include <map>
using namespace std;

#include <QRect>

#include "videooutbase.h"
#include "videoouttypes.h"
#include "mythrender_opengl.h"

enum OpenGLFilterType
{
    kGLFilterNone = 0,

    // Conversion filters
    kGLFilterYUV2RGB,

    // Frame scaling/resizing filters
    kGLFilterResize,
    kGLFilterBicubic,
};

enum DisplayBuffer
{
    kDefaultBuffer,
    kFrameBufferObject
};

class OpenGLFilter;
typedef map<OpenGLFilterType,OpenGLFilter*> glfilt_map_t;

#include "util-opengl.h"

class OpenGLVideo
{
  public:
    OpenGLVideo();
   ~OpenGLVideo();

    bool Init(MythRenderOpenGL *glcontext, VideoColourSpace *colourspace,
              QSize videoDim, QRect displayVisibleRect,
              QRect displayVideoRect, QRect videoRect,
              bool viewport_control,  QString options,
              bool hwaccel,
              LetterBoxColour letterbox_colour = kLetterBoxColour_Black);

    uint GetInputTexture(void);
    uint GetTextureType(void);
    void SetInputUpdated(void);
    void UpdateInputFrame(const VideoFrame *frame, bool soft_bob = false);

    /// \brief Public interface to AddFilter(OpenGLFilterType filter)
    bool AddFilter(const QString &filter)
         { return AddFilter(StringToFilter(filter)); }
    bool RemoveFilter(const QString &filter)
         { return RemoveFilter(StringToFilter(filter)); }

    bool AddDeinterlacer(const QString &deinterlacer);
    void SetDeinterlacing(bool deinterlacing);
    QString GetDeinterlacer(void) const
         { return hardwareDeinterlacer; };
    void SetSoftwareDeinterlacer(const QString &filter);

    void PrepareFrame(bool topfieldfirst, FrameScanType scan,
                      bool softwareDeinterlacing,
                      long long frame, bool draw_border = false);

    void  SetMasterViewport(QSize size)   { masterViewportSize = size; }
    QSize GetViewPort(void)         const { return viewportSize; }
    void  SetVideoRect(const QRect &dispvidrect, const QRect &vidrect)
                      { display_video_rect = dispvidrect; video_rect = vidrect;}
    QSize GetVideoSize(void)        const { return actual_video_dim;}

  private:
    void Teardown(void);
    void SetViewPort(const QSize &new_viewport_size);
    bool AddFilter(OpenGLFilterType filter);
    bool RemoveFilter(OpenGLFilterType filter);
    void CheckResize(bool deinterlacing, bool allow = true);
    bool OptimiseFilters(void);
    bool AddFrameBuffer(uint &framebuffer, uint &texture, QSize vid_size);
    uint AddFragmentProgram(OpenGLFilterType name,
                            QString deint = QString::null,
                            FrameScanType field = kScan_Progressive);
    uint CreateVideoTexture(QSize size, QSize &tex_size,
                            bool use_pbo = false);
    QString GetProgramString(OpenGLFilterType filter,
                             QString deint = QString::null,
                             FrameScanType field = kScan_Progressive);
    void GetProgramStrings(QString &vertex, QString &fragment,
                           OpenGLFilterType filter,
                           QString deint = QString::null,
                           FrameScanType field = kScan_Progressive);
    static QString FilterToString(OpenGLFilterType filter);
    static OpenGLFilterType StringToFilter(const QString &filter);
    QSize GetTextureSize(const QSize &size);
    void SetFiltering(void);

    void RotateTextures(void);
    void SetTextureFilters(vector<GLuint> *textures, int filt, int clamp);
    void DeleteTextures(vector<GLuint> *textures);
    void TearDownDeinterlacer(void);
    uint ParseOptions(QString options);

    MythRenderOpenGL *gl_context;
    QSize          video_dim;
    QSize          actual_video_dim;
    QSize          viewportSize;
    QSize          masterViewportSize;
    QRect          display_visible_rect;
    QRect          display_video_rect;
    QRect          video_rect;
    QRect          frameBufferRect;
    QString        softwareDeinterlacer;
    QString        hardwareDeinterlacer;
    bool           hardwareDeinterlacing;
    VideoColourSpace *colourSpace;
    bool           viewportControl;
    vector<GLuint>   referenceTextures;
    vector<GLuint>   inputTextures;
    QSize          inputTextureSize;
    glfilt_map_t   filters;
    long long      currentFrameNum;
    bool           inputUpdated;
    int            refsNeeded;
    bool           textureRects;
    uint           textureType;
    uint           helperTexture;
    OpenGLFilterType defaultUpsize;
    uint           gl_features;
    bool           using_ycbcrtex;
    bool           using_hardwaretex;
    LetterBoxColour gl_letterbox_colour;
};
#endif // _OPENGL_VIDEO_H__
