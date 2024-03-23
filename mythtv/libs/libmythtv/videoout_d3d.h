// -*- Mode: c++ -*-

#ifndef VIDEOOUT_D3D_H_
#define VIDEOOUT_D3D_H_

// MythTV headers
#include "mythvideooutgpu.h"
#include "libmythui/mythrender_d3d9.h"
#include "libmythui/mythpainter_d3d9.h"

#ifdef USING_DXVA2
#include "dxva2decoder.h"
#endif

class VideoOutputD3D : public MythVideoOutputGPU
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    VideoOutputD3D(MythMainWindow* MainWindow, MythRenderD3D9* Render,
                   MythD3D9Painter* Painter, MythDisplay* Display,
                   const MythVideoProfilePtr& VideoProfile, QString& Profile);
   ~VideoOutputD3D();

    bool Init(QSize video_dim_buf, QSize video_dim_disp, float video_aspect,
              QRect win_rect, MythCodecID codec_id) override; // VideoOutput
    void RenderFrame(MythVideoFrame *buffer, FrameScanType) override; // VideoOutput
    void RenderEnd() override; // VideoOutput
    void PrepareFrame(MythVideoFrame *frame, FrameScanType scan) override; // VideoOutput
    void EndFrame() override;
    bool InputChanged(QSize        video_dim_buf,
                      QSize        video_dim_disp,
                      float        video_aspect,
                      MythCodecID  av_codec_id,
                      bool        &aspect_only,
                      int          reference_frames,
                      bool         force_change) override; // VideoOutput
    void UpdatePauseFrame(std::chrono::milliseconds &disp_timecode, FrameScanType Scan = kScan_Progressive) override; // VideoOutput
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetSupportedCodec(AVCodecContext **Context,
                                         const AVCodec ** Codec,
                                         const QString &decoder,
                                         uint stream_type);

  private:
    void TearDown(void);
    bool SetupContext(void);
    bool CreateBuffers(void);
    bool InitBuffers(void);
    void SetProfile(void);
    void DestroyContext(void);
    void UpdateFrame(MythVideoFrame *frame, D3D9Image *img);

  private:
    MythVideoFrame              m_pauseFrame;
    QRecursiveMutex         m_lock;
    HWND                    m_hWnd           {nullptr};
    MythRenderD3D9         *m_render         {nullptr};
    D3D9Image              *m_video          {nullptr};
    bool                    m_renderValid    {false};
    bool                    m_renderReset    {false};

    bool CreateDecoder(void);
    void DeleteDecoder(void);
#ifdef USING_DXVA2
    DXVA2Decoder *m_decoder      {nullptr};
#endif
    void         *m_pauseSurface {nullptr};
};

#endif
