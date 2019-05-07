// -*- Mode: c++ -*-

#ifndef VIDEOOUT_D3D_H_
#define VIDEOOUT_D3D_H_

// MythTV headers
#include "videooutbase.h"
#include "mythrender_d3d9.h"
#include "mythpainter_d3d9.h"

#ifdef USING_DXVA2
#include "dxva2decoder.h"
#endif

class VideoOutputD3D : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputD3D();
   ~VideoOutputD3D();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd) override; // VideoOutput
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan) override; // VideoOutput
    void Show(FrameScanType ) override; // VideoOutput
    void WindowResized(const QSize &new_size) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only) override; // VideoOutput
    void MoveResizeWindow(QRect new_rect) override {;} // VideoOutput
    void UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutput
    void DrawUnusedRects(bool) override {;} // VideoOutput
    void Zoom(ZoomDirection direction) override; // VideoOutput
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    bool hasFullScreenOSD(void) const override // VideoOutput
        { return true; }
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             const QString &decoder,
                                             uint stream_type,
                                             bool no_acceleration,
                                             AVPixelFormat &pix_fmt);

    void ShowPIP(VideoFrame  *frame,
                 MythPlayer  *pipplayer,
                 PIPLocation  loc) override; // VideoOutput
    void RemovePIP(MythPlayer *pipplayer) override; // VideoOutput
    bool IsPIPSupported(void) const override // VideoOutput
        { return true; }
    MythPainter *GetOSDPainter(void) override; // VideoOutput
    bool hasHWAcceleration(void) const override // VideoOutput
        { return !codec_is_std(video_codec_id); }
    bool ApproveDeintFilter(const QString& filtername) const override; // VideoOutput
    void* GetDecoderContext(unsigned char* buf, uint8_t*& id) override; // VideoOutput

    bool CanVisualise(AudioPlayer *audio, MythRender */*render*/) override // VideoOutput
        { return VideoOutput::CanVisualise(audio, (MythRender*)m_render); }
    bool SetupVisualisation(AudioPlayer *audio, MythRender */*render*/,
                            const QString &name) override // VideoOutput
        { return VideoOutput::SetupVisualisation(audio, (MythRender*)m_render, name); }

  private:
    void TearDown(void);
    bool SetupContext(void);
    bool CreateBuffers(void);
    bool InitBuffers(void);
    bool CreatePauseFrame(void);
    void SetProfile(void);
    void DestroyContext(void);
    void UpdateFrame(VideoFrame *frame, D3D9Image *img);

  private:
    VideoFrame              m_pauseFrame;
    QMutex                  m_lock;
    HWND                    m_hWnd;
    HWND                    m_hEmbedWnd;
    MythRenderD3D9         *m_render;
    D3D9Image              *m_video;
    bool                    m_render_valid;
    bool                    m_render_reset;

    QMap<MythPlayer*,D3D9Image*> m_pips;
    QMap<MythPlayer*,bool>       m_pip_ready;
    D3D9Image                   *m_pip_active;

    MythD3D9Painter        *m_osd_painter;

    bool CreateDecoder(void);
    void DeleteDecoder(void);
#ifdef USING_DXVA2
    DXVA2Decoder *m_decoder;
#endif
    void         *m_pause_surface;
};

#endif
