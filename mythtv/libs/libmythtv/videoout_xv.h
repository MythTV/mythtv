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

typedef enum VideoOutputSubType
{
    XVUnknown = 0, Xlib, XShm, XVideo,
} VOSType;

class VideoOutputXv : public VideoOutput
{
    friend class ChromaKeyOSD;
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputXv();
   ~VideoOutputXv();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput

    bool SetDeinterlacingEnabled(bool) override; // VideoOutput
    bool SetupDeinterlace(bool interlaced, const QString& overridefilter="") override; // VideoOutput
    bool ApproveDeintFilter(const QString& filtername) const override; // VideoOutput

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan) override; // VideoOutput

    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd) override; // VideoOutput
    void Show(FrameScanType) override; // VideoOutput

    void ClearAfterSeek(void) override; // VideoOutput

    void WindowResized(const QSize &new_size) override; // VideoOutput

    void MoveResize(void) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only) override; // VideoOutput
    void Zoom(ZoomDirection direction) override; // VideoOutput
    void VideoAspectRatioChanged(float aspect) override; // VideoOutput
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    void MoveResizeWindow(QRect new_rect) override; // VideoOutput
    void DrawUnusedRects(bool sync = true) override; // VideoOutput
    void UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutput
    int  SetPictureAttribute(PictureAttribute attribute, int newValue) override; // VideoOutput
    void InitPictureAttributes(void) override; // VideoOutput

    bool IsPIPSupported(void) const override // VideoOutput
        { return XVideo == VideoOutputSubType(); }
    bool IsPBPSupported(void) const override // VideoOutput
        { return XVideo == VideoOutputSubType(); }
    bool NeedExtraAudioDecode(void) const override // VideoOutput
        { return false; }

    QRect GetPIPRect(PIPLocation  location,
                     MythPlayer  *pipplayer = nullptr,
                     bool         do_pixel_adj = true) const override; // VideoOutput

    static MythCodecID GetBestSupportedCodec(uint stream_type);

    int GrabSuitableXvPort(MythXDisplay* disp, Window root,
                                  MythCodecID type,
                                  uint width, uint height,
                                  bool &xvsetdefaults,
                                  QString *adaptor_name = nullptr);
    static void UngrabXvPort(MythXDisplay* disp, int port);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    VOSType VideoOutputSubType() const { return video_output_subtype; }
    MythPainter* GetOSDPainter(void) override; // VideoOutput

  private:
    bool hasFullScreenOSD(void) const override // VideoOutput
        { return chroma_osd; }
    void DiscardFrame(VideoFrame*) override; // VideoOutput
    void DiscardFrames(bool next_frame_keyframe) override; // VideoOutput

    void PrepareFrameXv(VideoFrame *);
    void PrepareFrameMem(VideoFrame *, FrameScanType);

    void ShowXVideo(FrameScanType scan);

    int  SetXVPictureAttribute(PictureAttribute attribute, int newValue);
    void InitColorKey(bool turnoffautopaint);

    bool InitVideoBuffers(bool use_xv, bool use_shm);

    bool InitXVideo(void);
    bool InitXShm(void);
    bool InitXlib(void);
    bool CreateOSD(void);

    bool CreateBuffers(VOSType subtype);
    vector<unsigned char*> CreateShmImages(uint num, bool use_xv);
    void CreatePauseFrame(VOSType subtype);

    void DeleteBuffers(VOSType subtype, bool delete_pause_frame);

    bool InitSetupBuffers(void);

    // Misc.
    VOSType              video_output_subtype;
    QMutex               global_lock;

    // Basic X11 info
    Window               XJ_win;
    Window               XJ_curwin;
    MythXDisplay        *disp;
    unsigned long        XJ_letterbox_colour;
    bool                 XJ_started;

    VideoFrame           av_pause_frame;
    vector<XShmSegmentInfo*> XJ_shm_infos;
    vector<YUVInfo>      XJ_yuv_infos;

    // Basic non-Xv drawing info
    XImage              *XJ_non_xv_image;
    long long            non_xv_frames_shown;
    int                  non_xv_show_frame;
    int                  non_xv_fps;
    AVPixelFormat        non_xv_av_format;
    time_t               non_xv_stop_time;

    // Basic Xv drawing info
    int                  xv_port;
    int                  xv_hue_base;
    int                  xv_colorkey;
    bool                 xv_draw_colorkey;
    int                  xv_chroma;
    bool                 xv_set_defaults;
    buffer_map_t         xv_buffers;
    bool                 xv_need_bobdeint_repaint;
    bool                 xv_use_picture_controls;
    QMap<PictureAttribute,int> xv_attribute_min;
    QMap<PictureAttribute,int> xv_attribute_max;
    QMap<PictureAttribute,int> xv_attribute_def;

    // Chromakey OSD info
    ChromaKeyOSD        *chroma_osd;
};

#endif // VIDEOOUT_XV_H_
