#ifndef VIDEOOUT_QUARTZ_H_
#define VIDEOOUT_QUARTZ_H_

class DVDV;
struct QuartzData;

#include "videooutbase.h"

class VideoOutputQuartz : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputQuartz(MythCodecID av_codec_id, void *codec_priv);
   ~VideoOutputQuartz();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame *buffer, FrameScanType t, OSD *osd);
    void Show(FrameScanType);

    void SetVideoFrameRate(float playback_fps);
    void ToggleAspectOverride(AspectOverrideMode aspectMode);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void VideoAspectRatioChanged(float aspect);
    void MoveResize(void);
    void Zoom(ZoomDirection direction);
    void ToggleAdjustFill(AdjustFillMode adjustFill);

    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);

    DisplayInfo GetDisplayInfo(void);
    void MoveResizeWindow(QRect new_rect) {;}
    void DrawUnusedRects(bool sync = true);

    void UpdatePauseFrame(void);

    void SetDVDVDecoder(DVDV *dvdvdec);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

    static MythCodecID GetBestSupportedCodec(
        uint width, uint height,
        uint osd_width, uint osd_height,
        uint stream_type, uint fourcc);
    virtual bool NeedExtraAudioDecode(void) const
        { return !codec_is_std(myth_codec_id); }

  private:
    void Exit(void);
    bool CreateQuartzBuffers(void);
    void DeleteQuartzBuffers(void);

    bool         Started;
    QuartzData  *data;
    VideoFrame   pauseFrame;
    MythCodecID  myth_codec_id;
};

#endif
