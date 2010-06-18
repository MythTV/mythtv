// -*- Mode: c++ -*-

#ifndef VIDEOOUT_D3D_H_
#define VIDEOOUT_D3D_H_

#include <windows.h>
#include <d3d9.h>

// MythTV headers
#include "videooutbase.h"

class D3D9PrivateContext;

class D3D9Context
{
  public:
    D3D9Context();
    ~D3D9Context();

    bool Create(QSize size, HWND window);
    bool Test(bool &reset);

    bool ClearBuffer(void);
    bool Begin(void);
    bool End(void);
    bool StretchRect(IDirect3DTexture9 *texture, IDirect3DSurface9 *surface);
    bool DrawTexturedQuad(IDirect3DVertexBuffer9 *vertexbuffer,
                          IDirect3DTexture9 *alpha_tex = NULL);
    bool DrawBorder(IDirect3DVertexBuffer9 *vertexbuffer);
    bool Present(HWND win);
    void DefaultTexturing(void);
    void MultiTexturing(IDirect3DTexture9 *texture);
    void EnableBlending(bool enable);

    IDirect3DTexture9*      CreateTexture(QSize size, bool alpha = false);
    void                    DeleteTexture(IDirect3DTexture9* texture);

    IDirect3DVertexBuffer9* CreateVertexBuffer(IDirect3DTexture9* texture = NULL);
    bool                    UpdateVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer,
                                               QRect dvr, QRect vr,
                                               bool border = false);
    void                    DeleteVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer);

    IDirect3DSurface9*      CreateSurface(QSize size, bool alpha = false);
    bool                    UpdateSurface(IDirect3DSurface9* surface,
                                          VideoFrame *buffer,
                                          const unsigned char *alpha = NULL);
    void                    DeleteSurface(IDirect3DSurface9* surface);

  private:
    bool                    FormatSupported(D3DFORMAT surface, D3DFORMAT adaptor);
    bool                    SetTexture(IDirect3DTexture9 *texture, int num = 0);
    void                    DeleteTextures(void);
    void                    DeleteVertexBuffers(void);
    void                    DeleteSurfaces(void);
    void                    Init2DState(void);

    D3D9PrivateContext     *m_priv;
    IDirect3D9             *m_d3d;
    IDirect3DDevice9       *m_d3dDevice;
    D3DFORMAT               m_adaptor_fmt;
    D3DFORMAT               m_videosurface_fmt;
    D3DFORMAT               m_alphasurface_fmt;
    D3DFORMAT               m_alphatexture_fmt;
    QMutex                  m_lock;
};

class D3D9Video
{
  public:
    D3D9Video(D3D9Context *ctx, QSize size, bool alpha = false);
    ~D3D9Video();

    bool  IsValid(void) { return m_valid; }
    QSize GetSize(void) { return m_size; }
    bool  UpdateVideo(VideoFrame *frame, const unsigned char *alpha = NULL);
    bool  UpdateVertices(QRect dvr, QRect vr);
    bool  Draw(bool border = false);

  private:
    QSize                   m_size;
    bool                    m_valid;
    D3D9Context            *m_ctx;
    IDirect3DVertexBuffer9 *m_vertexbuffer;
    IDirect3DTexture9      *m_texture;
    IDirect3DSurface9      *m_surface;
    IDirect3DTexture9      *m_alpha_texture;
    IDirect3DSurface9      *m_alpha_surface;

};

class VideoOutputD3D : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputD3D();
   ~VideoOutputD3D();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
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
    DisplayInfo GetDisplayInfo(void);
    void MoveResizeWindow(QRect new_rect) {;}
    void UpdatePauseFrame(void);
    void DrawUnusedRects(bool) {;}
    void Zoom(ZoomDirection direction);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    bool hasFullScreenOSD(void) const { return m_d3d_osd; }
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    void ShowPIP(VideoFrame        *frame,
                 NuppelVideoPlayer *pipplayer,
                 PIPLocation        loc);
    void RemovePIP(NuppelVideoPlayer *pipplayer);
    bool IsPIPSupported(void) const { return true; }

  private:
    void TearDown(void);
    bool SetupContext(void);
    void DestroyContext(void);
    int  DisplayOSD(VideoFrame *frame, OSD *osd,
                    int stride = -1, int revision = -1);

  private:
    VideoFrame              m_pauseFrame;
    QMutex                  m_lock;
    HWND                    m_hWnd;
    HWND                    m_hEmbedWnd;
    D3D9Context            *m_ctx;
    D3D9Video              *m_video;
    D3D9Video              *m_osd;
    bool                    m_d3d_osd;
    bool                    m_osd_ready;
    bool                    m_ctx_valid;
    bool                    m_ctx_reset;

    QMap<NuppelVideoPlayer*,D3D9Video*> m_pips;
    QMap<NuppelVideoPlayer*,bool>       m_pip_ready;
    D3D9Video                          *m_pip_active;
};

#endif
