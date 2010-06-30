using namespace std;
#define _WIN32_WINNT 0x500

#include <QRect>
#include <QMap>
#include <QMutex>

#include "mythverbose.h"
#include "mythrender_d3d9.h"

class MythD3DVertexBuffer
{
  public:
    MythD3DVertexBuffer(IDirect3DTexture9* tex = NULL) :
        m_color(0xFFFFFFFF), m_dest(QRect(QPoint(0,0),QSize(0,0))),
        m_src(QRect(QPoint(0,0),QSize(0,0))), m_texture(tex)
    {
    }

    uint32_t           m_color;
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
} TEXTUREVERTEX;

typedef struct
{
    FLOAT       x;
    FLOAT       y;
    FLOAT       z;
    FLOAT       rhw;
    D3DCOLOR    diffuse;
} VERTEX;

D3D9Image::D3D9Image(MythRenderD3D9 *render, QSize size, bool video)
  : m_size(size), m_valid(false), m_render(render), m_vertexbuffer(NULL),
    m_texture(NULL), m_surface(NULL)
{
    if (m_render)
    {
        m_texture      = m_render->CreateTexture(m_size);
        m_vertexbuffer = m_render->CreateVertexBuffer(m_texture);
        m_surface      = m_render->CreateSurface(m_size, video);
    }
    m_valid = m_texture && m_vertexbuffer && m_surface;
}

D3D9Image::~D3D9Image()
{
    if (!m_render)
        return;

    if (m_texture)
        m_render->DeleteTexture(m_texture);
    if (m_vertexbuffer)
        m_render->DeleteVertexBuffer(m_vertexbuffer);
    if (m_surface)
        m_render->DeleteSurface(m_surface);
}

bool D3D9Image::SetAsRenderTarget(void)
{
    if (m_valid)
        return m_render->SetRenderTarget(m_texture);
    return m_valid;
}

bool D3D9Image::UpdateImage(const MythImage *img)
{
    bool result = true;
    if (m_valid)
    {
        result &= m_render->UpdateSurface(m_surface, img);
        result &= m_render->StretchRect(m_texture, m_surface);
    }
    return m_valid && result;
}

bool D3D9Image::UpdateVertices(const QRect &dvr, const QRect &vr, int alpha,
                               bool video)
{
    if (m_valid)
        return m_render->UpdateVertexBuffer(m_vertexbuffer, dvr, vr,
                                            alpha, video);
    return m_valid;
}

bool D3D9Image::Draw(void)
{
    if (m_valid)
        return m_render->DrawTexturedQuad(m_vertexbuffer);
    return m_valid;
}

uint8_t* D3D9Image::GetBuffer(bool &hardware_conv, uint &pitch)
{
    if (!m_valid)
        return NULL;

    hardware_conv = m_render->HardwareYUVConversion();
    return m_render->GetBuffer(m_surface, pitch);
}

void D3D9Image::ReleaseBuffer(void)
{
    if (!m_valid)
        return;
    m_render->ReleaseBuffer(m_surface);
    m_render->StretchRect(m_texture, m_surface);
}

QRect D3D9Image::GetRect(void)
{
    if (!m_valid)
        return QRect();
    return m_render->GetRect(m_vertexbuffer);
}

#define D3DFVF_TEXTUREVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1|D3DFVF_TEX2)
#define D3DFVF_VERTEX        (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DLOC QString("MythRenderD3D9: ")
#define D3DERR QString("MythRenderD3D9 Error: ")

MythRenderD3D9::MythRenderD3D9(void)
  : m_d3d(NULL), m_d3dDevice(NULL),
    m_adaptor_fmt(D3DFMT_UNKNOWN),
    m_videosurface_fmt((D3DFORMAT)MAKEFOURCC('Y','V','1','2')),
    m_surface_fmt(D3DFMT_A8R8G8B8), m_texture_fmt(D3DFMT_A8R8G8B8),
    m_rect_vertexbuffer(NULL), m_default_surface(NULL), m_current_surface(NULL),
    m_lock(QMutex::Recursive),
    m_blend(true), m_multi_texturing(true), m_texture_vertices(true)
{
}

MythRenderD3D9::~MythRenderD3D9(void)
{
    QMutexLocker locker(&m_lock);

    VERBOSE(VB_GENERAL, D3DLOC + "Deleting D3D9 resources.");

    if (m_rect_vertexbuffer)
        m_rect_vertexbuffer->Release();
    if (m_current_surface)
        m_current_surface->Release();
    if (m_default_surface)
        m_default_surface->Release();

    DeleteTextures();
    DeleteVertexBuffers();
    DeleteSurfaces();

    if (m_d3dDevice)
    {
        VERBOSE(VB_GENERAL, D3DLOC + "Deleting D3D9 device.");
        m_d3dDevice->Release();
    }

    if (m_d3d)
    {
        VERBOSE(VB_GENERAL, D3DLOC + "Deleting D3D9.");
        m_d3d->Release();
    }
}

bool MythRenderD3D9::FormatSupported(D3DFORMAT surface, D3DFORMAT adaptor)
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

bool MythRenderD3D9::Create(QSize size, HWND window)
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

    // TODO - check adaptor format is reasonable and try alternatives
    m_adaptor_fmt = d3ddm.Format;
    bool default_ok = FormatSupported(m_videosurface_fmt, m_adaptor_fmt);
    if (!default_ok)
        m_videosurface_fmt = m_adaptor_fmt;

    VERBOSE(VB_GENERAL, D3DLOC +
        QString("Default Adaptor Format %1 - Hardware YV12 to RGB %2 ")
            .arg(toString(m_adaptor_fmt))
            .arg(default_ok ? "supported" : "unsupported"));

    // TODO - try alternative formats if necessary
    if (!FormatSupported(m_surface_fmt, m_adaptor_fmt))
        VERBOSE(VB_IMPORTANT, D3DERR + QString("%1 surface format not supported.")
                                          .arg(toString(m_surface_fmt)));
    else
        VERBOSE(VB_GENERAL, D3DLOC + QString("Using %1 surface format.")
                                          .arg(toString(m_surface_fmt)));

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

    VERBOSE(VB_GENERAL, D3DLOC +
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
            VERBOSE(VB_GENERAL, D3DLOC + QString("Device: %1")
                    .arg(ident.Description));
            VERBOSE(VB_GENERAL, D3DLOC + QString("Driver: %1.%2.%3.%4")
                    .arg(HIWORD(ident.DriverVersion.HighPart))
                    .arg(LOWORD(ident.DriverVersion.HighPart))
                    .arg(HIWORD(ident.DriverVersion.LowPart))
                    .arg(LOWORD(ident.DriverVersion.LowPart)));
        }
    }

    Init2DState();
    return true;
}

bool MythRenderD3D9::Test(bool &reset)
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

bool MythRenderD3D9::ClearBuffer(void)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET,
                                    D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Clear() failed.");
        return false;
    }
    return true;
}

bool MythRenderD3D9::Begin(void)
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

bool MythRenderD3D9::End(void)
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

bool MythRenderD3D9::StretchRect(IDirect3DTexture9 *texture,
                              IDirect3DSurface9 *surface)
{
    if (!m_textures.contains(texture) ||
        !m_surfaces.contains(surface))
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

bool MythRenderD3D9::DrawTexturedQuad(IDirect3DVertexBuffer9 *vertexbuffer)
{
    if (!m_vertexbuffers.contains(vertexbuffer))
        return false;

    QMutexLocker locker(&m_lock);

    IDirect3DTexture9 *texture = m_vertexbuffers[vertexbuffer].m_texture;

    if (texture && !SetTexture(texture))
        return false;

    EnableBlending(true);
    SetTextureVertices(true);
    MultiTexturing(false);

    HRESULT hr = m_d3dDevice->SetStreamSource(0, vertexbuffer,
                                              0, sizeof(TEXTUREVERTEX));
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

    return true;
}

void MythRenderD3D9::DrawRect(const QRect &rect, const QColor &color)
{
    if (!m_rect_vertexbuffer)
    {
        HRESULT hr = m_d3dDevice->CreateVertexBuffer(
                sizeof(VERTEX)*4,     D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                D3DFVF_VERTEX,        D3DPOOL_DEFAULT,
                &m_rect_vertexbuffer, NULL);

        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create vertex buffer");
            return;
        }
    }

    EnableBlending(false);
    SetTextureVertices(false);
    MultiTexturing(false);
    SetTexture(NULL, 0);

    QMutexLocker locker(&m_lock);
    D3DCOLOR clr = D3DCOLOR_ARGB(color.alpha(), color.red(),
                                 color.green(), color.blue());
    VERTEX *p_vertices;
    HRESULT hr = m_rect_vertexbuffer->Lock(0, 0, (VOID **)(&p_vertices),
                                           D3DLOCK_DISCARD);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to lock vertex buffer.");
        return;
    }

    p_vertices[0].x       = (float)rect.left();
    p_vertices[0].y       = (float)rect.top();
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = clr;
    p_vertices[0].rhw     = 1.0f;
    p_vertices[1].x       = (float)(rect.left() + rect.width());
    p_vertices[1].y       = (float)rect.top();
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = clr;
    p_vertices[1].rhw     = 1.0f;
    p_vertices[2].x       = (float)(rect.left() + rect.width());
    p_vertices[2].y       = (float)(rect.top() + rect.height());
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = clr;
    p_vertices[2].rhw     = 1.0f;
    p_vertices[3].x       = (float)rect.left();
    p_vertices[3].y       = (float)(rect.top() + rect.height());
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = clr;
    p_vertices[3].rhw     = 1.0f;

    hr = m_rect_vertexbuffer->Unlock();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to unlock vertex buffer");
        return;
    }

    hr = m_d3dDevice->SetStreamSource(0, m_rect_vertexbuffer,
                                      0, sizeof(VERTEX));
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetStreamSource() failed");
        return;
    }

    hr = m_d3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "DrawPrimitive() failed");
        return;
    }
}

void MythRenderD3D9::MultiTexturing(bool enable, IDirect3DTexture9 *texture)
{
    if (!m_d3dDevice || (m_multi_texturing == enable))
        return;

    if (enable)
    {
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
    else
    {
        m_d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
        m_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        m_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
        m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
        m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        m_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        m_d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
        m_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
        SetTexture(NULL, 1);
    }
    m_multi_texturing = enable;
}

bool MythRenderD3D9::Present(HWND win)
{
    QMutexLocker locker(&m_lock);
    HRESULT hr = m_d3dDevice->Present(NULL, NULL, win, NULL);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Present() failed)");
        return false;
    }
    SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    return true;
}

QRect MythRenderD3D9::GetRect(IDirect3DVertexBuffer9 *vertexbuffer)
{
    if (!m_vertexbuffers.contains(vertexbuffer))
        return QRect();
    return m_vertexbuffers[vertexbuffer].m_dest;
}

bool MythRenderD3D9::SetRenderTarget(IDirect3DTexture9 *texture)
{
    bool ret = true;
    HRESULT hr;
    if (texture && m_textures.contains(texture))
    {
        if (!m_default_surface)
        {
            hr = m_d3dDevice->GetRenderTarget(0, &m_default_surface);
            if (FAILED(hr))
            {
                VERBOSE(VB_IMPORTANT, QString("Failed to get default surface."));
                return false;
            }
        }

        IDirect3DSurface9 *new_surface = NULL;
        hr = texture->GetSurfaceLevel(0, &new_surface);
        if (FAILED(hr))
            VERBOSE(VB_IMPORTANT, QString("Failed to get surface level."));
        else
        {
            if (m_current_surface && m_current_surface != new_surface)
                m_current_surface->Release();
            m_current_surface = new_surface;
            hr = m_d3dDevice->SetRenderTarget(0, m_current_surface);
            if (FAILED(hr))
                VERBOSE(VB_IMPORTANT, QString("Failed to set render target."));
        }
    }
    else if (!texture)
    {
        if (m_default_surface)
        {
            hr = m_d3dDevice->SetRenderTarget(0, m_default_surface);
            if (FAILED(hr))
                VERBOSE(VB_IMPORTANT, QString("Failed to set render target."));
        }
        else
            VERBOSE(VB_IMPORTANT, QString("No default surface for render target."));
    }
    else
        ret = false;
    return ret;
}

bool MythRenderD3D9::SetTexture(IDirect3DTexture9 *texture, int num)
{
    HRESULT hr = m_d3dDevice->SetTexture(num, (LPDIRECT3DBASETEXTURE9)texture);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetTexture() failed");
        return false;
    }
    return true;
}

IDirect3DTexture9* MythRenderD3D9::CreateTexture(const QSize &size)
{
    QMutexLocker locker(&m_lock);
    IDirect3DTexture9* temp_texture = NULL;

    HRESULT hr = m_d3dDevice->CreateTexture(
                    size.width(),  size.height(), 1, D3DUSAGE_RENDERTARGET,
                    m_texture_fmt, D3DPOOL_DEFAULT, &temp_texture, NULL);

    if (FAILED(hr) || !temp_texture)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create texture.");
        return NULL;
    }

    m_textures[temp_texture] = size;;
    return temp_texture;
}

void MythRenderD3D9::DeleteTextures(void)
{
    QMap<IDirect3DTexture9*,QSize>::iterator it;
    for (it = m_textures.begin(); it != m_textures.end(); ++it)
        it.key()->Release();
    m_textures.clear();
}

void MythRenderD3D9::DeleteTexture(IDirect3DTexture9* texture)
{
    QMutexLocker locker(&m_lock);
    if (m_textures.contains(texture))
    {
        texture->Release();
        m_textures.remove(texture);
    }
}

IDirect3DSurface9* MythRenderD3D9::CreateSurface(const QSize &size, bool video)
{
    QMutexLocker locker(&m_lock);
    IDirect3DSurface9* temp_surface = NULL;

    D3DFORMAT format = video ? m_videosurface_fmt : m_surface_fmt;

    HRESULT hr = m_d3dDevice->CreateOffscreenPlainSurface(
                    size.width(), size.height(), format,
                    D3DPOOL_DEFAULT, &temp_surface, NULL);

    if (FAILED(hr)|| !temp_surface)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create surface.");
        return NULL;
    }

    m_surfaces[temp_surface] = MythD3DSurface(size, format);
    m_d3dDevice->ColorFill(temp_surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

    return temp_surface;
}

bool MythRenderD3D9::UpdateSurface(IDirect3DSurface9 *surface,
                                   const MythImage *image)
{
    if (!surface || !image || !m_surfaces.contains(surface))
        return false;

    if (m_surfaces[surface].m_size.width()  != image->width() ||
        m_surfaces[surface].m_size.height() != image->height())
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Frame size does not equal "
                                       "surface size.");
        return false;
    }

    uint d3dpitch = 0;
    uint8_t *buf = GetBuffer(surface, d3dpitch);

    if (!(buf && d3dpitch))
        return false;

    D3DFORMAT format = m_surfaces[surface].m_fmt;
    if (format == D3DFMT_A8R8G8B8)
    {
        int i;
        uint pitch = image->width() << 2;
        uint8_t *dst = buf;
        uint8_t *src = (uint8_t*)image->bits();
        for (i = 0; i < image->height(); i++)
        {
            memcpy(dst, src, pitch);
            dst += d3dpitch;
            src += pitch;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Surface format not supported.");
    }

    ReleaseBuffer(surface);
    return true;
}

void MythRenderD3D9::DeleteSurfaces(void)
{
    QMap<IDirect3DSurface9*, MythD3DSurface>::iterator it;
    for (it = m_surfaces.begin(); it != m_surfaces.end(); ++it)
        it.key()->Release();
    m_surfaces.clear();
}

void MythRenderD3D9::DeleteSurface(IDirect3DSurface9 *surface)
{
    QMutexLocker locker(&m_lock);
    if (m_surfaces.contains(surface))
    {
        surface->Release();
        m_surfaces.remove(surface);
    }
}

uint8_t* MythRenderD3D9::GetBuffer(IDirect3DSurface9* surface, uint &pitch)
{
    if (!m_surfaces.contains(surface))
        return NULL;

    m_lock.lock(); // unlocked in release buffer
    D3DLOCKED_RECT d3drect;
    HRESULT hr = surface->LockRect(&d3drect, NULL, 0);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to lock picture surface.");
        m_lock.unlock();
        return false;
    }

    pitch = d3drect.Pitch;
    return (uint8_t*)d3drect.pBits;
}

void MythRenderD3D9::ReleaseBuffer(IDirect3DSurface9* surface)
{
    if (!m_surfaces.contains(surface))
        return;

    HRESULT hr = surface->UnlockRect();
    if (FAILED(hr))
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to unlock picture surface.");
    m_lock.unlock();
}

IDirect3DVertexBuffer9* MythRenderD3D9::CreateVertexBuffer(IDirect3DTexture9* texture)
{
    QMutexLocker locker(&m_lock);

    if (texture && !m_textures.contains(texture))
        return false;

    IDirect3DVertexBuffer9* temp_vbuf = NULL;
    HRESULT hr = m_d3dDevice->CreateVertexBuffer(
        sizeof(TEXTUREVERTEX)*4, D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
        D3DFVF_TEXTUREVERTEX,    D3DPOOL_DEFAULT,
        &temp_vbuf,             NULL);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create vertex buffer");
        return false;
    }

    m_vertexbuffers[temp_vbuf] = MythD3DVertexBuffer(texture);
    return temp_vbuf;
}

void MythRenderD3D9::DeleteVertexBuffers(void)
{
    QMap<IDirect3DVertexBuffer9*,MythD3DVertexBuffer>::iterator it;
    for (it = m_vertexbuffers.begin();
         it != m_vertexbuffers.end(); ++it)
    {
        it.key()->Release();
    }
    m_vertexbuffers.clear();
}

void MythRenderD3D9::DeleteVertexBuffer(IDirect3DVertexBuffer9 *vertexbuffer)
{
    QMutexLocker locker(&m_lock);
    if (m_vertexbuffers.contains(vertexbuffer))
    {
        vertexbuffer->Release();
        m_vertexbuffers.remove(vertexbuffer);
    }
}

bool MythRenderD3D9::UpdateVertexBuffer(IDirect3DVertexBuffer9* vertexbuffer,
                                        const QRect &dst, const QRect &src,
                                        int alpha, bool video)
{
    if (!m_vertexbuffers.contains(vertexbuffer))
        return false;

    MythD3DVertexBuffer mythvb = m_vertexbuffers[vertexbuffer];
    uint32_t clr = (alpha << 24) + (255 << 16) + (255 << 8) + 255;

    int width  = dst.width();
    int height = dst.height();
    if (!video)
    {
        width  = std::min(src.width(),  width);
        height = std::min(src.height(), height);
    }
    QRect dest(dst.left(), dst.top(), width, height);

    // FIXME - with alpha pulse, this updates far more textures than necessary
    if (dest == mythvb.m_dest &&
        src  == mythvb.m_src  &&
        clr  == mythvb.m_color)
        return true;

    QSize norm = src.size();
    if (video && mythvb.m_texture)
        norm = m_textures[mythvb.m_texture];

    QMutexLocker locker(&m_lock);
    TEXTUREVERTEX *p_vertices;
    HRESULT hr = vertexbuffer->Lock(0, 0, (VOID **)(&p_vertices),
                                    D3DLOCK_DISCARD);
    D3DCOLOR color = D3DCOLOR_ARGB(alpha, 255, 255, 255);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to lock vertex buffer.");
        return false;
    }

    p_vertices[0].x       = (float)dest.left();
    p_vertices[0].y       = (float)dest.top();
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = color;
    p_vertices[0].rhw     = 1.0f;
    p_vertices[0].t1u     = ((float)src.left() - 0.5f) / (float)norm.width();
    p_vertices[0].t1v     = ((float)src.top() - 0.5f) / (float)norm.height();

    p_vertices[1].x       = (float)(dest.left() + dest.width());
    p_vertices[1].y       = (float)dest.top();
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = color;
    p_vertices[1].rhw     = 1.0f;
    p_vertices[1].t1u     = ((float)(src.left() + src.width()) - 0.5f) /
                            (float)norm.width();
    p_vertices[1].t1v     = ((float)src.top() - 0.5f) / (float)norm.height();

    p_vertices[2].x       = (float)(dest.left() + dest.width());
    p_vertices[2].y       = (float)(dest.top() + dest.height());
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = color;
    p_vertices[2].rhw     = 1.0f;
    p_vertices[2].t1u     = ((float)(src.left() + src.width()) - 0.5f) /
                            (float)norm.width();
    p_vertices[2].t1v     = ((float)(src.top() + src.height()) - 0.5f) /
                            (float)norm.height();

    p_vertices[3].x       = (float)dest.left();
    p_vertices[3].y       = (float)(dest.top() + dest.height());
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = color;
    p_vertices[3].rhw     = 1.0f;
    p_vertices[3].t1u     = ((float)src.left() - 0.5f) / (float)norm.width();
    p_vertices[3].t1v     = ((float)(src.top() + src.height()) - 0.5f) /
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

    m_vertexbuffers[vertexbuffer].m_dest  = dest;
    m_vertexbuffers[vertexbuffer].m_src   = src;
    m_vertexbuffers[vertexbuffer].m_color = clr;
    return true;
}

void MythRenderD3D9::Init2DState(void)
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

    SetTextureVertices(false);
    MultiTexturing(false);
    EnableBlending(false);
}

void MythRenderD3D9::EnableBlending(bool enable)
{
    if (!m_d3dDevice || (m_blend == enable))
        return;
    m_blend = enable;
    m_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

void MythRenderD3D9::SetTextureVertices(bool enable)
{
    if (!m_d3dDevice || (m_texture_vertices == enable))
        return;
    m_texture_vertices = enable;
    m_d3dDevice->SetFVF(enable ? D3DFVF_TEXTUREVERTEX : D3DFVF_VERTEX);
}
