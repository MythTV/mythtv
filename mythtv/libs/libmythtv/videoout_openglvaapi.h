#ifndef VIDEOOUTPUTOPENGLVAAPI_H
#define VIDEOOUTPUTOPENGLVAAPI_H

#include "videoout_opengl.h"
#include "mythopenglinterop.h"

class VideoOutputOpenGLVAAPI : public VideoOutputOpenGL
{
  public:
    static void GetRenderOptions(render_opts &opts);
    static bool AllowVAAPIDisplay();

    VideoOutputOpenGLVAAPI();
   ~VideoOutputOpenGLVAAPI() override;

    bool  Init(const QSize &video_dim_buf,
               const QSize &video_dim_disp,
               float aspect, WId winid,
               const QRect &win_rect, MythCodecID codec_id) override; // VideoOutputOpenGL
    bool  CreateBuffers(void) override; // VideoOutputOpenGL
    void  SetProfile(void) override; // VideoOutputOpenGL
    bool  InputChanged(const QSize &video_dim_buf,
                       const QSize &video_dim_disp,
                       float aspect,
                       MythCodecID  av_codec_id, void *codec_private,
                       bool &aspect_only) override; // VideoOutputOpenGL
    void  UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutputOpenGL
    void  PrepareFrame(VideoFrame *frame, FrameScanType scan, OSD *osd) override; // VideoOutputOpenGL
    bool  ApproveDeintFilter(const QString& filtername) const override; // VideoOutputOpenGL
    bool  SetDeinterlacingEnabled(bool enable) override; // VideoOutputOpenGL
    bool  SetupDeinterlace(bool interlaced, const QString& overridefilter="") override; // VideoOutputOpenGL
    void  InitPictureAttributes(void) override; // VideoOutputOpenGL
    int   SetPictureAttribute(PictureAttribute attribute, int newValue) override; // VideoOutputOpenGL

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration,
                                             AVPixelFormat &pix_fmt);

  private:
    MythOpenGLInterop m_vaapiInterop;
};
#endif // VIDEOOUTPUTOPENGLVAAPI_H

