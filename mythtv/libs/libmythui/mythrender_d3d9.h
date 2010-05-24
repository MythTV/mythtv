#ifndef MYTHRENDER_D3D9_H
#define MYTHRENDER_D3D9_H

#include <windows.h>
#include <d3d9.h>

#include "mythimage.h"

class MythD3DVertexBuffer;
class MythD3DSurface;
class MythRenderD3D9;

class D3D9Image
{
  public:
    D3D9Image(MythRenderD3D9 *render, QSize size, bool video = false);
    ~D3D9Image();

    bool     IsValid(void) const { return m_valid; }
    QSize    GetSize(void) const { return m_size;  }
    bool     SetAsRenderTarget(void);
    bool     UpdateImage(const MythImage *img);
    bool     UpdateVertices(const QRect &dvr, const QRect &vr, int alpha = 255,
                            bool video = false);
    bool     Draw(void);
    uint8_t* GetBuffer(bool &hardware_conv, uint &pitch);
    void     ReleaseBuffer(void);
    QRect    GetRect(void);

  private:
    QSize                   m_size;
    bool                    m_valid;
    MythRenderD3D9         *m_render;
    IDirect3DVertexBuffer9 *m_vertexbuffer;
    IDirect3DTexture9      *m_texture;
    IDirect3DSurface9      *m_surface;
};

class MythRenderD3D9
{
  public:
    MythRenderD3D9();
    ~MythRenderD3D9();

    bool Create(QSize size, HWND window);
    bool Test(bool &reset);

    bool ClearBuffer(void);
    bool Begin(void);
    bool End(void);
    bool StretchRect(IDirect3DTexture9 *texture, IDirect3DSurface9 *surface);
    bool DrawTexturedQuad(IDirect3DVertexBuffer9 *vertexbuffer);
    void DrawRect(const QRect &rect,  const QColor &color);
    bool Present(HWND win);
    bool HardwareYUVConversion(void)
        { return m_videosurface_fmt == (D3DFORMAT)MAKEFOURCC('Y','V','1','2'); }
    QRect GetRect(IDirect3DVertexBuffer9 *vertexbuffer);
    bool SetRenderTarget(IDirect3DTexture9 *texture);

    IDirect3DTexture9*      CreateTexture(const QSize &size);
    void                    DeleteTexture(IDirect3DTexture9* texture);

    IDirect3DVertexBuffer9* CreateVertexBuffer(IDirect3DTexture9* texture = NULL);
    bool                    UpdateVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer,
                                               const QRect &dvr, const QRect &vr,
                                               int alpha = 255, bool video = false);
    void                    DeleteVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer);

    IDirect3DSurface9*      CreateSurface(const QSize &size, bool video = false);
    bool                    UpdateSurface(IDirect3DSurface9* surface,
                                          const MythImage *image);
    void                    DeleteSurface(IDirect3DSurface9* surface);
    uint8_t*                GetBuffer(IDirect3DSurface9* surface, uint &pitch);
    void                    ReleaseBuffer(IDirect3DSurface9* surface);

  private:
    bool                    FormatSupported(D3DFORMAT surface, D3DFORMAT adaptor);
    bool                    SetTexture(IDirect3DTexture9 *texture, int num = 0);
    void                    DeleteTextures(void);
    void                    DeleteVertexBuffers(void);
    void                    DeleteSurfaces(void);
    void                    Init2DState(void);
    void                    EnableBlending(bool enable);
    void                    MultiTexturing(bool enable,
                                           IDirect3DTexture9 *texture = NULL);
    void                    SetTextureVertices(bool enable);

  private:
    QMap<IDirect3DTexture9*, QSize>                    m_textures;
    QMap<IDirect3DVertexBuffer9*, MythD3DVertexBuffer> m_vertexbuffers;
    QMap<IDirect3DSurface9*, MythD3DSurface>           m_surfaces;

    IDirect3D9             *m_d3d;
    IDirect3DDevice9       *m_d3dDevice;
    D3DFORMAT               m_adaptor_fmt;
    D3DFORMAT               m_videosurface_fmt;
    D3DFORMAT               m_surface_fmt;
    D3DFORMAT               m_texture_fmt;
    IDirect3DVertexBuffer9 *m_rect_vertexbuffer;
    IDirect3DSurface9      *m_default_surface;
    IDirect3DSurface9      *m_current_surface;

    QMutex                  m_lock;
    bool                    m_blend;
    bool                    m_multi_texturing;
    bool                    m_texture_vertices;
};

#endif // MYTHRENDER_D3D9_H
