// AccelUtils is based on Accellent by John Dagliesh:
//   http://www.defyne.org/dvb/accellent.html

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <AGL/agl.h>
#import <OpenGL/glext.h>
#include <iostream>
#include <qmutex.h>
#include <qstring.h>
#include <qptrqueue.h>
#include <qmap.h>
using namespace std;


#include "videoout_accel_utils.h"
#include "avcodec.h"
#include "dvdv.h"
#include "videoout_accel_private.h"
#include "yuv2rgb.h"

// Global which Nigel has not been able to put anywhere else:
static DVDV_CurPtrs gDVDVState;

// Default number of buffers in Apple's code
const int kAppleBuffers = 4;
// Storage for subpictures during decoding
const int kSubpictureSize = 8 * 1024 * 1024;

// Number of frames we can buffer; this should be
// no less than kNumBuffers from VideoOutput
const int kAccelBuffers = 33;

// Make a poor copy of Myth's VERBOSE -- we can't include
// mythcontext.h without running into compilation errors.
#define VERBOSE(mask,msg) cout << "AccelUtils: " << msg << endl;


// All the data we need to encapsulate one frame.
struct FrameData
{
  // This is the storage pointed to by state
  // and dec.
  uint8 *data;
  
  // This is the DVDV state used when decoding to this frame.
  DVDV_CurPtrs state;
  
  // Frame data is constant, store it here instead of new'ing it.
  DVDV_Frame stateframe;
    
  // After the frame is processed, this block
  // will be passed to DVDVideoDecode.
  DecodeParams dec;
  
  // Which buffer should be shown when calling
  // DVDVideoShow for this frame?
  int showBuffer;
  
  // Which VideoFrame was attached to this buffer?
  VideoFrame *vf;
};

// We hide the icky stuff here, to avoid header bloat.
struct AccelUtilsData
{
  // The reference ID for our video decoding.
  DVDVideoContext * gDVDContext;
  
  // These three items are used to interact with the
  // drawing surface where the video is shown.
  CGSConnectionID gConn;
  int gWindowID;
  CGSSurfaceID gSurfaceID;
  
  // Remember where our content rectangle starts,
  // so we draw in the correct spot instead of overwriting
  // the window frame.
  Point gWindowContentOrigin;
  
  // This holds the measurements for the video size.
  short gRealRect[4];
  
  // The Carbon and Cocoa hooks to our window.
  WindowRef gWindow;
  NSWindow * gNsWindow;
  
  // Our OSD items.
  WindowRef overlayWindow;
  AGLContext overlayContext;
  int osdWidth;
  int osdHeight;
  GLuint yuvTexture;
  GLuint alphaTexture;
  bool osdVisible;
  
  // Our buffer of frames, and the map to look
  // up frames by VideoFrame.
  FrameData gFrames[kAccelBuffers];
  QPtrQueue<FrameData> freeFrames;
  QPtrQueue<FrameData> usedFrames;
  QMap<VideoFrame *, FrameData *> usedMap;
  
  // A temporary holding area for one FrameData struct.
  FrameData *curFrame;
  
  // Static subpicture storage. (I don't think it's used for anything.)
  uint8 subpic[kSubpictureSize];
    
  // The latest buffer to show. This is a past version of
  // gBufPresent (below), which tracks the latest version.
  int gBufShow;
  
  // Remember which buffers we've been using. All of these
  // will be set between 0 and 3 when we're running.
  int gBufPast;
  int gBufFuture;
  int gBufRecent;
  
  // This mutex synchronizes the decode and show threads.
  QMutex mutex;
  
  // This keeps Cocoa from leaking in pthreads.
  NSAutoreleasePool *gPool;
};


// Dummy NSThread for Cocoa multithread initialization
@implementation NSThread (Dummy)

- (void) run;
{
  // do nothing
}

@end


// static class vars
AccelUtils *AccelUtils::m_singleton = NULL;

AccelUtils::AccelUtils()
{
  d = new AccelUtilsData();
  d->gPool = NULL;
  d->gConn = 0;
  d->gSurfaceID = 0;
  d->gDVDContext = NULL;
  d->curFrame = NULL;
  d->overlayWindow = NULL;
  d->overlayContext = NULL;
  
  // Since we are dealing with Cocoa in non-main
  // threads, we need to initialize multi-threaded
  // mode by starting an NSThread. Seriously, this
  // is the Apple-recommended way to do this.
  if (![NSThread isMultiThreaded])
  {
    NSThread *thr = [[NSThread alloc] init];
    SEL threadSelector = @selector(run);
    [NSThread detachNewThreadSelector:threadSelector toTarget:thr withObject:nil];
  }
  
  m_singleton = this;
}


AccelUtils::~AccelUtils()
{
  m_singleton = NULL;
  Teardown();
  delete d;
}

void AccelUtils::Teardown()
{
  d->mutex.lock();
  
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
  }
  d->gSurfaceID = 0;
  
  if (d->gDVDContext)
  {
    DVDVideoClearMP(d->gDVDContext);
    DVDVideoEnableMP(d->gDVDContext, false);

    DVDVideoCloseDevice(d->gDVDContext);
  }
  d->gDVDContext = 0;
  
  if (d->overlayContext)
    aglDestroyContext(d->overlayContext);
  d->overlayContext = NULL;
  if (d->overlayWindow)
    DisposeWindow(d->overlayWindow);
  d->overlayWindow = NULL;
  
  if (d->gPool)
    [d->gPool release];
  d->gPool = NULL;
  
  d->mutex.unlock();
}

// Initialize the Accel params with the size of video to be decoded.
void AccelUtils::SetVideoSize(int width, int height)
{
  d->mutex.lock();

  // Make sure we have an autorelease pool in our thread.
  if (!d->gPool)
    d->gPool = [[NSAutoreleasePool alloc] init];
  
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

  d->mutex.unlock();
}

// Resize and reposition the video subwindow.
void AccelUtils::MoveResize(int imgx, int imgy, int imgw, int imgh,
                            int dispxoff, int dispyoff,
                            int dispwoff, int disphoff)
{
  // This function handles some zoom modes incorrectly.
  (void)imgx;
  (void)imgy;
  (void)imgw;
  (void)imgh;
  
  d->mutex.lock();
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

  d->mutex.unlock();
}

// Update the OSD display.
void AccelUtils::DrawOSD(unsigned char *y,
                         unsigned char *u,
                         unsigned char *v,
                         unsigned char *alpha)
{
  d->mutex.lock();
  
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
    d->mutex.unlock();
    return;
  }

  // Convert 4:2:0 to 4:2:2
  yuv2vuy_fun convert = yuv2vuy_init_altivec();
  uint8_t *yuvData = new uint8_t[d->osdWidth * d->osdHeight * 2];
  convert(yuvData, y, u, v, d->osdWidth, d->osdHeight, 0, 0, 0);

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
  d->mutex.unlock();
}

// Reset the Accel state.
void AccelUtils::Reset(void)
{
  d->mutex.lock();
  
  // Set our buffers to be unused.
  d->gBufShow = kAppleBuffers;
  d->gBufPast = d->gBufFuture = d->gBufRecent = 0;
  
  // Invalidate frames currently in queue.
  while (FrameData *frame = d->usedFrames.dequeue())
  {
    d->usedMap.erase(frame->vf);
    d->freeFrames.enqueue(frame);
  }
  
  d->mutex.unlock();
}

// Prepare the Accel code for a call to avcodec_decode_video.
bool AccelUtils::PreProcessFrame(AVCodecContext *context)
{
  d->mutex.lock();
  
  d->curFrame = d->freeFrames.dequeue();
  if (!d->curFrame)
  {
    VERBOSE(VB_PLAYBACK, "ERROR! No free frames available!");
    //d->mutex.unlock();
    //return false;
 
    int i = d->usedFrames.count();
    if (i > 5)
      i = 5;
    
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
      VERBOSE(VB_PLAYBACK, "ERROR! Still no free frames available!");
      d->mutex.unlock();
      return false;
    }
  }    
  
  d->curFrame->showBuffer = kAppleBuffers;
  d->curFrame->vf = NULL;

  gDVDVState = d->curFrame->state;
  context->accellent = &gDVDVState;
  
  // Allow accel decoding path for the next decode_video call.
  context->accellent_enabled = 1;
  
  d->mutex.unlock();
  return true;
}

// Process the macroblocks collected by avcodec_decode_video.
void AccelUtils::PostProcessFrame(AVCodecContext *context,
                                  VideoFrame *pic, int pict_type,
				  bool gotpicture)
{  
  d->mutex.lock();
  
  // Turn accel decoding path off again until the next PreProcessFrame.
  context->accellent_enabled = 0;
  
  // get the buffer we used for this run
  FrameData *frame = d->curFrame;
  if (!frame)
  {
    VERBOSE(VB_IMPORTANT, "ERROR! no current frame!");
    d->mutex.unlock();
    return;
  }
  
  // skip if we didn't decode a frame
  if (!gotpicture)
  {
    d->freeFrames.enqueue(frame);
    d->mutex.unlock();
    return;
  }
  
  // skip if we got a bad B frame (often happens at start or after SeekReset)
  if (gDVDVState.mb == frame->state.mb)
  {
    d->freeFrames.enqueue(frame);
    d->mutex.unlock();
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
        if (d->gBufFuture == i || d->gBufRecent == i)
          continue;
        frame->dec.dstBuf = i;
        break;
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
        if (d->gBufFuture == i || d->gBufPast == i || d->gBufRecent == i)
          continue;
        frame->dec.dstBuf = i;
        break;
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
  d->mutex.unlock();
}

// Decode a buffered frame
void AccelUtils::DecodeFrame(VideoFrame *pic)
{
  d->mutex.lock();
  
  // Check if pic is in decode queue
  if (!d->usedMap[pic])
  {
    VERBOSE(VB_PLAYBACK, QString("VF %1 not in decode queue")
                                 .arg((unsigned long)pic));
    d->mutex.unlock();
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
//     else
//       VERBOSE(VB_PLAYBACK, "Decoding extra VF to catch up");
  }
  
  d->mutex.unlock();
}

// Draw the most recently decoded video frame to the screen.
void AccelUtils::ShowFrame(void)
{
  d->mutex.lock();
  if (d->gDVDContext && d->gBufShow < kAppleBuffers)
  {
//     DeinterlaceParams1 dp1;
//     memset(&dp1, 0, sizeof(dp1));
//     dp1.buf1 = d->gBufShow;
//     dp1.buf2 = d->gBufShow;
//     dp1.w1 = 1;
//     dp1.w2 = 1;
//     dp1.w3 = 2;
//     DeinterlaceParams2 dp2;
//     memset(&dp2, 0, sizeof(dp2));
//     dp2.unk1 = 0x00020000;
//     dp2.usedValue = 1;
//     DVDVideoDeinterlace(d->gDVDContext, &dp1, &dp2, 0);

    ShowBufferParams sp;
    memset(&sp, 0, sizeof(sp));
    sp.h1 = 2;
    sp.w = 1;
    DVDVideoShowMPBuffer(d->gDVDContext, d->gBufShow, &sp, 0);
    
    // don't show another frame until DecodeFrame loads a new frame
    d->gBufShow = kAppleBuffers;
  }
  d->mutex.unlock();
}
