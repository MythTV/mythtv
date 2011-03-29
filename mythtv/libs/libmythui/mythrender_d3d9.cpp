#define _WIN32_WINNT 0x500

#include <algorithm>
using std::min;

#include <QLibrary>
#include <QRect>
#include <QMap>
#include <QMutex>

#include "mythverbose.h"
#include "mythrender_d3d9.h"

#define DXVA2_E_NEW_VIDEO_DEVICE MAKE_HRESULT(1, 4, 4097)

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

bool D3D9Image::UpdateImage(IDirect3DSurface9 *surface)
{
    if (m_valid)
        return m_render->StretchRect(m_texture, surface, false);
    return false;
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

#define mD3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define mD3DFMT_IYUV (D3DFORMAT)MAKEFOURCC('I','Y','U','V')
#define mD3DFMT_I420 (D3DFORMAT)MAKEFOURCC('I','4','2','0')
#define mD3DFMT_YV16 (D3DFORMAT)MAKEFOURCC('Y','V','1','6')
#define D3DFVF_TEXTUREVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1|D3DFVF_TEX2)
#define D3DFVF_VERTEX        (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)
#define D3DLOC QString("MythRenderD3D9: ")
#define D3DERR QString("MythRenderD3D9 Error: ")

D3D9Locker::D3D9Locker(MythRenderD3D9 *render) : m_render(render)
{
}

D3D9Locker::~D3D9Locker()
{
    if (m_render)
        m_render->ReleaseDevice();
}

IDirect3DDevice9* D3D9Locker::Acquire(void)
{
    IDirect3DDevice9* result = NULL;
    if (m_render)
        result = m_render->AcquireDevice();
    if (!result)
        VERBOSE(VB_IMPORTANT, "D3D9Locker: Failed to acquire device.");
    return result;
}

void* MythRenderD3D9::ResolveAddress(const char* lib, const char* proc)
{
    return QLibrary::resolve(lib, proc);
}

MythRenderD3D9::MythRenderD3D9(void)
  : MythRender(kRenderDirect3D9),
    m_d3d(NULL), m_rootD3DDevice(NULL),
    m_adaptor_fmt(D3DFMT_UNKNOWN),
    m_videosurface_fmt(D3DFMT_UNKNOWN),
    m_surface_fmt(D3DFMT_UNKNOWN), m_texture_fmt(D3DFMT_UNKNOWN),
    m_rect_vertexbuffer(NULL), m_default_surface(NULL), m_current_surface(NULL),
    m_lock(QMutex::Recursive),
    m_blend(true), m_multi_texturing(true), m_texture_vertices(true),
    m_deviceManager(NULL), m_deviceHandle(NULL), m_deviceManagerToken(0)
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

    DestroyDeviceManager();

    if (m_rootD3DDevice)
    {
        VERBOSE(VB_GENERAL, D3DLOC + "Deleting D3D9 device.");
        m_rootD3DDevice->Release();
    }

    if (m_d3d)
    {
        VERBOSE(VB_GENERAL, D3DLOC + "Deleting D3D9.");
        m_d3d->Release();
    }
}

static const QString toString(D3DFORMAT fmt)
{
    switch (fmt)
    {
        case D3DFMT_A8:
            return "A8";
        case D3DFMT_A8R8G8B8:
            return "A8R8G8B8";
        case D3DFMT_X8R8G8B8:
            return "X8R8G8B8";
        case D3DFMT_A8B8G8R8:
            return "A8B8G8R8";
        case D3DFMT_X8B8G8R8:
            return "X8B8G8R8";
        case mD3DFMT_YV12:
            return "YV12";
        case D3DFMT_UYVY:
            return "UYVY";
        case D3DFMT_YUY2:
            return "YUY2";
        case mD3DFMT_IYUV:
            return "IYUV";
        case mD3DFMT_I420:
            return "I420";
        case mD3DFMT_YV16:
            return "YV16";
        default:
            break;
    }
    return QString().setNum((ulong)fmt,16);
}

bool MythRenderD3D9::Create(QSize size, HWND window)
{
    QMutexLocker locker(&m_lock);

    typedef LPDIRECT3D9 (WINAPI *LPFND3DC)(UINT SDKVersion);
    static  LPFND3DC  OurDirect3DCreate9 = NULL;

    OurDirect3DCreate9 = (LPFND3DC)ResolveAddress("D3D9","Direct3DCreate9");
    if (!OurDirect3DCreate9)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "FATAL :Failed to find Direct3DCreate9.");
        return false;
    }

    m_d3d = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!m_d3d)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not create Direct3D9 instance.");
        return false;
    }

    D3DCAPS9 d3dCaps;
    ZeroMemory(&d3dCaps, sizeof(d3dCaps));
    if (D3D_OK != m_d3d->GetDeviceCaps(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not read adapter capabilities.");
    }

    D3DDISPLAYMODE d3ddm;
    if (D3D_OK != m_d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not read adapter display mode.");
        return false;
    }

    // Check the adaptor format is reasonable
    static const D3DFORMAT bfmt[] =
    {
        D3DFMT_A8R8G8B8,
        D3DFMT_A8B8G8R8,
        D3DFMT_X8R8G8B8,
        D3DFMT_X8B8G8R8,
        D3DFMT_R8G8B8
    };

    m_adaptor_fmt = d3ddm.Format;
    bool is_reasonable = false;
    for (uint i = 0; i < sizeof(bfmt) / sizeof(bfmt[0]); i++)
        if (bfmt[i] == m_adaptor_fmt)
            is_reasonable = true;
    VERBOSE(VB_GENERAL, D3DLOC + QString("Default adaptor format %1.")
                                         .arg(toString(m_adaptor_fmt)));
    if (!is_reasonable)
    {
        VERBOSE(VB_GENERAL, D3DLOC +
            QString("Warning: Default adaptor format may not work."));
    }

    // Choose a surface format
    for (unsigned i = 0; i < sizeof bfmt / sizeof bfmt[0]; ++i)
    {
        if (SUCCEEDED(m_d3d->CheckDeviceType(D3DADAPTER_DEFAULT,
                      D3DDEVTYPE_HAL, m_adaptor_fmt, bfmt[i], TRUE)))
        {
            m_surface_fmt = bfmt[i];
            break;
        }
    }

    if (D3DFMT_UNKNOWN == m_surface_fmt)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to choose surface format - "
                                       "using default back buffer format.");
        m_surface_fmt = m_adaptor_fmt;
    }

    m_texture_fmt = m_surface_fmt;
    VERBOSE(VB_GENERAL, D3DLOC + QString("Chosen surface and texture format: %1")
            .arg(toString(m_surface_fmt)));


    // Test whether a YV12 video surface is available
    if (FAILED(m_d3d->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
               D3DDEVTYPE_HAL, D3DFMT_UNKNOWN, m_adaptor_fmt)) &&
        SUCCEEDED(m_d3d->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                  D3DDEVTYPE_HAL, mD3DFMT_YV12, m_surface_fmt)))
    {
        m_videosurface_fmt = mD3DFMT_YV12;
    }
    else
    {
        m_videosurface_fmt = m_surface_fmt;
    }

    VERBOSE(VB_GENERAL, D3DLOC + QString("Chosen video surface format %1.")
        .arg(toString(m_videosurface_fmt)));
    VERBOSE(VB_GENERAL, D3DLOC + QString("Hardware YV12 to RGB conversion %1.")
        .arg(m_videosurface_fmt != mD3DFMT_YV12 ? "unavailable" : "available"));

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
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                      D3DCREATE_MULTITHREADED,
                                      &d3dpp, &m_rootD3DDevice))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Could not create the D3D device.");
        return false;
    }

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

    CreateDeviceManager();
    Init2DState();
    return true;
}

bool MythRenderD3D9::HardwareYUVConversion(void)
{
    return m_videosurface_fmt == mD3DFMT_YV12;
}

bool MythRenderD3D9::Test(bool &reset)
{
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    bool result = true;
    HRESULT hr = dev->TestCooperativeLevel();
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
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    HRESULT hr = dev->Clear(0, NULL, D3DCLEAR_TARGET,
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
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    HRESULT hr = dev->BeginScene();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "BeginScene() failed.");
        return false;
    }
    return true;
}

bool MythRenderD3D9::End(void)
{
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    HRESULT hr = dev->EndScene();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "EndScene() failed.");
        return false;
    }
    return true;
}

void MythRenderD3D9::CopyFrame(void* surface, D3D9Image *img)
{
    if (surface && img)
        img->UpdateImage((IDirect3DSurface9*)surface);
}

bool MythRenderD3D9::StretchRect(IDirect3DTexture9 *texture,
                                 IDirect3DSurface9 *surface,
                                 bool known_surface)
{
    if (!m_textures.contains(texture) ||
       (known_surface && !m_surfaces.contains(surface)))
        return false;

    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    LPDIRECT3DSURFACE9 d3ddest;
    HRESULT hr = texture->GetSurfaceLevel(0, &d3ddest);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "GetSurfaceLevel() failed");
        return false;
    }

    hr = dev->StretchRect(surface, NULL, d3ddest,
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

    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    IDirect3DTexture9 *texture = m_vertexbuffers[vertexbuffer].m_texture;

    if (texture && !SetTexture(dev, texture))
        return false;

    EnableBlending(dev, true);
    SetTextureVertices(dev, true);
    MultiTexturing(dev, false);

    HRESULT hr = dev->SetStreamSource(0, vertexbuffer,
                                              0, sizeof(TEXTUREVERTEX));
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetStreamSource() failed");
        return false;
    }

    hr = dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "DrawPrimitive() failed");
        return false;
    }

    return true;
}

void MythRenderD3D9::DrawRect(const QRect &rect, const QColor &color, int alpha)
{
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return;

    if (!m_rect_vertexbuffer)
    {
        HRESULT hr = dev->CreateVertexBuffer(
                sizeof(VERTEX)*4,     D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
                D3DFVF_VERTEX,        D3DPOOL_DEFAULT,
                &m_rect_vertexbuffer, NULL);

        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create vertex buffer");
            return;
        }
    }

    EnableBlending(dev, true);
    SetTextureVertices(dev, false);
    MultiTexturing(dev, false);
    SetTexture(dev, NULL, 0);

    int alphamod = (int)(color.alpha() * (alpha / 255.0));
    D3DCOLOR clr = D3DCOLOR_ARGB(alphamod, color.red(),
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

    hr = dev->SetStreamSource(0, m_rect_vertexbuffer,
                                      0, sizeof(VERTEX));
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetStreamSource() failed");
        return;
    }

    hr = dev->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "DrawPrimitive() failed");
        return;
    }
}

void MythRenderD3D9::MultiTexturing(IDirect3DDevice9* dev, bool enable,
                                    IDirect3DTexture9 *texture)
{
    if (m_multi_texturing == enable)
        return;

    if (!dev)
        return;

    if (enable)
    {
        SetTexture(dev, texture, 1);
        dev->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
        dev->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
        dev->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
        dev->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        dev->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);
        dev->SetTextureStageState(2, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
    }
    else
    {
        dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
        dev->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);
        dev->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE);
        SetTexture(dev, NULL, 1);
    }
    m_multi_texturing = enable;
}

bool MythRenderD3D9::Present(HWND win)
{
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    HRESULT hr = dev->Present(NULL, NULL, win, NULL);
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
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return false;

    bool ret = true;
    HRESULT hr;
    if (texture && m_textures.contains(texture))
    {
        if (!m_default_surface)
        {
            hr = dev->GetRenderTarget(0, &m_default_surface);
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
            hr = dev->SetRenderTarget(0, m_current_surface);
            if (FAILED(hr))
                VERBOSE(VB_IMPORTANT, QString("Failed to set render target."));
        }
    }
    else if (!texture)
    {
        if (m_default_surface)
        {
            hr = dev->SetRenderTarget(0, m_default_surface);
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

bool MythRenderD3D9::SetTexture(IDirect3DDevice9* dev,
                                IDirect3DTexture9 *texture, int num)
{
    if (!dev)
        return false;

    HRESULT hr = dev->SetTexture(num, (LPDIRECT3DBASETEXTURE9)texture);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "SetTexture() failed");
        return false;
    }
    return true;
}

IDirect3DTexture9* MythRenderD3D9::CreateTexture(const QSize &size)
{
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return NULL;

    IDirect3DTexture9* temp_texture = NULL;

    HRESULT hr = dev->CreateTexture(
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
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return NULL;

    IDirect3DSurface9* temp_surface = NULL;

    D3DFORMAT format = video ? m_videosurface_fmt : m_surface_fmt;

    HRESULT hr = dev->CreateOffscreenPlainSurface(
                    size.width(), size.height(), format,
                    D3DPOOL_DEFAULT, &temp_surface, NULL);

    if (FAILED(hr)|| !temp_surface)
    {
        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create surface.");
        return NULL;
    }

    m_surfaces[temp_surface] = MythD3DSurface(size, format);
    dev->ColorFill(temp_surface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

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
    switch (format)
    {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
            {
                uint pitch = image->width() << 2;
                uint8_t *dst = buf;
                uint8_t *src = (uint8_t*)image->bits();
                for (int i = 0; i < image->height(); i++)
                {
                    memcpy(dst, src, pitch);
                    dst += d3dpitch;
                    src += pitch;
                }
            }
            break;
        default:
            VERBOSE(VB_IMPORTANT, D3DERR + "Surface format not supported.");
            break;
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
    D3D9Locker locker(this);
    IDirect3DDevice9* dev = locker.Acquire();
    if (!dev)
        return NULL;

    if (texture && !m_textures.contains(texture))
        return false;

    IDirect3DVertexBuffer9* temp_vbuf = NULL;
    HRESULT hr = dev->CreateVertexBuffer(
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
        width  = min(src.width(),  width);
        height = min(src.height(), height);
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
    IDirect3DDevice9* dev = AcquireDevice();
    if (!dev)
        return;

    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_DITHERENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetVertexShader(NULL);
    SetTextureVertices(dev, false);
    MultiTexturing(dev, false);
    EnableBlending(dev, false);

    ReleaseDevice();
}

void MythRenderD3D9::EnableBlending(IDirect3DDevice9* dev, bool enable)
{
    if (m_blend == enable)
        return;
    m_blend = enable;

    if (dev)
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, enable);
}

void MythRenderD3D9::SetTextureVertices(IDirect3DDevice9* dev, bool enable)
{
    if (m_texture_vertices == enable)
        return;
    m_texture_vertices = enable;

    if (dev)
        dev->SetFVF(enable ? D3DFVF_TEXTUREVERTEX : D3DFVF_VERTEX);
}

IDirect3DDevice9* MythRenderD3D9::AcquireDevice(void)
{
    m_lock.lock();
#ifdef USING_DXVA2
    if (m_deviceManager)
    {
        IDirect3DDevice9* result = NULL;

        HRESULT hr = IDirect3DDeviceManager9_LockDevice(m_deviceManager, m_deviceHandle, &result, true);

        if (hr == DXVA2_E_NEW_VIDEO_DEVICE)
        {
            hr = IDirect3DDeviceManager9_CloseDeviceHandle(m_deviceManager, m_deviceHandle);

            if (SUCCEEDED(hr))
                hr = IDirect3DDeviceManager9_OpenDeviceHandle(m_deviceManager, &m_deviceHandle);

            if (SUCCEEDED(hr))
                hr = IDirect3DDeviceManager9_LockDevice(m_deviceManager, m_deviceHandle, &result, true);
        }

        if (SUCCEEDED(hr))
            return result;

        VERBOSE(VB_IMPORTANT, D3DERR + "Failed to acquire D3D9 device.");
        m_lock.unlock();
        return NULL;
    }
#endif
    return m_rootD3DDevice;
}

void MythRenderD3D9::ReleaseDevice(void)
{
#ifdef USING_DXVA2
    if (m_deviceManager)
    {
        HRESULT hr = IDirect3DDeviceManager9_UnlockDevice(m_deviceManager, m_deviceHandle, false);
        if (!SUCCEEDED(hr))
            VERBOSE(VB_IMPORTANT, D3DERR + "Failed to release D3D9 device.");
    }
#endif
    m_lock.unlock();
}

#ifdef USING_DXVA2
typedef HRESULT (WINAPI *CreateDeviceManager9Ptr)(UINT *pResetToken,
                                                  IDirect3DDeviceManager9 **);
#endif

void MythRenderD3D9::CreateDeviceManager(void)
{
#ifdef USING_DXVA2
    CreateDeviceManager9Ptr CreateDeviceManager9 =
        (CreateDeviceManager9Ptr)ResolveAddress("DXVA2",
                                                "DXVA2CreateDirect3DDeviceManager9");
    if (CreateDeviceManager9)
    {
        UINT resetToken = 0;
        HRESULT hr = CreateDeviceManager9(&resetToken, &m_deviceManager);
        if (SUCCEEDED(hr))
        {
            IDirect3DDeviceManager9_ResetDevice(m_deviceManager, m_rootD3DDevice, resetToken);
            IDirect3DDeviceManager9_AddRef(m_deviceManager);
            m_deviceManagerToken = resetToken;
            VERBOSE(VB_GENERAL, D3DLOC + "Created DXVA2 device manager.");
            hr = IDirect3DDeviceManager9_OpenDeviceHandle(m_deviceManager, &m_deviceHandle);
            if (SUCCEEDED(hr))
            {
                VERBOSE(VB_GENERAL, D3DLOC + "Retrieved device handle.");
                return;
            }
            VERBOSE(VB_IMPORTANT, D3DERR + "Failed to retrieve device handle.");
        }
        else
        {
            VERBOSE(VB_IMPORTANT, D3DERR + "Failed to create DXVA2 device manager.");
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, D3DERR +
            "Failed to get DXVA2CreateDirect3DDeviceManager9 proc address.");
    }
#endif
    m_deviceManager = NULL;
    m_deviceManagerToken = 0;
    VERBOSE(VB_IMPORTANT, D3DLOC +
        "DXVA2 support not available - not using device manager");
}

void MythRenderD3D9::DestroyDeviceManager(void)
{
#ifdef USING_DXVA2
    if (m_deviceHandle && m_deviceManager)
        IDirect3DDeviceManager9_CloseDeviceHandle(m_deviceManager, m_deviceHandle);
    if (m_deviceManager)
        IDirect3DDeviceManager9_Release(m_deviceManager);
#endif
    m_deviceHandle  = NULL;
    m_deviceManager = NULL;
}
