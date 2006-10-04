/* videoout_directfb.cpp
*
* Use DirectFB for video output while watvhing TV
* Most of this is ripped from videoout_* and mplayer's vo_directfb */

// POSIX headers
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

// Linux headers
#include <linux/fb.h>
#include <directfb_version.h>

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <qapplication.h>
#include <qwidget.h>
#include <qevent.h>
#include <qmutex.h>

// MythTV headers
#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_directfb.h"
#include "frame.h"
#include "tv.h"

#define DFBCHECKFAIL(dfbcommand, returnstmt...)\
{\
    DFBResult err = dfbcommand;\
\
        if (err != DFB_OK)\
        {\
                fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );\
                DirectFBError( #dfbcommand, err );\
                return returnstmt;\
        }\
}

#define DFBCHECK(x...)\
{\
    DFBResult err = x;\
\
        if (err != DFB_OK)\
        {\
                fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ );\
                DirectFBError( #x, err );\
        }\
}
#define DFB_KBID_OFFSET 62976

#define INCREMENT(x, step, min, max)\
{\
        int __n;\
        __n = x + step;\
        x = ( __n < min )       ? min : ( ( __n > max ) ? max : __n );\
}

static const unsigned int QT_KEYS[DIKI_NUMBER_OF_KEYS][2] =
    {
        {0xffff,0 }, // unknown key
        {0x41,  0x61},  // a...z
        {0x42,  0x62},
        {0x43,  0x63},
        {0x44,  0x64},
        {0x45,  0x65},
        {0x46,  0x66},
        {0x47,  0x67},
        {0x48,  0x68},
        {0x49,  0x69},
        {0x4a,  0x6a},
        {0x4b,  0x8b},
        {0x4c,  0x6c},
        {0x4d,  0x6d},
        {0x4e,  0x6e},
        {0x4f,  0x6f},
        {0x50,  0x70},
        {0x51,  0x71},
        {0x52,  0x72},
        {0x53,  0x73},
        {0x54,  0x74},
        {0x55,  0x75},
        {0x56,  0x76},
        {0x57,  0x77},
        {0x58,  0x78},
        {0x59,  0x79},
        {0x5a,  0x7a},
        {0x30,  0x30},  // 0-9
        {0x31,  0x31},
        {0x32,  0x32},
        {0x33,  0x33},
        {0x34,  0x34},
        {0x35,  0x35},
        {0x36,  0x36},
        {0x37,  0x37},
        {0x38,  0x38},
        {0x39,  0x39},
        {0x1030,0x00},  // function keys F1 - F12
        {0x1031,0x00},
        {0x1032,0x00},
        {0x1033,0x00},
        {0x1034,0x00},
        {0x1035,0x00},
        {0x1036,0x00},
        {0x1037,0x00},
        {0x1038,0x00},
        {0x1039,0x00},
        {0x103a,0x00},
        {0x103b,0x00},
        {0x1020,0x00},  // Shift Left
        {0x1020,0x00},  // Shift Right
        {0x1021,0x00}, // Control Left
        {0x1021,0x00},  // Control Right
        {0x1023,0x00},  // ALT Left
        {0x1023,0x00},  // ALT Right
        {0xffff,0x00}, // DIKS_ALTGR  not sure what QT Key is
        {0x1022,0x00},  // META Left
        {0x1022,0x00},  // META Right
        {0x1053,0x00}, // Super Left
        {0x1054,0x00},  // Super Right
        {0x1056,0x00},  // Hyper Left
        {0x1057,0x00},  // Hyper Right
        {0x1024,0x00},  // CAPS Lock
        {0x1025,0x00},  // NUM Locka
        {0x1026,0x00},  // Scroll Lock
        {0x1000,0x1b},  // Escape
        {0x1012,0x00},  // Left
        {0x1014,0x00},  // Right
        {0x1013,0x00},  // Up
        {0x1015,0x00},  // Down
        {0x1001,0x09},  // Tab
        {0x1004,0x0d},  // Enter
        {0x20,  0x20},  // 7 bit printable ASCII
        {0x1003,0x00},  // Backspace
        {0x1006,0x00},  // Insert
        {0x1007,0x7f},  // Delete
        {0x1010,0x00},  // Home
        {0x1011,0x00},  // End
        {0x1016,0x00},  // Page Up
        {0x1017,0x00},  // Page Down
        {0x1009,0x00},  // Print
        {0x1008,0x00},  // Pause
        {0x60,  0x27},  // Quote Left
        {0x2d,  0x2d},  // Minus
        {0x3d,  0x3d},  // Equals
        {0x5b,  0x5b},  // Bracket Left
        {0x5d,  0x5d},  // Bracket Right
        {0x5c,  0x5c},  // Back Slash
        {0x3b,  0x3b},  // Semicolon
        {0xffff,0x00},  // DIKS_QUOTE_RIGHT not sure what QT Key is...
        {0x2c,  0x2c},  // Comma
        {0x2e,  0x2e},  // Period
        {0x2f,  0x2f},  // Slash
        {0x3c,  0x3c},  // Less Than

        // keypad keys.
        // from what i can tell QT doiesnt have a seperate key code for them.

        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00},
        {0xffff,0x00}
    };

#ifndef DSCAPS_DOUBLE
#define DSCAPS_DOUBLE DSCAPS_FLIPPING
#endif

//8 1 3 2 works, but not for everyone :-(
const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;
typedef map<unsigned char *, IDirectFBSurface *> BufferMap;

class DirectfbData
{
  public:
    DirectfbData()
      : dfb(NULL),            primaryLayer(NULL),
        primarySurface(NULL), videoLayer(NULL),
        videoSurface(NULL),   inputbuf(NULL),
        screen_width(0),      screen_height(0),
        bufferLock(true)
    {
        bzero(&videoLayerDesc,           sizeof(DFBDisplayLayerDescription));
        bzero(&videoLayerConfig,         sizeof(DFBDisplayLayerConfig));
        bzero(&videoSurfaceCapabilities, sizeof(DFBSurfaceCapabilities));
#if (DIRECTFB_MINOR_VERSION <= 9) && (DIRECTFB_MICRO_VERSION <= 22)
        bzero(&cardCapabilities, sizeof(DFBCardCapabilities));
#else
        bzero(&cardDescription,  sizeof(DFBGraphicsDeviceDescription));
#endif
    }

    //DirectFB hook
    IDirectFB                   *dfb;
    IDirectFBDisplayLayer       *primaryLayer;
    IDirectFBSurface            *primarySurface;
    IDirectFBDisplayLayer       *videoLayer;
    IDirectFBSurface            *videoSurface;
    IDirectFBEventBuffer        *inputbuf;
    int                          screen_width;
    int                          screen_height;
    BufferMap                    buffers;
    QMutex                       bufferLock;
    DFBDisplayLayerDescription   videoLayerDesc;
    DFBDisplayLayerConfig        videoLayerConfig;
    DFBSurfaceCapabilities       videoSurfaceCapabilities;

    //video output
#if (DIRECTFB_MINOR_VERSION <= 9) && (DIRECTFB_MICRO_VERSION <= 22)
    DFBCardCapabilities          cardCapabilities;
#else
    DFBGraphicsDeviceDescription cardDescription;
#endif
};

static struct {
    int flags;
    unsigned short brightness;
    unsigned short contrast;
    unsigned short hue;
    unsigned short saturation;
    unsigned short volume;
} adj = {0, 0x8000, 0x8000, 0x8000, 0x8000, 50};

VideoOutputDirectfb::VideoOutputDirectfb(void)
    : VideoOutput(), XJ_started(0), widget(NULL), data(NULL)
{
    init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
    data = new DirectfbData();
}

VideoOutputDirectfb::~VideoOutputDirectfb()
{
    DeleteDirectfbBuffers();

    // cleanup
    if (data->inputbuf)
        data->inputbuf->Release(data->inputbuf);
    if (data->videoSurface)
        data->videoSurface->Release(data->videoSurface);
    if (data->primarySurface)
        data->primarySurface->Release(data->primarySurface);
    if (data->primaryLayer)
        data->primaryLayer->Release(data->primaryLayer);
    if (data->videoLayer)
        data->videoLayer->Release(data->videoLayer);
    if (data->dfb)
        data->dfb->Release(data->dfb);

    if (pauseFrame.buf)
    {
        delete [] pauseFrame.buf;
        init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
    }

    if (XJ_started)
    {
        XJ_started = false;
    }

    if (data)
    {
        delete data;
        data = NULL;
    }
}

int VideoOutputDirectfb::GetRefreshRate(void)
{
    int fh, v;
    struct fb_var_screeninfo si;
    double drate;
    double hrate;
    double vrate;
    long htotal;
    long vtotal;
    char *fb_dev_name = NULL;
    if (!fb_dev_name && !(fb_dev_name = getenv("FRAMEBUFFER")))
        fb_dev_name = strdup("/dev/fb0");

    fh = open(fb_dev_name, O_RDONLY);
    if (-1 == fh) {
        return -1;
    }

    if (ioctl(fh, FBIOGET_VSCREENINFO, &si)) {
        close(fh);
        return -1;
    }

    htotal = si.left_margin + si.xres + si.right_margin + si.hsync_len;
    vtotal = si.upper_margin + si.yres + si.lower_margin + si.vsync_len;

    switch (si.vmode & FB_VMODE_MASK) {
    case FB_VMODE_INTERLACED:
        break;
    case FB_VMODE_DOUBLE:
        vtotal <<= 2;
        break;
    default:
        vtotal <<= 1;
        break;
    }

    drate = 1E12 / si.pixclock;
    hrate = drate / htotal;
    vrate = hrate / vtotal * 2;

    v = (int)(1E3 / vrate + 0.5);
    /* h = hrate / 1E3; */

    close(fh);
    return v;
}

bool VideoOutputDirectfb::Init(int width, int height, float aspect, WId winid,
                               int winx, int winy, int /*winw*/, int /*winh*/,
                               WId embedid)
{
    DFBResult ret;
    DFBSurfaceDescription desc;
    DFBDisplayLayerConfig conf;
    //DFBDisplayLayerDescription ldesc;

    widget = QWidget::find(winid);

    //setup DirectFB
    DFBCHECKFAIL(DirectFBInit(NULL,NULL), false);

    DirectFBSetOption("bg-none",NULL);
    DirectFBSetOption("no-cursor",NULL);

    DFBCHECKFAIL(DirectFBCreate( &(data->dfb) ), false);
    DFBCHECKFAIL(data->dfb->SetCooperativeLevel(data->dfb, DFSCL_FULLSCREEN), false);

    //setup primary layer
    //the screen width and height is supposed to correspond to the
    //dimensions of the primary layer
    DFBCHECKFAIL(data->dfb->GetDisplayLayer(data->dfb, DLID_PRIMARY, &(data->primaryLayer)), false);
    DFBCHECKFAIL(data->primaryLayer->GetConfiguration(data->primaryLayer, &conf), false);
    data->screen_width = conf.width;
    data->screen_height = conf.height;
    display_aspect = ((float)(conf.width)) / ((float)(conf.height));


    //determine output card capacities
#if (DIRECTFB_MINOR_VERSION <= 9) && (DIRECTFB_MICRO_VERSION <= 22)
    DFBCHECKFAIL(data->dfb->GetCardCapabilities(data->dfb, &(data->cardCapabilities)), false);
    VERBOSE(VB_PLAYBACK, QString("DirectFB output : card : %1")
            .arg((data->cardCapabilities.acceleration_mask & DFXL_BLIT) > 0 ?
                 "hardware blit support" : "NO hardware blit support"));
#else
    DFBCHECKFAIL(data->dfb->GetDeviceDescription(data->dfb, &(data->cardDescription)), false);
    VERBOSE(VB_PLAYBACK, QString("DirectFB output : card : %1")
            .arg((data->cardDescription.acceleration_mask & DFXL_BLIT) > 0 ?
                 "hardware blit support" : "NO hardware blit support"));
#endif

    //clear primary layer
    desc.flags = DSDESC_CAPS;
    desc.caps = DSCAPS_PRIMARY;
#if (DIRECTFB_MINOR_VERSION <= 9) && (DIRECTFB_MICRO_VERSION <= 22)
    if (data->cardCapabilities.acceleration_mask & DFXL_BLIT)
#else
    if (data->cardDescription.acceleration_mask & DFXL_BLIT)
#endif
        desc.caps = (DFBSurfaceCapabilities)(desc.caps | DSCAPS_DOUBLE);
    DFBCHECKFAIL(data->dfb->CreateSurface(data->dfb, &desc, &(data->primarySurface)), false);
    DFBCHECKFAIL(data->primarySurface->Clear(data->primarySurface, 0, 0, 0, 0xff), false);
    DFBCHECKFAIL(data->primarySurface->Flip(data->primarySurface, 0, DSFLIP_ONSYNC), false);

    //look up an output layer that supports the right format, begin with the video format we have as input, fall back to others
    data->videoLayerConfig.flags = (DFBDisplayLayerConfigFlags)(DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT);
    data->videoLayerConfig.width = width;
    data->videoLayerConfig.height = height;
    data->videoLayerConfig.pixelformat = DSPF_I420;

    DFBCHECK(data->dfb->EnumDisplayLayers(data->dfb, LayerCallback, data));

    if (data->videoLayer == NULL) {
        data->videoLayerConfig.pixelformat = DSPF_YV12;
        DFBCHECK(data->dfb->EnumDisplayLayers(data->dfb, LayerCallback, data));
        if (data->videoLayer == NULL) {
            ret = data->primaryLayer->TestConfiguration(data->primaryLayer, &(data->videoLayerConfig), NULL);
            if (DFB_OK == ret) {
                data->primaryLayer->AddRef(data->primaryLayer);
                data->videoLayer = data->primaryLayer;
            } else {
                VERBOSE(VB_IMPORTANT, QString("DirectFB could not find appropriate video output layer"));
                return false;
            }
        }
    }

    //setup video output layer
    DFBCHECKFAIL(data->videoLayer->SetCooperativeLevel(data->videoLayer, DLSCL_EXCLUSIVE), false);

    //determine buffering capacities
    data->videoLayerConfig.flags = (DFBDisplayLayerConfigFlags)(data->videoLayerConfig.flags | DLCONF_BUFFERMODE);
    data->videoLayerConfig.buffermode = DLBM_TRIPLE;
    if (data->videoLayer->TestConfiguration(data->videoLayer, &(data->videoLayerConfig), NULL))
    {
        //try double buffering in video memory
        data->videoLayerConfig.buffermode = DLBM_BACKVIDEO;
        if (data->videoLayer->TestConfiguration(data->videoLayer, &(data->videoLayerConfig), NULL))
        {
            //fall back to double buffering in system memory
            data->videoLayerConfig.buffermode = DLBM_BACKSYSTEM;
        }
    }

    DFBCHECKFAIL(data->videoLayer->SetConfiguration(data->videoLayer, &(data->videoLayerConfig)), false);

    VERBOSE(VB_PLAYBACK, QString("DirectFB output : videoLayer : %1 : %2x%3, %4, %5 buffering")
            .arg(data->videoLayerDesc.name)
            .arg(data->videoLayerConfig.width)
            .arg(data->videoLayerConfig.height)
            .arg(data->videoLayerConfig.pixelformat == DSPF_I420 ? "I420 : Yuv" : "YV12 : Yvu")
            .arg(
                data->videoLayerConfig.buffermode == DLBM_TRIPLE ? "triple (video memory)" :
                data->videoLayerConfig.buffermode == DLBM_BACKVIDEO ? "double (video memory)" :
                "double (system memory)")
           );

    //setup video output videoSurface
    DFBCHECKFAIL(data->videoLayer->GetSurface(data->videoLayer, &(data->videoSurface)), false);

    DFBCHECKFAIL(data->videoSurface->SetBlittingFlags(data->videoSurface, DSBLIT_NOFX), false);

    DFBCHECKFAIL(data->videoSurface->GetCapabilities(data->videoSurface, &data->videoSurfaceCapabilities), false);

    VERBOSE(VB_PLAYBACK, QString("DirectFB output : videoSurface : %1, %2, %3")
            .arg((data->videoSurfaceCapabilities & DSCAPS_VIDEOONLY) > 0 ? "in video memory" : "in sytem memory")
            .arg((data->videoSurfaceCapabilities & DSCAPS_PRIMARY) > 0 ? "primary surface" : "no primary surface")
            .arg((data->videoSurfaceCapabilities & DSCAPS_INTERLACED) > 0 ? "interlaced" : "not interlaced")
           );

    //setup video input buffers
    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall, 
                  kKeepPrebuffer);
    desc.flags = (DFBSurfaceDescriptionFlags)(DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT);
    desc.width = width;
    desc.height = height;
    desc.pixelformat = data->videoLayerConfig.pixelformat;

    if (!CreateDirectfbBuffers(desc))
        return false;

    VERBOSE(VB_PLAYBACK, QString("DirectFB input : %1 videoSurface buffers : %1x%2, %3")
            .arg(kNumBuffers)
            .arg(desc.width)
            .arg(desc.height)
            .arg(desc.pixelformat == DSPF_I420 ? "I420 : Yuv" : "YV12 : Yvu")
           );

    //setup input handling
    DFBCHECK(data->dfb->CreateInputEventBuffer(data->dfb, DICAPS_KEYS, (DFBBoolean)1, &(data->inputbuf)));

    //this stuff is right from Xv - look at this sometime
    //first frame of the buffers

    init(&pauseFrame, vbuffers.GetScratchFrame()->codec,
         new unsigned char[pauseFrame.size + 64],
         vbuffers.GetScratchFrame()->width, vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->bpp, vbuffers.GetScratchFrame()->size);

    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    VideoOutput::Init(width, height, aspect, winid, winx, winy, data->screen_width, data->screen_height,
                      embedid);

    VERBOSE(VB_PLAYBACK, QString("DirectFB output : screen size %1x%2").arg(data->screen_width).arg(data->screen_height));
    MoveResize();

    InitPictureAttributes();

    //display video output
    DFBCHECK(data->videoLayer->SetOpacity(data->videoLayer, 0xff));

    display_dim = QSize(data->screen_width, data->screen_height);
    if (db_display_dim.width() > 0 && db_display_dim.height() > 0)
        display_dim = db_display_dim;

    display_aspect = (((float)display_dim.width()) /
                      ((float)display_dim.height()));

    XJ_started = true;
    return true;
}

void VideoOutputDirectfb::PrepareFrame(VideoFrame *buffer, FrameScanType)
{
    QMutexLocker locker(&data->bufferLock);

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    framesPlayed = buffer->frameNumber + 1;

    IDirectFBSurface *bufferSurface = data->buffers[buffer->buf];

    if (!bufferSurface)
        return;
#if (DIRECTFB_MINOR_VERSION <= 9) && (DIRECTFB_MICRO_VERSION <= 22)
    if ((data->cardCapabilities.acceleration_mask & DFXL_BLIT) > 0)
#else
    if ((data->cardDescription.acceleration_mask & DFXL_BLIT) > 0)
#endif
    {
        DFBCHECK(data->videoSurface->Blit(data->videoSurface, bufferSurface, NULL, 0, 0));
    }
    else
    {
        unsigned char *src = NULL;
        int y_src_pitch = 0;
        DFBCHECKFAIL(bufferSurface->Lock(
                         bufferSurface, DSLF_READ,
                         (void**) &src, &y_src_pitch));

        unsigned char *dst = NULL;
        int y_dst_pitch = 0;
        DFBCHECKFAIL(data->videoSurface->Lock(
                         data->videoSurface, DSLF_WRITE,
                         (void **)&dst, &y_dst_pitch));

        DFBSurfacePixelFormat fmt;
        data->videoSurface->GetPixelFormat(data->videoSurface, &fmt);

        unsigned char *y_src = src;
        unsigned char *u_src = NULL;
        unsigned char *v_src = NULL;
        unsigned char *y_dst = dst;
        unsigned char *u_dst = NULL;
        unsigned char *v_dst = NULL;

        int y_width      = buffer->width;
        int y_height     = buffer->height;
        int uv_src_pitch = y_src_pitch >> 1;
        int uv_dst_pitch = y_dst_pitch >> 1;
        int uv_width     = uv_width    >> 1;
        int uv_height    = y_height    >> 1;

        if (DSPF_YV12 == fmt)
        {
            v_src = y_src + ( y_src_pitch *  y_height);
            u_src = v_src + (uv_src_pitch * uv_height);
            v_dst = y_dst + ( y_dst_pitch *  y_height);
            u_dst = v_dst + (uv_dst_pitch * uv_height);
        }
        else if (DSPF_I420 == fmt)
        {
            u_src = y_src + ( y_src_pitch  * y_height);
            v_src = u_src + (uv_src_pitch * uv_height);
            u_dst = y_dst + ( y_dst_pitch  * y_height);
            v_dst = u_dst + (uv_dst_pitch * uv_height);
        }

        if (y_src && v_src && u_src)
        {
            // copy Y-plane
            memcpy_pic(y_dst, y_src, y_width, y_height,
                       y_dst_pitch, y_src_pitch);

            // copy U-plane
            memcpy_pic(u_dst, u_src, uv_width, uv_height,
                       uv_dst_pitch, uv_src_pitch);

            // copy V-plane
            memcpy_pic(v_dst, v_src, uv_width, uv_height,
                       uv_dst_pitch, uv_src_pitch);
        }

        DFBCHECK(bufferSurface->Unlock(bufferSurface));
        DFBCHECK(data->videoSurface->Unlock(data->videoSurface));
    }
}

void VideoOutputDirectfb::Show(FrameScanType)
{

    DFBCHECK(data->videoSurface->Flip(data->videoSurface, NULL, DSFLIP_ONSYNC));
/*
    DFBInputEvent event;
    if (data->inputbuf->GetEvent( data->inputbuf, DFB_EVENT(&event) ) == DFB_OK)
    {
        if (event.type == DIET_KEYPRESS) {
            QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyPress, QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][0], QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][1], 0));
        }
        else if (event.type == DIET_KEYRELEASE)
        {
            QApplication::postEvent(widget, new QKeyEvent(QEvent::KeyRelease, QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][0], QT_KEYS[(event.key_id)-DFB_KBID_OFFSET][1], 0));
        }
    }
*/
}

void VideoOutputDirectfb::DrawUnusedRects(bool)
{
    /* DirectFB only draws what is needed :-) */
}

void VideoOutputDirectfb::UpdatePauseFrame(void)
{
    //**FIXME - is this all we need?
    VideoFrame *pauseb = vbuffers.GetScratchFrame();
    VideoFrame *pauseu = vbuffers.head(kVideoBuffer_used);
    if (pauseu)
        memcpy(pauseFrame.buf, pauseu->buf, pauseu->size);
    else
        memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

void VideoOutputDirectfb::ProcessFrame(VideoFrame *frame, OSD *osd,
                                       FilterChain *filterList,
                                       NuppelVideoPlayer *pipPlayer)
{
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &pauseFrame);
    }

    if (m_deinterlacing && m_deintFilter != NULL)
	m_deintFilter->ProcessFrame(frame);
    if (filterList)
        filterList->ProcessFrame(frame);

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);

}

void VideoOutputDirectfb::InputChanged(int width, int height, float aspect,
                                       MythCodecID av_codec_id)
{
    VideoOutput::InputChanged(width, height, aspect, av_codec_id);

    DFBSurfaceDescription desc;
    desc.flags = (DFBSurfaceDescriptionFlags)(DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT);
    desc.width = width;
    desc.height = height;
    desc.pixelformat = data->videoLayerConfig.pixelformat;

    DeleteDirectfbBuffers();
    CreateDirectfbBuffers(desc);
    MoveResize();

    if (pauseFrame.buf)
    {
        delete [] pauseFrame.buf;
        pauseFrame.buf = NULL;
    }

    init(&pauseFrame, vbuffers.GetScratchFrame()->codec,
         new unsigned char[vbuffers.GetScratchFrame()->size + 64],
         vbuffers.GetScratchFrame()->width,
         vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->bpp,
         vbuffers.GetScratchFrame()->size);

    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;
}

void VideoOutputDirectfb::Zoom(int direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputDirectfb::MoveResize(void)
{
    VideoOutput::MoveResize();

    VERBOSE(VB_PLAYBACK,
            QString("DirectFB MoveResize : screen size %1x%2, "
                    "proposed x : %3, y : %4, w : %5, h : %6")
            .arg(data->screen_width).arg(data->screen_height)
            .arg(display_video_rect.left()).arg(display_video_rect.top())
            .arg(display_video_rect.width()).arg(display_video_rect.height()));

    // TODO FIXME support for zooming when 
    // dispwoff > screenwidth || disphoff > screenheight

    if (data->videoLayerDesc.caps & DLCAPS_SCREEN_LOCATION)
    {
        float dispxoff = display_video_rect.left();
        float dispyoff = display_video_rect.top();
        float dispwoff = display_video_rect.width();
        float disphoff = display_video_rect.height();
        DFBCHECK(data->videoLayer->SetScreenLocation(data->videoLayer,
                 dispxoff/data->screen_width, dispyoff/data->screen_height,
                 dispwoff/data->screen_width, disphoff/data->screen_height));
    }
}

bool VideoOutputDirectfb::CreateDirectfbBuffers(DFBSurfaceDescription desc)
{
    VERBOSE(VB_IMPORTANT, "CreateDirectfbBuffers");

    QMutexLocker locker(&data->bufferLock);

    if (!data || !data->dfb)
        return false;

    // allocate each surface in system memory
    desc.flags = (DFBSurfaceDescriptionFlags)(desc.flags | DSDESC_CAPS);
    desc.caps  = DSCAPS_SYSTEMONLY;

    vector<unsigned char*> bufs;
    vector<YUVInfo>        yuvinfo;

    DFBResult err = DFB_OK;

    for (uint i = 0; i < vbuffers.allocSize(); i++)
    {
        IDirectFBSurface *bufferSurface     = NULL;
        unsigned char    *bufferSurfaceData = NULL;
        int pitches[3];
        int offsets[4];

        err = data->dfb->CreateSurface(
            data->dfb, &desc, &bufferSurface);

        if (err != DFB_OK)
        {
            DirectFBError("CreateDirectfbBuffers -- CreateSurface", err);
            break;
        }

        err = bufferSurface->Lock(
            bufferSurface, DSLF_WRITE, (void **)&bufferSurfaceData, pitches);

        if (err != DFB_OK)
        {
            DirectFBError("CreateDirectfbBuffers -- Lock", err);
            DFBCHECK(bufferSurface->Release(bufferSurface));
            break;
        }

        data->buffers[bufferSurfaceData] = bufferSurface;

        pitches[0] = desc.width;
        pitches[1] = pitches[0] >> 1;
        pitches[2] = pitches[0] >> 1;

        offsets[0] = 0;
        offsets[1] = pitches[0] * desc.height;
        offsets[2] = offsets[1] + pitches[1] * (desc.height >> 1);
        offsets[3] = offsets[2] + pitches[2] * (desc.height >> 1);

        if (DSPF_YV12 == desc.pixelformat)
        {
            swap(pitches[1], pitches[2]);
            swap(offsets[1], offsets[2]);
        }

        YUVInfo tmp(desc.width, desc.height, offsets[3], pitches, offsets);

        bufs.push_back(bufferSurfaceData);
        yuvinfo.push_back(tmp);
    }

    if (err != DFB_OK)
    {
        for (uint i = 0; i < bufs.size(); i++)
        {
            BufferMap::iterator it = data->buffers.find(bufs[i]);
            if (it != data->buffers.end())
            {
                DFBCHECK(it->second->Unlock(it->second));
                DFBCHECK(it->second->Release(it->second));
                data->buffers.erase(it);
            }
        }
        return false;
    }

    bool ok = vbuffers.CreateBuffers(desc.width, desc.height, bufs, yuvinfo);

    return ok;
}

void VideoOutputDirectfb::DeleteDirectfbBuffers(void)
{
    QMutexLocker locker(&data->bufferLock);

    for (uint i = 0; i < vbuffers.allocSize(); i++)
    {
        vbuffers.at(i)->buf = NULL;

        if (vbuffers.at(i)->qscale_table)
        {
            delete [] vbuffers.at(i)->qscale_table;
            vbuffers.at(i)->qscale_table = NULL;
        }
    }
    BufferMap::iterator iter;
    for ( iter = data->buffers.begin() ; iter != data->buffers.end() ; iter++ )
    {
        if (iter->second) {
            DFBCHECK(iter->second->Unlock(iter->second));
            DFBCHECK(iter->second->Release(iter->second));
        }
    }
    data->buffers.clear();
}

int VideoOutputDirectfb::SetPictureAttribute(int attribute, int newValue)
{
    data->videoLayer->GetColorAdjustment(data->videoLayer,
                                         (DFBColorAdjustment*) &adj);
    adj.flags = 0;

    newValue   = min(max(newValue, 0), 100);
    uint value = (0xffff * newValue) / 100;

    if ((kPictureAttribute_Brightness == attribute) &&
        (data->videoLayerDesc.caps & DLCAPS_BRIGHTNESS))
    {
        adj.flags = DCAF_BRIGHTNESS;
        adj.brightness = value;
    }
    else if ((kPictureAttribute_Contrast == attribute) &&
             (data->videoLayerDesc.caps & DLCAPS_CONTRAST))
    {
        adj.flags = DCAF_CONTRAST;
        adj.contrast = value;
    }
    else if ((kPictureAttribute_Colour == attribute) &&
             (data->videoLayerDesc.caps & DLCAPS_SATURATION))
    {
        adj.flags = DCAF_SATURATION;
        adj.saturation = value;
    }
    else if ((kPictureAttribute_Hue == attribute) &&
             (data->videoLayerDesc.caps & DLCAPS_HUE))
    {
        adj.flags = DCAF_HUE;
        adj.hue = value;
    }

    if (adj.flags)
    {
        data->videoLayer->SetColorAdjustment(data->videoLayer,
                                             (DFBColorAdjustment*) &adj);
        SetPictureAttributeDBValue(attribute, newValue);
        return newValue;
    }

    return -1;
}

DFBEnumerationResult LayerCallback(unsigned int id,
                                   DFBDisplayLayerDescription desc, void *data)
{
    struct DirectfbData *vodata = (DirectfbData*)data;
    DFBResult ret;
    //IDirectFBSurface *surface;

    if (id == DLID_PRIMARY)
        return DFENUM_OK;

    DFBCHECKFAIL(vodata->dfb->GetDisplayLayer(vodata->dfb, id, &(vodata->videoLayer)), DFENUM_OK);
    VERBOSE(VB_PLAYBACK, QString("DirectFB Layer %1 %2:").arg(id).arg(desc.name));

    if (desc.caps & DLCAPS_SURFACE)
        VERBOSE(VB_PLAYBACK, "  - Has a surface.");

    if (desc.caps & DLCAPS_ALPHACHANNEL)
        VERBOSE(VB_PLAYBACK, "  - Supports blending based on alpha channel.");

    if (desc.caps & DLCAPS_SRC_COLORKEY)
        VERBOSE(VB_PLAYBACK, "  - Supports source color keying.");

    if (desc.caps & DLCAPS_DST_COLORKEY)
        VERBOSE(VB_PLAYBACK, "  - Supports destination color keying.");

    if (desc.caps & DLCAPS_FLICKER_FILTERING)
        VERBOSE(VB_PLAYBACK, "  - Supports flicker filtering.");

    if (desc.caps & DLCAPS_DEINTERLACING)
        VERBOSE(VB_PLAYBACK, "  - Can deinterlace interlaced video for progressive display.  ");

    if (desc.caps & DLCAPS_OPACITY)
        VERBOSE(VB_PLAYBACK, "  - Supports blending based on global alpha factor.");

    if (desc.caps & DLCAPS_SCREEN_LOCATION)
        VERBOSE(VB_PLAYBACK, "  - Can be positioned on the screen.");

    if (desc.caps & DLCAPS_BRIGHTNESS)
        VERBOSE(VB_PLAYBACK, "  - Brightness can be adjusted.");

    if (desc.caps & DLCAPS_CONTRAST)
        VERBOSE(VB_PLAYBACK, "  - Contrast can be adjusted.");

    if (desc.caps & DLCAPS_HUE)
        VERBOSE(VB_PLAYBACK, "  - Hue can be adjusted.");

    if (desc.caps & DLCAPS_SATURATION)
        VERBOSE(VB_PLAYBACK, "  - Saturation can be adjusted.");

    ret = vodata->videoLayer->TestConfiguration(vodata->videoLayer, &(vodata->videoLayerConfig), NULL);

    if (DFB_OK == ret)
    {
        vodata->videoLayerDesc = desc;
        return DFENUM_CANCEL;
    }
    else {
        vodata->videoLayer->Release(vodata->videoLayer);
    }
    return DFENUM_OK;
}
