#ifndef MYTHRENDER_D3D9_H
#define MYTHRENDER_D3D9_H

#include <QMap>

#include "libmythbase/compat.h"
#include <d3d9.h>

#include "mythimage.h"
#include "mythuiexp.h"
#include "mythrender_base.h"

#ifdef USING_DXVA2
#if defined (_WIN32)
#define CINTERFACE
#define COBJMACROS
#endif
#include "dxva2api.h"
#else
using IDirect3DDeviceManager9 = void*;
#endif

class MythRenderD3D9;

class MUI_PUBLIC D3D9Image
{
  public:
    D3D9Image(MythRenderD3D9 *render, QSize size, bool video = false);
    ~D3D9Image();

    bool     IsValid(void) const { return m_valid; }
    QSize    GetSize(void) const { return m_size;  }
    int      GetDataSize(void) const { return m_size.width() * m_size.height() * 4; }
    bool     SetAsRenderTarget(void);
    bool     UpdateImage(IDirect3DSurface9 *surface);
    bool     UpdateImage(const MythImage *img);
    bool     UpdateVertices(const QRect &dvr, const QRect &vr, int alpha = 255,
                            bool video = false);
    bool     Draw(void);
    uint8_t* GetBuffer(bool &hardware_conv, uint &pitch);
    void     ReleaseBuffer(void);
    QRect    GetRect(void);

  private:
    QSize                   m_size;
    bool                    m_valid        {false};
    MythRenderD3D9         *m_render       {nullptr};
    IDirect3DVertexBuffer9 *m_vertexbuffer {nullptr};
    IDirect3DTexture9      *m_texture      {nullptr};
    IDirect3DSurface9      *m_surface      {nullptr};
};

class MUI_PUBLIC D3D9Locker
{
  public:
    explicit D3D9Locker(MythRenderD3D9 *render)
        : m_render(render) {}
   ~D3D9Locker();
    IDirect3DDevice9* Acquire(void);
  private:
    MythRenderD3D9 *m_render {nullptr};
};

class MythD3DVertexBuffer
{
  public:
    explicit MythD3DVertexBuffer(IDirect3DTexture9* tex = nullptr) :
        m_dest(QRect(QPoint(0,0),QSize(0,0))),
        m_src(QRect(QPoint(0,0),QSize(0,0))), m_texture(tex)
    {
    }

    uint32_t           m_color {0xFFFFFFFF};
    QRect              m_dest;
    QRect              m_src;
    IDirect3DTexture9 *m_texture;
};

class MythD3DSurface
{
  public:
    MythD3DSurface(QSize size = QSize(0,0), D3DFORMAT fmt = D3DFMT_UNKNOWN) :
        m_size(size), m_fmt(fmt)
    {
    }

    QSize     m_size;
    D3DFORMAT m_fmt;
};

class MUI_PUBLIC MythRenderD3D9 : public MythRender
{
  public:
    static void* ResolveAddress(const char* lib, const char* proc);

    MythRenderD3D9() : MythRender(kRenderDirect3D9) {}

    bool Create(QSize size, HWND window);
    bool Test(bool &reset);

    bool ClearBuffer(void);
    bool Begin(void);
    bool End(void);
    void CopyFrame(void* surface, D3D9Image *img);
    bool StretchRect(IDirect3DTexture9 *texture, IDirect3DSurface9 *surface,
                     bool known_surface = true);
    bool DrawTexturedQuad(IDirect3DVertexBuffer9 *vertexbuffer);
    void DrawRect(const QRect &rect,  const QColor &color, int alpha);
    bool Present(HWND win);
    bool HardwareYUVConversion(void);
    QRect GetRect(IDirect3DVertexBuffer9 *vertexbuffer);
    bool SetRenderTarget(IDirect3DTexture9 *texture);

    IDirect3DTexture9*      CreateTexture(const QSize &size);
    void                    DeleteTexture(IDirect3DTexture9* texture);

    IDirect3DVertexBuffer9* CreateVertexBuffer(IDirect3DTexture9* texture = nullptr);
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
    virtual ~MythRenderD3D9();
    bool                    SetTexture(IDirect3DDevice9* dev,
                                       IDirect3DTexture9 *texture,
                                       int num = 0);
    void                    DeleteTextures(void);
    void                    DeleteVertexBuffers(void);
    void                    DeleteSurfaces(void);
    void                    Init2DState(void);
    void                    EnableBlending(IDirect3DDevice9* dev, bool enable);
    void                    MultiTexturing(IDirect3DDevice9* dev, bool enable,
                                           IDirect3DTexture9 *texture = nullptr);
    void                    SetTextureVertices(IDirect3DDevice9* dev, bool enable);

  private:
    QMap<IDirect3DTexture9*, QSize>                    m_textures;
    QMap<IDirect3DVertexBuffer9*, MythD3DVertexBuffer> m_vertexbuffers;
    QMap<IDirect3DSurface9*, MythD3DSurface>           m_surfaces;

    IDirect3D9             *m_d3d                 {nullptr};
    IDirect3DDevice9       *m_rootD3DDevice       {nullptr};
    D3DFORMAT               m_adaptor_fmt         {D3DFMT_UNKNOWN};
    D3DFORMAT               m_videosurface_fmt    {D3DFMT_UNKNOWN};
    D3DFORMAT               m_surface_fmt         {D3DFMT_UNKNOWN};
    D3DFORMAT               m_texture_fmt         {D3DFMT_UNKNOWN};
    IDirect3DVertexBuffer9 *m_rect_vertexbuffer   {nullptr};
    IDirect3DSurface9      *m_default_surface     {nullptr};
    IDirect3DSurface9      *m_current_surface     {nullptr};

    QRecursiveMutex         m_lock;
    bool                    m_blend               {true};
    bool                    m_multi_texturing     {true};
    bool                    m_texture_vertices    {true};

  public:
    IDirect3DDevice9* AcquireDevice(void);
    void              ReleaseDevice(void);
    IDirect3DDeviceManager9* GetDeviceManager(void) { return m_deviceManager; }

  private:
    void CreateDeviceManager(void);
    void DestroyDeviceManager(void);

  private:
    IDirect3DDeviceManager9 *m_deviceManager      {nullptr};
    HANDLE                   m_deviceHandle       {nullptr};
    uint                     m_deviceManagerToken {0};
};

#endif // MYTHRENDER_D3D9_H
