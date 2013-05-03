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
 * - Drawing on the desktop only updates the main screen?
 * - The floating window can only be created on the main screen?
 * - The fullscreen window doesn't display anything when created
 *   on any screen except, you guessed it, the main screen.
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
#include "mythcorecontext.h"
#include "filtermanager.h"
#define AVCODEC_AVCODEC_H   // prevent clash with QuickTime CodecType
#include "videoout_quartz.h"

#include "util-osx.h"

#ifdef USING_QUARTZ_VIDEO
#import <QuartzCore/CoreVideo.h>
#else
#import <CoreGraphics/CGBase.h>
#import <CoreGraphics/CGDisplayConfiguration.h>
#import <CoreGraphics/CGImage.h>
#endif
#import <Carbon/Carbon.h>
#import <QuickTime/QuickTime.h>

#include "osd.h"
#include "mythconfig.h"
#include "mythlogging.h"
#include "videodisplayprofile.h"

#define LOC     QString("VideoOutputQuartz::")

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

    virtual void HideForGUI(void);
    virtual void ShowAfterGUI(QRect size);

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

        embeddedView(NULL)
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
        LOG(VB_GENERAL, LOG_ERR,
            QString("VOQV::Begin(%1) - No graphics port available")
                .arg(name));
        viewLock.unlock();
        return false;
    }

    // Set initial output size
    Rect portBounds;
    GetPortBounds(thePort, &portBounds);
    LOG(VB_PLAYBACK, LOG_INFO, QString("%0Viewport currently %1,%2 -> %3,%4")
                         .arg(name).arg(portBounds.left).arg(portBounds.top)
                         .arg(portBounds.right).arg(portBounds.bottom));
    m_desired.setWidth(portBounds.right);
    m_desired.setHeight(portBounds.bottom);
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
        LOG(VB_GENERAL, LOG_ERR,
            QString("VOQV::Begin(%1) - DecompressSequenceBeginS failed")
                .arg(name));
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

    // constants for transformation operations
    Fixed one, zero;
    one  = Long2Fix(1);
    zero = Long2Fix(0);

    LOG(VB_PLAYBACK, LOG_INFO, QString("%0Viewport is %1 x %2")
                                .arg(name).arg(w).arg(h));
    LOG(VB_PLAYBACK, LOG_INFO, QString("%0Image is %1 x %2")
                                .arg(name).arg(sw).arg(sh));

    double hscale = (double) w / sw;
    double vscale = (double) h / sh;

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
        LOG(VB_PLAYBACK, LOG_INFO, QString("%0Scaling to %1 x %2 of original")
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
        LOG(VB_PLAYBACK, LOG_INFO, QString("%0Centering with %1, %2")
                             .arg(name).arg((w - sw)/2.0).arg((h - sh)/2.0));
        TranslateMatrix(&matrix, X2Fix((w - sw) / 2.0), X2Fix((h - sh) / 2.0));
    }

    // apply graphics port or embedding offset
    if (x || y)
    {
        LOG(VB_PLAYBACK, LOG_INFO, QString("%0Translating to %1, %2")
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
          LOG(VB_GENERAL, LOG_ERR,
              QString("VOQV::Show(%1)- DecompressSequenceFrameWhen failed")
                  .arg(name));
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
        Transform(newRect);
}

/// Subclasses that block the main window should hide
/// their output so that the GUI behind is fully visible.
void VideoOutputQuartzView::HideForGUI(void)
{
    // do nothing in default version
}

/// Subclasses that block the main window should re-enable their
/// output after the user has finished interacing with the GUI.
void VideoOutputQuartzView::ShowAfterGUI(QRect size)
{
    // do nothing in default version
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
        name = (char *) "Main window: ";
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvMainWindow::BeginPort() - GetWindowPort failed");
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

    void HideForGUI(void)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "VOQV::HideForGUI() main window");
        End();
    }

    void ShowAfterGUI(QRect size)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "VOQV::ShowAfterGUI() main window");
        Begin();
        Transform(size);
    }
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
        name = (char *) "Embedded window: ";
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvEmbedded::BeginPort() - GetWindowPort failed");
            viewLock.unlock();
            return false;
        }

        // Ensure Qt updates by invalidating the window.
        Rect portBounds;
        GetPortBounds(thePort, &portBounds);
        InvalWindowRect(parentData->window, &portBounds);

        viewLock.unlock();
        return true;
    };

    // Simple scaler setup that just uses m_desired to fill embed area:
    bool Begin(void)
    {
        viewLock.lock();
        if (DecompressSequenceBeginS(&theCodec, parentData->imgDesc,
                                     NULL, 0, thePort, NULL, NULL, NULL,
                                     srcCopy, theMask, 0,
                                     codecNormalQuality, bestSpeedCodec))
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("VOQV::Begin(%1) - DecompressSequenceBeginS failed")
                    .arg(name));
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
        name = (char *) "Full screen: ";
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvFullScreen::BeginPort() - Could not capture display");
            viewLock.unlock();
            return false;
        }

        // switch screen resolution if desired
        if (gCoreContext->GetNumSetting("UseVideoModes", 0))
        {
            DisplayRes *disp = DisplayRes::GetDisplayRes();
            disp->SwitchToVideo(parentData->srcWidth, parentData->srcHeight);
        }

        CGDisplayHideCursor(d);

        thePort = CreateNewPortForCGDisplayID((UInt32)d);
        if (!thePort)
        {
            LOG(VB_GENERAL, LOG_ERR, "VoqvFullScreen::BeginPort() - "
                                     "CreateNewPortForCGDisplayID failed");
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
        if (gCoreContext->GetNumSetting("UseVideoModes", 0))
            DisplayRes::GetDisplayRes()->SwitchToGUI();

        if (d)
        {
            CGDisplayShowCursor(d);
            CGDisplayRelease(d);
            d = NULL;
        }
        viewLock.unlock();
    };

    void HideForGUI(void)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "VOQV::HideForGUI() full screen");
        End();
        EndPort();
    }

    void ShowAfterGUI(QRect size)
    {
        LOG(VB_PLAYBACK, LOG_INFO, "VOQV::ShowAfterGUI() full screen");
        BeginPort();
        Begin();
        Transform(size);
    }
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
        name = (char *) "Dock icon: ";
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
            LOG(VB_GENERAL, LOG_ERR, "VoqvDock::BeginPort() - "
                    "BeginQDContextForApplicationDockTile failed");
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
        name = (char *) "Floating window: ";
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
    ToolboxObjectClassRef myClass;
    WindowRef             window;
    float                 alpha;
    bool                  resizing;

    bool BeginPort(void)
    {
        viewLock.lock();

        Rect bounds;
        bounds.top = bounds.left = bounds.right = bounds.bottom = 50;
        switch ((int)(10 * parentData->srcAspect))
        {
            case 17:
            case 18:
                bounds.right  += 320;
                bounds.bottom += 180;
                break;
            case 13:
                bounds.right  += 280;
                bounds.bottom += 210;
                break;
            default:
                bounds.right  += CGDisplayPixelsWide(parentData->screen) / 3;
                bounds.bottom += CGDisplayPixelsHigh(parentData->screen) / 3;
        }

        // custom window definition
        EventHandlerUPP myUPP = NewEventHandlerUPP(VoqvFloater_Callback);
        EventTypeSpec defEvents[] =
            { { kEventClassWindow, kEventWindowHitTest },
              { kEventClassWindow, kEventWindowDrawFrame },
              { kEventClassWindow, kEventWindowClickResizeRgn } };
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvFloater::BeginPort() - CreateCustomWindow failed");
            viewLock.unlock();
            return false;
        }
        SetWindowAlpha(window, alpha);
        RGBColor black = { 0, 0, 0 };
        SetWindowContentColor(window, &black);

        thePort = GetWindowPort(window);
        if (!thePort)
        {
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvFloater::BeginPort() - GetWindowPort failed");
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
        UnregisterToolboxObjectClass(myClass);
        viewLock.unlock();
    };
};

// The event callback for the floating window above
OSStatus VoqvFloater_Callback(EventHandlerCallRef inHandlerCallRef,
                              EventRef inEvent,
                              void *inUserData)
{
    (void)inHandlerCallRef;
    VoqvFloater *floater = reinterpret_cast<VoqvFloater*>(inUserData);
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
        name = (char *) "Desktop: ";
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvDesktop::BeginPort() - CreateNewWindow failed");
            viewLock.unlock();
            return false;
        }
        WindowGroupRef winGroup;
        if (CreateWindowGroup(0, &winGroup))
        {
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvDesktop::BeginPort() - CreateWindowGroup failed");
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
            LOG(VB_GENERAL, LOG_ERR,
                "VoqvDesktop::BeginPort() - GetWindowPort failed");
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

void VideoOutputQuartz::GetRenderOptions(render_opts &opts,
                                         QStringList &cpudeints)
{
    opts.renderers->append("quartz-blit");
    opts.deints->insert("quartz-blit", cpudeints);
    (*opts.osds)["quartz-blit"].append("softblend");
    (*opts.safe_renderers)["dummy"].append("quartz-blit");
    (*opts.safe_renderers)["nuppel"].append("quartz-blit");
    if (opts.decoders->contains("ffmpeg"))
        (*opts.safe_renderers)["ffmpeg"].append("quartz-blit");
    if (opts.decoders->contains("vda"))
        (*opts.safe_renderers)["vda"].append("quartz-blit");
    if (opts.decoders->contains("crystalhd"))
        (*opts.safe_renderers)["crystalhd"].append("quartz-blit");
    (*opts.render_group)["quartz"].append("quartz-blit");
    opts.priorities->insert("quartz-blit", 70);
}

/** \class VideoOutputQuartz
 *  \brief Implementation of Quartz (Mac OS X windowing system) video output
 */
VideoOutputQuartz::VideoOutputQuartz() :
    VideoOutput(), Started(false), data(new QuartzData())
{
    init(&pauseFrame, FMT_YV12, NULL, 0, 0, 0, 0);
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
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("VideoAspectRatioChanged(aspect=%1) [was %2]")
            .arg(aspect).arg(data->srcAspect));

    VideoOutput::VideoAspectRatioChanged(aspect);

    data->srcAspect = aspect;
    data->srcMode   = db_aspectoverride;
}

// this is documented in videooutbase.cpp
void VideoOutputQuartz::Zoom(ZoomDirection direction)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Zoom(direction=%1)").arg(direction));

    VideoOutput::Zoom(direction);
    MoveResize();
}

void VideoOutputQuartz::ToggleAdjustFill(AdjustFillMode adjustFill)
{
    // Calculate the desired output rectangle for this fill mode.
    VideoOutput::ToggleAdjustFill(adjustFill);

    // We could change all the views, but the user probably only
    // wants the main one (window or fullscreen) to change.
    data->views[0]->MoveResize(window.GetDisplayVideoRect());
}

void VideoOutputQuartz::MoveResize(void)
{
    // This recalculates the desired output rectangle, based on
    // the user's current aspect/fill/letterbox/zoom settings.
    VideoOutput::MoveResize();

    QRect newRect = window.GetDisplayVideoRect();

    vector<VideoOutputQuartzView*>::iterator it;
    for (it = data->views.begin(); it != data->views.end(); ++it)
    {
        (*it)->MoveResize(newRect);
    }
}

void VideoOutputQuartz::ToggleAspectOverride(AspectOverrideMode aspectMode)
{
    VideoOutput::ToggleAspectOverride(aspectMode);
    MoveResize();
}

bool VideoOutputQuartz::InputChanged(const QSize &video_dim_buf,
                                     const QSize &video_dim_disp,
                                     float        aspect,
                                     MythCodecID  av_codec_id,
                                     void        *codec_private,
                                     bool        &aspect_only)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("InputChanged(WxH = %1x%2, aspect=%3")
            .arg(video_dim_disp.width())
            .arg(video_dim_disp.height()).arg(aspect));

    bool cid_changed = (video_codec_id != av_codec_id);
    bool res_changed = video_dim_disp != window.GetActualVideoDim();
    bool asp_changed = aspect != window.GetVideoAspect();

    VideoOutput::InputChanged(video_dim_buf, video_dim_disp,
                              aspect, av_codec_id, codec_private,
                              aspect_only);

    if (!res_changed && !cid_changed)
    {
        aspect_only = true;
        // TODO we should clear our buffers to black here..
        if (asp_changed)
        {
            MoveResize();
        }
        return true;
    }

    const QSize video_dim = window.GetVideoDispDim();

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

bool VideoOutputQuartz::Init(const QSize &video_dim_buf,
                             const QSize &video_dim_disp,
                             float aspect,
                             WId winid, const QRect &win_rect,
                             MythCodecID codec_id)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("Init(WxH %1x%2, aspect=%3, winid=%4\n\t\t\t"
                "win_bounds(x %5, y%6, WxH %7x%8))")
            .arg(video_dim_disp.width()).arg(video_dim_disp.height())
            .arg(aspect).arg(winid)
            .arg(win_rect.x()).arg(win_rect.y())
            .arg(win_rect.width()).arg(win_rect.height()));

    vbuffers.Init(kNumBuffers, true, kNeedFreeFrames,
                  kPrebufferFramesNormal, kPrebufferFramesSmall,
                  kKeepPrebuffer);
    VideoOutput::Init(video_dim_buf, video_dim_disp, aspect, winid, win_rect, codec_id);

    const QSize video_dim = window.GetVideoDim();
    data->srcWidth  = video_dim.width();
    data->srcHeight = video_dim.height();
    data->srcAspect = aspect;
    data->srcMode   = db_aspectoverride;

    // Initialize QuickTime
    if (EnterMovies())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Init() - EnterMovies failed");
        return false;
    }

    // Find the main window
    data->window = FrontNonFloatingWindow();
    if (!data->window)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Init() - Find window failed");
        return false;
    }

    // It may be possible to locate a display that best matches the requested
    // dimensions and aspect ratio, but for simplicity we will just use the
    // display that contains the MythTV window.

    if (GetWindowBounds(data->window,
                        kWindowStructureRgn, &(data->windowBounds)))
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Init() - GetWindowBounds failed");
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
        window.SetDisplayDim(QSize((uint) size_in_mm.width,
                                       (uint) size_in_mm.height));
        window.SetDisplayAspect(size_in_mm.width / size_in_mm.height);
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Screen size is %1 x %2 (mm), aspect %3")
                             .arg(size_in_mm.width).arg(size_in_mm.height)
                             .arg(size_in_mm.width / size_in_mm.height));
    }

    // Global configuration options
    data->scaleUpVideo = gCoreContext->GetNumSetting("MacScaleUp", 1);
    data->drawInWindow = gCoreContext->GetNumSetting("GuiSizeForTV", 0);
    data->windowedMode = gCoreContext->GetNumSetting("RunFrontendInWindow", 0);
    data->correctGamma = gCoreContext->GetNumSetting("MacGammaCorrect", 0);

    data->convertI420to2VUY = get_i420_2vuy_conv();


    if (data->drawInWindow)
    {
        // display_aspect and _dim have to be scaled to actual window size
        float winWidth  = size_in_mm.width  * win_rect.width()
                          / get_int_CF(m, kCGDisplayWidth);
        float winHeight = size_in_mm.height * win_rect.height()
                          / get_int_CF(m, kCGDisplayHeight);
        window.SetDisplayDim(QSize(winWidth, winHeight));
        window.SetDisplayAspect(winWidth / winHeight);
        LOG(VB_PLAYBACK, LOG_INFO,
            QString("Main window is %1 x %2 (mm), aspect %3")
                             .arg((int)winWidth).arg((int)winHeight)
                             .arg(winWidth / winHeight));
    }

    if (!CreateQuartzBuffers())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Init() - CreateQuartzBuffers failed");
        return false;
    }


    // Create the output view objects
    VideoOutputQuartzView *tmp;
    if (!data->drawInWindow)
    {
        // Fullscreen will take over everything
        tmp = new VoqvFullscreen(data);
        tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacFullSkip", 0));
        data->views.push_back(tmp);
    }
    else if (!data->windowedMode)
    {
        // Full GUI is hidden, only show the main window
        tmp = new VoqvMainWindow(data, 1.0);
        tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacFullSkip", 0));
        data->views.push_back(tmp);
    }
    else
    {
        // Full GUI is shown, many output options
        if (gCoreContext->GetNumSetting("MacMainEnabled", 1))
        {
            float opacity =
                gCoreContext->GetNumSetting("MacMainOpacity", 100) / 100.0;
            tmp = new VoqvMainWindow(data, opacity);
            tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacMainSkip", 0));
            data->views.push_back(tmp);
        }
        else
        {
            // If video in the main window is not enabled,
            // hide (shrink) it so it is not in the way
            LOG(VB_PLAYBACK, LOG_INFO, "Shrinking Main Window to 1x1");
            SizeWindow(data->window, 1, 1, true);
        }
        if (gCoreContext->GetNumSetting("MacFloatEnabled", 0))
        {
            float opacity =
                gCoreContext->GetNumSetting("MacFloatOpacity", 100) / 100.0;
            tmp = new VoqvFloater(data, opacity);
            tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacFloatSkip", 0));
            data->views.push_back(tmp);
        }
        if (gCoreContext->GetNumSetting("MacDesktopEnabled", 0))
        {
            tmp = new VoqvDesktop(data);
            tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacDesktopSkip", 0));
            data->views.push_back(tmp);
        }
        if (gCoreContext->GetNumSetting("MacDockEnabled", 1))
        {
            tmp = new VoqvDock(data);
            tmp->SetFrameSkip(gCoreContext->GetNumSetting("MacDockSkip", 3));
            data->views.push_back(tmp);
        }
    }

    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
    {
        if (!(*it)->Init())
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Init() - QuartzView Init() failed");
        }
    }

    MoveResize();
    Started = true;

    return true;
}

void VideoOutputQuartz::SetVideoFrameRate(float playback_fps)
{
    LOG(VB_PLAYBACK, LOG_INFO, QString("SetVideoFrameRate(%1) - unimplemented?")
                         .arg(playback_fps));
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
    const QSize video_dim = window.GetVideoDim();
    db_vdisp_profile->SetInput(video_dim);
    QStringList renderers = GetAllowedRenderers(video_codec_id, video_dim);
    QString     renderer  = QString::null;

    QString tmp = db_vdisp_profile->GetVideoRenderer();
    LOG(VB_PLAYBACK, LOG_INFO, LOC + 
        QString("CreateQuartzBuffers() render: %1, allowed: %2")
            .arg(tmp).arg(toCommaList(renderers)));

    if (renderers.contains(tmp))
        renderer = tmp;
    else if (!renderers.empty())
        renderer = renderers[0];
    else
    {
        LOG(VB_GENERAL, LOG_ERR, "Failed to find a video renderer");
        return false;
    }

    // reset this so that all the prefs are reinitialized
    db_vdisp_profile->SetVideoRenderer(renderer);
    LOG(VB_GENERAL, LOG_INFO, LOC + "VProf: " + db_vdisp_profile->toString());

    vbuffers.CreateBuffers(FMT_YV12, video_dim.width(), video_dim.height());

    // Set up pause frame
    if (pauseFrame.buf)
        delete [] pauseFrame.buf;

    VideoFrame *scratch = vbuffers.GetScratchFrame();

    init(&pauseFrame, FMT_YV12, new unsigned char[scratch->size],
         scratch->width, scratch->height, scratch->size);

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
            LOG(VB_PLAYBACK, LOG_INFO,
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

void VideoOutputQuartz::EmbedInWidget(const QRect &rect)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("EmbedInWidget(x=%1, y=%2, w=%3, h=%4)")
                         .arg(rect.left()).arg(rect.top())
                         .arg(rect.width()).arg(rect.height()));

    if (window.IsEmbedding())
        return;

    VideoOutput::EmbedInWidget(rect);
    // Base class has now calculated Aspect/Fill,
    // so copy for precision sizing of new widget:
    QRect newArea = window.GetDisplayVideoRect();
    LOG(VB_PLAYBACK, LOG_INFO, LOC +
        QString("now - EmbedInWidget(x=%1, y=%2, w=%3, h=%4)")
                         .arg(newArea.left()).arg(newArea.top())
                         .arg(newArea.width()).arg(newArea.height()));

    data->pixelLock.lock();

    // create embedded widget
    data->embeddedView = new VoqvEmbedded(data, newArea.left(), newArea.top(),
                                          newArea.width(), newArea.height());
    if (data->embeddedView)
    {
        data->embeddedView->Init();
        data->views.push_back(data->embeddedView);
    }

    data->pixelLock.unlock();
}

void VideoOutputQuartz::StopEmbedding(void)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "StopEmbedding()");

    if (!window.IsEmbedding())
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

    data->pixelLock.unlock();
}

/**
 * Makes sure we have a valid frame to show.
 */
void VideoOutputQuartz::PrepareFrame(VideoFrame *buffer, FrameScanType t,
                                     OSD *osd)
{
    (void)osd;
    (void)t;

    if (buffer)
        framesPlayed = buffer->frameNumber + 1;
}

/** \brief
 * Display the frame
 * \sa VideoOutputQuartzView
 */
void VideoOutputQuartz::Show(FrameScanType t)
{
    (void)t;

    data->pixelLock.lock();
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
        (*it)->Show();
    data->pixelLock.unlock();
}

void VideoOutputQuartz::DrawUnusedRects(bool)
{
}

void VideoOutputQuartz::UpdatePauseFrame(int64_t &disp_timecode)
{
    if (!pauseFrame.buf)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "UpdatePauseFrame() - no buffers?");
        return;
    }

    VideoFrame *pauseb = vbuffers.GetScratchFrame();
    VideoFrame *pauseu = vbuffers.Head(kVideoBuffer_used);
    if (pauseu)
        CopyFrame(&pauseFrame, pauseu);
    else
        CopyFrame(&pauseFrame, pauseb);

    disp_timecode = pauseFrame.disp_timecode;
}

/**
 * Draw OSD, apply filters and deinterlacing,
 * copy frame buffer if using QuickTime to decode.
 */
void VideoOutputQuartz::ProcessFrame(VideoFrame *frame, OSD *osd,
                                     FilterChain *filterList,
                                     const PIPMap &pipPlayers,
                                     FrameScanType scan)
{
    if (!frame)
    {
        frame = vbuffers.GetScratchFrame();
        CopyFrame(vbuffers.GetScratchFrame(), &pauseFrame);
    }

    CropToDisplay(frame);

    if (filterList)
        filterList->ProcessFrame(frame);

    if (m_deinterlacing &&
        m_deintFilter != NULL &&
        m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    ShowPIPs(frame, pipPlayers);
    if (osd && !window.IsEmbedding())
        DisplayOSD(frame, osd);

    if (m_deinterlacing &&
        m_deintFilter != NULL &&
        !m_deinterlaceBeforeOSD)
    {
        m_deintFilter->ProcessFrame(frame, scan);
    }

    QMutexLocker locker(&data->pixelLock);
    if (!data->pixelData)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "ProcessFrame(): NULL pixelData!");
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

/// Subclassed so we can hide the QuickTime drawn layer and show the GUI
void VideoOutputQuartz::ResizeForGui(void)
{
    data->pixelLock.lock();
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
        (*it)->HideForGUI();
    data->pixelLock.unlock();

    VideoOutput::ResizeForGui();
}

/// Subclassed so we can redisplay the QuickTime layer after ResizeForGui()
void VideoOutputQuartz::ResizeForVideo(uint width, uint height)
{
    VideoOutput::ResizeForVideo(width, height);

    // Base class gives us the correct dimensions for main window/screen:
    QRect size = window.GetDisplayVideoRect();

    data->pixelLock.lock();
    vector<VideoOutputQuartzView*>::iterator it = data->views.begin();
    for (; it != data->views.end(); ++it)
        (*it)->ShowAfterGUI(size);
    data->pixelLock.unlock();
}

QStringList VideoOutputQuartz::GetAllowedRenderers(
    MythCodecID myth_codec_id, const QSize &video_dim)
{
    (void) video_dim;

    QStringList list;

    if (codec_is_std(myth_codec_id))
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
    if (dec == "ffmpeg")
        return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
    return (MythCodecID)(kCodec_MPEG1 + (stream_type-1));
}
