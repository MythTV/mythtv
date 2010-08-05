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
extern "C" {
#include <directfb.h>
#include <directfb_version.h>
}

// C++ headers
#include <algorithm>
#include <iostream>
using namespace std;

// Qt headers
#include <QCoreApplication>
#include <QWidget>
#include <QMutex>
#include <QKeyEvent>
#include <QObject>
#include <QSize>

// MythTV headers
#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_directfb.h"
#include "frame.h"
#include "tv.h"

#define LOC QString("DirectFB: ")
#define LOC_WARN QString("DirectFB, Warning: ")
#define LOC_ERR QString("DirectFB, Error: ")

#define ENOFB QString("\n\t\t\tFB Error: %1").arg(DirectFBErrorString(fberr))

static DFBEnumerationResult layer_cb(unsigned int id,
                                     DFBDisplayLayerDescription desc,
                                     void *data);

class QtKey
{
  public:
    QtKey() : key(0xffff), ascii(0x00) { }
    QtKey(int k, int a) : key(k), ascii(a) { }

    int key;
    int ascii;
};

static void init_keymap(QMap<uint,QtKey> &qtKeyMap);

#ifndef DSCAPS_DOUBLE
#define DSCAPS_DOUBLE DSCAPS_FLIPPING
#endif

//8 1 3 2 works, but not for everyone :-(
const int kNumBuffers = 31;
const int kNeedFreeFrames = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer = 2;
typedef QMap<unsigned char*, IDirectFBSurface*> BufferMap;

class DirectfbData
{
  public:
    DirectfbData()
      : dfb(NULL),            primaryLayer(NULL),
        primarySurface(NULL), videoLayer(NULL),
        videoSurface(NULL),   inputbuf(NULL),
        screen_width(0),      screen_height(0),
        bufferLock(QMutex::Recursive),
        has_blit(false)
    {
        bzero(&videoLayerDesc,           sizeof(DFBDisplayLayerDescription));
        bzero(&videoLayerConfig,         sizeof(DFBDisplayLayerConfig));
        bzero(&videoSurfaceCapabilities, sizeof(DFBSurfaceCapabilities));
        init_keymap(qtKeyMap);
    }

    bool CreateBuffers(VideoBuffers &vbuffers, DFBSurfaceDescription);
    void DeleteBuffers(VideoBuffers &vbuffers);

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
    bool                         has_blit;
    QMap<uint,QtKey>             qtKeyMap;
};

bool DirectfbData::CreateBuffers(VideoBuffers &vbuffers,
                                 DFBSurfaceDescription desc)
{
    VERBOSE(VB_IMPORTANT, "CreateBuffers");

    QMutexLocker locker(&bufferLock);

    if (!dfb)
        return false;

    // allocate each surface in system memory
    desc.flags = (DFBSurfaceDescriptionFlags)(desc.flags | DSDESC_CAPS);
    desc.caps  = DSCAPS_SYSTEMONLY;

    vector<unsigned char*> bufs;
    vector<YUVInfo>        yuvinfo;

    DFBResult fberr = DFB_OK;

    for (uint i = 0; i < vbuffers.allocSize(); i++)
    {
        IDirectFBSurface *bufferSurface = NULL;

        fberr = dfb->CreateSurface(dfb, &desc, &bufferSurface);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "CreateSurface" + ENOFB);
            break;
        }

        void *ptr = NULL;
        int pitches[3];

        fberr = bufferSurface->Lock(bufferSurface, DSLF_WRITE, &ptr,
                                    &pitches[0]);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock buffer" + ENOFB);
            fberr = (DFBResult)bufferSurface->Release(bufferSurface);
            if (DFB_OK != fberr)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to release buffer" + ENOFB);
            }
            break;
        }

        unsigned char *bufferSurfaceData = (unsigned char*) ptr;
        buffers[bufferSurfaceData] = bufferSurface;

        pitches[1] = pitches[0] >> 1;
        pitches[2] = pitches[0] >> 1;

        int offsets[3];
        offsets[0] = 0;
        offsets[1] = pitches[0] * desc.height;
        offsets[2] = offsets[1] + pitches[1] * (desc.height >> 1);

        int size = offsets[2] + pitches[2] * (desc.height >> 1);

        if (DSPF_YV12 == desc.pixelformat)
        {
            swap(pitches[1], pitches[2]);
            swap(offsets[1], offsets[2]);
        }

        YUVInfo tmp(desc.width, desc.height, size, pitches, offsets);

        bufs.push_back(bufferSurfaceData);
        yuvinfo.push_back(tmp);

        fberr = bufferSurface->Unlock(bufferSurface);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to unlock buffer" + ENOFB);
            break;
        }
    }

    if (DFB_OK != fberr)
    {
        for (uint i = 0; i < bufs.size(); i++)
        {
            BufferMap::iterator it = buffers.find(bufs[i]);
            if (it != buffers.end())
            {
                IDirectFBSurface *surf = *it;

                fberr = surf->Unlock(surf);
                if (DFB_OK != fberr)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Failed to unlock buffer" + ENOFB);
                }

                fberr = (DFBResult)surf->Release(surf);
                if (DFB_OK != fberr)
                {
                    VERBOSE(VB_IMPORTANT, LOC_ERR +
                            "Failed to release buffer" + ENOFB);
                }

                buffers.erase(it);
            }
        }
        return false;
    }

    bool ok = vbuffers.CreateBuffers(desc.width, desc.height, bufs, yuvinfo);

    return ok;
}

void DirectfbData::DeleteBuffers(VideoBuffers &vbuffers)
{
    QMutexLocker locker(&bufferLock);

    for (uint i = 0; i < vbuffers.allocSize(); i++)
    {
        vbuffers.at(i)->buf = NULL;

        if (vbuffers.at(i)->qscale_table)
        {
            delete [] vbuffers.at(i)->qscale_table;
            vbuffers.at(i)->qscale_table = NULL;
        }
    }

    BufferMap::iterator it;
    for (it = buffers.begin() ; it != buffers.end() ; it++)
    {
        IDirectFBSurface *surf = *it;
        if (surf)
        {
            DFBResult fberr = surf->Unlock(surf);
            if (DFB_OK != fberr)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to unlock buffer" + ENOFB);
            }

            fberr = (DFBResult)surf->Release(surf);
            if (DFB_OK != fberr)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Failed to release buffer" + ENOFB);
            }
        }
    }

    buffers.clear();
}

void VideoOutputDirectfb::GetRenderOptions(render_opts &opts,
                                           QStringList &cpudeints)
{
    opts.renderers->append("directfb");
    opts.deints->insert("directfb", cpudeints);
    (*opts.osds)["directfb"].append("softblend");
    (*opts.safe_renderers)["dummy"].append("directfb");
    (*opts.safe_renderers)["nuppel"].append("directfb");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("directfb");
    if (opts.decoders->contains("libmpeg2"))
        (*opts.safe_renderers)["libmpeg2"].append("directfb");
    opts.priorities->insert("directfb", 60);
}

VideoOutputDirectfb::VideoOutputDirectfb()
    : VideoOutput(), XJ_started(false), widget(NULL),
      data(new DirectfbData())
{
    init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
}

VideoOutputDirectfb::~VideoOutputDirectfb()
{
    if (!data)
        return;

    data->DeleteBuffers(vbuffers);

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

    XJ_started = false;

    delete data;
    data = NULL;
}

/// Correct for underalignment
static QSize fix_alignment(QSize raw)
{
    return QSize((raw.width()  + 15) & (~0xf),
                 (raw.height() + 15) & (~0xf));
}

bool VideoOutputDirectfb::Init(int width, int height, float aspect, WId winid,
                               int winx, int winy, int winw, int winh,
                               MythCodecID codec_id, WId embedid)
{
    // Hack to avoid embedded video output...
    if ((winw < 320) || (winh < 240))
    {
        return false;
    }

    widget = QWidget::find(winid);

    // Setup DirectFB
    DFBResult fberr = DirectFBInit(NULL, NULL);
    if (DFB_OK != fberr)
    {
        return false;
    }

    DirectFBSetOption("bg-none", NULL);
    DirectFBSetOption("no-cursor", NULL);

    fberr = DirectFBCreate(&(data->dfb));
    if (DFB_OK != fberr)
    {
        return false;
    }

    fberr = data->dfb->SetCooperativeLevel(data->dfb, DFSCL_FULLSCREEN);
    if (DFB_OK != fberr)
    {
        return false;
    }

    // Setup primary layer (this is like the X11 root window)
    fberr = data->dfb->GetDisplayLayer(
        data->dfb, DLID_PRIMARY, &(data->primaryLayer));
    if (DFB_OK != fberr)
    {
        return false;
    }

    DFBDisplayLayerConfig conf;
    fberr = data->primaryLayer->GetConfiguration(data->primaryLayer, &conf);
    if (DFB_OK != fberr)
    {
        return false;
    }

    if ((winw != conf.width) || (winh != conf.height))
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Window size mismatch "
                <<QString("%1x%2 -> %3x%4")
                .arg(winw).arg(winh).arg(conf.width).arg(conf.height));
    }

    window.SetDisplayVisibleRect(
        QRect(winx, winy, conf.width, conf.height));

    // We can't query the physical dimentions of the screen so use DB..
    QSize display_dim = db_display_dim;

    if (!display_dim.height() || !display_dim.width())
        display_dim = QSize(400, 300);

    window.SetDisplayAspect(
        ((float)(display_dim.width())) / ((float)(display_dim.height())));

    window.SetDisplayDim(display_dim);

    const QRect display_visible_rect = window.GetDisplayVisibleRect();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("output : screen pixel size %1x%2")
            .arg(display_visible_rect.width())
            .arg(display_visible_rect.height()));

    // Determine if we can use blit
#if (DIRECTFB_MAJOR_VERSION == 0) && \
    (DIRECTFB_MINOR_VERSION <= 9) && \
    (DIRECTFB_MICRO_VERSION <= 22)
    DFBCardCapabilities info;
    bzero(&info, sizeof(DFBCardCapabilities));
    fberr = data->dfb->GetCardCapabilities(data->dfb, &info);
#else
    DFBGraphicsDeviceDescription info;
    bzero(&info, sizeof(DFBGraphicsDeviceDescription));
    fberr = data->dfb->GetDeviceDescription(data->dfb, &info);
#endif

    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to query card capabilities" + ENOFB);
        return false;
    }

    data->has_blit = info.acceleration_mask & DFXL_BLIT;
    VERBOSE(VB_PLAYBACK,
            QString("DirectFB output : card : %1hardware blit support")
            .arg(((data->has_blit) ? "" : "No ")));

    // Clear primary layer
    DFBSurfaceDescription desc;
    desc.flags = DSDESC_CAPS;
    desc.caps = DSCAPS_PRIMARY;
    if (data->has_blit)
        desc.caps = (DFBSurfaceCapabilities)(desc.caps | DSCAPS_DOUBLE);

    fberr = data->dfb->CreateSurface(
        data->dfb, &desc, &(data->primarySurface));
    if (DFB_OK != fberr)
    {
        return false;
    }

    fberr = data->primarySurface->Clear(data->primarySurface, 0, 0, 0, 0xff);
    if (DFB_OK != fberr)
    {
        return false;
    }

    fberr = data->primarySurface->Flip(data->primarySurface, 0, DSFLIP_ONSYNC);
    if (DFB_OK != fberr)
    {
        return false;
    }

    const QSize video_dim = fix_alignment(QSize(width, height));
    window.SetVideoDim(video_dim);

    // Find an output layer that supports a format we can deal with.
    // begin with the video format we have as input, fall back to others

    data->videoLayerConfig.flags = (DFBDisplayLayerConfigFlags)
        (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT);
    data->videoLayerConfig.width = video_dim.width();
    data->videoLayerConfig.height = video_dim.height();
    data->videoLayerConfig.pixelformat = DSPF_I420;

    fberr = data->dfb->EnumDisplayLayers(data->dfb, layer_cb, data);
    if (DFB_OK != fberr)
    {
        return false;
    }

    if (!data->videoLayer)
    {
        data->videoLayerConfig.pixelformat = DSPF_YV12;
        fberr = data->dfb->EnumDisplayLayers(data->dfb, layer_cb, data);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN +
                    "Failed to enumerate layers" + ENOFB);
        }

        if (!data->videoLayer)
        {
            fberr = data->primaryLayer->TestConfiguration(
                data->primaryLayer, &(data->videoLayerConfig), NULL);
            if (DFB_OK != fberr)
            {
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                        "Could not find usable video output layer");

                return false;
            }

            data->primaryLayer->AddRef(data->primaryLayer);
            data->videoLayer = data->primaryLayer;
        }
    }

    QString fb_format =
        (data->videoLayerConfig.pixelformat == DSPF_I420) ? "I420" : "YV12";

    // Force exclusive ownership of the video output layer
    fberr = data->videoLayer->SetCooperativeLevel(
        data->videoLayer, DLSCL_EXCLUSIVE);
    if (DFB_OK != fberr)
    {
        return false;
    }

    // Determine buffering capacities
    data->videoLayerConfig.flags = (DFBDisplayLayerConfigFlags)
        (data->videoLayerConfig.flags | DLCONF_BUFFERMODE);

    // Try triple buffering in video memory
    data->videoLayerConfig.buffermode = DLBM_TRIPLE;
    if (data->videoLayer->TestConfiguration(
            data->videoLayer, &(data->videoLayerConfig), NULL))
    {
        // On failure, try double buffering in video memory
        data->videoLayerConfig.buffermode = DLBM_BACKVIDEO;
        if (data->videoLayer->TestConfiguration(
                data->videoLayer, &(data->videoLayerConfig), NULL))
        {
            // On failure, fall back to double buffering in system memory
            data->videoLayerConfig.buffermode = DLBM_BACKSYSTEM;
        }
    }

    QString buf_desc =
        (data->videoLayerConfig.buffermode == DLBM_TRIPLE) ?
        "triple (video memory)" :
        ((data->videoLayerConfig.buffermode == DLBM_BACKVIDEO) ?
         "double (video memory)" : "double (system memory)");

    fberr = data->videoLayer->SetConfiguration(
        data->videoLayer, &(data->videoLayerConfig));
    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Failed to initialize %1 buffering.")
                .arg(buf_desc) + ENOFB);

        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("output : videoLayer : %1 : %2x%3, %4, %5 buffering")
            .arg(data->videoLayerDesc.name)
            .arg(data->videoLayerConfig.width)
            .arg(data->videoLayerConfig.height)
            .arg(fb_format).arg(buf_desc));

    // Set up video output videoSurface
    fberr = data->videoLayer->GetSurface(
        data->videoLayer, &(data->videoSurface));
    if (DFB_OK != fberr)
    {
        return false;
    }

    fberr = data->videoSurface->SetBlittingFlags(
        data->videoSurface, DSBLIT_NOFX);
    if (DFB_OK != fberr)
    {
        return false;
    }

    fberr = data->videoSurface->GetCapabilities(
        data->videoSurface, &data->videoSurfaceCapabilities);
    if (DFB_OK != fberr)
    {
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("output : videoSurface : %1, %2, %3")
            .arg((data->videoSurfaceCapabilities & DSCAPS_VIDEOONLY) ?
                 "in video memory" : "in sytem memory")
            .arg((data->videoSurfaceCapabilities & DSCAPS_PRIMARY) ?
                 "primary surface" : "no primary surface")
            .arg((data->videoSurfaceCapabilities & DSCAPS_INTERLACED) ?
                 "interlaced" : "not interlaced"));

    // Setup keyboard input handling
    fberr = data->dfb->CreateInputEventBuffer(
        data->dfb, DICAPS_KEYS, (DFBBoolean)1, &(data->inputbuf));
    if (DFB_OK != fberr)
    {
        return false;
    }

    // Setup video input buffers
    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    desc.flags = (DFBSurfaceDescriptionFlags)
        (DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT);
    desc.width = video_dim.width();
    desc.height = video_dim.height();
    desc.pixelformat = data->videoLayerConfig.pixelformat;

    if (!data->CreateBuffers(vbuffers, desc))
    {
        return false;
    }

    VERBOSE(VB_PLAYBACK, LOC +
            QString("input : %1 videoSurface buffers : %1x%2, %3")
            .arg(kNumBuffers).arg(desc.width).arg(desc.height)
            .arg(desc.pixelformat == DSPF_I420 ? "I420" : "YV12"));

    // Create buffers
    init(&pauseFrame, vbuffers.GetScratchFrame()->codec,
         new unsigned char[vbuffers.GetScratchFrame()->size + 64],
         vbuffers.GetScratchFrame()->width, vbuffers.GetScratchFrame()->height,
         vbuffers.GetScratchFrame()->bpp,   vbuffers.GetScratchFrame()->size,
         vbuffers.GetScratchFrame()->pitches,
         vbuffers.GetScratchFrame()->offsets);

    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    // Initialize base class
    if (!VideoOutput::Init(width, height, aspect, winid,
                           display_visible_rect.x(),
                           display_visible_rect.y(),
                           display_visible_rect.width(),
                           display_visible_rect.height(),
                           codec_id, embedid))
    {
        return false;
    }

    MoveResize();

    InitPictureAttributes();

    // Make video layer completely opaque
    fberr = data->videoLayer->SetOpacity(data->videoLayer, 0xff);
    if (DFB_OK != fberr)
    {
        return false;
    }

    XJ_started = true;

    return true;
}

void VideoOutputDirectfb::PrepareFrame(VideoFrame *frame, FrameScanType,
                                       OSD *osd)
{
    (void)osd;
    QMutexLocker locker(&data->bufferLock);
    DFBResult fberr = DFB_OK;

    if (!frame)
        frame = vbuffers.GetScratchFrame();

    framesPlayed = frame->frameNumber + 1;

    IDirectFBSurface *bufferSurface = data->buffers[frame->buf];
    if (!bufferSurface)
        return;

    if (data->has_blit)
    {
        fberr = data->videoSurface->Blit(
            data->videoSurface, bufferSurface, NULL, 0, 0);

        if (fberr)
            VERBOSE(VB_IMPORTANT, LOC_ERR + "Blit failed" + ENOFB);

        return;
    }

    void *vsrc = NULL, *vdst = NULL;
    int y_src_pitch = 0, y_dst_pitch = 0;

    fberr = bufferSurface->Lock(
        bufferSurface, DSLF_READ, &vsrc, &y_src_pitch);

    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock buf surf" + ENOFB);
        return;
    }

    fberr = data->videoSurface->Lock(
        data->videoSurface, DSLF_WRITE, &vdst, &y_dst_pitch);

    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock video surf" + ENOFB);

        fberr = bufferSurface->Unlock(bufferSurface);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to unlock buf surf" + ENOFB);
        }

        return;
    }

    unsigned char *src = (unsigned char*) vsrc;
    unsigned char *dst = (unsigned char*) vdst;

    DFBSurfacePixelFormat fmt;
    data->videoSurface->GetPixelFormat(data->videoSurface, &fmt);

    int pitches[3];
    pitches[0] = y_dst_pitch;
    pitches[1] = pitches[2] = (y_dst_pitch>>1);

    int offsets[3] = { 0, 0, 0, };
    if (DSPF_YV12 == fmt)
    {
        offsets[1] = offsets[0] + pitches[0] * frame->height;
        offsets[2] = offsets[1] + pitches[1] * (frame->height>>1);
    }
    else if (DSPF_I420 == fmt)
    {
        offsets[1] = offsets[0] + pitches[0] * frame->height;
        offsets[2] = offsets[1] + pitches[1] * (frame->height>>1);
    }
    else
    {
        VERBOSE(VB_IMPORTANT, "Unknown Pixel Format: 0x"<<hex<<fmt<<dec);
    }

    if ((DSPF_YV12 == fmt) || (DSPF_I420 == fmt))
    {
        VideoFrame src_frame;
        init(&src_frame, FMT_YV12, src, frame->width, frame->height,
             frame->bpp, frame->size, frame->pitches, frame->offsets);

        VideoFrame dst_frame;
        init(&dst_frame, FMT_YV12, dst, frame->width, frame->height, 12,
             frame->offsets[2] + (pitches[2]>>1) * (frame->height>>1),
             pitches, offsets);

        CopyFrame(&dst_frame, &src_frame);

#if 0
        cerr<<"off: "<<dst_frame.offsets[0]<<","
            <<dst_frame.offsets[1]<<","
            <<dst_frame.offsets[2]
            <<" p: "<<dst_frame.pitches[0]<<","
            <<dst_frame.pitches[1]<<","
            <<dst_frame.pitches[2]
            <<" WxH: "<<dst_frame.width<<"x"<<dst_frame.height
            <<" size: "<<dst_frame.size<<endl;
#endif

#if 0
        memset(dst_frame.buf + dst_frame.offsets[0], 0x7f,
               (dst_frame.pitches[0]) * (dst_frame.height>>1));

        bzero(dst_frame.buf + dst_frame.offsets[1],
              (dst_frame.pitches[1]) * (dst_frame.height>>2));

        bzero(dst_frame.buf + dst_frame.offsets[2],
              (dst_frame.pitches[2]) * (dst_frame.height>>2));
#endif
    }

    fberr = data->videoSurface->Unlock(data->videoSurface);
    if (DFB_OK != fberr)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to unlock video surf" + ENOFB);

    fberr = bufferSurface->Unlock(bufferSurface);
    if (DFB_OK != fberr)
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to unlock buf surf" + ENOFB);
}

void VideoOutputDirectfb::Show(FrameScanType)
{
    DFBResult fberr = DFB_OK;

    fberr = data->videoSurface->Flip(data->videoSurface, NULL, DSFLIP_ONSYNC);

    DFBInputEvent event;
    fberr = data->inputbuf->GetEvent(data->inputbuf, DFB_EVENT(&event));

    if (DFB_OK != fberr)
        return;

    QMap<uint,QtKey>::const_iterator it = data->qtKeyMap.find(event.key_id);
    if (it == data->qtKeyMap.end())
        return;

    QtKey      key      = *it;
    QKeyEvent *keyevent = NULL;

#ifdef QT3_SUPPORT
    if (event.type == DIET_KEYPRESS)
        keyevent = new QKeyEvent(QEvent::KeyPress,   key.key, key.ascii, 0);
    else if (event.type == DIET_KEYRELEASE)
        keyevent = new QKeyEvent(QEvent::KeyRelease, key.key, key.ascii, 0);
#else
    if (event.type == DIET_KEYPRESS)
    {
        keyevent = new QKeyEvent(QEvent::KeyPress,   key.key, 0,
                                 QString((char)key.ascii));
    }
    else if (event.type == DIET_KEYRELEASE)
    {
        keyevent = new QKeyEvent(QEvent::KeyRelease, key.key, 0,
                                 QString((char)key.ascii));
    }
#endif

    if (keyevent)
        QCoreApplication::postEvent(widget, keyevent);
}

void VideoOutputDirectfb::UpdatePauseFrame(void)
{
    VideoFrame *used_frame = vbuffers.head(kVideoBuffer_used);
    if (!used_frame)
        used_frame = vbuffers.GetScratchFrame();


    IDirectFBSurface *bufferSurface = data->buffers[used_frame->buf];
    if (!bufferSurface)
        return;

    void *vsrc = NULL;
    int y_src_pitch = 0;
    DFBResult fberr = bufferSurface->Lock(
        bufferSurface, DSLF_READ, &vsrc, &y_src_pitch);
    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock scratch surf" + ENOFB);
        return;
    }

    unsigned char *src = (unsigned char*) vsrc;

    VideoFrame src_frame;
    init(&src_frame, FMT_YV12, src,
         used_frame->width, used_frame->height, used_frame->bpp,
         used_frame->size, used_frame->pitches, used_frame->offsets);

    CopyFrame(&pauseFrame, &src_frame);

    fberr = bufferSurface->Unlock(bufferSurface);
    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to unlock scratch surf" + ENOFB);
    }
}

void VideoOutputDirectfb::ProcessFrame(VideoFrame *frame, OSD *osd,
                                       FilterChain *filterList,
                                       const PIPMap &pipPlayers,
                                       FrameScanType scan)
{
    bool copy_from_pause = false;
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        copy_from_pause = true;
    }

    IDirectFBSurface *bufferSurface = data->buffers[frame->buf];
    if (!bufferSurface)
        return;

    void *vsrc = NULL;
    int y_src_pitch = 0;
    DFBResult fberr = bufferSurface->Lock(
        bufferSurface, DSLF_READ, &vsrc, &y_src_pitch);
    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR + "Failed to lock process surf" + ENOFB);
        return;
    }

    unsigned char *src = (unsigned char*) vsrc;

    VideoFrame mem_frame;
    init(&mem_frame, FMT_YV12, src,
         frame->width, frame->height, frame->bpp,
         frame->size, frame->pitches, frame->offsets);

    if (copy_from_pause)
        CopyFrame(&mem_frame, &pauseFrame);

    if (m_deinterlacing && m_deintFilter != NULL)
        m_deintFilter->ProcessFrame(&mem_frame, scan);

    if (filterList)
        filterList->ProcessFrame(&mem_frame);

    ShowPIPs(&mem_frame, pipPlayers);
    DisplayOSD(&mem_frame, osd);

    fberr = bufferSurface->Unlock(bufferSurface);
    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
                "Failed to unlock process surf" + ENOFB);
    }
}

bool VideoOutputDirectfb::InputChanged(const QSize &input_size,
                                       float        aspect,
                                       MythCodecID  av_codec_id,
                                       void        *codec_private,
                                       bool        &aspect_only)
{
    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private,
                              aspect_only);

    DFBSurfaceDescription desc;
    bzero(&desc, sizeof(DFBSurfaceDescription));

    desc.flags = (DFBSurfaceDescriptionFlags)
        (DSDESC_HEIGHT | DSDESC_WIDTH | DSDESC_PIXELFORMAT);
    const QSize video_dim = window.GetVideoDim();
    desc.width  = video_dim.width();
    desc.height = video_dim.height();
    desc.pixelformat = data->videoLayerConfig.pixelformat;

    data->DeleteBuffers(vbuffers);
    data->CreateBuffers(vbuffers, desc);
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

    return true;
}

// this is documented in videooutbase.cpp
void VideoOutputDirectfb::Zoom(ZoomDirection direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputDirectfb::MoveResize(void)
{
    VideoOutput::MoveResize();

    const QRect display_visible_rect = window.GetDisplayVisibleRect();
    const QRect display_video_rect   = window.GetDisplayVideoRect();
    VERBOSE(VB_PLAYBACK, LOC +
            QString("MoveResize : screen size %1x%2, "
                    "proposed x : %3, y : %4, w : %5, h : %6")
            .arg(display_visible_rect.width())
            .arg(display_visible_rect.height())
            .arg(display_video_rect.left())
            .arg(display_video_rect.top())
            .arg(display_video_rect.width())
            .arg(display_video_rect.height()));

    // TODO FIXME support for zooming when
    // dispwoff > screenwidth || disphoff > screenheight

    if (data->videoLayerDesc.caps & DLCAPS_SCREEN_LOCATION)
    {
        float dispxoff = display_video_rect.left();
        float dispyoff = display_video_rect.top();
        float dispwoff = display_video_rect.width();
        float disphoff = display_video_rect.height();

        DFBResult fberr = data->videoLayer->SetScreenLocation(
            data->videoLayer,
            dispxoff / display_visible_rect.width(),
            dispyoff / display_visible_rect.height(),
            dispwoff / display_visible_rect.width(),
            disphoff/display_visible_rect.height());

        if (fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_WARN + "MoveResize" + ENOFB);
        }
    }
}

// this is documented in videooutbase.cpp
int VideoOutputDirectfb::SetPictureAttribute(
    PictureAttribute attribute, int newValue)
{
    DFBColorAdjustment adj = { DCAF_NONE, 0x8000, 0x8000, 0x8000, 0x8000, };

    data->videoLayer->GetColorAdjustment(data->videoLayer, &adj);
    adj.flags = DCAF_NONE;

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

QStringList VideoOutputDirectfb::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    if (codec_is_std(myth_codec_id))
    {
        list += "directfb";
    }

    return list;
}

DFBEnumerationResult layer_cb(
    unsigned int id, DFBDisplayLayerDescription desc, void *data)
{
    class DirectfbData *vodata = (DirectfbData*) data;

    if (id == DLID_PRIMARY)
        return DFENUM_OK;

    DFBResult fberr = vodata->dfb->GetDisplayLayer(
        vodata->dfb, id, &(vodata->videoLayer));

    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN +
                "Failed to get display layer" + ENOFB);

        return DFENUM_OK;
    }

    VERBOSE(VB_PLAYBACK, QString("DirectFB Layer %1 %2:")
            .arg(id).arg(desc.name));

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
        VERBOSE(VB_PLAYBACK, "  - Can deinterlace.");

    if (desc.caps & DLCAPS_OPACITY)
        VERBOSE(VB_PLAYBACK, "  - Supports blending with global alpha.");

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

    fberr = vodata->videoLayer->TestConfiguration(
        vodata->videoLayer, &(vodata->videoLayerConfig), NULL);

    if (DFB_OK != fberr)
    {
        VERBOSE(VB_IMPORTANT, LOC_WARN + "Failed to test config" + ENOFB);

        fberr = (DFBResult)vodata->videoLayer->Release(vodata->videoLayer);
        if (DFB_OK != fberr)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                    "Failed to release display layer" + ENOFB);
        }

        return DFENUM_OK;
    }

    vodata->videoLayerDesc = desc;

    return DFENUM_CANCEL;
}

static void init_keymap(QMap<uint,QtKey> &qtKeyMap)
{
    qtKeyMap[DIKI_UNKNOWN]    = QtKey(0xffff, 0x00); // unknown key

    qtKeyMap[DIKI_A]          = QtKey(0x41,   0x61); // a...z
    qtKeyMap[DIKI_B]          = QtKey(0x42,   0x62);
    qtKeyMap[DIKI_C]          = QtKey(0x43,   0x63);
    qtKeyMap[DIKI_D]          = QtKey(0x44,   0x64);
    qtKeyMap[DIKI_E]          = QtKey(0x45,   0x65);
    qtKeyMap[DIKI_F]          = QtKey(0x46,   0x66);
    qtKeyMap[DIKI_G]          = QtKey(0x47,   0x67);
    qtKeyMap[DIKI_H]          = QtKey(0x48,   0x68);
    qtKeyMap[DIKI_I]          = QtKey(0x49,   0x69);
    qtKeyMap[DIKI_J]          = QtKey(0x4a,   0x6a);
    qtKeyMap[DIKI_K]          = QtKey(0x4b,   0x8b);
    qtKeyMap[DIKI_L]          = QtKey(0x4c,   0x6c);
    qtKeyMap[DIKI_M]          = QtKey(0x4d,   0x6d);
    qtKeyMap[DIKI_N]          = QtKey(0x4e,   0x6e);
    qtKeyMap[DIKI_O]          = QtKey(0x4f,   0x6f);
    qtKeyMap[DIKI_P]          = QtKey(0x50,   0x70);
    qtKeyMap[DIKI_Q]          = QtKey(0x51,   0x71);
    qtKeyMap[DIKI_R]          = QtKey(0x52,   0x72);
    qtKeyMap[DIKI_S]          = QtKey(0x53,   0x73);
    qtKeyMap[DIKI_T]          = QtKey(0x54,   0x74);
    qtKeyMap[DIKI_U]          = QtKey(0x55,   0x75);
    qtKeyMap[DIKI_V]          = QtKey(0x56,   0x76);
    qtKeyMap[DIKI_W]          = QtKey(0x57,   0x77);
    qtKeyMap[DIKI_X]          = QtKey(0x58,   0x78);
    qtKeyMap[DIKI_Y]          = QtKey(0x59,   0x79);
    qtKeyMap[DIKI_Z]          = QtKey(0x5a,   0x7a);

    qtKeyMap[DIKI_0]          = QtKey(0x30,   0x30); // 0-9
    qtKeyMap[DIKI_1]          = QtKey(0x31,   0x31);
    qtKeyMap[DIKI_2]          = QtKey(0x32,   0x32);
    qtKeyMap[DIKI_3]          = QtKey(0x33,   0x33);
    qtKeyMap[DIKI_4]          = QtKey(0x34,   0x34);
    qtKeyMap[DIKI_5]          = QtKey(0x35,   0x35);
    qtKeyMap[DIKI_6]          = QtKey(0x36,   0x36);
    qtKeyMap[DIKI_7]          = QtKey(0x37,   0x37);
    qtKeyMap[DIKI_8]          = QtKey(0x38,   0x38);
    qtKeyMap[DIKI_9]          = QtKey(0x39,   0x39);

    qtKeyMap[DIKI_F1]         = QtKey(0x1030, 0x00); // function keys F1 - F12
    qtKeyMap[DIKI_F2]         = QtKey(0x1031, 0x00);
    qtKeyMap[DIKI_F3]         = QtKey(0x1032, 0x00);
    qtKeyMap[DIKI_F4]         = QtKey(0x1033, 0x00);
    qtKeyMap[DIKI_F5]         = QtKey(0x1034, 0x00);
    qtKeyMap[DIKI_F6]         = QtKey(0x1035, 0x00);
    qtKeyMap[DIKI_F7]         = QtKey(0x1036, 0x00);
    qtKeyMap[DIKI_F8]         = QtKey(0x1037, 0x00);
    qtKeyMap[DIKI_F9]         = QtKey(0x1038, 0x00);
    qtKeyMap[DIKI_F10]        = QtKey(0x1039, 0x00);
    qtKeyMap[DIKI_F11]        = QtKey(0x103a, 0x00);
    qtKeyMap[DIKI_F12]        = QtKey(0x103b, 0x00);

    qtKeyMap[DIKI_SHIFT_L]    = QtKey(0x1020, 0x00); // Shift Left
    qtKeyMap[DIKI_SHIFT_R]    = QtKey(0x1020, 0x00); // Shift Right
    qtKeyMap[DIKI_CONTROL_L]  = QtKey(0x1021, 0x00); // Control Left
    qtKeyMap[DIKI_CONTROL_R]  = QtKey(0x1021, 0x00); // Control Right
    qtKeyMap[DIKI_ALT_L]      = QtKey(0x1023, 0x00); // ALT Left
    qtKeyMap[DIKI_ALT_R]      = QtKey(0x1023, 0x00); // ALT Right
    qtKeyMap[DIKI_META_L]     = QtKey(0x1022, 0x00); // META Left
    qtKeyMap[DIKI_META_R]     = QtKey(0x1022, 0x00); // META Right
    qtKeyMap[DIKI_SUPER_L]    = QtKey(0x1053, 0x00); // Super Left
    qtKeyMap[DIKI_SUPER_R]    = QtKey(0x1054, 0x00); // Super Right
    qtKeyMap[DIKI_HYPER_L]    = QtKey(0x1056, 0x00); // Hyper Left
    qtKeyMap[DIKI_HYPER_R]    = QtKey(0x1057, 0x00); // Hyper Right

    qtKeyMap[DIKI_CAPS_LOCK]  = QtKey(0x1024, 0x00); // CAPS Lock
    qtKeyMap[DIKI_NUM_LOCK]   = QtKey(0x1025, 0x00); // NUM Locka
    qtKeyMap[DIKI_SCROLL_LOCK]= QtKey(0x1026, 0x00); // Scroll Lock

    qtKeyMap[DIKI_ESCAPE]     = QtKey(0x1000, 0x1b); // Escape
    qtKeyMap[DIKI_LEFT]       = QtKey(0x1012, 0x00); // Left
    qtKeyMap[DIKI_RIGHT]      = QtKey(0x1014, 0x00); // Right
    qtKeyMap[DIKI_UP]         = QtKey(0x1013, 0x00); // Up
    qtKeyMap[DIKI_DOWN]       = QtKey(0x1015, 0x00); // Down
    qtKeyMap[DIKI_TAB]        = QtKey(0x1001, 0x09); // Tab
    qtKeyMap[DIKI_ENTER]      = QtKey(0x1004, 0x0d); // Enter
    qtKeyMap[DIKI_SPACE]      = QtKey(0x20,   0x20); // Space
    qtKeyMap[DIKI_BACKSPACE]  = QtKey(0x1003, 0x00); // Backspace
    qtKeyMap[DIKI_INSERT]     = QtKey(0x1006, 0x00); // Insert
    qtKeyMap[DIKI_DELETE]     = QtKey(0x1007, 0x7f); // Delete
    qtKeyMap[DIKI_HOME]       = QtKey(0x1010, 0x00); // Home
    qtKeyMap[DIKI_END]        = QtKey(0x1011, 0x00); // End
    qtKeyMap[DIKI_PAGE_UP]    = QtKey(0x1016, 0x00); // Page Up
    qtKeyMap[DIKI_PAGE_DOWN]  = QtKey(0x1017, 0x00); // Page Down
    qtKeyMap[DIKI_PRINT]      = QtKey(0x1009, 0x00); // Print
    qtKeyMap[DIKI_PAUSE]      = QtKey(0x1008, 0x00); // Pause

    qtKeyMap[DIKI_QUOTE_LEFT] = QtKey(0x60,   0x27); // Quote Left
    qtKeyMap[DIKI_MINUS_SIGN] = QtKey(0x2d,   0x2d); // Minus
    qtKeyMap[DIKI_EQUALS_SIGN]= QtKey(0x3d,   0x3d); // Equals
    qtKeyMap[DIKI_BRACKET_LEFT]=QtKey(0x5b,   0x5b); // Bracket Left
    qtKeyMap[DIKI_BRACKET_RIGHT]=QtKey(0x5d,   0x5d); // Bracket Right
    qtKeyMap[DIKI_BACKSLASH]  = QtKey(0x5c,   0x5c); // Backslash
    qtKeyMap[DIKI_SEMICOLON]  = QtKey(0x3b,   0x3b); // Semicolon
    //qtKeyMap[DIKI_QUOTE_RIGHT]   = QtKey(0xffff, 0x00);
    qtKeyMap[DIKI_COMMA]      = QtKey(0x2c,   0x2c); // Comma
    qtKeyMap[DIKI_PERIOD]     = QtKey(0x2e,   0x2e); // Period
    qtKeyMap[DIKI_SLASH]      = QtKey(0x2f,   0x2f); // Slash

    qtKeyMap[DIKI_LESS_SIGN]  = QtKey(0x3c,   0x3c); // Less Than

    // keypad keys

    qtKeyMap[DIKI_KP_DIV]     = QtKey(0xf7,   '/'); // keypad div
    qtKeyMap[DIKI_KP_MULT]    = QtKey(0xd7,   '*'); // keypad mult
    qtKeyMap[DIKI_KP_MINUS]   = QtKey(0x2d,   '-'); // keypad minus
    qtKeyMap[DIKI_KP_PLUS]    = QtKey(0x2b,   '+'); // keypad plus
    qtKeyMap[DIKI_KP_ENTER]   = QtKey(0x1004, '\r'); // keypad enter
    qtKeyMap[DIKI_KP_SPACE]   = QtKey(0x20,   ' '); // keypad space
    qtKeyMap[DIKI_KP_TAB]     = QtKey(0x1001, '\t'); // keypad tab
    qtKeyMap[DIKI_KP_F1]      = QtKey(0x1030, 0x00); // keypad F1
    qtKeyMap[DIKI_KP_F2]      = QtKey(0x1031, 0x00); // keypad F2
    qtKeyMap[DIKI_KP_F3]      = QtKey(0x1032, 0x00); // keypad F3
    qtKeyMap[DIKI_KP_F4]      = QtKey(0x1033, 0x00); // keypad F4
    qtKeyMap[DIKI_KP_EQUAL]   = QtKey(0x3d,   '='); // keypad equal
    //qtKeyMap[DIKI_KP_SEPERATOR] = QtKey(0xffff, 0x00); // keypad seperator

    //qtKeyMap[DIKI_KP_DECIMAL] = QtKey(0xffff, 0x00); // keypad decimal

    qtKeyMap[DIKI_KP_0]       = QtKey(0x30,   0x30); // keypad 0-9
    qtKeyMap[DIKI_KP_1]       = QtKey(0x31,   0x31);
    qtKeyMap[DIKI_KP_2]       = QtKey(0x32,   0x32);
    qtKeyMap[DIKI_KP_3]       = QtKey(0x33,   0x33);
    qtKeyMap[DIKI_KP_4]       = QtKey(0x34,   0x34);
    qtKeyMap[DIKI_KP_5]       = QtKey(0x35,   0x35);
    qtKeyMap[DIKI_KP_6]       = QtKey(0x36,   0x36);
    qtKeyMap[DIKI_KP_7]       = QtKey(0x37,   0x37);
    qtKeyMap[DIKI_KP_8]       = QtKey(0x38,   0x38);
    qtKeyMap[DIKI_KP_9]       = QtKey(0x39,   0x39);
}
