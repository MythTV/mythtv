/******************************************************************************
 * = NAME
 * videoout_quartz.cpp
 *
 * = DESCRIPTION
 * Basic video for Mac OS X, using an unholy amalgamation of QuickTime,
 * QuickDraw, and Quartz/Core Graphics.
 *
 * = POSSIBLE ENHANCEMENTS
 * - Expand choices for the possibility of multiple displays
 * - Improve performance of zoomed aspect modes
 * - Other viewing options?
 *
 * = KNOWN BUGS
 * - Aspect switching occasionally fails
 * - Floating window needs testing. Resizing, or viewing something
 *   a second time, may cause a crash (backtraces appreciated)
 * 
 * = REVISION
 * $Id$
 *
 * = AUTHORS
 * Nigel Pearson, Jeremiah Morris
 *****************************************************************************/

// ****************************************************************************
// Configuration:

// Default numbers of buffers from some of the other videoout modules:
const int kNumBuffers      = 31;
const int kNeedFreeFrames  = 1;
const int kPrebufferFramesNormal = 12;
const int kPrebufferFramesSmall = 4;
const int kKeepPrebuffer   = 2;

// ****************************************************************************

#include <map>
#include <iostream>
using namespace std;

#include <qptrlist.h>
#include <qmutex.h>

#include "DisplayRes.h"
#include "yuv2rgb.h"
#include "uitypes.h"
#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_quartz.h"

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <CoreGraphics/CGImage.h>
#import <Carbon/Carbon.h>
#import <QuickTime/QuickTime.h>

class VideoOutputQuartzView;

/*
 * The floating window class needs an event callback.
 */
OSStatus VoqvFloater_Callback(EventHandlerCallRef inHandlerCallRef, 
                              EventRef inEvent, 
                              void *inUserData);

/*
 * The structure containing most of VideoOutQuartz's variables
 */
struct QuartzData
{
    // Stored information about the media stream:
    int                srcWidth,
                       srcHeight;
    float              srcAspect;
    int                srcMode;           // letterbox mode

    // Pixel storage for the media stream:
    ImageDescriptionHandle imgDesc;       // source description header
    PlanarPixmapInfoYUV420 *pixmap;       // frame header + data
    size_t             pixmapSize;        // pixmap size
    void *             pixelData;         // start of data section
    size_t             pixelSize;         // data size
    QMutex             pixelLock;         // to update pixels safely

    // Information about the display:
    WindowRef          window;            // MythTV window
    CGDirectDisplayID  screen;            // screen containing main window
    float              refreshRate;       // for screen above

    // Global preferences:
    bool               drawInWindow;      // Fullscreen or in GUI view?
    bool               windowedMode;      // GUI runs in window?
    bool               scaleUpVideo;      // Enlarge video as needed?
    bool               correctGamma;      // Video gamma correction
    yuv2vuy_fun        yuvConverter;      // 420 -> 2vuy conversion function
    
    // Zoom preferences:
    int                ZoomedIn;          // These mirror the videooutbase
    int                ZoomedUp;          // variables, for the benefit of
    int                ZoomedRight;       // the views

    // Output viewports:
    QPtrList<VideoOutputQuartzView> views;   // current views

    // Embedding:
    VideoOutputQuartzView * embeddedView;    // special embedded widget
};

/*
 * These utility functions are used for querying parameters
 * from a CFDictionary.
 */
// static int32_t getCFint32(CFDictionaryRef dict, CFStringRef key)
// {
//     CFNumberRef  ref = (CFNumberRef) CFDictionaryGetValue(dict, key);
// 
//     if (ref)
//     {
//         int32_t  val;
// 
//         if (CFNumberGetValue(ref, kCFNumberSInt32Type, &val) )
//             return val;
//         else
//             puts("getCFint32() - Failed to get 32bit int from value");
//     }
//     else
//         puts("getCFint32() - Failed to get value");
// 
//     return 0;
// }

static double getCFdouble(CFDictionaryRef dict, CFStringRef key)
{
    CFNumberRef  ref = (CFNumberRef) CFDictionaryGetValue(dict, key);

    if (ref)
    {
        double val;

        if (CFNumberGetValue(ref, kCFNumberDoubleType, &val))
            return val;
        else
            puts("getCFdouble() - Failed to get float from value");
    }
    else
        puts("getCFdouble() - Failed to get value");

    return 0.0;
}

/*
 * This abstract class is used for implementing output viewports.
 * See the subclasses below.
 */
class VideoOutputQuartzView
{
  public:
    VideoOutputQuartzView(QuartzData *pData);
    virtual ~VideoOutputQuartzView();

    virtual bool Init(void);

    virtual void SetFrameSkip(int numskip);
    virtual void Show(void);

    virtual void InputChanged(int width, int height, float aspect);
    virtual void VideoAspectRatioChanged(float aspect);
    virtual void Zoom(int direction);

    virtual void EmbedChanged(bool embedded);

  protected:
    virtual bool Begin(void);
    virtual void End(void);
    virtual void Transform(void);
    virtual void BlankScreen(bool deferred);

    // Subclasses implement the following methods:
    virtual bool BeginPort(void) = 0;   // Set up a valid thePort object
    virtual void EndPort(void) = 0;     // Release thePort

    char *             name;          // Name, for verbose output

    QuartzData *       parentData;    // information about video source is here

    CGrafPtr           thePort;       // QuickDraw graphics port
    int                desiredWidth,
                       desiredHeight,
                       desiredXoff,
                       desiredYoff;   // output size characteristics
    ImageSequence      theCodec;      // QuickTime sequence ID
    RgnHandle          theMask;       // clipping region

    int                frameSkip;     // do we skip frames?
    int                frameCounter;  // keep track of skip status
    bool               drawBlank;     // draw a blank frame before next Show

    bool               applyTVoffset; // subclasses set this to affect transform

    QMutex             viewLock;
};

VideoOutputQuartzView::VideoOutputQuartzView(QuartzData *pData)
{
    parentData = pData;

    thePort = NULL;
    desiredWidth = desiredHeight = desiredXoff = desiredYoff = 0;
    theCodec = NULL;
    theMask = NULL;

    frameSkip = 1;
    frameCounter = 0;
    drawBlank = true;

    // This variable is set by subclasses to indicate whether
    // to apply the scan displacement TV mode settings.
    // Embedded or small viewports should set this to false.
    applyTVoffset = true;
}

VideoOutputQuartzView::~VideoOutputQuartzView()
{
    End();
}

// Set up the port and the QuickTime decompressor.
// We assume that the parent has set up the pixel storage.
bool VideoOutputQuartzView::Begin(void)
{
    viewLock.lock();
    if (!thePort)
    {
        puts("No graphics port available");
        viewLock.unlock();
        return false;
    }

    // Set output size and clipping mask (if necessary)
    Rect portBounds;
    GetPortBounds(thePort, &portBounds);
    desiredXoff += portBounds.left;
    desiredYoff += portBounds.top;

    if (!desiredWidth && !desiredHeight)
    {
        // no mask, set width and height to that of port
        desiredWidth = portBounds.right - portBounds.left;
        desiredHeight = portBounds.bottom - portBounds.top;
        theMask = NULL;
    }
    else
    {
        // mask to requested bounds
        theMask = NewRgn();
        SetRectRgn(theMask, desiredXoff, desiredYoff,
                   desiredXoff + desiredWidth,
                   desiredYoff + desiredHeight);
    }

    // create the decompressor
    if (DecompressSequenceBeginS(&theCodec,
                                 parentData->imgDesc,
                                 NULL,
                                 0,
                                 thePort,
                                 NULL,
                                 NULL,
                                 NULL,
                                 srcCopy,
                                 theMask,
                                 0,
                                 codecNormalQuality,
                                 bestSpeedCodec))
    {
        puts("DecompressSequenceBeginS failed");
        viewLock.unlock();
        return false;
    }
    
    // Turn off gamma correction unless requested
    if (!parentData->correctGamma)
        QTSetPixMapHandleRequestedGammaLevel(GetPortPixMap(thePort),
                                             kQTUseSourceGammaLevel);
    
    SetDSequenceFlags(theCodec,
                      codecDSequenceFlushInsteadOfDirtying,
                      codecDSequenceFlushInsteadOfDirtying);
    viewLock.unlock();

    // set transformation matrix
    Transform();

    return true;
}

// Clean up the codec.  
void VideoOutputQuartzView::End(void)
{
    viewLock.lock();
    // Destroy codec
    if (theCodec)
    {
        CDSequenceEnd(theCodec);
        theCodec = NULL;
        if (theMask)
        {
            DisposeRgn(theMask);
            theMask = NULL;
        }
    }
    viewLock.unlock();
}

// Build the transformation matrix to scale the video appropriately.
void VideoOutputQuartzView::Transform(void)
{
    MatrixRecord matrix;
    SetIdentityMatrix(&matrix);

    int x, y, w, h, sw, sh;
    x = desiredXoff;
    y = desiredYoff;
    w = desiredWidth;
    h = desiredHeight;
    sw = parentData->srcWidth;
    sh = parentData->srcHeight;
    float aspect = parentData->srcAspect;

    // constants for transformation operations
    Fixed one, zero;
    one  = Long2Fix(1);
    zero = Long2Fix(0);

    VERBOSE(VB_PLAYBACK, QString("%0Viewport is %1 x %2")
                                .arg(name).arg(w).arg(h));
    VERBOSE(VB_PLAYBACK, QString("%0Image is %1 x %2")
                                .arg(name).arg(sw).arg(sh));

    // scale for non-square pixels
    if (fabsf(aspect - (sw * 1.0 / sh)) > 0.01)
    {
        if (parentData->scaleUpVideo)
        {
            // scale width up, leave height alone
            double aspectScale = aspect * sh / sw;
            VERBOSE(VB_PLAYBACK, QString("%0Scaling to %1 of width")
                                        .arg(name).arg(aspectScale));
            ScaleMatrix(&matrix,
                        X2Fix(aspectScale),
                        one,
                        zero, zero);

            // reset sw to be apparent width
            sw = (int)lroundf(sh * aspect);
        }
        else
        {
            // scale height down
            double aspectScale = sw / (aspect * sh);
            VERBOSE(VB_PLAYBACK,
                    QString("Scaling to %1 of height")
                           .arg(name).arg(aspectScale));
            ScaleMatrix(&matrix,
                        one,
                        X2Fix(aspectScale),
                        zero, zero);

            // reset sw to be apparent width
            sh = (int)lroundf(sw / aspect);
        }
    }

    // figure out how much zooming we want
    double hscale, vscale;
    switch (parentData->srcMode)
    {
        case kLetterbox_4_3_Zoom:
            // height only fills 3/4 of image, zoom up
            hscale = vscale = h * 1.0 / (sh * 0.75);
            break;
        case kLetterbox_16_9_Zoom:
            // width only fills 3/4 of image, zoom up
            hscale = vscale = w * 1.0 / (sw * 0.75);
            break;
        case kLetterbox_16_9_Stretch:
            // like 16 x 9 standard, but with a horizontal stretch applied
            hscale = vscale = fmin(h * 1.0 / sh, w * 1.0 / sw);
            hscale *= 4.0 / 3.0;
            break;
        case kLetterbox_4_3:
        case kLetterbox_16_9:
        default:
            // standard, fill viewport with scaled image
            hscale = vscale = fmin(h * 1.0 / sh, w * 1.0 / sw);
            break;
    }
    if (parentData->ZoomedIn)
    {
        hscale *= 1 + (parentData->ZoomedIn * .01);
        vscale *= 1 + (parentData->ZoomedIn * .01);
    }

    // cap zooming if we requested it
    if (!parentData->scaleUpVideo)
    {
        double maxScale = fmax(hscale, vscale);
        hscale /= maxScale;
        vscale /= maxScale;
    }

    if ((hscale < 0.99) || (hscale > 1.01) ||
        (vscale < 0.99) || (vscale > 1.01))
    {
        VERBOSE(VB_PLAYBACK, QString("%0Scaling to %1 x %2 of original")
                                    .arg(name).arg(hscale).arg(vscale));
        ScaleMatrix(&matrix,
                    X2Fix(hscale),
                    X2Fix(vscale),
                    zero, zero);

        // reset sw, sh for new apparent width/height
        sw = (int)(sw * hscale);
        sh = (int)(sh * vscale);
    }

    // center image in viewport
    if ((h != sh) || (w != sw))
    {
        VERBOSE(VB_PLAYBACK, QString("%0Centering with %1, %2")
                                    .arg(name).arg((w - sw)/2.0).arg((h - sh)/2.0));
        TranslateMatrix(&matrix, X2Fix((w - sw) / 2.0), X2Fix((h - sh) / 2.0));
    }

    // apply over/underscan
    int hscan = gContext->GetNumSetting("HorizScanPercentage", 5);
    int vscan = gContext->GetNumSetting("VertScanPercentage", 5);
    if (hscan || vscan)
    {
        if (vscan > 0)
        {
            vscan *= 2;   // Confusing, but matches X behavior
        }
        if (hscan > 0)
        {
            hscan *= 2;
        }
        
        VERBOSE(VB_PLAYBACK, QString("%0Overscanning to %1, %2")
                                    .arg(name).arg(hscan).arg(vscan));
        ScaleMatrix(&matrix,
                    X2Fix((double)(1.0 + (hscan / 50.0))),
                    X2Fix((double)(1.0 + (vscan / 50.0))),
                    X2Fix(sw / 2.0),
                    X2Fix(sh / 2.0));
    }

    // apply TV mode offset
    if (applyTVoffset)
    {
        int tv_xoff = gContext->GetNumSetting("xScanDisplacement", 0);
        int tv_yoff = gContext->GetNumSetting("yScanDisplacement", 0);
        if (tv_xoff || tv_yoff)
        {
            VERBOSE(VB_PLAYBACK,
                    QString("%0TV offset by %1, %2").arg(name).arg(tv_xoff).arg(tv_yoff));
            TranslateMatrix(&matrix, Long2Fix(tv_xoff), Long2Fix(tv_yoff));
        }
    }
    
    // apply zoomed offsets
    if (parentData->ZoomedIn)
    { 
        // calculate original vs. zoomed dimensions
        int zw = (int)(sw / (1.0 + (parentData->ZoomedIn * .01)));
        int zh = (int)(sh / (1.0 + (parentData->ZoomedIn * .01)));
                
        int zoomx = (int)((sw - zw) * parentData->ZoomedRight * .005);
        int zoomy = (int)((sh - zh) * parentData->ZoomedUp    * .005);
        
        VERBOSE(VB_PLAYBACK, QString("%0Zoom translating to %1, %2")
                                    .arg(name).arg(zoomx).arg(zoomy));
        TranslateMatrix(&matrix, Long2Fix(zoomx), Long2Fix(zoomy));
    }

    // apply graphics port or embedding offset
    if (x || y)
    {
        VERBOSE(VB_PLAYBACK, QString("%0Translating to %1, %2")
                                    .arg(name).arg(x).arg(y));
        TranslateMatrix(&matrix, Long2Fix(x), Long2Fix(y));
    }

    // Apply the transformation
    viewLock.lock();
    SetDSequenceMatrix(theCodec, &matrix);
    viewLock.unlock();
    BlankScreen(true);   // clean up screen of artifacts at next Show
}

void VideoOutputQuartzView::BlankScreen(bool deferred)
{
    if (deferred)
    {
        drawBlank = true;
        return;
    }

    viewLock.lock();
    if (thePort)
    {
        SetPort(thePort);
        
        // set clipping rectangle
        Rect clipRect;
        if (desiredWidth && desiredHeight)
        {
            clipRect.left   = desiredXoff;
            clipRect.top    = desiredYoff;
            clipRect.right  = desiredWidth - desiredXoff;
            clipRect.bottom = desiredHeight - desiredYoff;
        }
        else
        {
            GetPortBounds(thePort, &clipRect);
        }
        RgnHandle clipRgn = NewRgn();
        RectRgn(clipRgn, &clipRect);

        // erase our rectangle to black
        RGBColor rgbBlack = { 0, 0, 0 };
        RGBBackColor(&rgbBlack);
        EraseRect(&clipRect);
        QDFlushPortBuffer(thePort, clipRgn);
        
        drawBlank = false;
    }
    viewLock.unlock();
}

bool VideoOutputQuartzView::Init(void)
{
    return (BeginPort() && Begin());
}

void VideoOutputQuartzView::SetFrameSkip(int numskip)
{
    frameSkip = numskip + 1;
}

void VideoOutputQuartzView::Show(void)
{
    if (drawBlank)
        BlankScreen(false);

    // we only draw when frameCounter is 0
    // if (frameSkip == 1), this is every time
    frameCounter = (frameCounter + 1) % frameSkip;
    if (frameCounter)
        return;

    viewLock.lock();
    if (theCodec && thePort && parentData->pixmap)
    {
      // tell QuickTime to draw the current frame
      if (DecompressSequenceFrameWhen(theCodec,
                                      (Ptr)parentData->pixmap,
                                      parentData->pixmapSize,
                                      0,
                                      NULL,
                                      NULL,
                                      NULL))
      {
          puts("DecompressSequenceFrameWhen failed");
      }
    }
    viewLock.unlock();
}

void VideoOutputQuartzView::InputChanged(int width, int height, float aspect)
{
    (void)width;
    (void)height;
    (void)aspect;

    // need to redo codec, but not the port
    End();
    Begin();
}

void VideoOutputQuartzView::VideoAspectRatioChanged(float aspect)
{
    (void)aspect;

    // need to redo transformation matrix
    Transform();
}

void VideoOutputQuartzView::Zoom(int direction)
{
    (void)direction;

    // need to redo transformation matrix
    Transform();
}

// Subclasses that block the main window should suspend
// playback, hide windows, etc. by overriding this method.
void VideoOutputQuartzView::EmbedChanged(bool embedded)
{
    // do nothing in default version
    (void)embedded;
}

/*
 * This view subclass implements full-size video display in the main window.
 */
class VoqvMainWindow : public VideoOutputQuartzView
{
  public:
    VoqvMainWindow(QuartzData *pData, float alphaBlend = 1.0)
    : VideoOutputQuartzView(pData)
    {
        alpha = fminf(1.0, fmaxf(0.0, alphaBlend));
        applyTVoffset = true;
    };

   ~VoqvMainWindow()
    {
        End();
        EndPort();
    };

  protected:
    float alpha;

    bool BeginPort(void)
    {
        viewLock.lock();
        thePort = GetWindowPort(parentData->window);
        if (!thePort)
        {
            puts("GetWindowPort failed");
            viewLock.unlock();
            return false;
        }

        SetWindowAlpha(parentData->window, alpha);
        RGBColor black = { 0, 0, 0 };
        SetWindowContentColor(parentData->window, &black);
        viewLock.unlock();
        return true;
    };

    bool Begin(void)
    {
        bool ret = VideoOutputQuartzView::Begin();

        if (ret && (alpha < 0.99))
        {
            // change QuickTime mode to transparent
            RGBColor black = { 0, 0, 0 };
            viewLock.lock();
            SetDSequenceTransferMode(theCodec, transparent, &black);
            viewLock.unlock();
        }
        return ret;
    };

    void EndPort(void)
    {
        viewLock.lock();
        SetWindowAlpha(parentData->window, 1.0);
        thePort = NULL;
        viewLock.unlock();
    };

    // On embedding, we stop video playback.
    void EmbedChanged(bool embedded)
    {
        if (embedded)
        {
            End();
            EndPort();
        }
        else
        {
            BeginPort();
            Begin();
        }
    };
};

/*
 * This view subclass implements embedded display in the main window.
 */
class VoqvEmbedded : public VideoOutputQuartzView
{
  public:
    VoqvEmbedded(QuartzData *pData, int x, int y, int w, int h)
    : VideoOutputQuartzView(pData)
    {
        desiredXoff = x;
        desiredYoff = y;
        desiredWidth = w;
        desiredHeight = h;
    };

   ~VoqvEmbedded()
    {
        End();
        EndPort();
    };

  protected:
    bool BeginPort(void)
    {
        viewLock.lock();
        thePort = GetWindowPort(parentData->window);
        if (!thePort)
        {
            puts("GetWindowPort failed");
            viewLock.unlock();
            return false;
        }
        
        // Ensure Qt updates by invalidating the window.
        Rect portBounds;
        GetPortBounds(thePort, &portBounds);
        InvalWindowRect(parentData->window, &portBounds);

        // The main class handles masking and resizing,
        // since we set the desiredXoff, etc. variables.
        viewLock.unlock();
        return true;
    };

    void EndPort(void)
    {
        viewLock.lock();
        thePort = NULL;
        viewLock.unlock();
    };
 };

/*
 * This view subclass implements fullscreen video display.
 */
class VoqvFullscreen : public VideoOutputQuartzView
{
  public:
    VoqvFullscreen(QuartzData *pData)
    : VideoOutputQuartzView(pData)
    {
        applyTVoffset = true;
        name = "";
    };

   ~VoqvFullscreen()
    {
        End();
        EndPort();
    };

  protected:
    CGDirectDisplayID d;

    bool BeginPort(void)
    {
        viewLock.lock();
        d = parentData->screen;

        if (CGDisplayCapture(d) != CGDisplayNoErr)
        {
            puts("Could not capture display");
            viewLock.unlock();
            return false;
        }
        
        // switch screen resolution if desired
        if (gContext->GetNumSetting("UseVideoModes", 0))
        {
            DisplayRes *disp = DisplayRes::GetDisplayRes();
            disp->SwitchToVideo(parentData->srcWidth, parentData->srcHeight);
        }

        CGDisplayHideCursor(d);

        thePort = CreateNewPortForCGDisplayID((UInt32)d);
        if (!thePort)
        {
            puts("CreateNewPortForCGDisplayID failed");
            viewLock.unlock();
            return false;
        }

        viewLock.unlock();
        return true;
    };

    void EndPort(void)
    {
        viewLock.lock();
        if (thePort)
        {
            DisposePort(thePort);
            thePort = NULL;
        }
        
        // return screen resolution to normal
        if (gContext->GetNumSetting("UseVideoModes", 0))
            DisplayRes::GetDisplayRes()->SwitchToGUI();

        if (d)
        {
            CGDisplayShowCursor(d);
            CGDisplayRelease(d);
            d = NULL;
        }
        viewLock.unlock();
    };

    // On embedding, we release the display, and
    // restore everything when we're through.
    void EmbedChanged(bool embedded)
    {
        if (embedded)
        {
            End();
            EndPort();
        }
        else
        {
            BeginPort();
            Begin();
        }
    };
};

/*
 * This view subclass implements drawing to the dock tile.
 */
class VoqvDock : public VideoOutputQuartzView
{
  public:
    VoqvDock(QuartzData *pData)
    : VideoOutputQuartzView(pData)
    {
        name = "Dock icon: ";
    };

   ~VoqvDock()
    {
        End();
        EndPort();
    };

  protected:
    bool BeginPort(void)
    {
        thePort = BeginQDContextForApplicationDockTile();
        if (!thePort)
        {
            puts("BeginQDContextForApplicationDockTile failed");
            return false;
        }
        return true;
    };

    void EndPort(void)
    {
        viewLock.lock();
        EndQDContextForApplicationDockTile(thePort);
        thePort = NULL;
        RestoreApplicationDockTileImage();
        viewLock.unlock();
    };
};

/*
 * This view subclass implements drawing to a floating
 * translucent window.
 */
class VoqvFloater : public VideoOutputQuartzView
{
  public:
    VoqvFloater(QuartzData *pData, float alphaBlend = 0.5)
    : VideoOutputQuartzView(pData)
    {
        alpha = fminf(1.0, fmaxf(0.0, alphaBlend));
        resizing = false;
        name = "Floating window: ";
    };

   ~VoqvFloater()
    {
        End();
        EndPort();
    };

    void Show(void)
    {
        if (resizing)
            return;     // don't update while user is resizing

        VideoOutputQuartzView::Show();
    }
    
    void ResizeChanged(bool startResizing)
    {
        if (!startResizing)
        {
            // Resize complete, reset the window drawing transformation
            Rect curBounds;
            GetPortBounds(thePort, &curBounds);
            desiredWidth  = curBounds.right - curBounds.left;
            desiredHeight = curBounds.bottom - curBounds.top;
            Transform();
        }
        resizing = startResizing;
    }

  protected:
    WindowRef window;
    float alpha;
    bool resizing;

    bool BeginPort(void)
    {
        viewLock.lock();

        Rect bounds;
        bounds.top = bounds.left = bounds.right = bounds.bottom = 50;
        bounds.right  += CGDisplayPixelsWide(parentData->screen) / 3;
        bounds.bottom += CGDisplayPixelsHigh(parentData->screen) / 3;
        
        // custom window definition
        EventHandlerUPP myUPP = NewEventHandlerUPP(VoqvFloater_Callback);
        EventTypeSpec defEvents[] =
            { { kEventClassWindow, kEventWindowHitTest },
              { kEventClassWindow, kEventWindowDrawFrame },
              { kEventClassWindow, kEventWindowClickResizeRgn } };
        ToolboxObjectClassRef myClass;
        RegisterToolboxObjectClass(CFSTR("org.mythtv.myth.VoqvFloater"),
                                   NULL,
                                   3,
                                   defEvents,
                                   myUPP,
                                   this,
                                   &myClass);
        WindowDefSpec mySpec;
        mySpec.defType = kWindowDefObjectClass;
        mySpec.u.classRef = myClass;
        if (CreateCustomWindow(&mySpec,
                               kUtilityWindowClass,
                               kWindowNoShadowAttribute |
                                  kWindowResizableAttribute |
                                  kWindowStandardHandlerAttribute,
                               &bounds,
                               &window))
        {
            puts("CreateCustomWindow failed");
            viewLock.unlock();
            return false;
        }
        SetWindowAlpha(window, alpha);
        RGBColor black = { 0, 0, 0 };
        SetWindowContentColor(window, &black);
        
        thePort = GetWindowPort(window);
        if (!thePort)
        {
            puts("GetWindowPort failed");
            viewLock.unlock();
            return false;
        }

        viewLock.unlock();
        ShowWindow(window);
        // don't lose focus from main window
        SelectWindow(parentData->window);

        return true;
    };

    bool Begin(void)
    {
        bool ret = VideoOutputQuartzView::Begin();

        if (ret && (alpha < 0.99))
        {
            // change QuickTime mode to transparent
            RGBColor black = { 0, 0, 0 };
            viewLock.lock();
            SetDSequenceTransferMode(theCodec, transparent, &black);
            viewLock.unlock();
        }
        return ret;
    };

    void EndPort(void)
    {
        viewLock.lock();
        thePort = NULL;
        if (window)
        {
            DisposeWindow(window);
            window = NULL;
        }
        viewLock.unlock();
    };

    // We hide the window during embedding.
    void EmbedChanged(bool embedded)
    {
        if (embedded)
        {
            End();
            HideWindow(window);
        }
        else
        {
            ShowWindow(window);
            Begin();
        }
    };
};

// The event callback for the floating window above
OSStatus VoqvFloater_Callback(EventHandlerCallRef inHandlerCallRef, 
                              EventRef inEvent, 
                              void *inUserData)
{
    (void)inHandlerCallRef;
    VoqvFloater *floater = (VoqvFloater *)inUserData;
    WindowRef window;
    Point mouseLoc;
    Rect winLoc;
    WindowDefPartCode where;
    
    switch (GetEventKind(inEvent))
    {
        case kEventWindowHitTest:
            // gather info about window and click
            GetEventParameter(inEvent,
                              kEventParamDirectObject,
                              typeWindowRef,
                              NULL,
                              sizeof(WindowRef),
                              NULL,
                              &window);
            GetEventParameter(inEvent,
                              kEventParamMouseLocation,
                              typeQDPoint,
                              NULL,
                              sizeof(mouseLoc),
                              NULL,
                              &mouseLoc);
            
            // see if user hit grow area
            GetWindowBounds(window,
                            kWindowGlobalPortRgn,
                            &winLoc);
            where = wInDrag;
            if (mouseLoc.h > (winLoc.right  - 12) &&
                mouseLoc.v > (winLoc.bottom - 12))
            {
                where = wInGrow;
            }
            SetEventParameter(inEvent,
                              kEventParamWindowDefPart,
                              typeWindowDefPartCode,
                              sizeof(WindowDefPartCode),
                              &where);
            break;
            
        case kEventWindowClickResizeRgn:
            // gather info about window and click
            GetEventParameter(inEvent,
                              kEventParamDirectObject,
                              typeWindowRef,
                              NULL,
                              sizeof(WindowRef),
                              NULL,
                              &window);
            GetEventParameter(inEvent,
                              kEventParamMouseLocation,
                              typeQDPoint,
                              NULL,
                              sizeof(mouseLoc),
                              NULL,
                              &mouseLoc);

            floater->ResizeChanged(true);
            ResizeWindow(window, mouseLoc, NULL, NULL);
            floater->ResizeChanged(false);
            break;
    }
    return noErr;
}


/*
 * This view subclass implements drawing to the desktop.
 */
class VoqvDesktop : public VideoOutputQuartzView
{
  public:
    VoqvDesktop(QuartzData *pData)
    : VideoOutputQuartzView(pData)
    {
        applyTVoffset = true;
        name = "Desktop: ";
    };

   ~VoqvDesktop()
    {
        End();
        EndPort();
    };

  protected:
    WindowRef window;

    bool BeginPort(void)
    {
        viewLock.lock();

        Rect bounds;
        bounds.top = bounds.left = 0;
        bounds.right  = CGDisplayPixelsWide(parentData->screen);
        bounds.bottom = CGDisplayPixelsHigh(parentData->screen);
        if (CreateNewWindow(kPlainWindowClass,
                            kWindowNoShadowAttribute |
                              kWindowOpaqueForEventsAttribute,
                            &bounds,
                            &window))
        {
            puts("CreateNewWindow failed");
            viewLock.unlock();
            return false;
        }
        WindowGroupRef winGroup;
        if (CreateWindowGroup(0, &winGroup))
        {
            puts("CreateWindowGroup failed");
            viewLock.unlock();
            return false;
        }
        SetWindowGroupLevel(winGroup, kCGDesktopIconWindowLevel - 1);
        SetWindowGroup(window, winGroup);
        RGBColor black = { 0, 0, 0 };
        SetWindowContentColor(window, &black);

        thePort = GetWindowPort(window);
        if (!thePort)
        {
            puts("GetWindowPort failed");
            viewLock.unlock();
            return false;
        }
        viewLock.unlock();
        ShowWindow(window);
        // don't lose focus from main window
        SelectWindow(parentData->window);

        return true;
    };

    void EndPort(void)
    {
        viewLock.lock();
        thePort = NULL;
        if (window)
        {
            DisposeWindow(window);
            window = NULL;
        }
        viewLock.unlock();
    };
};

/*
 * VideoOutputQuartz implementation
 */
VideoOutputQuartz::VideoOutputQuartz(void)
                 : VideoOutput()
{
    Started = 0; 

    pauseFrame.buf = NULL;

    data = new QuartzData();
    data->views.setAutoDelete(true);
}

VideoOutputQuartz::~VideoOutputQuartz()
{
    Exit();

    delete data;
}

void VideoOutputQuartz::VideoAspectRatioChanged(float aspect)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::VideoAspectRatioChanged"
                    "(aspect=%1) [was %2]")
            .arg(aspect).arg(data->srcAspect));

    VideoOutput::VideoAspectRatioChanged(aspect);

    data->srcAspect = aspect;
    data->srcMode   = letterbox;

    VideoOutputQuartzView *view = NULL;
    for (view = data->views.first(); view; view = data->views.next())
        view->VideoAspectRatioChanged(aspect);
}

void VideoOutputQuartz::Zoom(int direction)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::Zoom(direction=%1)").arg(direction));

    VideoOutput::Zoom(direction);
    MoveResize();
    data->ZoomedIn = ZoomedIn;
    data->ZoomedUp = ZoomedUp;
    data->ZoomedRight = ZoomedRight;

    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
        view->Zoom(direction);
    }
}

void VideoOutputQuartz::InputChanged(int width, int height, float aspect)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::InputChanged(width=%1, height=%2, aspect=%3")
                   .arg(width).arg(height).arg(aspect));

    VideoOutput::InputChanged(width, height, aspect);

    DeleteQuartzBuffers();

    data->srcWidth  = width;
    data->srcHeight = height;
    data->srcAspect = aspect;
    data->srcMode   = letterbox;

    CreateQuartzBuffers();

    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
        view->InputChanged(width, height, aspect);
    }

    MoveResize();    
}

int VideoOutputQuartz::GetRefreshRate(void)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::GetRefreshRate() [returning %1]")
                   .arg((int)data->refreshRate));

    return (int)data->refreshRate;
}

bool VideoOutputQuartz::Init(int width, int height, float aspect,
                             WId winid, int winx, int winy,
                             int winw, int winh, WId embedid)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::Init(width=%1, height=%2, aspect=%3, winid=%4\n winx=%5, winy=%6, winw=%7, winh=%8, WId embedid=%9)")
                   .arg(width)
                   .arg(height)
                   .arg(aspect)
                   .arg(winid)
                   .arg(winx)
                   .arg(winy)
                   .arg(winw)
                   .arg(winh)
                   .arg(embedid));

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames, 
                  kPrebufferFramesNormal, kPrebufferFramesSmall, 
                  kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    data->srcWidth  = width;
    data->srcHeight = height;
    data->srcAspect = aspect;
    data->srcMode   = letterbox;
    
    data->ZoomedIn = 0;
    data->ZoomedUp = 0;
    data->ZoomedRight = 0;

    // Initialize QuickTime
    if (EnterMovies())
    {
        puts("EnterMovies failed");
        return false;
    }

    // Find the main window
    data->window = FrontNonFloatingWindow();
    if (!data->window)
    {
        puts("Find window failed");
        return false;
    }

    // It may be possible to locate a display that best matches the requested
    // dimensions and aspect ratio, but for simplicity we will just use the
    // display that contains the MythTV window.

    Rect windowBounds;
    if (GetWindowBounds(data->window, kWindowContentRgn, &windowBounds))
    {
        puts("GetWindowBounds failed");
        return false;
    }
    CGPoint pt;
    pt.x = windowBounds.left;
    pt.y = windowBounds.top;
    CGDisplayCount ct;
    data->screen = NULL;
    if (CGGetDisplaysWithPoint(pt, 1, &data->screen, &ct))
    {
        // window is offscreen? use main display instead
        data->screen = CGMainDisplayID();
    }

    // Find the refresh rate of our screen
    CFDictionaryRef m;
    m = CGDisplayCurrentMode(data->screen);
    data->refreshRate = getCFdouble(m, kCGDisplayRefreshRate);
    if (data->refreshRate == 0.0)	// LCD display?
        data->refreshRate = 150;

    // Global configuration options
    data->scaleUpVideo = gContext->GetNumSetting("MacScaleUp", 1);
    data->drawInWindow = gContext->GetNumSetting("GuiSizeForTV", 0);
    data->windowedMode = gContext->GetNumSetting("RunFrontendInWindow", 0);
    data->correctGamma = gContext->GetNumSetting("MacGammaCorrect", 0);
    
    if (gContext->GetNumSetting("MacYuvConversion", 1))
        data->yuvConverter = yuv2vuy_init_altivec();
    else
        data->yuvConverter = NULL;

    if (!CreateQuartzBuffers())
    {
        puts("CreateQuartzBuffers failed");
        return false;
    }

    VideoOutputQuartzView *tmp;
    if (!data->drawInWindow)
    {
        // Fullscreen will take over everything
        tmp = new VoqvFullscreen(data);
        tmp->SetFrameSkip(gContext->GetNumSetting("MacFullSkip", 0));
        data->views.append(tmp);
    }
    else if (!data->windowedMode)
    {
        // Full GUI is hidden, only show the main window
        tmp = new VoqvMainWindow(data, 1.0);
        tmp->SetFrameSkip(gContext->GetNumSetting("MacFullSkip", 0));
        data->views.append(tmp);
    }
    else
    {
        // Full GUI is shown, many output options
        if (gContext->GetNumSetting("MacMainEnabled", 1))
        {
            float opacity =
                gContext->GetNumSetting("MacMainOpacity", 100) / 100.0;
            tmp = new VoqvMainWindow(data, opacity);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacMainSkip", 0));
            data->views.append(tmp);
        }
        if (gContext->GetNumSetting("MacFloatEnabled", 0))
        {
            float opacity =
                gContext->GetNumSetting("MacFloatOpacity", 100) / 100.0;
            tmp = new VoqvFloater(data, opacity);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacFloatSkip", 0));
            data->views.append(tmp);
        }
        if (gContext->GetNumSetting("MacDesktopEnabled", 0))
        {
            tmp = new VoqvDesktop(data);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacDesktopSkip", 0));
            data->views.append(tmp);
        }
        if (gContext->GetNumSetting("MacDockEnabled", 1))
        { 
            tmp = new VoqvDock(data);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacDockSkip", 3));
            data->views.append(tmp);
        }
    }

    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
         if (!view->Init())
         {
            puts("Init failed on view");
         }
    }

    MoveResize();
    Started = true;

    return true;
}

bool VideoOutputQuartz::CreateQuartzBuffers(void)
{
    for (int i = 0; i < vbuffers.allocSize(); i++)
    {
        vbuffers.at(i)->width  = XJ_width;
        vbuffers.at(i)->height = XJ_height;
        vbuffers.at(i)->bpp    = 12;
        vbuffers.at(i)->size   = XJ_width * XJ_height * vbuffers.at(i)->bpp / 8;
        vbuffers.at(i)->codec  = FMT_YV12;

        vbuffers.at(i)->buf = new unsigned char[vbuffers.at(i)->size + 64];
    }

    // Set up pause and scratch frames
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    pauseFrame.height = vbuffers.GetScratchFrame()->height;
    pauseFrame.width  = vbuffers.GetScratchFrame()->width;
    pauseFrame.bpp    = vbuffers.GetScratchFrame()->bpp;
    pauseFrame.size   = vbuffers.GetScratchFrame()->size;
    pauseFrame.buf    = new unsigned char[pauseFrame.size];
    pauseFrame.frameNumber = vbuffers.GetScratchFrame()->frameNumber;

    // Set up pixel storage and image description for source
    data->pixelLock.lock();

    int width, height;
    width = data->srcWidth;
    height = data->srcHeight;

    // Set up description to display YUV data.
    // The views will use this to initialize
    // their QuickTime decompressor.
    data->imgDesc =
        (ImageDescriptionHandle) NewHandleClear(sizeof(ImageDescription));
    HLock((Handle)(data->imgDesc));

    ImageDescription *desc = *data->imgDesc;

    desc->idSize = sizeof(ImageDescription);
    desc->cType = kYUV420CodecType;
    desc->version = 1;
    desc->revisionLevel = 0;
    desc->spatialQuality = codecNormalQuality;
    desc->width = width;
    desc->height = height;
    desc->hRes = Long2Fix(72);
    desc->vRes = Long2Fix(72);
    desc->depth = 24;
    desc->frameCount = 0;
    desc->dataSize = 0;
    desc->clutID = -1;
    
    if (data->yuvConverter)
    {
        desc->cType = k422YpCbCr8CodecType;
        desc->version = 2;
    }

    HUnlock((Handle)(data->imgDesc));

    // Set up storage area for one YUV frame (header + data)
    if (data->yuvConverter)
    {
        // 2VUY data needs no header
        data->pixelSize = width * height * 2;
        data->pixmapSize = data->pixelSize;
        data->pixmap = (PlanarPixmapInfoYUV420 *) new char[data->pixmapSize];
        data->pixelData = data->pixmap;
    }
    else
    {
        // YUV420 uses a descriptive header
        data->pixelSize = (width * height * 3) / 2;
        data->pixmapSize = sizeof(PlanarPixmapInfoYUV420) + data->pixelSize;
        data->pixmap = (PlanarPixmapInfoYUV420 *) new char[data->pixmapSize];
        data->pixelData = &(data->pixmap[1]);
    
        long offset = sizeof(PlanarPixmapInfoYUV420);
        data->pixmap->componentInfoY.offset = offset;
        data->pixmap->componentInfoY.rowBytes = width;
    
        offset += width * height;
        data->pixmap->componentInfoCb.offset = offset;
        data->pixmap->componentInfoCb.rowBytes = width / 2;
    
        offset += (width * height) / 4;
        data->pixmap->componentInfoCr.offset = offset;
        data->pixmap->componentInfoCr.rowBytes = width / 2;
    }
    
    data->pixelLock.unlock();

    return true;
}

void VideoOutputQuartz::Exit(void)
{
    if (Started) 
    {
        Started = false;

        data->views.clear();
        DeleteQuartzBuffers();
    }
}

void VideoOutputQuartz::DeleteQuartzBuffers()
{
    data->pixelLock.lock();
    if (data->imgDesc)
    {
        DisposeHandle((Handle)(data->imgDesc));
        data->imgDesc = NULL;
    }
    if (data->pixmap)
    {
        delete [] data->pixmap;
        data->pixmap = NULL;
        data->pixmapSize = 0;
        data->pixelData = NULL;
        data->pixelSize = 0;
    }
    data->pixelLock.unlock();

    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    for (int i = 0; i < vbuffers.allocSize(); i++)
    {
        delete [] vbuffers.at(i)->buf;
        vbuffers.at(i)->buf = NULL;
    }
}

void VideoOutputQuartz::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::EmbedInWidget(wid=%1, x=%2, y=%3, w=%4, h=%5)")
                   .arg(wid)
                   .arg(x)
                   .arg(y)
                   .arg(w)
                   .arg(h));

    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);
    
    data->pixelLock.lock();

    // warn other views that embedding is starting
    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
        view->EmbedChanged(true);
    }

    // create embedded widget
    data->embeddedView = new VoqvEmbedded(data, x, y, w, h);
    if (data->embeddedView)
    {
        data->embeddedView->Init();
        data->views.append(data->embeddedView);
    }
    
    data->pixelLock.unlock();
}

void VideoOutputQuartz::StopEmbedding(void)
{
    VERBOSE(VB_PLAYBACK,
        QString("VideoOutputQuartz::StopEmbedding()"));

    if (!embedding)
        return;

    VideoOutput::StopEmbedding();

    data->pixelLock.lock();
    
    // delete embedded widget
    if (data->embeddedView)
    {
        data->views.removeRef(data->embeddedView);
        data->embeddedView = NULL;
    }

    // tell other views to return to normal
    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
        view->EmbedChanged(false);
    }
    
    data->pixelLock.unlock();
}

void VideoOutputQuartz::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)t;

    if (!buffer)
        buffer = vbuffers.GetScratchFrame();

    framesPlayed = buffer->frameNumber + 1;
}

void VideoOutputQuartz::Show(FrameScanType t)
{
    (void)t;

    data->pixelLock.lock();
    for (VideoOutputQuartzView *view = data->views.first();
         view;
         view = data->views.next())
    {
        view->Show();
    }
    data->pixelLock.unlock();
}

void VideoOutputQuartz::DrawUnusedRects(bool)
{
}

void VideoOutputQuartz::UpdatePauseFrame(void)
{
    VideoFrame *pauseb = vbuffers.GetScratchFrame();
    VideoFrame *pauseu = vbuffers.head(kVideoBuffer_used);
    if (pauseu)
        memcpy(pauseFrame.buf, pauseu->buf, pauseu->size);
    else
        memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

void VideoOutputQuartz::ProcessFrame(VideoFrame *frame, OSD *osd,
                                     FilterChain *filterList,
                                     NuppelVideoPlayer *pipPlayer)
{
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &pauseFrame);
    }

    if (filterList)
        filterList->ProcessFrame(frame);

    if (m_deinterlacing &&
        m_deintFilter != NULL &&
        m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    ShowPip(frame, pipPlayer);
    DisplayOSD(frame, osd);

    if (m_deinterlacing &&
        m_deintFilter != NULL &&
        !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame);
    }

    // copy data to our buffer
    data->pixelLock.lock();
    if (data->pixelData)
    {
        if (data->yuvConverter)
        {
            int frameSize = frame->width * frame->height;
            data->yuvConverter((uint8_t *)(data->pixelData),
                               frame->buf,
                               &frame->buf[frameSize],
                               &frame->buf[frameSize * 5 / 4],
                               frame->width, frame->height,
                               (frame->width % 2), (frame->width % 2), 0);
        }
        else
            memcpy(data->pixelData, frame->buf, frame->size);
    }
    data->pixelLock.unlock();
}
