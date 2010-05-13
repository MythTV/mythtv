// -*- Mode: c++ -*-

#include <map>
#include <iostream>
#include <algorithm>
using namespace std;

#define _WIN32_WINNT 0x500
#include "mythverbose.h"
#include "videoout_d3d.h"
#include "osd.h"
#include "osdsurface.h"
#include "filtermanager.h"
#include "fourcc.h"
#include "videodisplayprofile.h"
#include "mythmainwindow.h"
#include "myth_imgconvert.h"
#include "NuppelVideoPlayer.h"

#include "mmsystem.h"
#include "tv.h"

#undef UNICODE

extern "C" {
#include "avcodec.h"
}

class MythD3DVertexBuffer
{
  public:
    MythD3DVertexBuffer(IDirect3DTexture9* tex = NULL) :
        m_display_video_rect(QRect(QPoint(0,0),QSize(0,0))),
        m_video_rect(QRect(QPoint(0,0),QSize(0,0))),
        m_texture(tex)
    {
    }

    QRect              m_display_video_rect;
    QRect              m_video_rect;
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

class D3D9PrivateContext
{
  public:
    map<IDirect3DTexture9*, QSize>                    m_textures;
    map<IDirect3DVertexBuffer9*, MythD3DVertexBuffer> m_vertexbuffers;
    map<IDirect3DSurface9*, MythD3DSurface>           m_surfaces;
};

typedef struct
{
    FLOAT       x;
    FLOAT       y;
    FLOAT       z;
    FLOAT       rhw;
    D3DCOLOR    diffuse;
    FLOAT       t1u;
    FLOAT       t1v;
    FLOAT       t2u;
    FLOAT       t2v;
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1|D3DFVF_TEX2)
#define D3DLOC QString("D3D9Context: ")
#define D3DERR QString("D3D9Context Error: ")

D3D9Context::D3D9Context(void)
  : m_priv(new D3D9PrivateContext()),
    m_d3d(NULL), m_d3dDevice(NULL),
    m_adaptor_fmt(D3DFMT_UNKNOWN),
    m_videosurface_fmt((D3DFORMAT)MAKEFOURCC('Y','V','1','2')),
    m_alphasurface_fmt(D3DFMT_A8R8G8B8), m_alphatexture_fmt(D3DFMT_A8R8G8B8),
    m_lock(QMutex::Recursive)
{
}

D3D9Context::~D3D9Context(void)
{
    QMutexLocker locker(&m_lock);

    if (m_priv)
    {
        VERBOSE(VB_PLAYBACK, D3DLOC + "Deleting D3D9 resources.");
        DeleteTextures();
        DeleteVertexBuffers();
        DeleteSurfaces();
        delete m_priv;
    }

    if (m_d3dDevice)
    {
        VERBOSE(VB_PLAYBACK, D3DLOC + "Deleting D3D9 device.");
        m_d3dDevice->Release();
    }

    if (m_d3d)
    {
        VERBOSE(VB_PLAYBACK, D3DLOC + "Deleting D3D9.");
        m_d3d->Release();
    }
}

bool D3D9Context::FormatSupported(D3DFORMAT surface, D3DFORMAT adaptor)
{
    if (!m_d3d)
        return false;

    HRESULT hr = m_d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                          adaptor, 0, D3DRTYPE_SURFACE, surface);
    if (SUCCEEDED(hr))
    {
        hr = m_d3d->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                                D3DDEVTYPE_HAL, surface, adaptor);
        if (SUCCEEDED(hr))
            return true;
    }
    return false;
}

static const QString toString(D3DFORMAT fmt)
{
    QString res = "Unknown";
    switch (fmt)
    {
        case D3DFMT_A8:
            return "A8";
        case D3DFMT_A8R8G8B8:
            return "A8R8G8B8";
        case D3DFMT_X8R8G8B8:
            return "X8R8G8B8";
        default:
            return res;
    }
    return res;
}

bool D3D9Context::Create(QSize size, HWND window)
{
    QMutexLocker locker(&m_lock);

    typedef LPDIRECT3D9 (WINAPI *LPFND3DC)(UINT SDKVersion);
    static  HINSTANCE hD3DLib            = NULL;
    static  LPFND3DC  OurDirect3DCreate9 = NULL;
    D3DCAPS9 d3dCaps;

    if (!hD3DLib)
    {
        hD3DLib = LoadLibrary(TEXT("D3D9.DLL"));
        if (!hD3DLib)
            VERBOSE(VB_IMPORTANT, D3DERR + "Cannot load 'd3d9.dll'.");
    }

    if (hD3DLib && !OurDirect3DCreate9)
    {
        OurDirect3DCreate9 = (LPFND3DC) GetProcAddress(
            hD3DLib, TEXT("Direct3DCreate9"));
        if (!OurDirect3DCreate9)
            VERBOSE(VB_IMPORTANT, D3DERR + "Cannot locate reference to "
                                           "Direct3DCreate9 ABI in DLL");
    }

    if (!(hD3DLib && OurDirect3DCreate9))
        return false;

    m_d3d = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!m_d3d)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not create Direct3D9 instance.");
        return false;
    }

    ZeroMemory(&d3dCaps, sizeof(d3dCaps));
    if (D3D_OK != m_d3d->GetDeviceCaps(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not read adapter capabilities.");
        return false;
    }

    D3DDISPLAYMODE d3ddm;
    if (D3D_OK != m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not read adapter display mode.");
        return false;
    }

    // TODO - check adaptor format is reasonable...
    m_adaptor_fmt = d3ddm.Format;
    bool default_ok = FormatSupported(m_videosurface_fmt, m_adaptor_fmt);
    if (!default_ok)
        m_videosurface_fmt = m_adaptor_fmt;

    VERBOSE(VB_PLAYBACK, D3DLOC +
        QString("Default Adaptor Format %1 - Hardware YV12 to RGB %2 ")
            .arg(toString(m_adaptor_fmt))
            .arg(default_ok ? "supported" : "unsupported"));

    if (FormatSupported(D3DFMT_A8, m_adaptor_fmt))
        m_alphasurface_fmt = D3DFMT_A8;
    VERBOSE(VB_PLAYBACK, D3DLOC + QString("Using %1 alpha surface format.")
                                          .arg(toString(m_alphasurface_fmt)));

    // TODO - try alternative adaptor formats if necessary

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp.BackBufferFormat       = m_adaptor_fmt;
    d3dpp.hDeviceWindow          = window;
    d3dpp.Windowed               = TRUE;
    d3dpp.BackBufferWidth        = size.width();
    d3dpp.BackBufferHeight       = size.height();
    d3dpp.BackBufferCount        = 1;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    if (D3D_OK != m_d3d->CreateDevice(D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL, d3dpp.hDeviceWindow,
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                      &d3dpp, &m_d3dDevice))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not create the D3D device.");
        return false;
    }

    VERBOSE(VB_PLAYBACK, D3DLOC +
               QString("Hardware YV12 to RGB conversion %1.")
               .arg(m_videosurface_fmt != (D3DFORMAT)MAKEFOURCC('Y','V','1','2') ?
                 "unavailable" : "enabled"));

    static bool debugged = false;
    if (!debugged)
    {
        debugged = true;
        D3DADAPTER_IDENTIFIER9 ident;
        if (D3D_OK == m_d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident))
        {
            VERBOSE(VB_PLAYBACK, D3DLOC + QString("Device: %1")
                    .arg(ident.Description));
            VERBOSE(VB_PLAYBACK, D3DLOC + QString("Driver: %1.%2.%3.%4")
                    .arg(HIWORD(ident.DriverVersion.HighPart))
                    .arg(LOWORD(ident.DriverVersion.HighPart))
                    .arg(HIWORD(ident.DriverVersion.LowPart))
                    .arg(LOWORD(ident.DriverVersion.LowPart)));
        }
    }

    Init2DState();
    return true;
}

bool D3D9Context::Test(bool &reset)
{
    QMutexLocker locker(&m_lock);
    bool result = true;
    HRESULT hr = m_d3dDevice->TestCooperativeLevel();
    if (FAILED(hr))
    {
        switch (hr)
        {
                case D3DERR_DEVICENOTRESET:
                    VERBOSE(VB_IMPORTANT, D3DLOC +
                            "The device was lost and needs to be reset.");
                    result  = false;
                    reset  |= true;
                    break;

                case D3DERR_DEVICELOST:
                    VERBOSE(VB_IMPORTANT, D3DLOC +
                            "The device has been lost and cannot be reset "
                            "at this time.");
                    result = false;
                    break;

                case D3DERR_DRIVERINTERNALERROR:
                    VERBOSE(VB_IMPORTANT, D3DERR +
                            "Internal driver error. "
                            "Please shut down the application.");
                    result = false;
                    break;

                default:
                    VERBOSE(VB_IMPORTANT, D3DERR +
                            "TestCooperativeLevel() failed.");
                    result = false;
        }
    }
    return result;
}

bool D3D9Context::ClearBuffer(void)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET,
                                    D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Clear() failed.");
        return false;
    }
    return true;
}

bool D3D9Context::Begin(void)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->BeginScene();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "BeginScene() failed.");
        return false;
    }
    return true;
}

bool D3D9Context::End(void)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->EndScene();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "EndScene() failed.");
        return false;
    }
    return true;
}

bool D3D9Context::StretchRect(IDirect3DTexture9 *texture,
                              IDirect3DSurface9 *surface)
{
    if (!m_priv->m_textures.count(texture) ||
        !m_priv->m_surfaces.count(surface))
        return false;

    QMutexLocker locker(&m_lock);
    LPDIRECT3DSURFACE9 d3ddest;
    HRESULT hr = texture->GetSurfaceLevel(0, &d3ddest);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "GetSurfaceLevel() failed");
        return false;
    }

    hr = m_d3dDevice->StretchRect(surface, NULL, d3ddest,
                                  NULL, D3DTEXF_POINT);
    d3ddest->Release();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "StretchRect() failed");
        return false;
    }
    return true;
}

bool D3D9Context::DrawBorder(IDirect3DVertexBuffer9 *vertexbuffer)
{
    if (!m_priv->m_vertexbuffers.count(vertexbuffer))
        return false;

    QMutexLocker locker(&m_lock);

    QRect old_dvr = m_priv->m_vertexbuffers[vertexbuffer].m_display_video_rect;
    QRect vr      = m_priv->m_vertexbuffers[vertexbuffer].m_video_rect;
    QRect new_dvr = old_dvr.adjusted(-10,-10,10,10);

    SetTexture(NULL, 0);
    UpdateVertexBuffer(vertexbuffer, new_dvr, vr, true);
    m_d3dDevice->SetStreamSource(0, vertexbuffer, 0, sizeof(CUSTOMVERTEX));
    m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    UpdateVertexBuffer(vertexbuffer, old_dvr, vr);

    return true;
}

bool D3D9Context::DrawTexturedQuad(IDirect3DVertexBuffer9 *vertexbuffer,
                                   IDirect3DTexture9 *alpha_tex)
{
    if (!m_priv->m_vertexbuffers.count(vertexbuffer))
        return false;

    QMutexLocker locker(&m_lock);

    IDirect3DTexture9 *texture = m_priv->m_vertexbuffers[vertexbuffer].m_texture;

    if (texture && !SetTexture(texture))
        return false;

    if (alpha_tex)
    {
        EnableBlending(true);
        MultiTexturing(alpha_tex);
    }

    HRESULT hr = m_d3dDevice->SetStreamSource(0, vertexbuffer,
                                              0, sizeof(CUSTOMVERTEX));
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetStreamSource() failed");
        return false;
    }

    hr = m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "DrawPrimitive() failed");
        return false;
    }

    if (alpha_tex)
    {
        DefaultTexturing();
        EnableBlending(false);
    }
    return true;
}

void D3D9Context::DefaultTexturing(void)
{
    if (!m_d3dDevice)
        return;

    m_d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    m_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
    m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
    SetTexture(NULL, 1);
}

void D3D9Context::MultiTexturing(IDirect3DTexture9 *texture)
{
    if (!m_d3dDevice)
        return;

    SetTexture(texture, 1);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
    m_d3dDevice->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);
    m_d3dDevice->SetTextureStageState(2, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
}

bool D3D9Context::Present(HWND win)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->Present(NULL, NULL, win, NULL);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Present() failed)");
        return false;
    }
    return true;
}

bool D3D9Context::SetTexture(IDirect3DTexture9 *texture, int num)
{
    HRESULT hr = m_d3dDevice->SetTexture(num, (LPDIRECT3DBASETEXTURE9)texture);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetTexture() failed");
        return false;
    }
    return true;
}

IDirect3DTexture9* D3D9Context::CreateTexture(QSize size, bool alpha)
{
    QMutexLocker locker(&m_lock);
    IDirect3DTexture9* temp_texture = NULL;

    D3DFORMAT format = alpha ? m_alphatexture_fmt : m_adaptor_fmt;
    HRESULT hr = m_d3dDevice->CreateTexture(
                    size.width(),  size.height(), 1, D3DUSAGE_RENDERTARGET,
                    format, D3DPOOL_DEFAULT, &temp_texture, NULL);

    if (FAILED(hr) || !temp_texture)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create texture.");
        return NULL;
    }

    VERBOSE(VB_PLAYBACK, D3DLOC + QString("Created texture (%1x%2)")
                .arg(size.width()).arg(size.height()));
    m_priv->m_textures[temp_texture] = size;;
    return temp_texture;
}

void D3D9Context::DeleteTextures(void)
{
    map<IDirect3DTexture9*,QSize>::iterator it;
    for (it = m_priv->m_textures.begin(); it != m_priv->m_textures.end(); it++)
        it->first->Release();
    m_priv->m_textures.clear();
}

void D3D9Context::DeleteTexture(IDirect3DTexture9* texture)
{
    QMutexLocker locker(&m_lock);
    if (m_priv->m_textures.count(texture))
    {
        texture->Release();
        m_priv->m_textures.erase(texture);
    }
}

IDirect3DSurface9* D3D9Context::CreateSurface(QSize size, bool alpha)
{
    QMutexLocker locker(&m_lock);
    IDirect3DSurface9* temp_surface = NULL;

    D3DFORMAT format = alpha ? m_alphasurface_fmt : m_videosurface_fmt;
    HRESULT hr = m_d3dDevice->CreateOffscreenPlainSurface(
                    size.width(), size.height(), format,
                    D3DPOOL_DEFAULT, &temp_surface, NULL);

    if (FAILED(hr)|| !temp_surface)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create surface.");
        return NULL;
    }

    VERBOSE(VB_PLAYBACK, D3DLOC + QString("Created surface (%1x%2)")
                .arg(size.width()).arg(size.height()));
    m_priv->m_surfaces[temp_surface] = MythD3DSurface(size, format);
    m_d3dDevice->ColorFill(temp_surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

    return temp_surface;
}

bool D3D9Context::UpdateSurface(IDirect3DSurface9 *surface,
                                VideoFrame *buffer,
                                const unsigned char *alpha)
{
    if (!surface || !buffer || !m_priv->m_surfaces.count(surface))
        return false;

    if (m_priv->m_surfaces[surface].m_size.width()  != buffer->width ||
        m_priv->m_surfaces[surface].m_size.height() != buffer->height)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Frame size does not equal "
                                       "surface size.");
        return false;
    }

    D3DLOCKED_RECT d3drect;
    HRESULT hr = surface->LockRect(&d3drect, NULL, 0);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to lock picture surface.");
        return false;
    }

    D3DFORMAT format = m_priv->m_surfaces[surface].m_fmt;
    if (format == D3DFMT_A8R8G8B8 && alpha)
    {
        int i,j;
        uint8_t *dst = (uint8_t*)d3drect.pBits + 3;
        uint8_t *src = (uint8_t*)alpha;
        uint8_t  ext = d3drect.Pitch - (buffer->width << 2);
        for (i = 0; i < buffer->height; i++)
        {
            for (j = 0; j < buffer->width; j++)
            {
                *(dst) = *(src++);
                dst += 4;
            }
            dst += ext;
        }
    }
    else if (format == D3DFMT_A8 && alpha)
    {
        int i;
        uint8_t *dst = (uint8_t*)d3drect.pBits;
        uint8_t *src = (uint8_t*)alpha;
        for (i = 0; i < buffer->height; i++)
        {
            memcpy(dst, src, buffer->width);
            dst += d3drect.Pitch;
            src += buffer->width;
        }
    }
    else if (format == (D3DFORMAT)MAKEFOURCC('Y','V','1','2'))
    {
        int i;
        uint8_t *dst      = (uint8_t*)d3drect.pBits;
        uint8_t *src      = buffer->buf;
        int chroma_width  = buffer->width >> 1;
        int chroma_height = buffer->height >> 1;
        int chroma_pitch  = d3drect.Pitch >> 1;

        for (i = 0; i < buffer->height; i++)
        {
            memcpy(dst, src, buffer->width);
            dst += d3drect.Pitch;
            src += buffer->width;
        }

        dst = (uint8_t*)d3drect.pBits +  (buffer->height * d3drect.Pitch);
        src = buffer->buf + (buffer->height * buffer->width * 5/4);

        for (i = 0; i < chroma_height; i++)
        {
            memcpy(dst, src, chroma_width);
            dst += chroma_pitch;
            src += chroma_width;
        }

        dst = (uint8_t*)d3drect.pBits + (buffer->height * d3drect.Pitch * 5/4);
        src = buffer->buf + (buffer->height * buffer->width);

        for (i = 0; i < chroma_height; i++)
        {
            memcpy(dst, src, chroma_width);
            dst += chroma_pitch;
            src += chroma_width;
        }
    }
    else if (format == D3DFMT_A8R8G8B8 || format == D3DFMT_X8R8G8B8)
    {
        AVPicture image_in, image_out;
        avpicture_fill(&image_out, (uint8_t*) d3drect.pBits,
                       PIX_FMT_RGB32, buffer->width, buffer->height);
        image_out.linesize[0] = d3drect.Pitch;
        avpicture_fill(&image_in, buffer->buf,
                       PIX_FMT_YUV420P, buffer->width, buffer->height);

        myth_sws_img_convert(&image_out, PIX_FMT_RGB32, &image_in,
                             PIX_FMT_YUV420P, buffer->width, buffer->height);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Surface format not supported.");
    }

    hr = surface->UnlockRect();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to unlock picture surface.");
        return false;
    }

    return true;
}

void D3D9Context::DeleteSurfaces(void)
{
    map<IDirect3DSurface9*, MythD3DSurface>::iterator it;
    for (it = m_priv->m_surfaces.begin(); it != m_priv->m_surfaces.end(); it++)
        it->first->Release();
    m_priv->m_surfaces.clear();
}

void D3D9Context::DeleteSurface(IDirect3DSurface9 *surface)
{
    QMutexLocker locker(&m_lock);
    if (m_priv->m_surfaces.count(surface))
    {
        surface->Release();
        m_priv->m_surfaces.erase(surface);
    }
}

IDirect3DVertexBuffer9* D3D9Context::CreateVertexBuffer(IDirect3DTexture9* texture)
{
    QMutexLocker locker(&m_lock);

    if (texture && !m_priv->m_textures.count(texture))
        return false;

    IDirect3DVertexBuffer9* temp_vbuf = NULL;
    HRESULT hr = m_d3dDevice->CreateVertexBuffer(
        sizeof(CUSTOMVERTEX)*4, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
        D3DFVF_CUSTOMVERTEX,    D3DPOOL_DEFAULT,
        &temp_vbuf,             NULL);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create vertex buffer");
        return false;
    }

    VERBOSE(VB_PLAYBACK, D3DLOC + "Created vertex buffer.");
    m_priv->m_vertexbuffers[temp_vbuf] = MythD3DVertexBuffer(texture);

    return temp_vbuf;
}

void D3D9Context::DeleteVertexBuffers(void)
{
    map<IDirect3DVertexBuffer9*,MythD3DVertexBuffer>::iterator it;
    for (it = m_priv->m_vertexbuffers.begin();
         it != m_priv->m_vertexbuffers.end(); it++)
    {
        it->first->Release();
    }
    m_priv->m_vertexbuffers.clear();
}

void D3D9Context::DeleteVertexBuffer(IDirect3DVertexBuffer9 *vertexbuffer)
{
    QMutexLocker locker(&m_lock);
    if (m_priv->m_vertexbuffers.count(vertexbuffer))
    {
        vertexbuffer->Release();
        m_priv->m_vertexbuffers.erase(vertexbuffer);
    }
}

bool D3D9Context::UpdateVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer,
                                    QRect dvr, QRect vr, bool border)
{
    if (!m_priv->m_vertexbuffers.count(vertexbuffer))
        return false;

    MythD3DVertexBuffer mythvb = m_priv->m_vertexbuffers[vertexbuffer];
    D3DCOLOR color = border ? D3DCOLOR_ARGB(255, 128,   0,   0) :
                              D3DCOLOR_ARGB(255, 255, 255, 255);

    if (dvr == mythvb.m_display_video_rect &&
        vr  == mythvb.m_video_rect)
        return true;

    QSize norm = vr.size();
    if (mythvb.m_texture)
        norm = m_priv->m_textures[mythvb.m_texture];

    QMutexLocker locker(&m_lock);
    CUSTOMVERTEX *p_vertices;
    HRESULT hr = vertexbuffer->Lock(0, 0, (VOID **)(&p_vertices),
                                    D3DLOCK_DISCARD);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to lock vertex buffer.");
        return false;
    }

    p_vertices[0].x       = (float)dvr.left();
    p_vertices[0].y       = (float)dvr.top();
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = color;
    p_vertices[0].rhw     = 1.0f;
    p_vertices[0].t1u     = (float)vr.left() / (float)norm.width();
    p_vertices[0].t1v     = (float)vr.top() / (float)norm.height();

    p_vertices[1].x       = (float)dvr.right();
    p_vertices[1].y       = (float)dvr.top();
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = color;
    p_vertices[1].rhw     = 1.0f;
    p_vertices[1].t1u     = (float)vr.right() / (float)norm.width();
    p_vertices[1].t1v     = (float)vr.top() / (float)norm.height();

    p_vertices[2].x       = (float)dvr.right();
    p_vertices[2].y       = (float)(dvr.top() + dvr.height());
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = color;
    p_vertices[2].rhw     = 1.0f;
    p_vertices[2].t1u     = (float)vr.right() / (float)norm.width();
    p_vertices[2].t1v     = (float)(vr.top() + vr.height()) /
                            (float)norm.height();

    p_vertices[3].x       = (float)dvr.left();
    p_vertices[3].y       = (float)(dvr.top() + dvr.height());
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = color;
    p_vertices[3].rhw     = 1.0f;
    p_vertices[3].t1u     = (float)vr.left() / (float)norm.width();
    p_vertices[3].t1v     = (float)(vr.top() + vr.height()) /
                            (float)norm.height();

    p_vertices[0].t2u     = p_vertices[0].t1u;
    p_vertices[0].t2v     = p_vertices[0].t1v;
    p_vertices[1].t2u     = p_vertices[1].t1u;
    p_vertices[1].t2v     = p_vertices[1].t1v;
    p_vertices[2].t2u     = p_vertices[2].t1u;
    p_vertices[2].t2v     = p_vertices[2].t1v;
    p_vertices[3].t2u     = p_vertices[3].t1u;
    p_vertices[3].t2v     = p_vertices[3].t1v;

    hr = vertexbuffer->Unlock();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to unlock vertex buffer");
        return false;
    }

    m_priv->m_vertexbuffers[vertexbuffer].m_display_video_rect = dvr;
    m_priv->m_vertexbuffers[vertexbuffer].m_video_rect = vr;
    return true;
}

void D3D9Context::Init2DState(void)
{
    if (!m_d3dDevice)
        return;

    m_d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_d3dDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_d3dDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_d3dDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    m_d3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));
    m_d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_d3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    m_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_d3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);
    m_d3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    m_d3dDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    m_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    m_d3dDevice->SetVertexShader(NULL);
    m_d3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);

    DefaultTexturing();
    EnableBlending(false);
}

void D3D9Context::EnableBlending(bool enable)
{
    if (!m_d3dDevice)
        return;
    m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

D3D9Video::D3D9Video(D3D9Context *ctx, QSize size, bool alpha)
  : m_size(size), m_valid(false), m_ctx(ctx), m_vertexbuffer(NULL),
    m_texture(NULL), m_surface(NULL),
    m_alpha_texture(NULL), m_alpha_surface(NULL)
{
    if (m_ctx)
    {
        m_texture      = m_ctx->CreateTexture(m_size);
        m_vertexbuffer = m_ctx->CreateVertexBuffer(m_texture);
        m_surface      = m_ctx->CreateSurface(m_size);
        if (alpha)
        {
            m_alpha_texture = m_ctx->CreateTexture(m_size, true);
            m_alpha_surface = m_ctx->CreateSurface(m_size, true);
        }
    }
    m_valid = m_texture && m_vertexbuffer && m_surface;

    if (alpha)
        m_valid &= (m_alpha_texture && m_alpha_surface);
}

D3D9Video::~D3D9Video()
{
    if (!m_ctx)
        return;

    if (m_texture)
        m_ctx->DeleteTexture(m_texture);
    if (m_vertexbuffer)
        m_ctx->DeleteVertexBuffer(m_vertexbuffer);
    if (m_surface)
        m_ctx->DeleteSurface(m_surface);
    if (m_alpha_texture)
        m_ctx->DeleteTexture(m_alpha_texture);
    if (m_alpha_surface)
        m_ctx->DeleteSurface(m_alpha_surface);
}

bool D3D9Video::UpdateVideo(VideoFrame *frame, const unsigned char *alpha)
{
    bool result = true;
    if (m_valid)
    {
        result &= m_ctx->UpdateSurface(m_surface, frame);
        result &= m_ctx->StretchRect(m_texture, m_surface);
        if (alpha && m_alpha_texture && m_alpha_surface)
        {
            result &= m_ctx->UpdateSurface(m_alpha_surface, frame, alpha);
            result &= m_ctx->StretchRect(m_alpha_texture, m_alpha_surface);
        }
    }
    return m_valid && result;
}

bool D3D9Video::UpdateVertices(QRect dvr, QRect vr)
{
    if (m_valid)
        return m_ctx->UpdateVertexBuffer(m_vertexbuffer, dvr, vr);
    return m_valid;
}

bool D3D9Video::Draw(bool border)
{
    if (m_valid)
    {
        if (border)
            m_ctx->DrawBorder(m_vertexbuffer);
        return m_ctx->DrawTexturedQuad(m_vertexbuffer, m_alpha_texture);
    }
    return m_valid;
}

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 10;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

#define LOC      QString("VideoOutputD3D: ")
#define LOC_WARN QString("VideoOutputD3D Warning: ")
#define LOC_ERR  QString("VideoOutputD3D Error: ")

void VideoOutputD3D::GetRenderOptions(render_opts &opts,
                                      QStringList &cpudeints)
{
    opts.renderers->append("direct3d");
    opts.deints->insert("direct3d", cpudeints);
    (*opts.osds)["direct3d"].append("softblend");
    (*opts.osds)["direct3d"].append("direct3d");
    (*opts.safe_renderers)["dummy"].append("direct3d");
    (*opts.safe_renderers)["nuppel"].append("direct3d");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("direct3d");
    if (opts.decoders->contains("libmpeg2"))
        (*opts.safe_renderers)["libmpeg2"].append("direct3d");
    opts.priorities->insert("direct3d", 55);
}

VideoOutputD3D::VideoOutputD3D(void)
  : VideoOutput(),         m_lock(QMutex::Recursive),
    m_hWnd(NULL),          m_ctx(NULL),
    m_video(NULL),         m_osd(NULL),
    m_d3d_osd(false),      m_osd_ready(false),
    m_ctx_valid(false),    m_ctx_reset(false),
    m_pip_active(NULL)
{
    m_pauseFrame.buf = NULL;
}

VideoOutputD3D::~VideoOutputD3D()
{
    TearDown();
}

void VideoOutputD3D::TearDown(void)
{
    QMutexLocker locker(&m_lock);
    vbuffers.DiscardFrames(true);
    vbuffers.Reset();
    vbuffers.DeleteBuffers();
    if (m_pauseFrame.buf)
    {
        delete [] m_pauseFrame.buf;
        m_pauseFrame.buf = NULL;
    }
    DestroyContext();
}

void VideoOutputD3D::DestroyContext(void)
{
    QMutexLocker locker(&m_lock);
    m_ctx_valid = false;
    m_ctx_reset = false;

    while (!m_pips.empty())
    {
        delete *m_pips.begin();
        m_pips.erase(m_pips.begin());
    }
    m_pip_ready.clear();

    if (m_osd)
    {
        delete m_osd;
        m_osd = NULL;
        m_osd_ready = false;
    }

    if (m_video)
    {
        delete m_video;
        m_video = NULL;
    }

    if (m_ctx)
    {
        delete m_ctx;
        m_ctx = NULL;
    }
}

void VideoOutputD3D::WindowResized(const QSize &new_size)
{
    // FIXME this now requires the context to be re-created
    /*
    QMutexLocker locker(&m_lock);
    windows[0].SetDisplayVisibleRect(QRect(QPoint(0, 0), new_size));
    windows[0].SetDisplayAspect(
        ((float)new_size.width()) / new_size.height());

    MoveResize();
    */
}

bool VideoOutputD3D::InputChanged(const QSize &input_size,
                                  float        aspect,
                                  MythCodecID  av_codec_id,
                                  void        *codec_private,
                                  bool        &aspect_only)
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InputChanged(%1,%2,%3) %4")
            .arg(input_size.width()).arg(input_size.height()).arg(aspect)
            .arg(toString(av_codec_id)));

    QMutexLocker locker(&m_lock);

    if (!codec_is_std(av_codec_id))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("New video codec is not supported."));
        errorState = kError_Unknown;
        return false;
    }

    if (input_size == windows[0].GetVideoDim())
    {
        if (windows[0].GetVideoAspect() != aspect)
        {
            aspect_only = true;
            VideoAspectRatioChanged(aspect);
            MoveResize();
        }
        return true;
    }

    TearDown();
    QRect disp = windows[0].GetDisplayVisibleRect();
    if (Init(input_size.width(), input_size.height(),
             aspect, m_hWnd, disp.left(), disp.top(),
             disp.width(), disp.height(), m_hEmbedWnd))
    {
        return true;
    }

    VERBOSE(VB_IMPORTANT, LOC_ERR +
        QString("Failed to re-initialise video output."));
    errorState = kError_Unknown;

    return false;
}

bool VideoOutputD3D::SetupContext()
{
    QMutexLocker locker(&m_lock);
    DestroyContext();
    QSize size = windows[0].GetVideoDim();
    m_ctx = new D3D9Context();
    if (!(m_ctx && m_ctx->Create(windows[0].GetDisplayVisibleRect().size(),
                                 m_hWnd)))
        return false;

    m_video = new D3D9Video(m_ctx, size);
    if (!(m_video && m_video->IsValid()))
        return false;

    if (m_d3d_osd)
    {
        QRect osdbounds = GetTotalOSDBounds();
        m_osd = new D3D9Video(m_ctx, osdbounds.size(), true);
        if (!(m_osd && m_osd->IsValid()))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create Direct3d9 OSD.");
            delete m_osd;
            m_osd = NULL;
            m_d3d_osd = false;
        }
        else
        {
            m_osd->UpdateVertices(osdbounds, osdbounds);
            VERBOSE(VB_PLAYBACK, LOC + QString("Created OSD (%1x%2)")
                .arg(osdbounds.size().width()).arg(osdbounds.size().height()));
        }
    }

    VERBOSE(VB_PLAYBACK, LOC +
            "Direct3D device successfully initialized.");
    m_ctx_valid = true;
    return true;
}

DisplayInfo VideoOutputD3D::GetDisplayInfo(void)
{
    DisplayInfo ret;
    HDC hdc = GetDC(m_hWnd);
    if (hdc)
    {
        int rate = GetDeviceCaps(hdc, VREFRESH);
        if (rate > 20 && rate < 200)
        {
            // see http://support.microsoft.com/kb/2006076
            switch (rate)
            {
                case 23:  ret.rate = 41708; break; // 23.976Hz
                case 29:  ret.rate = 33367; break; // 29.970Hz
                case 47:  ret.rate = 20854; break; // 47.952Hz
                case 59:  ret.rate = 16683; break; // 59.940Hz
                case 71:  ret.rate = 13903; break; // 71.928Hz
                case 119: ret.rate = 8342;  break; // 119.880Hz
                default:  ret.rate = 1000000 / rate;
            }
        }
        int width  = GetDeviceCaps(hdc, HORZSIZE);
        int height = GetDeviceCaps(hdc, VERTSIZE);
        ret.size   = QSize((uint)width, (uint)height);
        width      = GetDeviceCaps(hdc, HORZRES);
        height     = GetDeviceCaps(hdc, VERTRES);
        ret.res    = QSize((uint)width, (uint)height);
    }
    return ret;
}

bool VideoOutputD3D::Init(int width, int height, float aspect,
                          WId winid, int winx, int winy, int winw,
                          int winh, WId embedid)
{
    QMutexLocker locker(&m_lock);
    m_hWnd      = winid;
    m_hEmbedWnd = embedid;
    windows[0].SetAllowPreviewEPG(true);

    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    if (db_vdisp_profile)
    {
        db_vdisp_profile->SetVideoRenderer("direct3d");
        if (db_vdisp_profile->GetOSDRenderer() == "direct3d")
            m_d3d_osd = true;
    }

    bool success = true;
    success &= SetupContext();
    InitDisplayMeasurements(width, height, false);

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    success &= vbuffers.CreateBuffers(windows[0].GetVideoDim().width(),
                                      windows[0].GetVideoDim().height());
    m_pauseFrame.height = vbuffers.GetScratchFrame()->height;
    m_pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    m_pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    m_pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    m_pauseFrame.buf    = new unsigned char[m_pauseFrame.size + 128];
    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    MoveResize();

    if (!success)
        TearDown();

    return success;
}

void VideoOutputD3D::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "PrepareFrame() called while IsErrored is true.");
        return;
    }

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    framesPlayed = buffer->frameNumber + 1;

    if (!m_ctx || !m_video)
        return;

    m_ctx_valid = m_ctx->Test(m_ctx_reset);
    if (m_ctx_valid)
    {
        QRect dvr = (vsz_enabled && m_d3d_osd && m_osd) ?
                        vsz_desired_display_rect :
                        windows[0].GetDisplayVideoRect();
        bool ok = m_ctx->ClearBuffer();
        if (ok)
            ok = m_video->UpdateVertices(dvr, windows[0].GetVideoRect());

        if (ok)
        {
            ok = m_ctx->Begin();
            if (ok)
            {
                m_video->Draw();
                QMap<NuppelVideoPlayer*,D3D9Video*>::iterator it = m_pips.begin();
                for (; it != m_pips.end(); ++it)
                {
                    if (m_pip_ready[it.key()])
                    {
                        bool active = m_pip_active == *it;
                       (*it)->Draw(active);
                    }
                }
                if (m_osd && m_osd_ready && m_d3d_osd)
                    m_osd->Draw();
                m_ctx->End();
            }
        }
    }
}

void VideoOutputD3D::Show(FrameScanType )
{
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Show() called while IsErrored is true.");
        return;
    }

    if (!m_ctx)
        return;

    m_ctx_valid = m_ctx->Test(m_ctx_reset);
    if (m_ctx_valid)
        m_ctx->Present(windows[0].IsEmbedding() ? m_hEmbedWnd : NULL);

    SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
}

void VideoOutputD3D::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (windows[0].IsEmbedding())
        return;

    VideoOutput::EmbedInWidget(x, y, w, h);
    m_hEmbedWnd = wid;
}

void VideoOutputD3D::StopEmbedding(void)
{
    if (!windows[0].IsEmbedding())
        return;

    VideoOutput::StopEmbedding();
}

void VideoOutputD3D::Zoom(ZoomDirection direction)
{
    QMutexLocker locker(&m_lock);
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputD3D::UpdatePauseFrame(void)
{
    QMutexLocker locker(&m_lock);
    VideoFrame *used_frame = vbuffers.head(kVideoBuffer_used);
    if (!used_frame)
        used_frame = vbuffers.GetScratchFrame();

    CopyFrame(&m_pauseFrame, used_frame);
}

void VideoOutputD3D::ProcessFrame(VideoFrame *frame, OSD *osd,
                                  FilterChain *filterList,
                                  const PIPMap &pipPlayers,
                                  FrameScanType scan)
{
    QMutexLocker locker(&m_lock);
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "ProcessFrame() called while IsErrored is true.");
        return;
    }

    bool deint_proc = m_deinterlacing && (m_deintFilter != NULL);

    bool pauseframe = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &m_pauseFrame);
        pauseframe = true;
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    bool safepauseframe = pauseframe && !IsBobDeint();
    if (deint_proc && m_deinterlaceBeforeOSD &&
       (!pauseframe || safepauseframe))
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (!windows[0].IsEmbedding())
    {
        ShowPIPs(frame, pipPlayers);
        if (osd)
            DisplayOSD(frame, osd);
    }

    if ((!pauseframe || safepauseframe) &&
        deint_proc && !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    if (m_ctx)
    {
        m_ctx_valid |= m_ctx->Test(m_ctx_reset);
        if (m_ctx_reset)
            SetupContext();
        if (m_ctx_valid)
        {
            if (m_video)
                m_video->UpdateVideo(frame);
        }
    }
}

int VideoOutputD3D::DisplayOSD(VideoFrame *frame, OSD *osd,
                               int stride, int revision)
{
    m_osd_ready = false;

    if (!m_d3d_osd)
        return VideoOutput::DisplayOSD(frame, osd, stride, revision);

    if (!(m_osd && osd))
        return -1;

    OSDSurface *surface = osd->Display();
    if (!surface)
        return -1;

    m_osd_ready = true;
    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);

    if (changed)
    {
        VideoFrame osdframe;
        osdframe.buf    = surface->yuvbuffer;
        osdframe.width  = surface->width;
        osdframe.height = surface->height;
        m_osd->UpdateVideo(&osdframe, surface->alpha);
    }
    return changed;
}

void VideoOutputD3D::ShowPIP(VideoFrame        *frame,
                             NuppelVideoPlayer *pipplayer,
                             PIPLocation        loc)
{
    if (!pipplayer)
        return;

    int pipw, piph;
    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);
    const float pipVideoAspect = pipplayer->GetVideoAspect();
    const QSize pipVideoDim    = pipplayer->GetVideoBufferSize();
    const bool  pipActive      = pipplayer->IsPIPActive();
    const bool  pipVisible     = pipplayer->IsPIPVisible();
    const uint  pipVideoWidth  = pipVideoDim.width();
    const uint  pipVideoHeight = pipVideoDim.height();

    if ((pipVideoAspect <= 0) || !pipimage ||
        !pipimage->buf || (pipimage->codec != FMT_YV12) || !pipVisible)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    QRect position = GetPIPRect(loc, pipplayer);
    QRect dvr = windows[0].GetDisplayVisibleRect();

    m_pip_ready[pipplayer] = false;
    D3D9Video *m_pip = m_pips[pipplayer];
    if (!m_pip)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Initialise PiP.");
        m_pip = new D3D9Video(m_ctx, QSize(pipVideoWidth, pipVideoHeight));
        m_pips[pipplayer] = m_pip;
        if (!m_pip->IsValid())
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }

    QSize current = m_pip->GetSize();
    if ((uint)current.width()  != pipVideoWidth ||
        (uint)current.height() != pipVideoHeight)
    {
        VERBOSE(VB_PLAYBACK, LOC + "Re-initialise PiP.");
        delete m_pip;
        m_pip = new D3D9Video(m_ctx, QSize(pipVideoWidth, pipVideoHeight));
        m_pips[pipplayer] = m_pip;
        if (!m_pip->IsValid())
        {
            pipplayer->ReleaseCurrentFrame(pipimage);
            return;
        }
    }
    m_pip->UpdateVertices(position, QRect(0, 0, pipVideoWidth, pipVideoHeight));
    m_pip->UpdateVideo(pipimage);
    m_pip_ready[pipplayer] = true;
    if (pipActive)
        m_pip_active = m_pip;

    pipplayer->ReleaseCurrentFrame(pipimage);
}

void VideoOutputD3D::RemovePIP(NuppelVideoPlayer *pipplayer)
{
    if (!m_pips.contains(pipplayer))
        return;

    QMutexLocker locker(&m_lock);

    D3D9Video *m_pip = m_pips[pipplayer];
    if (m_pip)
        delete m_pip;
    m_pip_ready.remove(pipplayer);
    m_pips.remove(pipplayer);
}

QStringList VideoOutputD3D::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    QStringList list;

    if (codec_is_std(myth_codec_id) && !getenv("NO_DIRECT3D"))
            list += "direct3d";

    return list;
}
/* vim: set expandtab tabstop=4 shiftwidth=4: */
