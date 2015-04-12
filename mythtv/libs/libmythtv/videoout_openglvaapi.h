#ifndef VIDEOOUTPUTOPENGLVAAPI_H
#define VIDEOOUTPUTOPENGLVAAPI_H

#include "videoout_opengl.h"

class VAAPIContext;

class VideoOutputOpenGLVAAPI : public VideoOutputOpenGL
{
  public:
    static void GetRenderOptions(render_opts &opts);

    VideoOutputOpenGLVAAPI();
   ~VideoOutputOpenGLVAAPI();

    bool  Init(const QSize &video_dim_buf,
               const QSize &video_dim_disp,
               float aspect, WId winid,
               const QRect &win_rect, MythCodecID codec_id);
    bool  CreateVAAPIContext(QSize size);
    void  DeleteVAAPIContext(void);
    bool  CreateBuffers(void);
    virtual void* GetDecoderContext(unsigned char* buf, uint8_t*& id);
    uint8_t* GetSurfaceIDPointer(void* buf);
    void  SetProfile(void);
    void  TearDown(void);
    bool  InputChanged(const QSize &video_dim_buf,
                       const QSize &video_dim_disp,
                       float aspect,
                       MythCodecID  av_codec_id, void *codec_private,
                       bool &aspect_only);
    virtual void UpdatePauseFrame(int64_t &disp_timecode);
    void  ProcessFrame(VideoFrame *frame, OSD *osd,
                       FilterChain *filterList,
                       const PIPMap &pipPlayers,
                       FrameScanType scan);
    bool  ApproveDeintFilter(const QString& filtername) const;
    bool  SetDeinterlacingEnabled(bool enable);
    bool  SetupDeinterlace(bool i, const QString& ovrf="");
    virtual void InitPictureAttributes(void);
    virtual int  SetPictureAttribute(PictureAttribute attribute, int newValue);

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration,
                                             PixelFormat &pix_fmt);

  private:
    VAAPIContext *m_ctx;
    void         *m_pauseBuffer;
};
#endif // VIDEOOUTPUTOPENGLVAAPI_H

