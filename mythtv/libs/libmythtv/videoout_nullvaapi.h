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

    virtual void* GetDecoderContext(unsigned char* buf, uint8_t*& id);
    virtual bool Init(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float aspect, WId winid,
                      const QRect &win_rect, MythCodecID codec_id);
    virtual bool InputChanged(const QSize &video_dim_buf,
                              const QSize &video_dim_disp,
                              float        aspect,
                              MythCodecID  av_codec_id,
                              void        *codec_private,
                              bool        &aspect_only);

    virtual bool SetupDeinterlace(bool, const QString &ovrf = "") { return false; }
    virtual bool SetDeinterlacingEnabled(bool)                    { return false; }
    virtual bool ApproveDeintFilter(const QString& filtername) const { return false; }
    virtual void ReleaseFrame(VideoFrame *frame);
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              const PIPMap &pipPlayers,
                              FrameScanType scan)         {;}
    virtual void PrepareFrame(VideoFrame *buffer,
                              FrameScanType, OSD *osd)    {;}
    virtual void Show(FrameScanType )                     {;}

    virtual void Zoom(ZoomDirection direction)            {;}
    virtual void EmbedInWidget(const QRect &rect)         {;}
    virtual void StopEmbedding(void)                      {;}
    virtual void DrawUnusedRects(bool sync = true)        {;}
    virtual void UpdatePauseFrame(int64_t &disp_timecode) {;}
    virtual void MoveResizeWindow(QRect )                 {;}
    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render)
        { return false; }
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name) { return false; }
    virtual MythPainter *GetOSDPainter(void) { return NULL; }
    virtual VideoFrame *GetLastDecodedFrame(void);
    virtual VideoFrame *GetLastShownFrame(void);
    virtual void DoneDisplayingFrame(VideoFrame *frame);

  private:
    void TearDown(void);
    bool CreateVAAPIContext(QSize size);
    void DeleteVAAPIContext(void);
    bool InitBuffers(void);
    void DeleteBuffers(void);
    void DiscardFrame(VideoFrame*);

  private:
    VAAPIContext *m_ctx;
    QMutex        m_lock;
    VideoBuffers *m_shadowBuffers;
};

#endif // VIDEOOUT_NULLVAAPI_H
