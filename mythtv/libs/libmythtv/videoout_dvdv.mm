// -*- Mode: c++ -*-
// DVDV is based on Accellent by John Dagliesh:
//   http://www.defyne.org/dvb/accellent.html

#include <iostream>
#include <algorithm>
using namespace std;

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <AGL/agl.h>
#import <OpenGL/glext.h>

#include <qmutex.h>
#include <qstring.h>
#include <qptrqueue.h>
#include <qmap.h>
#include <qsize.h>

#include "libmyth/mythconfig.h"
#include "libmyth/mythverbose.h"

#include "videoout_dvdv.h"
#undef ABS
extern "C" {
#include "avcodec.h"
}
#include "dvdv.h"
#include "videoout_dvdv_private.h"
#include "yuv2rgb.h"

// Default number of buffers in Apple's code
const int kAppleBuffers = 4;

// Storage for subpictures during decoding
const int kSubpictureSize = 8 * 1024 * 1024;

// Number of frames we can buffer; this should be
// no less than kNumBuffers from VideoOutput
const int kAccelBuffers = 33;


#define LOC     "DVDV::" << __FUNCTION__ << " "
#define LOC_ERR "DVDV::" << __FUNCTION__ << " - Error: "

/// All the data we need to encapsulate one frame.
class FrameData
{
  public:
    FrameData() : data(NULL), showBuffer(0), vf(NULL) { }

    /// This is the storage pointed to by state and dec.
    uint8        *data;
    /// This is the DVDV state used when decoding to this frame.
    DVDV_CurPtrs  state;
    /// Frame data is constant, store it here instead of new'ing it.
    DVDV_Frame    stateframe;
    /// After the frame is processed, this block will be passed to
    /// DVDVideoDecode.
    DecodeParams  dec;
    /// Which buffer should be shown when calling DVDVideoShow for this frame?
    int           showBuffer;
    /// Which VideoFrame was attached to this buffer?
    VideoFrame   *vf;
};

/// We hide the icky stuff here, to avoid header bloat.
class DVDV_Private
{
  public:
    DVDV_Private() :
        gDVDContext(NULL),    gConn(0),
        gWindowID(0),         gSurfaceID(0),
        gWindow(NULL),        gNsWindow(NULL),
        overlayWindow(NULL),  overlayContext(NULL),
        osdWidth(0),          osdHeight(0),
        yuvTexture(0),        alphaTexture(0),
        osdVisible(false),    curFrame(NULL),
        gBufShow(0),          gBufPast(0),
        gBufFuture(0),        gBufRecent(0),
        mutex(false)
    {
        gWindowContentOrigin.v = 0;
        gWindowContentOrigin.h = 0;
        bzero(gRealRect, sizeof(gRealRect));
        bzero(subpic, sizeof(subpic));
    }

    /// The reference ID for our video decoding.
    DVDVideoContext      *gDVDContext;
    /// These three items are used to interact with the drawing surface
    /// where the video is shown.
    CGSConnectionID       gConn;
    int                   gWindowID;
    CGSSurfaceID          gSurfaceID;
    /// Remember where our content rectangle starts,
    /// so we draw in the correct spot instead of overwriting
    /// the window frame.
    Point                 gWindowContentOrigin;
    /// This holds the measurements for the video size.
    short                 gRealRect[4];

    /// The Carbon hook to our window.
    WindowRef             gWindow;
    /// Cocoa hook to our window.
    NSWindow             *gNsWindow;

    // Our OSD items.
    WindowRef             overlayWindow;
    AGLContext            overlayContext;
    int                   osdWidth;
    int                   osdHeight;
    GLuint                yuvTexture;
    GLuint                alphaTexture;
    bool                  osdVisible;

    // Our buffer of frames, and the map to look
    // up frames by VideoFrame.
    FrameData             gFrames[kAccelBuffers];
    QPtrQueue<FrameData>  freeFrames;
    QPtrQueue<FrameData>  usedFrames;
    QMap<VideoFrame*, FrameData*> usedMap;

    /// A temporary holding area for one FrameData struct.
    FrameData            *curFrame;

    /// Static subpicture storage. (I don't think it's used for anything.)
    uint8                 subpic[kSubpictureSize];

    /// The latest buffer to show. This is a past version of
    /// gBufPresent (below), which tracks the latest version.
    int                   gBufShow;

    // Remember which buffers we've been using. All of these
    // will be set between 0 and 3 when we're running.
    int                   gBufPast;
    int                   gBufFuture;
    int                   gBufRecent;

    /// This mutex synchronizes the decode and show threads.
    QMutex                mutex;
};


DVDV::DVDV() : d(new DVDV_Private())
{
}

DVDV::~DVDV()
{
    if (d)
    {
        Teardown();
        delete d;
        d = NULL;
    }
}

void DVDV::Teardown(void)
{
    QMutexLocker locker(&d->mutex);

    // Disassemble our video buffers
    for (int i = 0; i < kAccelBuffers; i++)
    {
        delete [] d->gFrames[i].data;
        d->gFrames[i].data = NULL;
    }

    while (FrameData *frame = d->usedFrames.dequeue())
    {
        d->usedMap.erase(frame->vf);
        d->freeFrames.enqueue(frame);
    }

    d->curFrame = NULL;

    if (d->gSurfaceID)
    {
        // I don't know how to actually dispose of the
        // surface, so we just hide it out of the way.
        CGRect surBounds = {{-1, 1}, {1, 1}};
        CGSSetSurfaceBounds(d->gConn, d->gWindowID, d->gSurfaceID, surBounds);
        CGSOrderSurface(d->gConn, d->gWindowID, d->gSurfaceID, 0, 0);
        d->gSurfaceID = NULL;
    }

    if (d->gDVDContext)
    {
        DVDVideoClearMP(d->gDVDContext);
        DVDVideoEnableMP(d->gDVDContext, false);
        DVDVideoCloseDevice(d->gDVDContext);
        d->gDVDContext = NULL;
    }

    if (d->overlayContext)
    {
        aglDestroyContext(d->overlayContext);
        d->overlayContext = NULL;
    }

    if (d->overlayWindow)
    {
        DisposeWindow(d->overlayWindow);
        d->overlayWindow = NULL;
    }
}

// Initialize the Accel params with the size of video to be decoded.
bool DVDV::SetVideoSize(const QSize &video_dim)
{
    int width  = video_dim.width();
    int height = video_dim.height();


    if (width < 16   || height < 16 ||
        width > 4096 || height > 2304)
    {
        // Probably a codec problem?
        VERBOSE(VB_PLAYBACK,
                LOC_ERR << QString("Stream has unlikely dimensions! (%1 x %2)")
                .arg(width).arg(height));
        return false;
    }

    QMutexLocker locker(&d->mutex);

    // Set up the video rectangle.
    d->gRealRect[0] = d->gRealRect[1] = 0;
    d->gRealRect[2] = height;
    d->gRealRect[3] = width;

    // We draw into the Myth main window; this is the easiest
    // hack to grab a reference to that window.
    d->gWindow = FrontNonFloatingWindow();

    // Calculate the offset of the content rect inside the
    // window. The CGSurface is oriented relative to the
    // entire window, including the titlebar, etc.
    Rect contentRect, frameRect;
    GetWindowBounds(d->gWindow, kWindowContentRgn, &contentRect);
    GetWindowBounds(d->gWindow, kWindowStructureRgn, &frameRect);
    d->gWindowContentOrigin.v = contentRect.top - frameRect.top;
    d->gWindowContentOrigin.h = contentRect.left - frameRect.left;

    // We need Cocoa for this one call: there's no way
    // in Carbon to get the windowNumber.
    d->gNsWindow = [[NSWindow alloc] initWithWindowRef:d->gWindow];
    d->gWindowID = [d->gNsWindow windowNumber];

    // Set up our window server connection and surface.
    if (!d->gConn)
        d->gConn = _CGSDefaultConnection();

    if (!d->gSurfaceID)
    {
        CGSAddSurface(d->gConn, d->gWindowID, &d->gSurfaceID);
    }

    // Set up the DVD context.
    if (!d->gDVDContext)
    {
        int someOutput[16];
        DVDVideoOpenDevice(CGMainDisplayID(),
                           d->gConn, d->gWindowID, d->gSurfaceID,
                           d->gRealRect,
                           someOutput,
                           &d->gDVDContext);
    }

    // Size the CGSurface to our window size, and show it.
    CGRect surBounds = {{d->gWindowContentOrigin.h,
                         d->gWindowContentOrigin.v},
                        {width, height}};
    CGSSetSurfaceBounds(d->gConn, d->gWindowID, d->gSurfaceID, surBounds);
    CGSOrderSurface(d->gConn, d->gWindowID, d->gSurfaceID, 1, 0);

    // Initialize DVD context.
    DVDVideoSetMPRects(d->gDVDContext, d->gRealRect,
                       d->gSurfaceID, d->gSurfaceID);
    DVDVideoSetMVLevel(d->gDVDContext, 0);
    DVDVideoClearMP(d->gDVDContext);
    DVDVideoEnableMP(d->gDVDContext, true);

    // Initialize the queue of video buffers.
    int numMBs = (d->gRealRect[2] / 16) * (d->gRealRect[3] / 16);
    int dataSize = (sizeof(DVDV_MBInfo) * numMBs * 2) +
        (sizeof(DVDV_DCTElt) * numMBs * 768) +
        (sizeof(uint8_t)     * numMBs * 2);

    for (int i = 0; i < kAccelBuffers; i++)
    {
        FrameData *frame = &d->gFrames[i];

        frame->state.frame = &frame->stateframe;

        memset(&frame->dec, 0, sizeof(frame->dec));
        frame->dec.sevenSixEight = 768;
        frame->dec.p4 = d->subpic;

        frame->showBuffer = kAppleBuffers;
        frame->vf = NULL;

        frame->data = new uint8[dataSize];
        uint8 *ptr = frame->data;
        frame->state.mb = (DVDV_MBInfo *)ptr;
        frame->dec.mbInfo = (MBInfo *)ptr;
        ptr += (sizeof(DVDV_MBInfo) * numMBs * 2);

        frame->state.dct = (DVDV_DCTElt *)ptr;
        frame->dec.dctSpecs = (DCTSpec *)ptr;
        ptr += (sizeof(DVDV_DCTElt) * numMBs * 768);

        frame->state.cbp = ptr;
        frame->dec.cbp = ptr;

        d->freeFrames.enqueue(frame);
    }

    // Set our buffers to be unused.
    d->gBufShow = kAppleBuffers;
    d->gBufPast = d->gBufFuture = d->gBufRecent = 0;

    return true;
}

// Resize and reposition the video subwindow.
void DVDV::MoveResize(int imgx, int imgy, int imgw, int imgh,
                      int dispxoff, int dispyoff,
                      int dispwoff, int disphoff)
{
    // This function handles some zoom modes incorrectly.
    (void) imgx;
    (void) imgy;
    (void) imgw;
    (void) imgh;

    QMutexLocker locker(&d->mutex);

    CGRect surBounds = { { dispxoff + d->gWindowContentOrigin.h,
                           dispyoff + d->gWindowContentOrigin.v },
                         { dispwoff,
                           disphoff } };
    CGSSetSurfaceBounds(d->gConn, d->gWindowID, d->gSurfaceID, surBounds);

    // set up overlay window for OSD
    if (d->overlayContext)
        aglDestroyContext(d->overlayContext);
    if (d->overlayWindow)
        DisposeWindow(d->overlayWindow);

    d->osdWidth = d->gRealRect[3];
    d->osdHeight = d->gRealRect[2];
    d->osdVisible = false;

    Rect parentBounds;
    GetWindowBounds(d->gWindow, kWindowContentRgn, &parentBounds);
    CreateNewWindow(kOverlayWindowClass, kWindowNoAttributes,
                    &parentBounds, &d->overlayWindow);
    ShowWindow(d->overlayWindow);

    // create new  GL context
    GLint attrib[] = { AGL_RGBA, AGL_DOUBLEBUFFER, AGL_NONE };
    AGLPixelFormat fmt = aglChoosePixelFormat(NULL, 0, attrib);
    d->overlayContext = aglCreateContext(fmt, NULL);
    aglSetDrawable(d->overlayContext, GetWindowPort(d->overlayWindow));
    aglSetCurrentContext(d->overlayContext);
    aglDestroyPixelFormat(fmt);

    GLint swap = 0;
    aglSetInteger(d->overlayContext, AGL_SWAP_INTERVAL, &swap);
    aglSetInteger(d->overlayContext, AGL_SURFACE_OPACITY, &swap);

    // Set up view
    glViewport(dispxoff, dispyoff, dispwoff, disphoff);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, d->osdWidth, d->osdHeight, 0, -999999, 999999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glEnable(GL_BLEND);

    // Use rectangular textures
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_TEXTURE_RECTANGLE_EXT);
}

// Update the OSD display.
void DVDV::DrawOSD(unsigned char *y, unsigned char *u,
                   unsigned char *v, unsigned char *alpha)
{
    QMutexLocker locker(&d->mutex);

    if (!y || !u || !v || !alpha)
    {
        // clear screen
        if (d->osdVisible)
        {
            aglSetCurrentContext(d->overlayContext);
            glClear(GL_COLOR_BUFFER_BIT);
            aglSwapBuffers(d->overlayContext);
            d->osdVisible = false;
        }

        return;
    }

    // The following is a very inefficient means to generate an OSD
    // we first convert from a planar I420 format to a the packed 2VUY
    // format in software, and then have OpenGL convert the 2VUY format
    // to an RGB texture. We could convert directly to RGB in software
    // using the appropriate function in yuv2rgb.h, or we could use
    // the I420 to RGB OpenGL 2.0 conversion supported by openglvideo.cpp,
    // or we could render the OSD directly to an RGB buffer.

    // Convert I420 to 2VUY
    conv_i420_2vuy_fun convert = get_i420_2vuy_conv();
    uint8_t *yuvData = new uint8_t[d->osdWidth * d->osdHeight * 2];
    convert(yuvData, d->osdWidth<<1,
            y, u, v,
            d->osdWidth, d->osdWidth>>1, d->osdWidth>>1,
            d->osdWidth, d->osdHeight);

    // Set up drawing
    aglSetCurrentContext(d->overlayContext);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up YUV texture
    if (!d->yuvTexture)
    {
        glGenTextures(1, &d->yuvTexture);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, d->yuvTexture);

        // Apple acceleration
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_STORAGE_HINT_APPLE,
                        GL_STORAGE_SHARED_APPLE);
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, 1);

        // Scale texture anti-aliased
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        // Initialize texture
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_RGBA,
                     d->osdWidth, d->osdHeight, 0,
                     GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_REV_APPLE, yuvData);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, d->yuvTexture);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0,
                        0, 0, d->osdWidth, d->osdHeight,
                        GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_REV_APPLE,
                        yuvData);
    }
    delete[] yuvData;
    yuvData = NULL;

    // Draw YUV texture
    glBlendFunc(GL_DST_ALPHA, GL_ZERO);
    glBlendFunc(GL_ONE, GL_ZERO);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    glTexCoord2f(d->osdWidth, 0);
    glVertex2f(d->osdWidth, 0);
    glTexCoord2f(d->osdWidth, d->osdHeight);
    glVertex2f(d->osdWidth, d->osdHeight);
    glTexCoord2f(0, d->osdHeight);
    glVertex2f(0, d->osdHeight);
    glEnd();

    // Set up alpha texture
    if (!d->alphaTexture)
    {
        glGenTextures(1, &d->alphaTexture);
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, d->alphaTexture);

        // Apple acceleration
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_STORAGE_HINT_APPLE,
                        GL_STORAGE_SHARED_APPLE);
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, 1);

        // Scale texture anti-aliased
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_EXT,
                        GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        // Initialize texture
        glTexImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, GL_ALPHA,
                     d->osdWidth, d->osdHeight, 0,
                     GL_ALPHA, GL_UNSIGNED_BYTE, alpha);
    }
    else
    {
        glBindTexture(GL_TEXTURE_RECTANGLE_EXT, d->alphaTexture);
        glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0,
                        0, 0, d->osdWidth, d->osdHeight,
                        GL_ALPHA, GL_UNSIGNED_BYTE, alpha);
    }

    // Draw alpha texture
    glBlendFunc(GL_ZERO, GL_SRC_ALPHA);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(0, 0);
    glTexCoord2f(d->osdWidth, 0);
    glVertex2f(d->osdWidth, 0);
    glTexCoord2f(d->osdWidth, d->osdHeight);
    glVertex2f(d->osdWidth, d->osdHeight);
    glTexCoord2f(0, d->osdHeight);
    glVertex2f(0, d->osdHeight);
    glEnd();

    // Finish drawing
    glFinishRenderAPPLE();
    aglSwapBuffers(d->overlayContext);

    d->osdVisible = true;
}

// Reset the Accel state.
void DVDV::Reset(void)
{
    QMutexLocker locker(&d->mutex);

    // Set our buffers to be unused.
    d->gBufShow = kAppleBuffers;
    d->gBufPast = d->gBufFuture = d->gBufRecent = 0;

    // Invalidate frames currently in queue.
    while (FrameData *frame = d->usedFrames.dequeue())
    {
        d->usedMap.erase(frame->vf);
        d->freeFrames.enqueue(frame);
    }
}

// Prepare the Accel code for a call to avcodec_decode_video.
bool DVDV::PreProcessFrame(AVCodecContext *context)
{
    QMutexLocker locker(&d->mutex);

    d->curFrame = d->freeFrames.dequeue();
    if (!d->curFrame)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR << "No free frames available!");

        int i = min(d->usedFrames.count(), 5U);
        while (i)
        {
            FrameData *frame = d->usedFrames.dequeue();
            d->usedMap.erase(frame->vf);
            d->freeFrames.enqueue(frame);
            --i;
        }

        d->curFrame = d->freeFrames.dequeue();
        if (!d->curFrame)
        {
            VERBOSE(VB_PLAYBACK,
                    LOC_ERR << "Still no free frames available!");
            return false;
        }
    }

    d->curFrame->showBuffer = kAppleBuffers;
    d->curFrame->vf = NULL;

    if (context->dvdv)
        memcpy(context->dvdv, &d->curFrame->state, sizeof(DVDV_CurPtrs));
    else
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR << "context->dvdv is NULL");
        return false;
    }

    return true;
}

// Process the macroblocks collected by avcodec_decode_video.
void DVDV::PostProcessFrame(AVCodecContext *context,
                            VideoFrame *pic, int pict_type, bool gotpicture)
{
    QMutexLocker locker(&d->mutex);

    // We used to turn decoding off here until the next PreProcessFrame,
    // but I assume that was just a concurrency guard on the global DVDV state

    // get the buffer we used for this run
    FrameData *frame = d->curFrame;
    if (!frame)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR << "No current frame!");
        return;
    }

    // skip if we didn't decode a frame
    if (!gotpicture)
    {
        d->freeFrames.enqueue(frame);
        return;
    }

    if (!context->dvdv)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR << "context->dvdv is NULL");
        d->freeFrames.enqueue(frame);
        return;
    }

    // MPEG data that has hopefully been processed by FFMPEG:
    DVDV_MBInfo *processed = ((DVDV_CurPtrs *)context->dvdv)->mb;

    // skip if we got a bad B frame (often happens at start or after SeekReset)
    if (processed == frame->state.mb ||
        memcmp(processed, frame->state.mb, sizeof(DVDV_MBInfo)) == 0)
    {
        //VERBOSE(VB_PLAYBACK, LOC << "We got some bad MPEG data?");
        d->freeFrames.enqueue(frame);
        return;
    }

    // Remember the VideoFrame for this decode run
    frame->vf = pic;
    d->usedFrames.enqueue(frame);
    d->usedMap[pic] = frame;

    frame->dec.pictType = pict_type;
    frame->dec.alternateScan = frame->state.frame->alternate_scan;

    switch (pict_type)
    {
        case 1: // I frame
        case 2: // P frame
            // find a buffer that isn't the current future buffer
            for (int i = 0; i < kAppleBuffers; i++)
            {
                if ((d->gBufFuture != i) && (d->gBufRecent != i))
                {
                    frame->dec.dstBuf = i;
                    break;
                }
            }

            // what was in the future is now in the past
            d->gBufPast = d->gBufFuture;
            // so display it now
            frame->showBuffer = d->gBufFuture;
            // and we should be using that if this is a P frame
            frame->dec.srcBufL = frame->dec.srcBufR = d->gBufPast;
            // but it is not yet time for the result of this decoding
            d->gBufFuture = frame->dec.dstBuf;
            break;
        case 3: // B frame
            // find a buffer that wasn't used recently
            for (int i = 0; i < kAppleBuffers; i++)
            {
                if ((d->gBufFuture != i) &&
                    (d->gBufPast   != i) &&
                    (d->gBufRecent != i))
                {
                    frame->dec.dstBuf = i;
                    break;
                }
            }

            // and remember that we used it recently
            d->gBufRecent = frame->dec.dstBuf;
            // the present draws on both the past and the future
            frame->dec.srcBufL = d->gBufPast;
            frame->dec.srcBufR = d->gBufFuture;
            // and it is also the one to display
            frame->showBuffer = frame->dec.dstBuf;
            break;
        default:
            VERBOSE(VB_IMPORTANT, QString("Unknown picture type %1")
                    .arg(pict_type));
            break;
    }
}

// Decode a buffered frame
void DVDV::DecodeFrame(VideoFrame *pic)
{
    QMutexLocker locker(&d->mutex);

    // Check if pic is in decode queue
    if (!d->usedMap[pic])
    {
        VERBOSE(VB_PLAYBACK, QString("VF %1 not in decode queue")
                .arg((unsigned long)pic));
        return;
    }

    // Decode frames up to and including the one associated
    // with pic
    while (FrameData *frame = d->usedFrames.dequeue())
    {
        d->usedMap.erase(frame->vf);

        // some frames don't contain valid data
        if (frame->showBuffer < kAppleBuffers)
        {
            DVDVideoDecode(d->gDVDContext, &frame->dec, d->gRealRect, 0);
            d->gBufShow = frame->showBuffer;
        }
        d->freeFrames.enqueue(frame);

        if (frame->vf == pic)
            break;
        //else
            //VERBOSE(VB_PLAYBACK, "Decoding extra VF to catch up");
    }
}

// Draw the most recently decoded video frame to the screen.
void DVDV::ShowFrame(void)
{
    QMutexLocker locker(&d->mutex);

    if (d->gDVDContext && d->gBufShow < kAppleBuffers)
    {
#if 0
        DeinterlaceParams1 dp1;
        bzero(&dp1, sizeof(dp1));
        dp1.buf1 = d->gBufShow;
        dp1.buf2 = d->gBufShow;
        dp1.w1 = 1;
        dp1.w2 = 1;
        dp1.w3 = 2;
        DeinterlaceParams2 dp2;
        bzero(&dp2, sizeof(dp2));
        dp2.unk1 = 0x00020000;
        dp2.usedValue = 1;
        DVDVideoDeinterlace(d->gDVDContext, &dp1, &dp2, 0);
#endif

        ShowBufferParams sp;
        memset(&sp, 0, sizeof(sp));
        sp.h1 = 2;
        sp.w = 1;
        DVDVideoShowMPBuffer(d->gDVDContext, d->gBufShow, &sp, 0);

        // don't show another frame until DecodeFrame loads a new frame
        d->gBufShow = kAppleBuffers;
    }
}
