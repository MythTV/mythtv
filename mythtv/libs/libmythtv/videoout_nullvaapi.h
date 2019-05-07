#ifndef VIDEOOUT_NULLVAAPI_H
#define VIDEOOUT_NULLVAAPI_H

#include "videooutbase.h"

class VAAPIContext;

class VideoOutputNullVAAPI : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id);
    VideoOutputNullVAAPI();
   ~VideoOutputNullVAAPI();

    void* GetDecoderContext(unsigned char* buf, uint8_t*& id) override; // VideoOutput
    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect, WId winid,
              const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only) override; // VideoOutput

    bool SetupDeinterlace(bool, const QString & = "") override // VideoOutput
        { return false; }
    bool SetDeinterlacingEnabled(bool) override // VideoOutput
        { return false; }
    bool ApproveDeintFilter(const QString&) const override // VideoOutput
        { return false; }
    void ReleaseFrame(VideoFrame *frame) override; // VideoOutput
    void ProcessFrame(VideoFrame *, OSD *,
                      FilterChain *,
                      const PIPMap &,
                      FrameScanType)        override {;} // VideoOutput
    void PrepareFrame(VideoFrame *,
                      FrameScanType, OSD *) override {;} // VideoOutput
    void Show(FrameScanType )               override {;} // VideoOutput

    void Zoom(ZoomDirection)                override {;} // VideoOutput
    void EmbedInWidget(const QRect &)       override {;} // VideoOutput
    void StopEmbedding(void)                override {;} // VideoOutput
    void DrawUnusedRects(bool = true)       override {;} // VideoOutput
    void UpdatePauseFrame(int64_t &)        override {;} // VideoOutput
    void MoveResizeWindow(QRect )           override {;} // VideoOutput
    bool CanVisualise(AudioPlayer *, MythRender *) override // VideoOutput
        { return false; }
    bool SetupVisualisation(AudioPlayer *, MythRender *,
                            const QString &) override // VideoOutput
        { return false; }
    MythPainter *GetOSDPainter(void) override // VideoOutput
        { return nullptr; }
    VideoFrame *GetLastDecodedFrame(void) override; // VideoOutput
    VideoFrame *GetLastShownFrame(void) override; // VideoOutput
    void DoneDisplayingFrame(VideoFrame *frame) override; // VideoOutput

  private:
    void TearDown(void);
    bool CreateVAAPIContext(QSize size);
    void DeleteVAAPIContext(void);
    bool InitBuffers(void);
    void DeleteBuffers(void);
    void DiscardFrame(VideoFrame*) override; // VideoOutput

  private:
    VAAPIContext *m_ctx;
    QMutex        m_lock;
    VideoBuffers *m_shadowBuffers;
};

#endif // VIDEOOUT_NULLVAAPI_H
