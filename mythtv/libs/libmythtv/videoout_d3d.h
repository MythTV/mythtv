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

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh,
              MythCodecID codec_id, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void Show(FrameScanType );
    void WindowResized(const QSize &new_size);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only);
    void MoveResizeWindow(QRect new_rect) {;}
    void UpdatePauseFrame(void);
    void DrawUnusedRects(bool) {;}
    void Zoom(ZoomDirection direction);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    bool hasFullScreenOSD(void) const { return true; }
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width, uint height,
                                             uint stream_type,
                                             bool no_acceleration,
                                             PixelFormat &pix_fmt);

    void ShowPIP(VideoFrame  *frame,
                 MythPlayer  *pipplayer,
                 PIPLocation  loc);
    void RemovePIP(MythPlayer *pipplayer);
    bool IsPIPSupported(void) const { return true; }
    virtual MythPainter *GetOSDPainter(void) { return (MythPainter*)m_osd_painter; }
    bool hasHWAcceleration(void) const { return !codec_is_std(video_codec_id); }
    virtual bool ApproveDeintFilter(const QString& filtername) const;
    void* GetDXVA2Decoder(void);

    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render)
        { return VideoOutput::CanVisualise(audio, (MythRender*)m_render); }
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render)
        { return VideoOutput::SetupVisualisation(audio, (MythRender*)m_render); }

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
