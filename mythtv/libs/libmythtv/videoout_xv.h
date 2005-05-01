#ifndef VIDEOOUT_XV_H_
#define VIDEOOUT_XV_H_

#include <set>
#include <qwindowdefs.h>

#include "DisplayRes.h"
#include "videooutbase.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#include "../libavcodec/avcodec.h"

#ifdef USING_XVMC
class XvMCOSD;
#include "XvMCSurfaceTypes.h"
#include "../libavcodec/xvmc_render.h"
typedef struct
{
    XvMCSurface         surface;
    XvMCBlockArray      blocks;
    XvMCMacroBlockArray macro_blocks;
} xvmc_vo_surf_t;
#else // if !USING_XVMC
class XvMCContext;
class XvMCSurfaceInfo;
#endif // !USING_XVMC

class NuppelVideoPlayer;

typedef enum VideoOutputSubType {
    XVUnknown = 0, Xlib, XShm, XVideo, XVideoMC, XVideoIDCT, XVideoVLD,
} VOSType;

class VideoOutputXv : public VideoOutput
{
  public:
    VideoOutputXv(MythCodecID av_codec_id);
   ~VideoOutputXv();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    bool SetupDeinterlace(bool interlaced);
    bool ApproveDeintFilter(const QString& filtername) const;

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);
    void PrepareFrame(VideoFrame*, FrameScanType);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    void Show(FrameScanType);

    void InputChanged(int width, int height, float aspect);
    void Zoom(int direction);
    void AspectChanged(float aspect);
    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    void StopEmbedding(void);
    int GetRefreshRate(void);
    void DrawUnusedRects(bool sync = true);
    float GetDisplayAspect(void);
    void UpdatePauseFrame(void);
    int ChangePictureAttribute(int attributeType, int newValue);

    virtual bool hasMCAcceleration(void) const
        { return XVideoMC <= VideoOutputSubType(); }
    virtual bool hasIDCTAcceleration(void) const
        { return XVideoIDCT <= VideoOutputSubType(); }
    virtual bool hasVLDAcceleration(void) const
        { return XVideoVLD == VideoOutputSubType(); }

    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             uint osd_width, uint osd_height,
                                             uint stream_type, int xvmc_chroma);

    static int GrabSuitableXvPort(Display* disp, Window root,
                                  MythCodecID type,
                                  uint width, uint height,
                                  int xvmc_chroma = 0,
                                  XvMCSurfaceInfo* si = NULL);

    static XvMCContext* CreateXvMCContext(Display* disp, int port,
                                          int surf_type,
                                          int width, int height);
    static void DeleteXvMCContext(Display* disp, XvMCContext*& ctx);

  private:
    VOSType VideoOutputSubType() const { return video_output_subtype; }

    VideoFrame *GetNextFreeFrame(bool allow_unsafe);
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(void);
    void DoneDisplayingFrame(void);

    void ProcessFrameXvMC(VideoFrame *frame, OSD *osd);
    void ProcessFrameMem(VideoFrame *frame, OSD *osd,
                         FilterChain *filterList,
                         NuppelVideoPlayer *pipPlayer);

    void PrepareFrameXvMC(VideoFrame *);
    void PrepareFrameXv(VideoFrame *);
    void PrepareFrameMem(VideoFrame *, FrameScanType);

    void ShowXvMC(FrameScanType scan);
    void ShowXVideo(FrameScanType scan);

    void ResizeForVideo(uint width, uint height);
    void InitDisplayMeasurements(uint width, uint height);
    void InitColorKey(bool turnoffautopaint);

    bool InitVideoBuffers(MythCodecID, bool use_xv, bool use_shm);
    bool InitXvMC(MythCodecID);
    bool InitXVideo(void);
    bool InitXShm(void);
    bool InitXlib(void);

    bool CreateXvMCBuffers(void);
    bool CreateBuffers(VOSType subtype);
    vector<void*> CreateXvMCSurfaces(uint num, bool create_xvmc_blocks);
    vector<unsigned char*> CreateShmImages(uint num, bool use_xv);
    void CreatePauseFrame(void);

    void DeleteBuffers(VOSType subtype, bool delete_pause_frame);

    // XvMC specific helper functions
    static bool IsDisplaying(VideoFrame* frame);
    static bool IsRendering(VideoFrame* frame);
    static void SyncSurface(VideoFrame* frame, int past_future = 0);
    static void FlushSurface(VideoFrame* frame);
    void CheckDisplayedFramesForAvailability(void);
#ifdef USING_XVMC 
    XvMCOSD* GetAvailableOSD();
    void ReturnAvailableOSD(XvMCOSD*);
#endif // USING_XVMC

    // Misc.
    MythCodecID          myth_codec_id;
    VOSType              video_output_subtype;
    DisplayRes          *display_res;
    float                display_aspect;
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
    int                  XJ_depth;
    int                  XJ_screenx;
    int                  XJ_screeny;
    int                  XJ_screenwidth;
    int                  XJ_screenheight;
    bool                 XJ_started;

    // Used for all non-XvMC drawing
    VideoFrame           av_pause_frame;
    vector<XShmSegmentInfo> XJ_shm_infos;

    // Basic non-Xv drawing info
    XImage              *XJ_non_xv_image;
    long long            non_xv_frames_shown;
    int                  non_xv_show_frame;
    int                  non_xv_fps;
    int                  non_xv_av_format;
    time_t               non_xv_stop_time;

    // Basic Xv drawing info
    int                  xv_port;
    int                  xv_colorkey;
    bool                 xv_draw_colorkey;
    int                  xv_chroma;
    unsigned char       *xv_color_conv_buf;
    buffer_map_t         xv_buffers;

#ifdef USING_XVMC 
    // Basic XvMC drawing info
    XvMCSurfaceInfo      xvmc_surf_info;
    int                  xvmc_chroma;
    XvMCContext         *xvmc_ctx;
    vector<void*>        xvmc_surfs;

    // Basic XvMC drawing info
    QMutex               xvmc_osd_lock;
    MythDeque<XvMCOSD*>  xvmc_osd_available;
    friend class XvMCOSD;
#endif // USING_XVMC
};

CodecID myth2av_codecid(MythCodecID codec_id,
                        bool& vld, bool& idct, bool& mc);

#endif // VIDEOOUT_XV_H_
