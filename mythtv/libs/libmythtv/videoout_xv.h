#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

#include <qwindowdefs.h>

#include "videooutbase.h"

#include "mythxdisplay.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/Xv.h>

#undef HAVE_AV_CONFIG_H
extern "C" {
#include "libavcodec/avcodec.h"
}

class MythPlayer;
class ChromaKeyOSD;
class XvMCBufferSettings;
class XvMCSurfaceTypes;
class XvMCTextures;
class XvMCOSD;

#ifdef USING_XVMC
#   include "XvMCSurfaceTypes.h"
#   include "libavcodec/xvmc.h"
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
    XVUnknown = 0, Xlib, XShm, XVideo,
    XVideoMC, XVideoIDCT, XVideoVLD, 
} VOSType;

class VideoOutputXv : public VideoOutput
{
    friend class ChromaKeyOSD;
    friend class XvMCOSD;
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputXv();
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh,
              MythCodecID codec_id, WId embedid = 0);

    bool SetDeinterlacingEnabled(bool);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);

    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    void Show(FrameScanType);

    void ClearAfterSeek(void);

    void WindowResized(const QSize &new_size);

    void MoveResize(void);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void Zoom(ZoomDirection direction);
    void VideoAspectRatioChanged(float aspect);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    void MoveResizeWindow(QRect new_rect);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);

    virtual bool IsPIPSupported(void) const
        { return XVideo == VideoOutputSubType(); }
    virtual bool IsPBPSupported(void) const
        { return XVideo == VideoOutputSubType(); }
    virtual bool NeedExtraAudioDecode(void) const
        { return XVideoMC <= VideoOutputSubType(); }
    virtual bool hasHWAcceleration(void) const
        { return XVideoMC <= VideoOutputSubType(); }

    void CheckFrameStates(void);

    virtual QRect GetPIPRect(PIPLocation  location,
                             MythPlayer  *pipplayer = NULL,
                             bool         do_pixel_adj = true) const;

    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             uint osd_width, uint osd_height,
                                             uint stream_type, int xvmc_chroma,
                                             bool test_surface, bool force_xv);

    static int GrabSuitableXvPort(MythXDisplay* disp, Window root,
                                  MythCodecID type,
                                  uint width, uint height,
                                  bool &xvsetdefaults,
                                  int xvmc_chroma = 0,
                                  XvMCSurfaceInfo* si = NULL,
                                  QString *adaptor_name = NULL);
    static void UngrabXvPort(MythXDisplay* disp, int port);

    static XvMCContext* CreateXvMCContext(MythXDisplay* disp, int port,
                                          int surf_type,
                                          int width, int height);
    static void DeleteXvMCContext(MythXDisplay* disp, XvMCContext*& ctx);


    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    VOSType VideoOutputSubType() const { return video_output_subtype; }
    virtual MythPainter* GetOSDPainter(void);

  private:
    virtual bool hasFullScreenOSD(void) const { return chroma_osd; }
    VideoFrame *GetNextFreeFrame(bool allow_unsafe);
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);
    void DoneDisplayingFrame(VideoFrame *frame);

    void ProcessFrameXvMC(VideoFrame *frame, OSD *osd);
    void ProcessFrameMem(VideoFrame *frame, OSD *osd,
                         FilterChain *filterList,
                         const PIPMap &pipPlayers,
                         FrameScanType scan);

    void PrepareFrameXvMC(VideoFrame *, FrameScanType);
    void PrepareFrameXv(VideoFrame *);
    void PrepareFrameMem(VideoFrame *, FrameScanType);

    void ShowXvMC(FrameScanType scan);
    void ShowXVideo(FrameScanType scan);

    virtual void ShowPIP(VideoFrame  *frame,
                         MythPlayer  *pipplayer,
                         PIPLocation  loc);

    void InitColorKey(bool turnoffautopaint);

    bool InitVideoBuffers(bool use_xv, bool use_shm);

    bool InitXvMC(void);
    bool InitXVideo(void);
    bool InitXShm(void);
    bool InitXlib(void);
    bool InitOSD(void);

    bool CreateXvMCBuffers(void);
    bool CreateBuffers(VOSType subtype);
    vector<void*> CreateXvMCSurfaces(uint num, bool surface_has_vld);
    vector<unsigned char*> CreateShmImages(uint num, bool use_xv);
    void CreatePauseFrame(VOSType subtype);

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

    // Misc.
    VOSType              video_output_subtype;
    QMutex               global_lock;

    // Basic X11 info
    Window               XJ_win;
    Window               XJ_curwin;
    MythXDisplay        *disp;
    unsigned long        XJ_letterbox_colour;
    bool                 XJ_started;

    // Used for all non-XvMC drawing
    VideoFrame           av_pause_frame;
    vector<XShmSegmentInfo*> XJ_shm_infos;
    vector<YUVInfo>      XJ_yuv_infos;

    // Basic non-Xv drawing info
    XImage              *XJ_non_xv_image;
    long long            non_xv_frames_shown;
    int                  non_xv_show_frame;
    int                  non_xv_fps;
    PixelFormat          non_xv_av_format;
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

    // Basic Xv drawing info
    int                  xv_port;
    int                  xv_hue_base;
    int                  xv_colorkey;
    bool                 xv_draw_colorkey;
    int                  xv_chroma;
    bool                 xv_set_defaults;
    buffer_map_t         xv_buffers;
    bool                 xv_need_bobdeint_repaint;
    QMap<PictureAttribute,int> xv_attribute_min;
    QMap<PictureAttribute,int> xv_attribute_max;
    QMap<PictureAttribute,int> xv_attribute_def;

    // Chromakey OSD info
    ChromaKeyOSD        *chroma_osd;
};

#endif // VIDEOOUT_XV_H_
