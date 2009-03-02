#ifndef _OPENGL_VIDEO_H__
#define _OPENGL_VIDEO_H__

#include <vector>
#include <map>
using namespace std;

#include <qrect.h>
#include <qmap.h>

#include "videooutbase.h"
#include "videoouttypes.h"

enum OpenGLFilterType
{
    kGLFilterNone = 0,

    // Conversion filters
    kGLFilterYUV2RGB,
    kGLFilterYUV2RGBA,

    // Frame scaling/resizing filters
    kGLFilterResize,
    kGLFilterBicubic,
};

enum DisplayBuffer
{
    kDefaultBuffer,
    kFrameBufferObject
};

class OpenGLContext;

class OpenGLFilter;
typedef map<OpenGLFilterType,OpenGLFilter*> glfilt_map_t;

#ifdef USING_OPENGL_VIDEO

#include "util-opengl.h"

class OpenGLVideo
{
  public:
    OpenGLVideo();
   ~OpenGLVideo();

    bool Init(OpenGLContext *glcontext, bool colour_control,
              QSize videoDim, QRect displayVisibleRect,
              QRect displayVideoRect, QRect videoRect,
              bool viewport_control,  QString options, bool osd = FALSE,
              LetterBoxColour letterbox_colour = kLetterBoxColour_Black);

    void UpdateInputFrame(const VideoFrame *frame, bool soft_bob = false);
    void UpdateInput(const unsigned char *buf, const int *offsets,
                     int format, QSize size,
                     const unsigned char *alpha);

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
    void SetVideoResize(const QRect &rect);
    void DisableVideoResize(void);

  private:
    void Teardown(void);
    void SetViewPort(const QSize &new_viewport_size);
    bool AddFilter(OpenGLFilterType filter);
    bool RemoveFilter(OpenGLFilterType filter);
    void CheckResize(bool deinterlacing);
    bool OptimiseFilters(void);
    bool AddFrameBuffer(uint &framebuffer, QSize fb_size,
                        uint &texture, QSize vid_size);
    uint AddFragmentProgram(OpenGLFilterType name,
                            QString deint = QString::null,
                            FrameScanType field = kScan_Progressive);
    uint CreateVideoTexture(QSize size, QSize &tex_size,
                            bool use_pbo = false);
    QString GetProgramString(OpenGLFilterType filter,
                             QString deint = QString::null,
                             FrameScanType field = kScan_Progressive);
    void CalculateResize(float &left,  float &top,
                         float &right, float &bottom);
    static QString FilterToString(OpenGLFilterType filter);
    static OpenGLFilterType StringToFilter(const QString &filter);
    void ShutDownYUV2RGB(void);
    QSize GetTextureSize(const QSize &size);
    void SetFiltering(void);

    void RotateTextures(void);
    void SetTextureFilters(vector<GLuint> *textures, int filt, int clamp);
    void DeleteTextures(vector<GLuint> *textures);
    void TearDownDeinterlacer(void);
    uint ParseOptions(QString options);

    OpenGLContext *gl_context;
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
    bool           useColourControl;
    bool           viewportControl;
    vector<GLuint>   referenceTextures;
    vector<GLuint>   inputTextures;
    QSize          inputTextureSize;
    glfilt_map_t   filters;
    long long      currentFrameNum;
    bool           inputUpdated;
    bool           textureRects;
    uint           textureType;
    uint           helperTexture;
    OpenGLFilterType defaultUpsize;

    QSize          convertSize;
    unsigned char *convertBuf;

    bool           videoResize;
    QRect          videoResizeRect;
 
    uint           gl_features;
    LetterBoxColour gl_letterbox_colour;
};

#else // if !USING_OPENGL_VIDEO

class OpenGLVideo
{
  public:
    OpenGLVideo() { }
    ~OpenGLVideo() { }

    bool Init(OpenGLContext*, bool, QSize, QRect,
              QRect, QRect, bool, QString, bool osd = false,
              LetterBoxColour letterbox = kLetterBoxColour_Black)
                { (void) osd; return false; }

    void UpdateInputFrame(const VideoFrame*, bool = false) { }
    void UpdateInput(const unsigned char*, const int*,
                     int, QSize, unsigned char* = NULL) { }

    bool AddFilter(const QString&) { return false; }
    bool RemoveFilter(const QString&) { return false; }

    bool AddDeinterlacer(const QString&) { return false; }
    void SetDeinterlacing(bool) { }
    QString GetDeinterlacer(void) const { return QString::null; }
    void SetSoftwareDeinterlacer(const QString&) { }

    void PrepareFrame(FrameScanType, bool, long long, bool) { }

    void  SetMasterViewport(QSize) { }
    QSize GetViewPort(void) const { return QSize(0,0); }
    void  SetVideoRect(const QRect&, const QRect&) { }
    QSize GetVideoSize(void) const { return QSize(0,0); }
    void SetVideoResize(const QRect&) { }
    void DisableVideoResize(void) { }
};

#endif // !USING_OPENGL_VIDEO

#endif // _OPENGL_VIDEO_H__
