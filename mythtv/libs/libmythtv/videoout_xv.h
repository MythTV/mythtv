#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

#include <set>
#include <qwindowdefs.h>

#include "DisplayRes.h"
#include "videooutbase.h"

#include "util-x11.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "avcodec.h"
}

class NuppelVideoPlayer;
class ChromaKeyOSD;

class OpenGLVideo;
class GLContextCreator;
class OpenGLContextGLX;

class XvMCBufferSettings;
class XvMCSurfaceTypes;
class XvMCTextures;
class XvMCOSD;

#ifdef USING_XVMC
#   include "XvMCSurfaceTypes.h"
#   include "../libavcodec/xvmc_render.h"
    typedef struct
    {
        XvMCSurface         surface;
        XvMCBlockArray      blocks;
        XvMCMacroBlockArray macro_blocks;
    } xvmc_vo_surf_t;
#else // if !USING_XVMC
    typedef int xvmc_vo_surf_t;
    typedef int XvMCSurfaceInfo;
    struct XvMCContext;
#endif // !USING_XVMC

typedef enum VideoOutputSubType
{
    XVUnknown = 0, Xlib, XShm, OpenGL, XVideo, XVideoVDPAU, 
    XVideoMC, XVideoIDCT, XVideoVLD, 
} VOSType;

class VDPAUContext;
class VideoOutputXv : public VideoOutput
{
    friend class ChromaKeyOSD;
    friend class OpenGLVideoSync;
    friend class GLContextCreator;
    friend class XvMCOSD;
  public:
    VideoOutputXv(MythCodecID av_codec_id);
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    bool SetDeinterlacingEnabled(bool);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers);

    void PrepareFrame(VideoFrame*, FrameScanType);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    void Show(FrameScanType);

    void ClearAfterSeek(void);

    void WindowResized(const QSize &new_size);

    void MoveResize(void);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);
    void Zoom(ZoomDirection direction);
    void VideoAspectRatioChanged(float aspect);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    void ResizeForGui(void); 
    void ResizeForVideo(void);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);

    /// Monitor refresh time in microseconds
    int  GetRefreshRate(void);

    virtual bool hasMCAcceleration(void) const
        { return XVideoMC <= VideoOutputSubType(); }
    virtual bool hasIDCTAcceleration(void) const
        { return XVideoIDCT <= VideoOutputSubType(); }
    virtual bool hasVLDAcceleration(void) const
        { return XVideoVLD == VideoOutputSubType(); }
    virtual bool hasXVAcceleration(void) const
        { return XVideo == VideoOutputSubType(); }
    virtual bool hasOpenGLAcceleration(void) const
        { return OpenGL == VideoOutputSubType(); }
    virtual bool hasHWAcceleration(void) const
        { return OpenGL <= VideoOutputSubType(); }
    virtual bool hasVDPAUAcceleration(void) const
        { return XVideoVDPAU == VideoOutputSubType(); }

    void CheckFrameStates(void);

    virtual QRect GetPIPRect(PIPLocation        location,
                             NuppelVideoPlayer *pipplayer = NULL,
                             bool               do_pixel_adj = true) const;
    virtual void RemovePIP(NuppelVideoPlayer *pipplayer);

    virtual void ShutdownVideoResize(void);

    // OpenGL
    OpenGLContextGLX *GetGLContext(void) { return gl_context; }
    OpenGLContextGLX *CreateGLContext(const QRect &display_rect,
                                   bool m_map_window);
    void GLContextCreatedNotify(void);
    void GLContextCreatedWait(void);

    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             uint osd_width, uint osd_height,
                                             uint stream_type, int xvmc_chroma,
                                             bool test_surface, bool force_xv);

    static int GrabSuitableXvPort(Display* disp, Window root,
                                  MythCodecID type,
                                  uint width, uint height,
                                  int xvmc_chroma = 0,
                                  XvMCSurfaceInfo* si = NULL,
                                  QString *adaptor_name = NULL);

    static XvMCContext* CreateXvMCContext(Display* disp, int port,
                                          int surf_type,
                                          int width, int height);
    static void DeleteXvMCContext(Display* disp, XvMCContext*& ctx);


    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    VOSType VideoOutputSubType() const { return video_output_subtype; }
    void SetNextFrameDisplayTimeOffset(int delayus);

  private:
    virtual QRect GetVisibleOSDBounds(float&, float&, float) const;
    virtual QRect GetTotalOSDBounds(void) const;
    virtual bool hasFullScreenOSD(void) const;
    QRect GetTotalVisibleRect(void) const;

    VideoFrame *GetNextFreeFrame(bool allow_unsafe);
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);
    void DoneDisplayingFrame(void);

    void ProcessFrameVDPAU(VideoFrame *frame, OSD *osd,
                           const PIPMap &pipPlayers);
    void ProcessFrameXvMC(VideoFrame *frame, OSD *osd);
    void ProcessFrameOpenGL(VideoFrame *frame, OSD *osd,
                            FilterChain *filterList,
                            const PIPMap &pipPlayers);
    void ProcessFrameMem(VideoFrame *frame, OSD *osd,
                         FilterChain *filterList,
                         const PIPMap &pipPlayers);

    void PrepareFrameVDPAU(VideoFrame *, FrameScanType);
    void PrepareFrameXvMC(VideoFrame *, FrameScanType);
    void PrepareFrameXv(VideoFrame *);
    void PrepareFrameOpenGL(VideoFrame *, FrameScanType);
    void PrepareFrameMem(VideoFrame *, FrameScanType);

    void ShowVDPAU(FrameScanType scan);
    void ShowXvMC(FrameScanType scan);
    void ShowXVideo(FrameScanType scan);

    virtual void ShowPIP(VideoFrame        *frame,
                         NuppelVideoPlayer *pipplayer,
                         PIPLocation        loc);

    virtual int DisplayOSD(VideoFrame *frame, OSD *osd,
                           int stride = -1, int revision = -1);

    void ResizeForVideo(uint width, uint height);
    void InitDisplayMeasurements(uint width, uint height);
    void InitColorKey(bool turnoffautopaint);

    bool InitVideoBuffers(MythCodecID, bool use_xv,
                          bool use_shm, bool use_opengl,
                          bool use_vdpau);

    bool InitXvMC(MythCodecID);
    bool InitXVideo(void);
    bool InitOpenGL(void);
    bool InitXShm(void);
    bool InitXlib(void);
    bool InitVDPAU(MythCodecID);
    bool InitOSD(const QString&);
    bool CheckOSDInit(void);

    bool CreateVDPAUBuffers(void);
    bool CreateXvMCBuffers(void);
    bool CreateBuffers(VOSType subtype);
    vector<void*> CreateXvMCSurfaces(uint num, bool surface_has_vld);
    vector<unsigned char*> CreateShmImages(uint num, bool use_xv);
    void CreatePauseFrame(VOSType subtype);
    void CopyFrame(VideoFrame *to, const VideoFrame *from);

    void DeleteBuffers(VOSType subtype, bool delete_pause_frame);

    bool InitSetupBuffers(void);

    // XvMC specific helper functions
    static bool IsDisplaying(VideoFrame* frame);
    static bool IsRendering(VideoFrame* frame);
    static void SyncSurface(VideoFrame* frame, int past_future = 0);
    static void FlushSurface(VideoFrame* frame);

#ifdef USING_XVMC 
    XvMCOSD* GetAvailableOSD();
    void ReturnAvailableOSD(XvMCOSD*);
#endif // USING_XVMC

    // OpenGL specific helper functions
    bool SetDeinterlacingEnabledOpenGL(bool enable);
    bool SetupDeinterlaceOpenGL(
        bool interlaced, const QString &overridefilter);

    // VDPAU specific helper functions
    bool SetDeinterlacingEnabledVDPAU(bool enable);
    bool SetupDeinterlaceVDPAU(
        bool interlaced, const QString &overridefilter);

    // Misc.
    MythCodecID          myth_codec_id;
    VOSType              video_output_subtype;
    DisplayRes          *display_res;
    QMutex               global_lock;

    // Basic X11 info
    Window               XJ_root;
    Window               XJ_win;
    Window               XJ_curwin;
    GC                   XJ_gc;
    Screen              *XJ_screen;
    Display             *XJ_disp;
    int                  XJ_screen_num;
    unsigned long        XJ_white;
    unsigned long        XJ_black;
    unsigned long        XJ_letterbox_colour;
    int                  XJ_depth;
    bool                 XJ_started;
    QSize                XJ_monitor_sz;
    QSize                XJ_monitor_dim;

    // Used for all non-XvMC drawing
    VideoFrame           av_pause_frame;
    vector<XShmSegmentInfo*> XJ_shm_infos;
    vector<YUVInfo>      XJ_yuv_infos;

    // Basic non-Xv drawing info
    XImage              *XJ_non_xv_image;
    long long            non_xv_frames_shown;
    int                  non_xv_show_frame;
    int                  non_xv_fps;
    int                  non_xv_av_format;
    time_t               non_xv_stop_time;

    // Basic XvMC drawing info
    XvMCBufferSettings  *xvmc_buf_attr;
    int                  xvmc_chroma;
    XvMCContext         *xvmc_ctx;
    vector<void*>        xvmc_surfs;
    QMutex               xvmc_osd_lock;
    MythDeque<XvMCOSD*>  xvmc_osd_available;
#ifdef USING_XVMC 
    XvMCSurfaceInfo      xvmc_surf_info;
#endif // USING_XVMC

    // Support for nVidia XvMC copy to texture feature
    XvMCTextures        *xvmc_tex;

#ifdef USING_VDPAU
    VDPAUContext        *vdpau;
#endif
    bool                 vdpau_use_osd;
    bool                 vdpau_use_pip;
    int                  vdpau_colorkey;

    // Basic Xv drawing info
    int                  xv_port;
    int                  xv_hue_base;
    int                  xv_colorkey;
    bool                 xv_draw_colorkey;
    int                  xv_chroma;
    buffer_map_t         xv_buffers;
    bool                 xv_need_bobdeint_repaint;
    QMap<PictureAttribute,int> xv_attribute_min;
    QMap<PictureAttribute,int> xv_attribute_max;

    // OpenGL drawing info
    QMutex               gl_context_lock;
    QMutex               gl_context_creator_lock;
    GLContextCreator    *gl_context_creator;
    QWaitCondition       gl_context_wait;
    OpenGLContextGLX    *gl_context;
    OpenGLVideo         *gl_videochain;
    QMap<NuppelVideoPlayer*,OpenGLVideo*> gl_pipchains;
    QMap<NuppelVideoPlayer*,bool>         gl_pip_ready;
    OpenGLVideo         *gl_pipchain_active;
    OpenGLVideo         *gl_osdchain;
    bool                 gl_use_osd_opengl2;
    bool                 gl_osd_ready;

    // Chromakey OSD info
    ChromaKeyOSD        *chroma_osd;
};

#endif // VIDEOOUT_XV_H_
