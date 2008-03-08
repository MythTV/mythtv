// -*- Mode: c++ -*-

#include <map>
#include <iostream>
#include <algorithm>
using namespace std;

#define _WIN32_WINNT 0x500
#include "mythcontext.h"
#include "videoout_d3d.h"
#include "filtermanager.h"
#include "fourcc.h"
#include "videodisplayprofile.h"
#include "libmythui/mythmainwindow.h"

#include "mmsystem.h"
#include "tv.h"

#include <qapplication.h>

#undef UNICODE

extern "C" {
#include "../libavcodec/avcodec.h"
}

typedef struct
{
    FLOAT       x;          // vertex untransformed x position
    FLOAT       y;          // vertex untransformed y position
    FLOAT       z;          // vertex untransformed z position
    FLOAT       rhw;        // eye distance
    D3DCOLOR    diffuse;    // diffuse color
    FLOAT       tu;         // texture u coordinate
    FLOAT       tv;         // texture v coordinate
} CUSTOMVERTEX;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 10;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;

#define LOC      QString("VideoOutputD3D: ")
#define LOC_WARN QString("VideoOutputD3D Warning: ")
#define LOC_ERR  QString("VideoOutputD3D Error: ")

VideoOutputD3D::VideoOutputD3D(void)
    : VideoOutput(), m_InputCX(0), m_InputCY(0),
      m_lock(true), m_RefreshRate(60),
      m_hWnd(NULL), m_hEmbedWnd(NULL),
      m_ddFormat(D3DFMT_UNKNOWN), m_pD3D(NULL), m_pd3dDevice(NULL),
      m_pSurface(NULL), m_pTexture(NULL), m_pVertexBuffer(NULL)
{
    VERBOSE(VB_PLAYBACK, LOC + "ctor");
    m_pauseFrame.buf = NULL;
}

VideoOutputD3D::~VideoOutputD3D()
{
    Exit();
}

void VideoOutputD3D::Exit(void)
{
    VERBOSE(VB_PLAYBACK, LOC + "Exit()");
    QMutexLocker locker(&m_lock);
    vbuffers.DeleteBuffers();
    if (m_pauseFrame.buf)
    {
        delete [] m_pauseFrame.buf;
        m_pauseFrame.buf = NULL;
    }
    UnInitD3D();
}

void VideoOutputD3D::UnInitD3D(void)
{
    QMutexLocker locker(&m_lock);

    if (m_pVertexBuffer)
    {
        m_pVertexBuffer->Release();
        m_pVertexBuffer = NULL;
    }

    if (m_pTexture)
    {
        m_pTexture->Release();
        m_pTexture = NULL;
    }

    if (m_pSurface)
    {
        m_pSurface->Release();
        m_pSurface = NULL;
    }

    if (m_pd3dDevice)
    {
        m_pd3dDevice->Release();
        m_pd3dDevice = NULL;
    }

    if (m_pD3D)
    {
        m_pD3D->Release();
        m_pD3D = NULL;
    }
}

bool VideoOutputD3D::InputChanged(const QSize &input_size,
                                  float        aspect,
                                  MythCodecID  av_codec_id,
                                  void        *codec_private)
{
    QMutexLocker locker(&m_lock);
    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);
    db_vdisp_profile->SetVideoRenderer("didect3d");

    if (video_dim.width() == m_InputCX && video_dim.height() == m_InputCY)
    {
        MoveResize();
        return true;
    }

    m_InputCX = video_dim.width();
    m_InputCY = video_dim.height();
    VERBOSE(VB_PLAYBACK, LOC + "InputChanged, x="<< m_InputCX
            << ", y=" << m_InputCY);

    vbuffers.DeleteBuffers();

    MoveResize();

    if (!vbuffers.CreateBuffers(m_InputCX, m_InputCY))
    {
        VERBOSE(VB_IMPORTANT, LOC + "InputChanged(): "
                "Failed to recreate buffers");
        errored = true;
    }

    if (!InitD3D())
        UnInitD3D();

    if (m_pauseFrame.buf)
    {
        delete [] m_pauseFrame.buf;
        m_pauseFrame.buf = NULL;
    }

    m_pauseFrame.height = vbuffers.GetScratchFrame()->height;
    m_pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    m_pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    m_pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    m_pauseFrame.buf    = new unsigned char[m_pauseFrame.size + 128];
    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    return true;
}

bool VideoOutputD3D::InitD3D()
{
    VERBOSE(VB_PLAYBACK, LOC + QString("InitD3D start (x=%1, y=%2)")
            .arg(m_InputCX).arg(m_InputCY));

    QMutexLocker locker(&m_lock);
    D3DCAPS9 d3dCaps;

    typedef LPDIRECT3D9 (WINAPI *LPFND3DC)(UINT SDKVersion);
    static LPFND3DC OurDirect3DCreate9 = NULL;
    static HINSTANCE hD3DLib = NULL;

    if (!hD3DLib)
    {
        hD3DLib = LoadLibrary(TEXT("D3D9.DLL"));
        if (!hD3DLib)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot load d3d9.dll (Direct3D)");
            return false;
        }
    }

    if (!OurDirect3DCreate9)
    {
        OurDirect3DCreate9 = (LPFND3DC) GetProcAddress(
            hD3DLib, TEXT("Direct3DCreate9"));

        if (!OurDirect3DCreate9)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Cannot locate reference to "
                    "Direct3DCreate9 ABI in DLL");

            return false;
        }
    }

    /* Create the D3D object. */
    m_pD3D = OurDirect3DCreate9(D3D_SDK_VERSION);
    if (!m_pD3D)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not create Direct3D9 instance.");

        return false;
    }

    // Get device capabilities
    ZeroMemory(&d3dCaps, sizeof(d3dCaps));

    if (D3D_OK != m_pD3D->GetDeviceCaps(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &d3dCaps))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not read adapter capabilities.");

        return false;
    }

    D3DDISPLAYMODE d3ddm;
    // Get the current desktop display mode, so we can set up a back
    // buffer of the same format
    if (D3D_OK != m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Could not read adapter display mode.");

        return false;
    }
    m_RefreshRate = d3ddm.RefreshRate;

    m_ddFormat = d3ddm.Format;
    D3DPRESENT_PARAMETERS d3dpp;

    /* Set up the structure used to create the D3DDevice. */
    ZeroMemory(&d3dpp, sizeof(D3DPRESENT_PARAMETERS));
    d3dpp.hDeviceWindow          = m_hWnd;
    d3dpp.Windowed               = TRUE;
    d3dpp.BackBufferWidth        = m_InputCX;
    d3dpp.BackBufferHeight       = m_InputCX;
    d3dpp.BackBufferCount        = 1;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.Flags                  = D3DPRESENTFLAG_VIDEO;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    // Create the D3DDevice
    if (D3D_OK != m_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL, d3dpp.hDeviceWindow,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &d3dpp, &m_pd3dDevice))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Could not create the D3D device!");
        return false;
    }

    //input seems to always be PIX_FMT_YUV420P
    //MAKEFOURCC('I','4','2','0');
    //IYUV I420

    D3DFORMAT format = D3DFMT_X8R8G8B8;
    /* test whether device can create a surface of that format */
    HRESULT hr = m_pD3D->CheckDeviceFormat(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_ddFormat, 0,
        D3DRTYPE_SURFACE, format);

    if (SUCCEEDED(hr))
    {
        /* test whether device can perform color-conversion
        ** from that format to target format
        */
        hr = m_pD3D->CheckDeviceFormatConversion(
            D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            format, m_ddFormat);

        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Device does not support conversion to RGB format");

            return false;
        }
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Device does not support surfaces in RGB format");

        return false;
    }

    hr = m_pd3dDevice->CreateOffscreenPlainSurface(
        m_InputCX,
        m_InputCY,
        format,
        D3DPOOL_DEFAULT,
        &m_pSurface,
        NULL);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create picture surface");
        return false;
    }

    /* fill surface with black color */
    m_pd3dDevice->ColorFill(m_pSurface, NULL, D3DCOLOR_ARGB(0xFF, 0, 0, 0) );

    /*
    ** Create a texture for use when rendering a scene
    ** for performance reason, texture format is identical to backbuffer
    ** which would usually be a RGB format
    */
    hr = m_pd3dDevice->CreateTexture(
        m_InputCX,
        m_InputCY,
        1,
        D3DUSAGE_RENDERTARGET,
        m_ddFormat,
        D3DPOOL_DEFAULT,
        &m_pTexture,
        NULL);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create texture");
        return false;
    }

    /*
    ** Create a vertex buffer for use when rendering scene
    */
    hr = m_pd3dDevice->CreateVertexBuffer(
        sizeof(CUSTOMVERTEX)*4,
        D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
        D3DFVF_CUSTOMVERTEX,
        D3DPOOL_DEFAULT,
        &m_pVertexBuffer,
        NULL);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to create vertex buffer");
        return false;
    }

    /* Update the vertex buffer */
    CUSTOMVERTEX *p_vertices;
    hr = m_pVertexBuffer->Lock(0, 0, (VOID **)(&p_vertices), D3DLOCK_DISCARD);

    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock vertex buffer");
        return false;
    }

    /* Setup vertices */
    p_vertices[0].x       = 0.0f;       // left
    p_vertices[0].y       = 0.0f;       // top
    p_vertices[0].z       = 0.0f;
    p_vertices[0].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[0].rhw     = 1.0f;
    p_vertices[0].tu      = 0.0f;
    p_vertices[0].tv      = 0.0f;

    p_vertices[1].x       = (float)m_InputCX;    // right
    p_vertices[1].y       = 0.0f;       // top
    p_vertices[1].z       = 0.0f;
    p_vertices[1].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[1].rhw     = 1.0f;
    p_vertices[1].tu      = 1.0f;
    p_vertices[1].tv      = 0.0f;

    p_vertices[2].x       = (float)m_InputCX;    // right
    p_vertices[2].y       = (float)m_InputCY;   // bottom
    p_vertices[2].z       = 0.0f;
    p_vertices[2].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[2].rhw     = 1.0f;
    p_vertices[2].tu      = 1.0f;
    p_vertices[2].tv      = 1.0f;

    p_vertices[3].x       = 0.0f;       // left
    p_vertices[3].y       = (float)m_InputCY;   // bottom
    p_vertices[3].z       = 0.0f;
    p_vertices[3].diffuse = D3DCOLOR_ARGB(255, 255, 255, 255);
    p_vertices[3].rhw     = 1.0f;
    p_vertices[3].tu      = 0.0f;
    p_vertices[3].tv      = 1.0f;

    hr = m_pVertexBuffer->Unlock();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to unlock vertex buffer");
        return false;
    }

    // Texture coordinates outside the range [0.0, 1.0] are set
    // to the texture color at 0.0 or 1.0, respectively.
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

    // Set linear filtering quality
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

    // set maximum ambient light
    m_pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(255,255,255));

    // Turn off culling
    m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    // Turn off the zbuffer
    m_pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

    // Turn off lights
    m_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Enable dithering
    m_pd3dDevice->SetRenderState(D3DRS_DITHERENABLE, TRUE);

    // disable stencil
    m_pd3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);

    // manage blending
    m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);
    m_pd3dDevice->SetRenderState(D3DRS_ALPHAREF, 0x10);
    m_pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER);

    // Set texture states
    m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP,D3DTOP_MODULATE);
    m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1,D3DTA_TEXTURE);
    m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2,D3DTA_DIFFUSE);

    // turn off alpha operation
    m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    VERBOSE(VB_GENERAL, LOC +
            "Direct3D device adapter successfully initialized");

    return true;
}

int VideoOutputD3D::GetRefreshRate(void)
{
    return 1000000 / m_RefreshRate;
}

bool VideoOutputD3D::Init(int width, int height, float aspect,
                          WId winid, int winx, int winy, int winw,
                          int winh, WId embedid)
{
    VERBOSE(VB_PLAYBACK, LOC +
            "Init w=" << width << " h=" << height);

    db_vdisp_profile->SetVideoRenderer("direct3d");

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);

    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    m_hWnd = winid;

    m_InputCX = video_dim.width();
    m_InputCY = video_dim.height();

    if (!vbuffers.CreateBuffers(video_dim.width(), video_dim.height()))
        return false;

    if (!InitD3D())
    {
        UnInitD3D();
        return false;
    }

    m_pauseFrame.height = vbuffers.GetScratchFrame()->height;
    m_pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    m_pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    m_pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    m_pauseFrame.buf    = new unsigned char[m_pauseFrame.size + 128];
    m_pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    MoveResize();

    return true;
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
    AVPicture image_in, image_out;

    D3DLOCKED_RECT d3drect;
    /* Lock the surface to get a valid pointer to the picture buffer */
    HRESULT hr = m_pSurface->LockRect(&d3drect, NULL, 0);
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock picture surface");
        return;
    }

    avpicture_fill(&image_out, (uint8_t*) d3drect.pBits,
                   PIX_FMT_RGBA32, m_InputCX, m_InputCY);
    image_out.linesize[0] = d3drect.Pitch;
    avpicture_fill(&image_in, buffer->buf,
                   PIX_FMT_YUV420P, m_InputCX, m_InputCY);
    img_convert(&image_out, PIX_FMT_RGBA32, &image_in,
                PIX_FMT_YUV420P, m_InputCX, m_InputCY);

    hr = m_pSurface->UnlockRect();
    if (FAILED(hr))
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to unlock picture surface");
        return;
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

    if (needrepaint)
        DrawUnusedRects(false);

    if (m_pd3dDevice)
    {
        // check if device is still available
        HRESULT hr = m_pd3dDevice->TestCooperativeLevel();
        if (FAILED(hr))
        {
            switch (hr)
            {
                case D3DERR_DEVICENOTRESET:
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "The device has been lost but can be reset "
                            "at this time. TODO: implement device reset");
                    // TODO: instead of goto renderError, reset the device
                    //m_pd3dDevice->Reset(...
                    goto RenderError;
                case D3DERR_DEVICELOST:
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "The device has been lost and cannot be reset "
                            "at this time.");
                    goto RenderError;
                case D3DERR_DRIVERINTERNALERROR:
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Internal driver error. "
                            "Please shut down the application.");
                    goto RenderError;
                default:
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "TestCooperativeLevel() failed.");
                    goto RenderError;
            }
        }

        /* Clear the backbuffer and the zbuffer */
        hr = m_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET,
                                 D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Clear() failed.");
            goto RenderError;
        }

        /* retrieve texture top-level surface */
        LPDIRECT3DSURFACE9      p_d3ddest;
        hr = m_pTexture->GetSurfaceLevel(0, &p_d3ddest);
        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "GetSurfaceLevel() failed");
            goto RenderError;
        }

        /* Copy picture surface into texture surface,
           color space conversion could happen here */
        hr = m_pd3dDevice->StretchRect(m_pSurface, NULL, p_d3ddest,
                                       NULL, D3DTEXF_LINEAR);
        p_d3ddest->Release();
        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "StretchRect() failed");
            goto RenderError;
        }

        // Begin the scene
        hr = m_pd3dDevice->BeginScene();
        if (FAILED(hr))
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "BeginScene() failed");
            goto RenderError;
        }

        // Setup our texture. Using textures introduces the texture
        // stage states, which govern how textures get blended together
        // (in the case of multiple textures) and lighting information.
        // In this case, we are modulating (blending) our texture with
        // the diffuse color of the vertices.
        hr = m_pd3dDevice->SetTexture(0, (LPDIRECT3DBASETEXTURE9)m_pTexture);
        if (FAILED(hr))
        {
            m_pd3dDevice->EndScene();

            VERBOSE(VB_IMPORTANT, LOC_ERR + "SetTexture() failed");
            goto RenderError;
        }

        // Render the vertex buffer contents
        hr = m_pd3dDevice->SetStreamSource(0, m_pVertexBuffer,
                                           0, sizeof(CUSTOMVERTEX));
        if (FAILED(hr))
        {
            m_pd3dDevice->EndScene();

            VERBOSE(VB_IMPORTANT, LOC_ERR + "SetStreamSource() failed");
            goto RenderError;
        }

        // we use FVF instead of vertex shader
        hr = m_pd3dDevice->SetVertexShader(NULL);
        //if (FAILED(hr)) // we don't need to know

        hr = m_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
        if (FAILED(hr))
        {
            m_pd3dDevice->EndScene();

            VERBOSE(VB_IMPORTANT, LOC_ERR + "SetFVF() failed");
            goto RenderError;
        }

        // draw rectangle
        hr = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
        if (FAILED(hr))
        {
            m_pd3dDevice->EndScene();

            VERBOSE(VB_IMPORTANT, LOC_ERR + "DrawPrimitive() failed");
            goto RenderError;
        }

        // End the scene
        hr = m_pd3dDevice->EndScene();
        if (FAILED(hr))
        {

            VERBOSE(VB_IMPORTANT, LOC_ERR + "EndScene() failed");
            goto RenderError;
        }

        {
            RECT rc_src =
            {
                video_rect.left(),  video_rect.top(),
                video_rect.right(), video_rect.bottom(),
            };

            RECT rc_dest =
            {
                display_video_rect.left(), display_video_rect.top(),
                display_video_rect.right(), display_video_rect.bottom(),
            };

            hr = m_pd3dDevice->Present(
                &rc_src, &rc_dest,
                (embedding) ? m_hEmbedWnd : NULL, NULL);
        }

        if (FAILED(hr))
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Present() failed)");

RenderError:

        qApp->wakeUpGuiThread();

        // reset system idle timeout
        SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Direct3D device not initialized");
    }
}

void VideoOutputD3D::DrawUnusedRects(bool sync)
{
    if (embedding)
        return;

    needrepaint = false;
    HDC hdc = GetDC(m_hWnd);
    if (hdc)
    {
        RECT rc;
        if (GetClientRect(m_hWnd, &rc))
        {
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }
        ReleaseDC(m_hWnd, hdc);
    }
}

void VideoOutputD3D::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
    m_hEmbedWnd = wid;
}

void VideoOutputD3D::StopEmbedding(void)
{
    if (!embedding)
        return;

    VideoOutput::StopEmbedding();
}

void VideoOutputD3D::Zoom(ZoomDirection direction)
{
    RECT rc;
    ::GetClientRect(m_hWnd, &rc);

    HDC hdc = ::GetDC(m_hWnd);
    if (hdc)
    {
        ::FillRect(hdc, &rc, (HBRUSH)::GetStockObject(BLACK_BRUSH));
        ::ReleaseDC(m_hWnd, hdc);
    }
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
                                  NuppelVideoPlayer *pipPlayer)
{
    QMutexLocker locker(&m_lock);
    if (IsErrored())
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "ProcessFrame() called while IsErrored is true.");
        return;
    }

    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &m_pauseFrame);
    }

    if (m_deinterlacing && m_deintFilter != NULL)
        m_deintFilter->ProcessFrame(frame);

    if (filterList)
        filterList->ProcessFrame(frame);

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);
}

float VideoOutputD3D::GetDisplayAspect(void) const
{
    float width  = display_visible_rect.width();
    float height = display_visible_rect.height();

    if (height <= 0.0001f)
        return 16.0f / 9.0f;

    return width / height;
}

QStringList VideoOutputD3D::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    QStringList list;
    list += "direct3d";
    return list;
}
