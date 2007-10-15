#ifndef _OPENGL_VIDEO_H__
#define _OPENGL_VIDEO_H__

#include <vector>
#include <map>
using namespace std;

#include <qrect.h>
#include <qmap.h>

#include "videooutbase.h"

enum OpenGLFilterType
{
    kGLFilterNone = 0,

    // Conversion filters
    kGLFilterYUV2RGB,
    kGLFilterYUV2RGBA,

    // Frame rate preserving deinterlacers
    kGLFilterLinearBlendDeint,
    kGLFilterKernelDeint,
    kGLFilterOneFieldDeint,

    // Frame rate doubling deinterlacers
    kGLFilterBobDeintDFR,
    kGLFilterLinearBlendDeintDFR,
    kGLFilterKernelDeintDFR,
    kGLFilterFieldOrderDFR,
    kGLFilterOneFieldDeintDFR,

    // Frame scaling/resizing filters
    kGLFilterResize,
};

enum DisplayBuffer
{
    kNoBuffer = 0,    // disable filter
    kDefaultBuffer,
    kFrameBufferObject
};

class OpenGLContext;

class OpenGLFilter;
typedef map<OpenGLFilterType,OpenGLFilter*> glfilt_map_t;

#ifdef USING_OPENGL_VIDEO

class OpenGLVideo
{
  public:
    OpenGLVideo();
   ~OpenGLVideo();

    bool Init(OpenGLContext *glcontext, bool colour_control, bool onscreen,
              QSize video_size, QRect visible_rect,
              QRect video_rect, QRect frame_rect,
              bool viewport_control, bool osd = FALSE);
    bool ReInit(OpenGLContext *gl, bool colour_control, bool onscreen,
              QSize video_size, QRect visible_rect,
              QRect video_rect, QRect frame_rect,
              bool viewport_control, bool osd = FALSE);

    void UpdateInputFrame(const VideoFrame *frame);
    void UpdateInput(const unsigned char *buf, const int *offsets,
                     uint texture_index, int format, QSize size);

    bool AddFilter(const QString &filter)
         { return AddFilter(StringToFilter(filter)); }
    bool RemoveFilter(const QString &filter)
         { return RemoveFilter(StringToFilter(filter)); }

    bool AddDeinterlacer(const QString &filter);
    void SetDeinterlacing(bool deinterlacing);
    QString GetDeinterlacer(void) const
         { return FilterToString(GetDeintFilter()); };
    void SetSoftwareDeinterlacer(const QString &filter)
         { softwareDeinterlacer = QDeepCopy<QString>(filter); };

    void PrepareFrame(FrameScanType scan, bool softwareDeinterlacing,
                      long long frame);

    void  SetMasterViewport(QSize size)   { masterViewportSize = size; }
    QSize GetViewPort(void)         const { return viewportSize; }
    void  SetVideoRect(const QRect &vidrect, const QRect &framerect)
        { videoRect = vidrect; frameRect = framerect;}
    QSize GetVideoSize(void)        const { return videoSize; }
    void SetVideoResize(const QRect &rect);
    void DisableVideoResize(void);
    int SetPictureAttribute(PictureAttribute attributeType, int newValue);
    PictureAttributeSupported GetSupportedPictureAttributes(void) const;

  private:
    void Teardown(void);
    void SetViewPort(const QSize &new_viewport_size);
    void SetViewPortPrivate(const QSize &new_viewport_size);
    bool AddFilter(OpenGLFilterType filter);
    bool RemoveFilter(OpenGLFilterType filter);
    bool OptimiseFilters(void);
    OpenGLFilterType GetDeintFilter(void) const;
    bool AddFrameBuffer(uint &framebuffer, uint &texture, QSize size);
    uint AddFragmentProgram(OpenGLFilterType name);
    uint CreateVideoTexture(QSize size, QSize &tex_size);
    QString GetProgramString(OpenGLFilterType filter);
    void CalculateResize(float &left,  float &top,
                         float &right, float &bottom);
    static QString FilterToString(OpenGLFilterType filter);
    static OpenGLFilterType StringToFilter(const QString &filter);
    void ShutDownYUV2RGB(void);
    void SetViewPort(bool last_stage);
    void InitOpenGL(void);
    QSize GetTextureSize(const QSize &size);
    void SetFiltering(void);

    void Rotate(vector<uint> *target);
    void SetTextureFilters(vector<uint> *textures, int filt);

    OpenGLContext *gl_context;
    QSize          videoSize;
    QSize          viewportSize;
    QSize          masterViewportSize;
    QRect          visibleRect;
    QRect          videoRect;
    QRect          frameRect;
    QRect          frameBufferRect;
    bool           invertVideo;
    QString        softwareDeinterlacer;
    bool           hardwareDeinterlacing;
    bool           useColourControl;
    bool           viewportControl;
    uint           frameBuffer;
    uint           frameBufferTexture;
    vector<uint>   inputTextures;
    QSize          inputTextureSize;
    glfilt_map_t   filters;
    long long      currentFrameNum;
    bool           inputUpdated;

    QSize            convertSize;
    unsigned char   *convertBuf;

    bool             videoResize;
    QRect            videoResizeRect;

    float pictureAttribs[kPictureAttribute_MAX];
};

#else // if !USING_OPENGL_VIDEO

class OpenGLVideo
{
  public:
    OpenGLVideo() { }
    ~OpenGLVideo() { }

    bool Init(OpenGLContext*, bool, bool, QSize, QRect,
              QRect, QRect, bool, bool osd = false)
        { (void) osd; return false; }

    bool ReInit(OpenGLContext*, bool, bool, QSize, QRect,
                QRect, QRect, bool, bool osd = false)
        { (void) osd; return false; }

    void UpdateInputFrame(const VideoFrame*) { }
    void UpdateInput(const unsigned char*, const int*, uint, int, QSize) { }

    bool AddFilter(const QString&) { return false; }
    bool RemoveFilter(const QString&) { return false; }

    bool AddDeinterlacer(const QString&) { return false; }
    void SetDeinterlacing(bool) { }
    QString GetDeinterlacer(void) const { return QString::null; }
    void SetSoftwareDeinterlacer(const QString&) { }

    void PrepareFrame(FrameScanType, bool, long long) { }

    void  SetMasterViewport(QSize) { }
    QSize GetViewPort(void) const { return QSize(0,0); }
    void  SetVideoRect(const QRect&, const QRect&) { }
    QSize GetVideoSize(void) const { return QSize(0,0); }
    void SetVideoResize(const QRect&) { }
    void DisableVideoResize(void) { }
    int SetPictureAttribute(PictureAttribute, int) { return -1; }
    PictureAttributeSupported GetSupportedPictureAttributes(void) const
        { return kPictureAttributeSupported_None; }
};

#endif // !USING_OPENGL_VIDEO

#endif // _OPENGL_VIDEO_H__
