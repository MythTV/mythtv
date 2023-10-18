// -*- Mode: c++ -*-

#ifndef VIDEOOUT_D3D_H_
#define VIDEOOUT_D3D_H_

// MythTV headers
#include "mythvideoout.h"
#include "libmythui/mythrender_d3d9.h"
#include "libmythui/mythpainter_d3d9.h"

#ifdef USING_DXVA2
#include "dxva2decoder.h"
#endif

class VideoOutputD3D : public MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    VideoOutputD3D();
   ~VideoOutputD3D();

    bool Init(const QSize &video_dim_buf, const QSize &video_dim_disp, float video_aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    void RenderFrame(MythVideoFrame *buffer, FrameScanType) override; // VideoOutput
    void RenderOverlays(OSD *osd) override; // VideoOutput
    void RenderEnd() override; // VideoOutput
    void PrepareFrame(MythVideoFrame *frame, FrameScanType scan) override; // VideoOutput
    void PrepareEnd() override; // VideoOutput
    void EndFrame() override;
    void WindowResized(const QSize &new_size) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        video_aspect,
                      MythCodecID  av_codec_id,
                      bool        &aspect_only) override; // VideoOutput
    void UpdatePauseFrame(std::chrono::milliseconds &disp_timecode, FrameScanType Scan = kScan_Progressive) override; // VideoOutput
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration,
                                             AVPixelFormat &pix_fmt);

    MythPainter *GetOSDPainter(void) override; // VideoOutput

  private:
    void TearDown(void);
    bool SetupContext(void);
    bool CreateBuffers(void);
    bool InitBuffers(void);
    bool CreatePauseFrame(void);
    void SetProfile(void);
    void DestroyContext(void);
    void UpdateFrame(MythVideoFrame *frame, D3D9Image *img);

  private:
    MythVideoFrame              m_pauseFrame;
#if QT_VERSION < QT_VERSION_CHECK(5,14,0)
    QMutex                  m_lock           {QMutex::Recursive};
#else
    QRecursiveMutex         m_lock;
#endif
    HWND                    m_hWnd           {nullptr};
    HWND                    m_hEmbedWnd      {nullptr};
    MythRenderD3D9         *m_render         {nullptr};
    D3D9Image              *m_video          {nullptr};
    bool                    m_renderValid    {false};
    bool                    m_renderReset    {false};

    MythD3D9Painter        *m_osdPainter     {nullptr};

    bool CreateDecoder(void);
    void DeleteDecoder(void);
#ifdef USING_DXVA2
    DXVA2Decoder *m_decoder      {nullptr};
#endif
    void         *m_pauseSurface {nullptr};
};

#endif
