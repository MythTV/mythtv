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

    bool Init(int width, int height, float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id);

    bool SetDeinterlacingEnabled(bool);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);

    void PrepareFrame(VideoFrame*, FrameScanType, OSD *osd);
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
    void EmbedInWidget(const QRect &rect);
    void StopEmbedding(void);
    void MoveResizeWindow(QRect new_rect);
    void DrawUnusedRects(bool sync = true);
    virtual void UpdatePauseFrame(int64_t &default_tc);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);

    virtual bool IsPIPSupported(void) const
        { return XVideo == VideoOutputSubType(); }
    virtual bool IsPBPSupported(void) const
        { return XVideo == VideoOutputSubType(); }
    virtual bool NeedExtraAudioDecode(void) const { return false; }

    virtual QRect GetPIPRect(PIPLocation  location,
                             MythPlayer  *pipplayer = NULL,
                             bool         do_pixel_adj = true) const;

    static MythCodecID GetBestSupportedCodec(uint stream_type);

    static int GrabSuitableXvPort(MythXDisplay* disp, Window root,
                                  MythCodecID type,
                                  uint width, uint height,
                                  bool &xvsetdefaults,
                                  QString *adaptor_name = NULL);
    static void UngrabXvPort(MythXDisplay* disp, int port);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    VOSType VideoOutputSubType() const { return video_output_subtype; }
    virtual MythPainter* GetOSDPainter(void);

  private:
    virtual bool hasFullScreenOSD(void) const { return chroma_osd; }
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);

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
    PixelFormat          non_xv_av_format;
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
