/********************************************************************************
 * = NAME
 * videoout_quartz.cpp
 *
 * = DESCRIPTION
 * Basic (non hardware-accellerated) video for Mac OS X.
 * Currently, this uses other MythTV libraries to do the colour space
 * conversion, and the Quartz 2D drawing routines to scale and draw.
 *
 * = PERFORMANCE
 * Is poor. If you have a very fast Mac, this module is usable.
 *
 * = STATISTICS
 * With audio disabled and NuppelVideoPlayer delay()s disabled,
 * on my PowerBook G4 1.3GHz, with a 720x576 25fps MPEG2 stream:
 *
 * Depth    |BLIT |BLIT+USE_LIBAVCODEC|USE_LIBAVCODEC|SCALE+USE_LIBAVCODEC
 * ---------+-----+-------------------+--------------+--------------------
 * Thousands+-----+    23-25fps       |   15-16fps   |     9.5-9.8 fps
 * Millions |13fps|    15-16fps       |      16fps   |     9.8-10.8fps
 * =======================================================================
 *
 * = POSSIBLE ENHANCEMENTS 
 * - Create CGImageRefs for each buffer
 *   (currently there is only one whose data is changed each ProcessFrame() )
 * - Use QuickTime to do the colour conversion, scaling and drawing
 * - Use OpenGL in the same way.
 *   I think it will be wiser to implement a separate OpenGL module,
 *   though, since I am sure a lot of the other Unixes could use that
 *
 * = KNOWN BUGS
 * - Changing video resolution, aspect ratio, or zooming is currently ignored
 * - Fullscreen only
 * - yuv2rgb colour conversion makes some mistakes (even with monochrome data)
 * 
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Nigel Pearson
 *******************************************************************************/

// *****************************************************************************
// Configuration:

// Define this if you want to change the display's mode to one
// that more closely matches the resolution of the video stream.
// This may improve performance when scaling (less bytes to copy/scale to))
//#define CHANGE_SCREEN_MODE

// Define this if we want to use libavcodec to do the conversion from YUV.
// This library does have Altivec optimisations, so it should be fastest
// The alternative is to use the yuv2rgb.cpp style conversion
#define USE_LIBAVCODEC

// Define this if we want to scale each frame to either the fullscreen,
// or the correct aspect ratio. The alternative is to just
// copy the image rectangle to the middle of the screen
//#define SCALE_VIDEO

// Drawing technique. Do a dumb memory copy?
#define BLIT

// Default numbers of buffers from some of the other videoout modules:
const int kNumBuffers      = 31;
const int kNeedFreeFrames  = 1;
const int kPrebufferFrames = 12;
const int kKeepPrebuffer   = 2;

// *****************************************************************************

#include <map>
#include <iostream>
using namespace std;

#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_quartz.h"

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <CoreGraphics/CGImage.h>

extern "C" {
    #include "../libavcodec/avcodec.h"
}

#ifndef USE_LIBAVCODEC
    #include "yuv2rgb.h"
#endif


struct QuartzData
{
    // Stored information about the media stream:
    int                srcWidth,
                       srcHeight;
    float              srcAspect;


    // An RGB copy of the media stream:
    uint8_t            *copyBuf;
    int                copyBitsPerSample, copyBytesPerPixel,
                       copyBytesPerRow,   copyBufSize;
    AVPicture          copy;
    enum PixelFormat   copyFormat;


    // What size/position does the user want the stream to be displayed at?
    int                desiredWidth,
                       desiredHeight,
                       desiredXoff,
                       desiredYoff;



    // Information about the display 
    CGDirectDisplayID  theDisplay;
#ifdef CHANGE_SCREEN_MODE
    CFDictionaryRef    originalMode,
                       newMode;
#endif

    int                width,
                       height,
                       bitsPerPixel,
                       bitsPerSample,
                       samplesPerPixel,
                       bytesPerRow;

    float              aspect,
                       refreshRate;



    enum PixelFormat   outputFormat;


    // Structures that we use for drawing:
    CGContextRef       cgContext;	// For smart drawing
    CGImageRef         cgImageRep;
    CGRect             cgRect;
    CGColorSpaceRef    colourSpace;
    CGDataProviderRef  provider;

    uint8_t           *frameBuffer;	// For dumb drawing (bit blitting)


    // In the future there will be a CGImageRep for each buffer?
    //map<unsigned char *, CGImageRef> buffers;


    // How can I use these? What type is a WId?
    WId                XJ_win;
    WId                XJ_curwin;
};

VideoOutputQuartz::VideoOutputQuartz(void)
                 : VideoOutput()
{
    Started = 0; 

    pauseFrame.buf = NULL;

    data = new QuartzData();
}

VideoOutputQuartz::~VideoOutputQuartz()
{
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    CGImageRelease(data->cgImageRep);
    delete data->copyBuf;

    if ( Started )
    {
        CGDisplayShowCursor(data->theDisplay);

#ifdef CHANGE_SCREEN_MODE
        CGDisplaySwitchToMode(data->theDisplay, data->originalMode);
#endif
        CGDisplayRelease(data->theDisplay);
    }

    Exit();
    delete data;
}

void VideoOutputQuartz::AspectChanged(float aspect)
{
    VideoOutput::AspectChanged(aspect);
    MoveResize();
}

void VideoOutputQuartz::Zoom(int direction)
{
    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputQuartz::InputChanged(int width, int height, float aspect)
{
    VideoOutput::InputChanged(width, height, aspect);

    DeleteQuartzBuffers();
    CreateQuartzBuffers();

    MoveResize();

    scratchFrame = &(vbuffers[kNumBuffers]);

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width  = scratchFrame->width;
    pauseFrame.bpp    = scratchFrame->bpp;
    pauseFrame.size   = scratchFrame->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];
}

int VideoOutputQuartz::GetRefreshRate(void)
{
    return (int)data->refreshRate;
}

#ifdef CHANGE_SCREEN_MODE
static int32_t getCFint32 (CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef  ref = (CFNumberRef) CFDictionaryGetValue(dict, key);

    if ( ref )
    {
        int32_t  val;

        if ( CFNumberGetValue(ref, kCFNumberSInt32Type, &val) )
            return val;
        else
            puts("getCFint32() - Failed to get 32bit int from value");
    }
    else
        puts("getCFint32() - Failed to get value");

    return 0;
}

static double getCFdouble (CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef  ref = (CFNumberRef) CFDictionaryGetValue(dict, key);

    if ( ref )
    {
        float  val;

        if ( CFNumberGetValue(ref, kCFNumberDoubleType, &val) )
            return val;
        else
            puts("getCFfloat() - Failed to get float from value");
    }
    else
        puts("getCFfloat() - Failed to get value");

    return 0.0;
}
#endif

//void 

bool VideoOutputQuartz::Init(int width, int height, float aspect,
                             WId winid, int winx, int winy,
                             int winw, int winh, WId embedid)
{
printf("VideoOutputQuartz::Init(width=%d, height=%d, aspect=%f, winid=%d\n                        winx=%d, winy=%d, winw=%d, winh=%d, WId embedid=%d)\n", width, height, aspect, winid, winx, winy, winw, winh, embedid);


    VideoOutput::InitBuffers(kNumBuffers, true, kNeedFreeFrames, 
                             kPrebufferFrames, kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);


    data->srcWidth  = width;
    data->srcHeight = height;
    data->srcAspect = aspect;


    // It may be possible to locate a display that best matches the requested
    // dimensions and aspect ratio, but for simplicity we will just use the
    // main display, which will probably be the one that MythTV started up on.
    // Note that on a machine with multiple display cards
    // (e.g. a PowerBook with an external display connected),
    // Qt, and thus the GUI, will span the screens a la Xinerama

    CGDirectDisplayID d = data->theDisplay = kCGDirectMainDisplay;


    // Before we store any dimensions, or possibly change screen mode,
    // we check the real display aspect ratio.

    CGSize dispMM  = CGDisplayScreenSize(d);
    float  actualW = dispMM.width;
    float  actualH = dispMM.height;
    if ( actualW == 0.0 || actualH == 0.0 )
        data->aspect = 1.33333333;
    else
        data->aspect = actualW / actualH;



    // From tv_play.cpp:
    bool switchMode = gContext->GetNumSetting("UseVideoModes", 0);

#ifndef CHANGE_SCREEN_MODE
    if ( switchMode )
    {
        VERBOSE(VB_ALL, "Changing of screen mode is not supported");
        switchMode = false;
    }
#endif

    // Try to work out the display size we should be using.

    data->desiredWidth = data->desiredHeight = 0;
    bool scale = false;
#ifdef SCALE_VIDEO
    scale = true;
#endif

    if ( ! gContext->GetNumSetting("GuiSizeForTV", 0) )
    {
        // If a separate size has been requested, then the user probably knows
        // what they are doing, and probably doesn't want fullscreen or scaling
        scale = false, switchMode = false;

        data->desiredWidth  = gContext->GetNumSetting("TVVidModeWidth",  0);
        data->desiredHeight = gContext->GetNumSetting("TVVidModeHeight", 0);

        // If the user didn't fill in a value for width/height,
        // we use the stream's height,
        // and if capable of scaling, calculate width
    }

    // For performance reasons, try to get the closest
    // aspect-ratio correct match to the media stream dimensions.
    if ( ! data->desiredWidth )
        if ( scale )
        {
            data->desiredWidth = (int)(0.5 + height * aspect);

            QString msg = QString("Trying to change to Width of %1")
                          . arg(data->desiredWidth);
            VERBOSE(VB_PLAYBACK, msg);
	}
        else
            data->desiredWidth = width;

    if ( ! data->desiredHeight )
        data->desiredHeight = height;


#ifdef CHANGE_SCREEN_MODE
    data->originalMode = CGDisplayCurrentMode(d);
    if ( ! data->originalMode )
    {
        puts("Could not get current mode of display");
        return false;
    }

    CFDictionaryRef m;
    m = CGDisplayBestModeForParameters(d, 24, data->desiredWidth,
                                       data->desiredHeight, NULL);
    //m = CGDisplayBestModeForParameters(d, 24, winw, winh, NULL);
    if ( ! m )
    {
        puts("Could not find a matching screen mode");
        return false;
    }
    data->newMode = m;
#endif

    if ( CGDisplayCapture(d) != CGDisplayNoErr )
    {
        puts("Could not capture display");
        return false;
    }

#ifdef CHANGE_SCREEN_MODE
    if ( CGDisplaySwitchToMode(d, m) != CGDisplayNoErr )
    {
        CGDisplayRelease(d);
        puts("Could not switch to matching screen mode");
        return false;
    }

    data->width           = getCFint32(m, kCGDisplayWidth);
    data->height          = getCFint32(m, kCGDisplayHeight);
    data->bitsPerPixel    = getCFint32(m, kCGDisplayBitsPerPixel);
    data->bitsPerSample   = getCFint32(m, kCGDisplayBitsPerSample);
    data->samplesPerPixel = getCFint32(m, kCGDisplaySamplesPerPixel);
    data->refreshRate     = getCFdouble(m, kCGDisplayRefreshRate);
    data->bytesPerRow     = getCFint32(m, kCGDisplayBytesPerRow);

    if ( data->refreshRate == 0.0 )	// LCD display?
        data->refreshRate = 150;

printf("Set display to %d x %d x %d @ %fHz\n", data->width,
       data->height, data->bitsPerPixel, (float)data->refreshRate);
#else
    data->width           = CGDisplayPixelsWide(d);
    data->height          = CGDisplayPixelsHigh(d);
    data->bitsPerPixel    = CGDisplayBitsPerPixel(d);
    data->bitsPerSample   = CGDisplayBitsPerSample(d);
    data->samplesPerPixel = CGDisplaySamplesPerPixel(d);
    data->refreshRate     = 150;
    data->bytesPerRow     = CGDisplayBytesPerRow(d);
#endif

#ifdef BLIT
    // For blitting the data to the screen (dumbest display method)
    data->frameBuffer = (uint8_t *) CGDisplayBaseAddress(d);
    if ( ! data->frameBuffer )
    {
  #ifdef CHANGE_SCREEN_MODE
        CGDisplaySwitchToMode(d, data->originalMode);
  #endif
        CGDisplayRelease(d);
        cerr << "Could not get base address of display\n";
        return false;
    }

    data->desiredXoff = (data->width  - width)  / 2;
    data->desiredYoff = (data->height - height) / 2;
#else
    // To enable us to use CoreGraphics drawing routines:
    data->cgContext   = CGDisplayGetDrawingContext(d);
    data->colourSpace = CGColorSpaceCreateDeviceRGB();

    data->cgRect.origin.x = (data->width  - data->desiredWidth)  / 2;
    data->cgRect.origin.y = (data->height - data->desiredHeight) / 2;
  #ifdef SCALE_VIDEO
    data->cgRect.size.width  = data->desiredWidth;
    data->cgRect.size.height = data->desiredHeight;
  #else
    data->cgRect.size.width  = width;
    data->cgRect.size.height = height;
  #endif
#endif

    CGDisplayHideCursor(d);

    // Record information about the display depth for the
    // MythTv decoders, and the RGB copy that we generate.

    // Sadly, the decoders always feed us the data as YUV420P.
    // If we set the format of the buffers to RGB ones, the OSD will be
    // displayed in that, but then the video frames will still be YUV.

    switch ( data->bitsPerPixel )
    {
        case 32:
#ifdef BLIT
                 data->outputFormat = data->copyFormat = PIX_FMT_RGBA32,
                 data->copyBitsPerSample = 8, data->copyBytesPerPixel = 4;
#else
                 data->outputFormat = data->copyFormat = PIX_FMT_RGB24,
                 data->copyBitsPerSample = 8, data->copyBytesPerPixel = 3;
#endif
                 break;
#ifdef USE_LIBAVCODEC
  #ifdef BLIT
        case 16: data->outputFormat = data->copyFormat = PIX_FMT_RGB555,
                 data->copyBitsPerSample = 5, data->copyBytesPerPixel = 2;
                 break;
  #else
        case 16: // Libavcodec generates 555 with a leading 1, which the
                 // CoreGraphics routines cannot process. 565 is close to
                 // being usable (error in blue?) but conversion/drawing
                 // is slower than any other modes, so we just use 24bit:
  #endif
        case 24: data->outputFormat = data->copyFormat = PIX_FMT_RGB24,
                 data->copyBitsPerSample = 8, data->copyBytesPerPixel = 3;
                 break;
        default: cerr << "Quartz only supports 15, 24, and 32 bpp displays\n";
#else
        default: cerr << "Sorry - yuv2rgb only supports 32bpp displays\n";
#endif
                 return false;
    }

    // Create a frame buffer that ProcessFrame() will colour translate into,
    // and that Show() will blit or CGdraw onto the screen

    data->copyBytesPerRow = data->copyBytesPerPixel * width;
    data->copyBufSize     = data->copyBytesPerRow * height;
    data->copyBuf         = new uint8_t[data->copyBufSize];
    avpicture_fill(&data->copy, data->copyBuf, data->copyFormat, width, height);


#ifndef BLIT
    // A CoreGraphics representation of the above buffer:

    enum CGImageAlphaInfo alpha = kCGImageAlphaNone;
    if ( data->copyBytesPerPixel > 3 )
  #ifdef USE_LIBAVCODEC
        alpha = kCGImageAlphaNoneSkipFirst; // PIX_FMT_RGBA32 is endian-specific
  #else
        alpha = kCGImageAlphaNoneSkipLast;
  #endif

    data->provider = CGDataProviderCreateWithData(NULL, data->copyBuf,
                                                  data->copyBufSize, NULL);

    data->cgImageRep = CGImageCreate(data->srcWidth, data->srcHeight,
                                     data->copyBitsPerSample,
                                     data->copyBytesPerPixel * 8,
                                     data->copyBytesPerRow,
                                     data->colourSpace,
                                     alpha,
                                     data->provider,
                                     NULL,      // colourMap translation table
                                     false,     // shouldInterpolate colours?
                                     kCGRenderingIntentDefault);
#endif

    if (!CreateQuartzBuffers())
        return false;

    scratchFrame = &(vbuffers[kNumBuffers]);

    pauseFrame.height = scratchFrame->height;
    pauseFrame.width  = scratchFrame->width;
    pauseFrame.bpp    = scratchFrame->bpp;
    pauseFrame.size   = scratchFrame->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];

    MoveResize();
    Started = true;

    return true;
}

bool VideoOutputQuartz::CreateQuartzBuffers(void)
{
    for (int i = 0; i < numbuffers + 1; i++)
    {
        vbuffers[i].width  = XJ_width;
        vbuffers[i].height = XJ_height;
        vbuffers[i].bpp    = data->bitsPerPixel;
        vbuffers[i].size   = XJ_width * XJ_height * data->bitsPerPixel / 8;
        vbuffers[i].codec  = FMT_YV12;

        vbuffers[i].buf = new unsigned char[vbuffers[i].size + 64];
    }

    return true;
}

void VideoOutputQuartz::Exit(void)
{
    if (Started) 
    {
        Started = false;

        DeleteQuartzBuffers();
    }
}

void VideoOutputQuartz::DeleteQuartzBuffers()
{
    for (int i = 0; i < numbuffers + 1; i++)
    {
        delete [] vbuffers[i].buf;
        vbuffers[i].buf = NULL;
    }
}

void VideoOutputQuartz::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
}
 
void VideoOutputQuartz::StopEmbedding(void)
{
    if (!embedding)
        return;

    VideoOutput::StopEmbedding();
}

void VideoOutputQuartz::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)buffer;
}

void VideoOutputQuartz::Show(FrameScanType)
{
#ifdef BLIT
    // Just copy the data.

    uint8_t * src        = data->copyBuf;
    int       srcRowSize = data->copyBytesPerRow;
    uint8_t * dst        = data->frameBuffer;
    int       dstRowSize = data->bytesPerRow;
    int       vOff       = 0;

    if ( data->srcWidth < data->width )
        dst += data->desiredXoff * data->copyBytesPerPixel;
    if ( data->srcHeight < data->height )
        vOff = data->desiredYoff;

    for ( int row = 0; row < data->srcHeight; ++row)
    {
        int dstOffset = (row + vOff) * dstRowSize;
        int srcOffset = row * srcRowSize;

        memcpy(dst + dstOffset, src + srcOffset, srcRowSize);
    }
#else
    CGContextDrawImage(data->cgContext, data->cgRect, data->cgImageRep);
#endif
}

void VideoOutputQuartz::DrawUnusedRects(void)
{
}

void VideoOutputQuartz::UpdatePauseFrame(void)
{
    VideoFrame *pauseb = scratchFrame;
    if (usedVideoBuffers.count() > 0)
        pauseb = usedVideoBuffers.head();
    memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

void VideoOutputQuartz::ProcessFrame(VideoFrame *frame, OSD *osd,
                                     FilterChain *filterList,
                                     NuppelVideoPlayer *pipPlayer)
{
    if (!frame)
    {
        frame = scratchFrame;
        CopyFrame(scratchFrame, &pauseFrame);
    }
    if (filterList)
        filterList->ProcessFrame(frame);

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);


    // Convert the YUV data to RGB

#ifdef USE_LIBAVCODEC
    AVPicture imageIn;

    avpicture_fill(&imageIn, frame->buf,
                   PIX_FMT_YUV420P, frame->width, frame->height);

    img_convert(&data->copy, data->copyFormat, &imageIn,
                PIX_FMT_YUV420P, frame->width, frame->height);
#else
    int         srcSiz  = frame->width * frame->height;
    yuv2rgb_fun convert = yuv2rgb_init_mmx(32, MODE_RGB);

    convert(data->copyBuf, frame->buf, frame->buf + srcSiz,
            frame->buf + srcSiz * 5 / 4, frame->width, frame->height, 0, 0, 0, 0);
#endif
}
