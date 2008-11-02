/******************************************************************************
 * = NAME
 * videoout_quartz.cpp
 *
 * = DESCRIPTION
 * Basic video for Mac OS X, using an unholy amalgamation of QuickTime,
 * QuickDraw, Quartz/Core Graphics, and undocumented DVD playback APIs.
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
#include <vector>
#include <iostream>
#include <algorithm>
using namespace std;

#include "DisplayRes.h"
#include "yuv2rgb.h"
#include "uitypes.h"
#include "mythcontext.h"
#include "filtermanager.h"
#include "videoout_quartz.h"

#include "util-osx.h"

#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <CoreGraphics/CGImage.h>
#import <Carbon/Carbon.h>
#import <QuickTime/QuickTime.h>

#include "osd.h"
#include "osdsurface.h"
#include "libmythdb/mythconfig.h"
#include "libmythdb/mythverbose.h"
#include "videodisplayprofile.h"
#include "videoout_dvdv.h"

#define LOC QString("VideoOutputQuartz: ")
#define LOC_ERR QString("VideoOutputQuartz Error: ")

/**
 * An abstract class for implementing QuickTime output viewports.
 *
 * This class is further sub-classed for different Mac OS X UI output types.
 * e.g. Main Window, Full Screen, Dock Icon, Finder/Desktop background
 */
class VideoOutputQuartzView
{
  public:
    VideoOutputQuartzView(QuartzData *pData);
    virtual ~VideoOutputQuartzView();

    virtual bool Init(void);

    virtual void SetFrameSkip(int numskip);
    virtual void Show(void);

    virtual void InputChanged(int width, int height, float aspect,
                              MythCodecID av_codec_id);
    virtual void MoveResize(QRect newRect);

    virtual void EmbedChanged(bool embedded);

  protected:
    virtual bool Begin(void);
    virtual void End(void);
    virtual void Transform(QRect newRect);
    virtual void BlankScreen(bool deferred);

    // Subclasses implement the following methods:
    virtual bool BeginPort(void) = 0;   // Set up a valid thePort object
    virtual void EndPort(void) = 0;     // Release thePort

    char *             name;          // Name, for verbose output

    QuartzData *       parentData;    // information about video source is here

    CGrafPtr           thePort;       // QuickDraw graphics port
    QRect              m_desired;     // Desired output size characteristics
    ImageSequence      theCodec;      // QuickTime sequence ID
    RgnHandle          theMask;       // clipping region

    int                frameSkip;     // do we skip frames?
    int                frameCounter;  // keep track of skip status
    bool               drawBlank;     // draw a blank frame before next Show

    /// Set if this view can use the aspect/fill/zoom calculations
    /// from the base class (which are passed in by MoveResize()),
    /// to rescale in the output rectangle via Transform().
    bool               applyMoveResize;

    QMutex             viewLock;
};


/*
 * The floating window class needs an event callback.
 */
OSStatus VoqvFloater_Callback(EventHandlerCallRef inHandlerCallRef,
                              EventRef inEvent,
                              void *inUserData);

/*
 * The class containing most of VideoOutputQuartz's variables
 */
class QuartzData
{
  public:
    QuartzData() :
        srcWidth(0),                srcHeight(0),
        srcAspect(1.3333f),         srcMode(kAspect_Off),

        pixelData(0),               pixelSize(0),
        pixelLock(),

        window(0),
        screen(0),                  refreshRate(60.0f),

        drawInWindow(false),        windowedMode(false),
        scaleUpVideo(false),        correctGamma(false),
        convertI420to2VUY(NULL),

        embeddedView(NULL),         dvdv(NULL)
    {;}
    ~QuartzData() { ClearViews(); }

    void ClearViews(void)
    {
        vector<VideoOutputQuartzView*>::iterator it = views.begin();
        for (; it != views.end(); ++it)
            delete *it;
        views.clear();
    }

    // Stored information about the media stream:
    int                srcWidth,
                       srcHeight;
    float              srcAspect;
    int                srcMode;           // letterbox mode

    // Pixel storage for the media stream:
    ImageDescriptionHandle imgDesc;       // source description header
    char *             pixelData;         // storage for one frame
    size_t             pixelSize;         // size of one frame
    QMutex             pixelLock;         // to update pixels safely

    // Information about the display:
    WindowRef          window;            // MythTV window
    Rect               windowBounds;      // dimensions, to restore size later
    CGDirectDisplayID  screen;            // screen containing main window
    float              refreshRate;       // for screen above

    // Global preferences:
    bool               drawInWindow;      // Fullscreen or in GUI view?
    bool               windowedMode;      // GUI runs in window?
    bool               scaleUpVideo;      // Enlarge video as needed?
    bool               correctGamma;      // Video gamma correction
    conv_i420_2vuy_fun convertI420to2VUY; // I420 -> 2VUY conversion function

    // Output viewports:
    vector<VideoOutputQuartzView*> views;   // current views

    // Embedding:
    VideoOutputQuartzView * embeddedView;    // special embedded widget

    DVDV                  * dvdv;            // MPEG acceleration data
};

VideoOutputQuartzView::VideoOutputQuartzView(QuartzData *pData)
    : name(NULL), parentData(pData),
    thePort(NULL), theCodec(0), theMask(NULL), frameSkip(1), frameCounter(0),
    drawBlank(true), applyMoveResize(false)
{
}

VideoOutputQuartzView::~VideoOutputQuartzView()
{
    End();
}

/// Set up the port and the QuickTime decompressor.
/// We assume that the parent has set up the pixel storage.
bool VideoOutputQuartzView::Begin(void)
{
    viewLock.lock();
    if (!thePort)
    {
        puts("No graphics port available");
        viewLock.unlock();
        return false;
    }

    // Set initial output size
    Rect portBounds;
    GetPortBounds(thePort, &portBounds);
    VERBOSE(VB_PLAYBACK, QString("%0Viewport currently %1,%2 -> %3,%4")
                         .arg(name).arg(portBounds.left).arg(portBounds.top)
                         .arg(portBounds.right).arg(portBounds.bottom));
    m_desired.setCoords(portBounds.left,  portBounds.top,
                     portBounds.right, portBounds.bottom);
#if 0
    // The clipping mask
    theMask = NewRgn();
    SetRectRgn(theMask, m_desired.left(),  m_desired.top(),
                        m_desired.width(), m_desired.height());
#endif

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
    Transform(m_desired);

    return true;
}

/// Clean up the codec.
void VideoOutputQuartzView::End(void)
{
    viewLock.lock();
    // Destroy codec
    if (theCodec)
    {
        CDSequenceEnd(theCodec);
        theCodec = 0;
        if (theMask)
        {
            DisposeRgn(theMask);
            theMask = NULL;
        }
    }
    viewLock.unlock();
}

/// Build the transformation matrix to scale the video appropriately.
void VideoOutputQuartzView::Transform(QRect newRect)
{
    MatrixRecord matrix;
    SetIdentityMatrix(&matrix);

    int x, y, w, h, sw, sh;
    x = newRect.left();
    y = newRect.top();
    w = newRect.width();
    h = newRect.height();
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
    float realAspect = sw * 1.0 / sh;
    if (fabsf(aspect - realAspect) > 0.015)
    {
        VERBOSE(VB_PLAYBACK, QString("Image aspect doesn't match"
                                     " its resolution (%1 vs %2).")
                             .arg(aspect).arg(realAspect));

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
                    QString("%0Scaling to %1 of height")
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
        case kAdjustFill_Full:
            // height only fills 3/4 of image, zoom up
            // (16:9 movie in 4:3 letterbox format on 16:9 screen)
            hscale = vscale = w * 1.0 / (sw * 0.75);
            break;
        case kAdjustFill_Half:
            // height only fills 3/4 of image, zoom up
            // (4:3 movie on 16:9 screen - 14:9 zoom is a good compromise)
            hscale = vscale = w * 7.0 / (sw * 6);
            break;
        case kAdjustFill_Stretch:
            // like 16 x 9 standard, but with a horizontal stretch applied
            hscale = vscale = fmin(h * 1.0 / sh, w * 1.0 / sw);
            hscale *= 4.0 / 3.0;
            break;
        default:
            // standard, fill viewport with scaled image
            hscale = vscale = fmin(h * 1.0 / sh, w * 1.0 / sw);
            break;
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

// apply the basic sizing to DVDV
#ifdef USING_DVDV
    if (parentData->dvdv)
    {
        parentData->dvdv->MoveResize(
            0, 0, parentData->srcWidth, parentData->srcHeight,
            (int)((w - sw) / 2.0), (int)((h - sh) / 2.0), sw, sh);
    }
#endif // USING_DVDV

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
        if (m_desired.width() && m_desired.height())
        {
            clipRect.left   = m_desired.left();
            clipRect.top    = m_desired.top();
            clipRect.right  = m_desired.right();
            clipRect.bottom = m_desired.bottom();
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
    if (theCodec && thePort && parentData->pixelData)
    {
      CodecFlags outFlags;

      // tell QuickTime to draw the current frame
      if (DecompressSequenceFrameWhen(theCodec,
                                      (Ptr)parentData->pixelData,
                                      parentData->pixelSize,
                                      0,
                                      &outFlags,
                                      NULL,
                                      NULL))
      {
          puts("DecompressSequenceFrameWhen failed");
      }
    }
    viewLock.unlock();
}

void VideoOutputQuartzView::InputChanged(int width, int height, float aspect,
                                         MythCodecID av_codec_id)
{
    (void)width;
    (void)height;
    (void)aspect;
    (void)av_codec_id;

    // need to redo codec, but not the port
    End();
    Begin();
}

void VideoOutputQuartzView::MoveResize(QRect newRect)
{
    if (applyMoveResize)
    {
        puts("MoveResize");
        Transform(newRect);
    }
}

/// Subclasses that block the main window should suspend
/// playback, hide windows, etc by overriding this method.
void VideoOutputQuartzView::EmbedChanged(bool embedded)
{
    // do nothing in default version
    (void)embedded;
}

/**
 * This view subclass implements full-size video display in the main window.
 */
class VoqvMainWindow : public VideoOutputQuartzView
{
  public:
    VoqvMainWindow(QuartzData *pData, float alphaBlend = 1.0)
    : VideoOutputQuartzView(pData)
    {
        alpha = fminf(1.0, fmaxf(0.0, alphaBlend));
        applyMoveResize = true;
        name = "Main window: ";
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

/**
 * This view subclass implements embedded display in the main window.
 */
class VoqvEmbedded : public VideoOutputQuartzView
{
  public:
    VoqvEmbedded(QuartzData *pData, int x, int y, int w, int h)
    : VideoOutputQuartzView(pData)
    {
        m_desired = QRect(x, y, w, h);
        name = "Embedded window: ";
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

        // The main class handles masking and resizing, since we set m_desired
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

/**
 * This view subclass implements fullscreen video display.
 */
class VoqvFullscreen : public VideoOutputQuartzView
{
  public:
    VoqvFullscreen(QuartzData *pData)
    : VideoOutputQuartzView(pData)
    {
        applyMoveResize = true;
        name = "Full screen: ";
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

/**
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

/**
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
            m_desired.setWidth(curBounds.right - curBounds.left);
            m_desired.setHeight(curBounds.bottom - curBounds.top);
            SetRectRgn(theMask, m_desired.left(),  m_desired.top(),
                                m_desired.width(), m_desired.height());
            Transform(m_desired);
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


/**
 * This view subclass implements drawing to the desktop.
 */
class VoqvDesktop : public VideoOutputQuartzView
{
  public:
    VoqvDesktop(QuartzData *pData)
    : VideoOutputQuartzView(pData)
    {
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

/** \class VideoOutputQuartz
 *  \brief Implementation of Quartz (Mac OS X windowing system) video output
 */
VideoOutputQuartz::VideoOutputQuartz(
    MythCodecID _myth_codec_id, void *codec_priv) :
    VideoOutput(), Started(false), data(new QuartzData()),
    myth_codec_id(_myth_codec_id)
{
    init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
    SetDVDVDecoder((DVDV*)codec_priv);
}

VideoOutputQuartz::~VideoOutputQuartz()
{
    if (data)
    {
        Exit();

        delete data;
        data = NULL;
    }
}

void VideoOutputQuartz::VideoAspectRatioChanged(float aspect)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::VideoAspectRatioChanged"
                    "(aspect=%1) [was %2]")
            .arg(aspect).arg(data->srcAspect));

    VideoOutput::VideoAspectRatioChanged(aspect);

    data->srcAspect = aspect;
    data->srcMode   = db_aspectoverride;
}

// this is documented in videooutbase.cpp
void VideoOutputQuartz::Zoom(ZoomDirection direction)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::Zoom(direction=%1)").arg(direction));

    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputQuartz::MoveResize(void)
{
    // This recalculates the desired output rectangle, based on
    // the user's current aspect/fill/letterbox/zoom settings.
    VideoOutput::MoveResize();

    vector<VideoOutputQuartzView*>::iterator it;
    for (it = data->views.begin(); it != data->views.end(); ++it)
    {
        (*it)->MoveResize(display_video_rect);
    }
}

bool VideoOutputQuartz::InputChanged(const QSize &input_size,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        *codec_private)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("InputChanged(WxH = %1x%2, aspect=%3")
            .arg(input_size.width())
            .arg(input_size.height()).arg(aspect));

    bool cid_changed = (myth_codec_id != av_codec_id);
    bool res_changed = input_size != video_disp_dim;
    bool asp_changed = aspect != video_aspect;

    VideoOutput::InputChanged(input_size, aspect, av_codec_id, codec_private);

    if (!res_changed && !cid_changed)
    {
        // TODO we should clear our buffers to black here..
        if (asp_changed)
            MoveResize();
        return true;
    }

    if (cid_changed)
    {
        myth_codec_id = av_codec_id;
        data->dvdv    = (DVDV*) codec_private;

        if ((data->dvdv && (kCodec_MPEG2_DVDV != myth_codec_id)) ||
            (!data->dvdv && (kCodec_NORMAL_END <= myth_codec_id)))
        {
            return false;
        }

        if (data->dvdv && !data->dvdv->SetVideoSize(video_dim))
        {
            return false;
        }
    }

    DeleteQuartzBuffers();

    data->srcWidth  = video_dim.width();
    data->srcHeight = video_dim.height();
    data->srcAspect = aspect;
    data->srcMode   = db_aspectoverride;

    CreateQuartzBuffers();

    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
    {
        (*it)->InputChanged(
            video_dim.width(), video_dim.height(), aspect, av_codec_id);
    }

    MoveResize();

    return true;
}

int VideoOutputQuartz::GetRefreshRate(void)
{
    VERBOSE(VB_PLAYBACK,
            QString("VideoOutputQuartz::GetRefreshRate() [returning %1]")
                   .arg((int)data->refreshRate));

    return (int) (1000000 / data->refreshRate);
}

bool VideoOutputQuartz::Init(int width, int height, float aspect,
                             WId winid, int winx, int winy,
                             int winw, int winh, WId embedid)
{
    VERBOSE(VB_PLAYBACK, LOC +
            QString("Init(WxH %1x%2, aspect=%3, winid=%4\n\t\t\t"
                    "win_bounds(x %5, y%6, WxH %7x%8), WId embedid=%9)")
            .arg(width).arg(height).arg(aspect).arg(winid)
            .arg(winx).arg(winy).arg(winw).arg(winh).arg(embedid));

    if ((data->dvdv && (kCodec_MPEG2_DVDV != myth_codec_id)) ||
        (!data->dvdv && (kCodec_NORMAL_END <= myth_codec_id)))
    {
        return false;
    }

    if (data->dvdv && !data->dvdv->SetVideoSize(QSize(width, height)))
    {
        return false;
    }

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    VideoOutput::Init(width, height, aspect, winid,
                      winx, winy, winw, winh, embedid);

    data->srcWidth  = video_dim.width();
    data->srcHeight = video_dim.height();
    data->srcAspect = aspect;
    data->srcMode   = db_aspectoverride;

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

    if (GetWindowBounds(data->window,
                        kWindowStructureRgn, &(data->windowBounds)))
    {
        puts("GetWindowBounds failed");
        return false;
    }
    CGPoint pt;
    pt.x = data->windowBounds.left;
    pt.y = data->windowBounds.top;
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
    data->refreshRate = get_float_CF(m, kCGDisplayRefreshRate);
    if (data->refreshRate == 0.0)    // LCD display?
        data->refreshRate = 150.0;   // Divisible by 25Hz and 30Hz
                                     // to minimise AV sync waiting

    // Find the display physical aspect ratio
    CGSize size_in_mm = CGDisplayScreenSize(data->screen);
    if ((size_in_mm.width > 0.0001f) && (size_in_mm.height > 0.0001f))
    {
        display_dim = QSize((uint) size_in_mm.width, (uint) size_in_mm.height);
        display_aspect = size_in_mm.width / size_in_mm.height;
    }

    // Global configuration options
    data->scaleUpVideo = gContext->GetNumSetting("MacScaleUp", 1);
    data->drawInWindow = gContext->GetNumSetting("GuiSizeForTV", 0);
    data->windowedMode = gContext->GetNumSetting("RunFrontendInWindow", 0);
    data->correctGamma = gContext->GetNumSetting("MacGammaCorrect", 0);

    data->convertI420to2VUY = get_i420_2vuy_conv();

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
        data->views.push_back(tmp);
    }
    else if (!data->windowedMode)
    {
        // Full GUI is hidden, only show the main window
        tmp = new VoqvMainWindow(data, 1.0);
        tmp->SetFrameSkip(gContext->GetNumSetting("MacFullSkip", 0));
        data->views.push_back(tmp);
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
            data->views.push_back(tmp);
        }
        else
        {
            // If video in the main window is not enabled,
            // hide (shrink) it so it is not in the way
            VERBOSE(VB_PLAYBACK, QString("Shrinking Main Window to 1x1"));
            SizeWindow(data->window, 1, 1, true);
        }
        if (gContext->GetNumSetting("MacFloatEnabled", 0))
        {
            float opacity =
                gContext->GetNumSetting("MacFloatOpacity", 100) / 100.0;
            tmp = new VoqvFloater(data, opacity);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacFloatSkip", 0));
            data->views.push_back(tmp);
        }
        if (gContext->GetNumSetting("MacDesktopEnabled", 0))
        {
            tmp = new VoqvDesktop(data);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacDesktopSkip", 0));
            data->views.push_back(tmp);
        }
        if (gContext->GetNumSetting("MacDockEnabled", 1))
        {
            tmp = new VoqvDock(data);
            tmp->SetFrameSkip(gContext->GetNumSetting("MacDockSkip", 3));
            data->views.push_back(tmp);
        }
    }

    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
    {
        if (!(*it)->Init())
        {
            puts("Init failed on view");
        }
    }

    MoveResize();
    Started = true;

    return true;
}

void VideoOutputQuartz::SetVideoFrameRate(float playback_fps)
{
    VERBOSE(VB_PLAYBACK, "SetVideoFrameRate("<<playback_fps<<")");
}

void VideoOutputQuartz::SetDVDVDecoder(DVDV *dvdvdec)
{
    QString renderer = "quartz-blit";

    (void) dvdvdec;

#ifdef USING_DVDV
    data->dvdv = dvdvdec;
    renderer = (data->dvdv) ? "quartz-accel" : renderer;
#endif // USING_DVDV

    db_vdisp_profile->SetVideoRenderer(renderer);
}

static QString toCommaList(const QStringList &list)
{
    QString ret = "";
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        ret += *it + ",";

    if (ret.length())
        return ret.left(ret.length()-1);

    return "";
}

bool VideoOutputQuartz::CreateQuartzBuffers(void)
{
    db_vdisp_profile->SetInput(video_dim);
    QStringList renderers = GetAllowedRenderers(myth_codec_id, video_dim);
    QString     renderer  = QString::null;

    QString tmp = db_vdisp_profile->GetVideoRenderer();
    VERBOSE(VB_PLAYBACK, LOC + "CreateQuartzBuffers() "
            <<QString("render: %1, allowed: %2")
            .arg(tmp).arg(toCommaList(renderers)));

    if (renderers.contains(tmp))
        renderer = tmp;
    else if (!renderers.empty())
        renderer = renderers[0];
    else
    {
        VERBOSE(VB_IMPORTANT, "Failed to find a video renderer");
        return false;
    }

    // reset this so that all the prefs are reinitialized
    db_vdisp_profile->SetVideoRenderer(renderer);
    VERBOSE(VB_IMPORTANT, LOC + "VProf: " + db_vdisp_profile->toString());

    vbuffers.CreateBuffers(video_dim.width(), video_dim.height());

    // Set up pause frame
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    VideoFrame *scratch = vbuffers.GetScratchFrame();

    init(&pauseFrame, FMT_YV12, new unsigned char[scratch->size],
         scratch->width, scratch->height, scratch->bpp, scratch->size);

    pauseFrame.frameNumber = scratch->frameNumber;


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
    desc->cType = k422YpCbCr8CodecType;
    desc->version = 2;
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

    HUnlock((Handle)(data->imgDesc));

    // Set up storage area for one YUV frame
    data->pixelSize = width * height * 2;
    data->pixelData = new char[data->pixelSize];

    data->pixelLock.unlock();

    return true;
}

void VideoOutputQuartz::Exit(void)
{
    if (Started)
    {
        Started = false;

        // Restore main window
        // (assuming it was shrunk i.e. we were not in full screen mode)
        if (data->windowedMode)
        {
            VERBOSE(VB_PLAYBACK,
                    QString("Restoring Main Window to %1x%2")
                    .arg(data->windowBounds.right - data->windowBounds.left)
                    .arg(data->windowBounds.bottom - data->windowBounds.top));
            SetWindowBounds(data->window, kWindowStructureRgn,
                            &(data->windowBounds));
        }

        data->ClearViews();
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
    if (data->pixelData)
    {
        delete [] data->pixelData;
        data->pixelData = NULL;
        data->pixelSize = 0;
    }
    data->pixelLock.unlock();

    if (pauseFrame.buf)
    {
        delete [] pauseFrame.buf;
        init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
    }

    vbuffers.DeleteBuffers();
}

void VideoOutputQuartz::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    VERBOSE(VB_PLAYBACK, "VideoOutputQuartz::EmbedInWidget(" +
            QString("wid=%1, x=%2, y=%3, w=%4, h=%5)")
            .arg(wid).arg(x).arg(y).arg(w).arg(h));

    if (embedding)
        return;

    VideoOutput::EmbedInWidget(wid, x, y, w, h);

    data->pixelLock.lock();

    // warn other views that embedding is starting
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
    {
        (*it)->EmbedChanged(true);
    }

    // create embedded widget
    data->embeddedView = new VoqvEmbedded(data, x, y, w, h);
    if (data->embeddedView)
    {
        data->embeddedView->Init();
        data->views.push_back(data->embeddedView);
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
        vector<VideoOutputQuartzView*>::iterator it =
            find(data->views.begin(), data->views.end(), data->embeddedView);
        if (it != data->views.end())
        {
            delete *it;
            data->views.erase(it);
        }
        data->embeddedView = NULL;
    }

    // tell other views to return to normal
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
        (*it)->EmbedChanged(false);

    data->pixelLock.unlock();
}

/**
 * If we are using DVDV hardware acceleration, decodes the frame.
 * Otherwise, just makes sure we have a valid frame to show.
 */
void VideoOutputQuartz::PrepareFrame(VideoFrame *buffer, FrameScanType t)
{
    (void)t;

#ifdef USING_DVDV
    if (data->dvdv && buffer)
        data->dvdv->DecodeFrame(buffer);
#endif // USING_DVDV

    if (buffer)
        framesPlayed = buffer->frameNumber + 1;
}

/** \brief
 * Display the frame, using either DVDV hardware acceleration,
 * or possibly several UI output types. \sa VideoOutputQuartzView
 */
void VideoOutputQuartz::Show(FrameScanType t)
{
    (void)t;

#ifdef USING_DVDV
    if (data->dvdv)
    {
        data->dvdv->ShowFrame();
        return;
    }
#endif // USING_DVDV

    data->pixelLock.lock();
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
        (*it)->Show();
    data->pixelLock.unlock();
}

void VideoOutputQuartz::DrawUnusedRects(bool)
{
}

void VideoOutputQuartz::UpdatePauseFrame(void)
{
    if (!pauseFrame.buf)
    {
        puts("VideoOutputQuartz::UpdatePauseFrame() - no buffers?");
        return;
    }

    VideoFrame *pauseb = vbuffers.GetScratchFrame();
    VideoFrame *pauseu = vbuffers.head(kVideoBuffer_used);
    if (pauseu)
        memcpy(pauseFrame.buf, pauseu->buf, pauseu->size);
    else
        memcpy(pauseFrame.buf, pauseb->buf, pauseb->size);
}

/**
 * Draw OSD, apply filters and deinterlacing,
 * copy frame buffer if using QuickTime to decode.
 */
void VideoOutputQuartz::ProcessFrame(VideoFrame *frame, OSD *osd,
                                     FilterChain *filterList,
                                     NuppelVideoPlayer *pipPlayer)
{
#ifdef USING_DVDV
    if (data->dvdv)
    {
        if (osd && osd->Visible())
        {
            OSDSurface *surface = osd->Display();
            if (surface && surface->Changed())
            {
                data->dvdv->DrawOSD(surface->y, surface->u,
                                    surface->v, surface->alpha);
            }
        }
        else
        {
            data->dvdv->DrawOSD(NULL, NULL, NULL, NULL);
        }
        return;   // no need to process frame, it won't be used
    }
#endif // USING_DVDV

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

    QMutexLocker locker(&data->pixelLock);
    if (!data->pixelData)
    {
        VERBOSE(VB_PLAYBACK,
                "VideoOutputQuartz::ProcessFrame(): NULL pixelData!");
        return;
    }

    // copy data to our buffer
    data->convertI420to2VUY(
        (unsigned char*) data->pixelData, frame->width<<1, // 2vuy
        frame->buf + frame->offsets[0], // Y plane
        frame->buf + frame->offsets[1], // U plane
        frame->buf + frame->offsets[2], // V plane
        frame->pitches[0], frame->pitches[1], frame->pitches[2],
        frame->width, frame->height);
}

QStringList VideoOutputQuartz::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    if (kCodec_MPEG2_DVDV == myth_codec_id)
    {
        list += "quartz-accel";
    }
    else if (kCodec_MPEG2_DVDV != myth_codec_id)
    {
        list += "quartz-blit";
    }

    return list;
}

MythCodecID VideoOutputQuartz::GetBestSupportedCodec(
    uint width, uint height,
    uint osd_width, uint osd_height,
    uint stream_type, uint fourcc)
{
    (void) osd_width;
    (void) osd_height;

    VideoDisplayProfile vdp;
    vdp.SetInput(QSize(width, height));
    QString dec = vdp.GetDecoder();
    if ((dec == "libmpeg2") || (dec == "ffmpeg"))
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    if ((dec == "macaccel") &&
        ((FOURCC_I420 == fourcc) || (FOURCC_IYUV == fourcc)) &&
        ((2 == stream_type) || (1 == stream_type)))
    {
        return kCodec_MPEG2_DVDV;
    }
    else
    {
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
    }
}
