// -*- Mode: c++ -*-

#ifndef VIDEOOUT_D3D_H_
#define VIDEOOUT_D3D_H_

/* ACK! <windows.h> and <d3d3.h> should only be in cpp's compiled in
 * windows only. Some of the variables in VideoOutputDX need to be
 * moved to a private class before removing these includes though.
 */
#include <windows.h> // HACK HACK HACK
#include <d3d9.h>    // HACK HACK HACK

// MythTV headers
#include "videooutbase.h"

class VideoOutputD3D : public VideoOutput
{
  public:
    VideoOutputD3D();
   ~VideoOutputD3D();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);

    bool InitD3D();
    void UnInitD3D();
    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      NuppelVideoPlayer *pipPlayer);
    void Show(FrameScanType );

    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);
    int GetRefreshRate(void);
    void UpdatePauseFrame(void);
    void DrawUnusedRects(bool);
    void Zoom(ZoomDirection direction);
    void EmbedInWidget(WId wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);

    float GetDisplayAspect(void) const;

    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);

  private:
    void Exit(void);

  private:
    int                     m_InputCX;
    int                     m_InputCY;

    VideoFrame              m_pauseFrame;
    QMutex                  m_lock;

    int                     m_RefreshRate;
    HWND                    m_hWnd;
    HWND                    m_hEmbedWnd;
    D3DFORMAT               m_ddFormat;
    IDirect3D9             *m_pD3D;
    IDirect3DDevice9       *m_pd3dDevice;
    IDirect3DSurface9      *m_pSurface;
    IDirect3DTexture9      *m_pTexture;
    IDirect3DVertexBuffer9 *m_pVertexBuffer;
};

#endif
