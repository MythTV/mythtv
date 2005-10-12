#include <cmath>

#include "videooutbase.h"
#include "osd.h"
#include "osdsurface.h"
#include "NuppelVideoPlayer.h"

#include "../libmyth/mythcontext.h"

#ifdef USING_XV
#include "videoout_xv.h"
#endif

#ifdef USING_IVTV
#include "videoout_ivtv.h"
#endif

#ifdef USING_DIRECTFB
#include "videoout_directfb.h"
#endif

#ifdef USING_DIRECTX
#include "videoout_dx.h"
#endif

#ifdef Q_OS_MACX
#include "videoout_quartz.h"
#endif

#include "videoout_null.h"

#include "dithertable.h"

#include "../libavcodec/avcodec.h"

#include "filtermanager.h"

/**
 * \fn VideoOutput::InitVideoOut(VideoOutputType, MythCodecID)
 * \brief Creates a VideoOutput class compatible with both "type" and
 *        "codec_id".
 * \return instance of VideoOutput if successful, NULL otherwise.
 */
VideoOutput *VideoOutput::InitVideoOut(VideoOutputType type, MythCodecID codec_id)
{
    (void)type;

#ifdef USING_IVTV
    if (type == kVideoOutput_IVTV)
        return new VideoOutputIvtv();
#endif

#ifdef USING_DIRECTFB
    return new VideoOutputDirectfb();
#endif

#ifdef USING_DIRECTX
    return new VideoOutputDX();
#endif

#ifdef USING_XV
    return new VideoOutputXv(codec_id);
#endif

#ifdef Q_OS_MACX
    return new VideoOutputQuartz();
#endif

    VERBOSE(VB_IMPORTANT, "Not compiled with any useable video output method.");
    return NULL;
}

/**
 * \class VideoOutput
 * \brief This class serves as the base class for all video output methods.
 *
 * The basic use is:
 * \code
 * VideoOutputType type = kVideoOutput_Default;
 * vo = VideoOutput::InitVideoOut(type);
 * vo->Init(width, height, aspect ...);
 *
 * // Then create two threads.
 * // In the decoding thread
 * while (decoding)
 * {
 *     if (vo->WaitForAvailable(1000)
 *     {
 *         frame = vo->GetNextFreeFrame(); // remove frame from "available"
 *         av_lib_process(frame);   // do something to fill it.
 *         // call DrawSlice()      // if you need piecemeal processing
 *                                  // by VideoOutput use DrawSlice
 *         vo->ReleaseFrame(frame); // enqueues frame in "used" queue
 *     }
 * }
 *  
 * // In the displaying thread
 * while (playing)
 * {
 *     if (videoOutput->EnoughPrebufferedFrames())
 *     {
 *         // Sets "Last Shown Frame" to head of "used" queue
 *         videoOutput->StartDisplayingFrame();
 *         // Get pointer to "Last Shown Frame"
 *         frame = videoOutput->GetLastShownFrame();
 *         // add OSD, do any filtering, etc.
 *         vo->ProcessFrame(frame, osd, filters, pict-in-pict);
 *         // tells show what frame to be show, do other last minute stuff
 *         vo->PrepareFrame(frame, scan);
 *         // here you wait until it's time to show the frame
 *         // Show blits the last prepared frame to the screen
 *         // as quickly as possible.
 *         vo->Show(scan);
 *         // remove frame from the head of "used",
 *         // vo must get it into "available" eventually.
 *         vo->DoneDisplayingFrame();
 *     }
 * }
 * delete vo;
 * \endcode
 *
 *  Note: Show() may be called multiple times between PrepareFrame() and
 *        DoneDisplayingFrame(). But if a frame is ever removed from
 *        available via GetNextFreeFrame(), you must either call
 *        DoneDisplayFrame() or call DiscardFrame(VideoFrame*) on it.
 *
 *  Note: ProcessFrame() may be called multiple times on a frame, to
 *        update an OSD for example.
 *
 *  The VideoBuffers class handles the buffer tracking,
 *  see it for more details on the states a buffer can 
 *  take before it becomes available for reuse.
 *
 * \see VideoBuffers, NuppelVideoPlayer
 */

/**
 * \fn VideoOutput::VideoOutput()
 * \brief This constructor for VideoOutput must be followed by an 
 *        Init(int,int,float,WId,int,int,int,int,WId) call.
 */
VideoOutput::VideoOutput() :

    // Physical dimensions
    w_mm(400),                h_mm(300),                display_aspect(1.3333),
    myth_dsw(0),              myth_dsh(0),

    // Pixel dimensions
    XJ_width(640),            XJ_height(480),           videoAspect(1.3333),

    // Aspect override
    XJ_aspect(1.3333),        letterbox(kLetterbox_Off),

    // Screen settings
    img_xoff(0),              img_yoff(0),
    img_hscanf(0.0f),         img_vscanf(0.0f),

    imgx(0),     imgy(0),     imgw(0),     imgh(0),
    dispxoff(0), dispyoff(0), dispwoff(0), disphoff(0),

    dispx(0),    dispy(0),    dispw(0),    disph(0),
    olddispx(0), olddispy(0), olddispw(0), olddisph(0),

    // Picture settings
    brightness(0),            contrast(0),
    colour(0),                hue(0),

    // Zoom
    ZoomedIn(0),              ZoomedUp(0),              ZoomedRight(0),

    // Picture-in-Picture stuff
    PIPLocation(0),
    desired_piph(128),        desired_pipw(160),
    piph_in(-1),              pipw_in(-1),
    piph_out(-1),             pipw_out(-1),
    piptmpbuf(NULL),          pipscontext(NULL),

    // Deinterlacing
    m_deinterlacing(false),   m_deintfiltername("linearblend"),
    m_deintFiltMan(NULL),     m_deintFilter(NULL),
    m_deinterlaceBeforeOSD(true),

    // Various state variables
    embedding(false),         needrepaint(false),
    allowpreviewepg(true),    framesPlayed(0),

    errored(false)
{
    myth_dsw = gContext->GetNumSetting("DisplaySizeWidth", 0);
    myth_dsh = gContext->GetNumSetting("DisplaySizeHeight", 0);
}

/**
 * \fn VideoOutput::~VideoOutput()
 * \brief Shuts down video output.
 */
VideoOutput::~VideoOutput()
{
    ShutdownPipResize();

    if (m_deintFilter)
        delete m_deintFilter;
    if (m_deintFiltMan)
        delete m_deintFiltMan;
}

/**
 * \fn Init(int,int,float,WId,int,int,int,int,WId)
 * \brief Performs most of the initialization for VideoOutput.
 * \return true if successful, false otherwise.
 */
bool VideoOutput::Init(int width, int height, float aspect, WId winid,
                       int winx, int winy, int winw, int winh, WId embedid)
{
    (void)winid;
    (void)embedid;

    XJ_width = width;
    XJ_height = height;
    AspectSet(aspect);
    
    QString HorizScanMode = gContext->GetSetting("HorizScanMode", "overscan");
    QString VertScanMode = gContext->GetSetting("VertScanMode", "overscan");

    img_hscanf = gContext->GetNumSetting("HorizScanPercentage", 5) / 100.0;
    img_vscanf = gContext->GetNumSetting("VertScanPercentage", 5) / 100.0;

    img_xoff = gContext->GetNumSetting("xScanDisplacement", 0);
    img_yoff = gContext->GetNumSetting("yScanDisplacement", 0);

    PIPLocation = gContext->GetNumSetting("PIPLocation", 0);

    if (VertScanMode == "underscan")
    {
        img_vscanf = 0 - img_vscanf;
    }
    if (HorizScanMode == "underscan")
    {
        img_hscanf = 0 - img_hscanf;
    }

    if (winw && winh)
        VERBOSE(VB_PLAYBACK,
                QString("Over/underscan. V: %1, H: %2, XOff: %3, YOff: %4")
                .arg(img_vscanf).arg(img_hscanf).arg(img_xoff).arg(img_yoff));

    dispx = 0; dispy = 0;
    dispw = winw; disph = winh;

    imgx = winx; imgy = winy;
    imgw = XJ_width; imgh = XJ_height;

    if (imgw == 1920 && imgh == 1088)
        imgh = 1080; // ATSC 1920x1080

    brightness = gContext->GetNumSettingOnHost("PlaybackBrightness", 
                                               gContext->GetHostName(), 50);
    contrast = gContext->GetNumSettingOnHost("PlaybackContrast", 
                                             gContext->GetHostName(), 50);
    colour = gContext->GetNumSettingOnHost("PlaybackColour", 
                                           gContext->GetHostName(), 50);
    hue = gContext->GetNumSettingOnHost("PlaybackHue", 
                                        gContext->GetHostName(), 0);

    letterbox = gContext->GetNumSetting("AspectOverride", 0);
    AspectSet(aspect);

    embedding = false;

    return true;
}

/**
 * \fn VideoOutput::SetupDeinterlace(bool)
 * \brief Attempts to enable or disable deinterlacing.
 * \return true if successful, false otherwise.
 * \param overridefilter optional, explicitly use this nondefault deint filter
 */
bool VideoOutput::SetupDeinterlace(bool interlaced, 
                                   const QString& overridefilter)
{
    if (VideoOutputNull *null = dynamic_cast<VideoOutputNull *>(this))
    {
        (void)null;
        // null vidout doesn't deinterlace
        return !interlaced;
    }

    if (m_deinterlacing == interlaced)
        return m_deinterlacing;

    if (m_deintFiltMan)
    {
        delete m_deintFiltMan;
        m_deintFiltMan = NULL;
    }
    if (m_deintFilter)
    {
        delete m_deintFilter;
        m_deintFilter = NULL;
    }

    m_deinterlacing = interlaced;

    if (m_deinterlacing) 
    {
        m_deinterlaceBeforeOSD = true;

        VideoFrameType itmp = FMT_YV12;
        VideoFrameType otmp = FMT_YV12;
        int btmp;
        
        m_deintfiltername = gContext->GetSetting("DeinterlaceFilter", 
                                                 "linearblend");
        if (overridefilter != "")
            m_deintfiltername = overridefilter;

        m_deintFiltMan = new FilterManager;
        m_deintFilter = NULL;

        if (ApproveDeintFilter(m_deintfiltername))
        {
            m_deintFilter = m_deintFiltMan->LoadFilters(
                m_deintfiltername, itmp, otmp, XJ_width, XJ_height, btmp);
        }

        if (m_deintFilter == NULL) 
        {
            VERBOSE(VB_IMPORTANT,QString("Couldn't load deinterlace filter %1")
                    .arg(m_deintfiltername));
            m_deinterlacing = false;
            m_deintfiltername = "";
        }

        VERBOSE(VB_PLAYBACK, QString("Using deinterlace method %1")
                .arg(m_deintfiltername));

        if (m_deintfiltername == "bobdeint")
            m_deinterlaceBeforeOSD = false;
    }

    return m_deinterlacing;
}

/**
 * \fn VideoOutput::NeedsDoubleFramerate() const
 * \brief Should Prepare() and Show() be called twice for every ProcessFrame().
 *
 * \return m_deintfiltername == "bobdeint" && m_deinterlacing
 */
bool VideoOutput::NeedsDoubleFramerate() const
{
    // Bob deinterlace requires doubling framerate
    return (m_deintfiltername == "bobdeint" && m_deinterlacing);
}

/**
 * \fn VideoOutput::ApproveDeintFilter(const QString& filtername) const
 * \brief Approves all deinterlace filters, except bobdeint, which
 *        must be supported by a specific video output class.
 *
 * \return filtername != "bobdeint"
 */
bool VideoOutput::ApproveDeintFilter(const QString& filtername) const
{
    // Default to not supporting bob deinterlace
    return (filtername != "bobdeint");
}

/**
 * \fn VideoOutput::AspectSet(float aspect)
 * \brief Sets VideoOutput::videoAspect to aspect, and sets 
 *        VideoOutput::XJ_aspect the letterbox type, unless
 *        the letterbox type is kLetterbox_Fill.
 * 
 * \param aspect aspect ratio to use
 */
void VideoOutput::AspectSet(float aspect)
{
    videoAspect = aspect;
    XJ_aspect = aspect;
    
    // Override video's aspect if configured to do so
    switch (letterbox) 
    {
        default:
        case kLetterbox_Off:
        case kLetterbox_Fill:           XJ_aspect = videoAspect;
                                        break;

        case kLetterbox_4_3:
        case kLetterbox_4_3_Zoom:       XJ_aspect = (4.0 / 3);
                                        break;
        case kLetterbox_16_9:
        case kLetterbox_16_9_Zoom:
        case kLetterbox_16_9_Stretch:   XJ_aspect = (16.0 / 9);
                                        break;
    }
}

/**
 * \fn VideoOutput::AspectChanged(float aspect)
 * \brief Calls AspectSet(float aspect).
 * \param aspect aspect ratio to use
 */
void VideoOutput::AspectChanged(float aspect)
{
    AspectSet(aspect);
}

/**
 * \fn VideoOutput::InputChanged(int, int, float)
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
void VideoOutput::InputChanged(int width, int height, float aspect)
{
    XJ_width = width;
    XJ_height = height;
    AspectSet(aspect);
    
    DiscardFrames();
}

/**
 * \fn VideoOutput::EmbedInWidget(WId, int, int, int, int)
 * \brief Tells video output to embed video in an existing window.
 * \param wid window to embed in.
 * \param x   X location where to locate video
 * \param y   Y location where to locate video
 * \param w   width of video
 * \param h   height of video
 * \sa StopEmbedding()
 */
void VideoOutput::EmbedInWidget(WId wid, int x, int y, int w, int h)
{
    (void)wid;

    olddispx = dispx;
    olddispy = dispy;
    olddispw = dispw;
    olddisph = disph;

    dispxoff = dispx = x;
    dispyoff = dispy = y;
    dispwoff = dispw = w;
    disphoff = disph = h;

    embedding = true;

    MoveResize();
}

/**
 * \fn VideoOutput::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(WId, int, int, int, int)
 */ 
void VideoOutput::StopEmbedding(void)
{
    dispx = olddispx;
    dispy = olddispy;
    dispw = olddispw;
    disph = olddisph;

    embedding = false;

    MoveResize();
}

/**
 * \fn VideoOutput::DrawSlice(VideoFrame*, int, int, int, int)
 * \brief Informs video output of new data for frame,
 *        used for XvMC acceleration.
 */
void VideoOutput::DrawSlice(VideoFrame *frame, int x, int y, int w, int h)
{
    (void)frame;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

/**
 * \fn VideoOutput::GetDrawSize(int&,int&,int&,int&)
 * \brief Returns video output width, height, and location.
 * \param xoff    X location where video is located in window
 * \param yoff    Y location where video is located in window
 * \param width   width of video output
 * \param height  height of video output
 */
void VideoOutput::GetDrawSize(int &xoff, int &yoff, int &width, int &height)
{
    xoff = imgx;
    yoff = imgy;
    width = imgw;
    height = imgh;
}

/**
 * \fn VideoOutput::GetVisibleSize(int&,int&,int&,int&)
 * \brief Returns visible portions of video.
 * \param xoff    X location where visible video is located in window
 * \param yoff    Y location where visible video is located in window
 * \param width   visible width of video output
 * \param height  visible height of video output
 */
void VideoOutput::GetVisibleSize(int &xoff, int &yoff, int &width, int &height)
{
    xoff   = imgx;
    yoff   = imgy;
    width  = imgw;
    height = imgh;

    switch (letterbox)
    {
        case kLetterbox_4_3_Zoom:
            width = (imgw*3)/4;
            xoff = imgw/8;
            height = (imgh*3)/4;
            yoff = imgh/8;
            break;
        case kLetterbox_16_9_Stretch:
        case kLetterbox_16_9_Zoom:
            width = (imgw*3)/4;
            xoff = imgw/8;
            break;
        case kLetterbox_Fill:
            if (GetDisplayAspect() > XJ_aspect)
            {
                int pixDisplayed = int(((XJ_aspect / GetDisplayAspect()) * imgh) + 0.5);

                height = pixDisplayed;
                yoff = (imgh - pixDisplayed) / 2;
            }
            else
            {
                int pixDisplayed = int(((GetDisplayAspect() / XJ_aspect) * imgw) + 0.5);

                width = pixDisplayed;
                xoff = (imgw - pixDisplayed) / 2;
            }
            break;
        default:
            break;
    }
}

/**
 * \fn VideoOutput::MoveResize()
 * \brief performs all the calculations for video framing and any resizing.
 *
 * \sa Zoom(int), ToggleLetterbox(int)
 */
void VideoOutput::MoveResize(void)
{
    int yoff, xoff;
    float displayAspect;

    // Preset all image placement and sizing variables.
    imgx = 0; imgy = 0;
    imgw = XJ_width; imgh = XJ_height;
    xoff = img_xoff; yoff = img_yoff;
    dispxoff = dispx; dispyoff = dispy;
    dispwoff = dispw; disphoff = disph;
    if (imgw == 1920 && imgh == 1088)
        imgh = 1080; // ATSC 1920x1080

    // Set an appropriate PiP size
    if (imgh > 1000)
        (desired_pipw = 368), (desired_piph = 288);
    else if (imgh > 700)
        (desired_pipw = 240), (desired_piph = 192);
    else
        (desired_pipw = 160), (desired_piph = 128);

    // Get display aspect and correct for rounding errors
    displayAspect = GetDisplayAspect();

    // Check if close to 4:3
    if(fabs(displayAspect - 1.333333) < 0.05)
        displayAspect = 1.333333;

    // Check if close to 16:9
    if(fabs(displayAspect - 1.777777) < 0.05)
        displayAspect = 1.777777;

/*
    Here we apply playback over/underscanning and offsetting (if any apply).

    It doesn't make any sense to me to offset an image such that it is clipped.
    Therefore, we only apply offsets if there is an underscan or overscan which
    creates "room" to move the image around. That is, if we overscan, we can
    move the "viewport". If we underscan, we change where we place the image
    into the display window. If no over/underscanning is performed, you just
    get the full original image scaled into the full display area.
*/

    if (img_vscanf > 0) 
    {
        // Veritcal overscan. Move the Y start point in original image.
        imgy = (int)floor(0.5 + imgh * img_vscanf);
        imgh = (int)floor(0.5 + imgh * (1 - 2 * img_vscanf));

        // If there is an offset, apply it now that we have a room.
        // To move the image down, move the start point up.
        if (yoff > 0) 
        {
            // Can't offset the image more than we have overscanned.
            if (yoff > imgy)
                yoff = imgy;
            imgy -= yoff;
        }
        // To move the image up, move the start point down.
        if (yoff < 0) 
        {
            // Again, can't offset more than overscanned.
            if (abs(yoff) > imgy)
                yoff = 0 - imgy;
            imgy -= yoff;
        }
    }

    if (img_hscanf > 0) 
    {
        // Horizontal overscan. Move the X start point in original image.
        imgx = (int)floor(0.5 + XJ_width * img_hscanf);
        imgw = (int)floor(0.5 + XJ_width * (1 - 2 * img_hscanf));
        if (xoff > 0) 
        {
            if (xoff > imgx) 
                xoff = imgx;
            imgx -= xoff;
        }
        if (xoff < 0) 
        {
            if (abs(xoff) > imgx) 
                xoff = 0 - imgx;
            imgx -= xoff;
        }
    }

    float vscanf, hscanf;
    if (img_vscanf < 0) 
    {
        // Vertical underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        vscanf = fabs(img_vscanf);
        dispyoff = (int)floor(0.5 + disph * vscanf) + dispy;
        disphoff = (int)floor(0.5 + disph * (1 - 2 * vscanf));
        // Now offset the image within the extra blank space created by
        // underscanning.
        // To move the image down, increase the Y offset inside the display
        // window.
        if (yoff > 0) 
        {
            // Can't offset more than we have underscanned.
            if (yoff > dispyoff) 
                yoff = dispyoff;
            dispyoff += yoff;
        }
        if (yoff < 0) 
        {
            if (abs(yoff) > dispyoff) 
                yoff = 0 - dispyoff;
            dispyoff += yoff;
        }
    }

    if (img_hscanf < 0) 
    {
        hscanf = fabs(img_hscanf);
        dispxoff = (int)floor(0.5 + dispw * hscanf) + dispx;
        dispwoff = (int)floor(0.5 + dispw * (1 - 2 * hscanf));
        if (xoff > 0) 
        {
            if (xoff > dispxoff) 
                xoff = dispxoff;
            dispxoff += xoff;
        }

        if (xoff < 0) 
        {
            if (abs(xoff) > dispxoff) 
                xoff = 0 - dispxoff;
            dispxoff += xoff;
        }
    }

    // Manage aspect ratio and letterbox settings.  Code should take into
    // account the aspect ratios of both the video as well as the actual
    // screen to allow proper letterboxing to take place.
    
    //cout << "XJ aspect " << XJ_aspect << endl;
    //cout << "Display aspect " << GetDisplayAspect() << endl;
    //printf("Before: %dx%d%+d%+d\n", dispwoff, disphoff, dispxoff, 
    //    dispyoff);
    
    if ((fabs(displayAspect - XJ_aspect) / displayAspect) > 0.1){
        if(letterbox == kLetterbox_Fill){
            if (displayAspect > XJ_aspect)
            {
                float pixNeeded = ((displayAspect / XJ_aspect) * (float)disphoff) + 0.5;

                dispyoff += (disphoff - (int)pixNeeded) / 2;
                disphoff = (int)pixNeeded;
            }
            else
            {
                float pixNeeded = ((XJ_aspect / displayAspect) * (float)dispwoff) + 0.5;

                dispxoff += (dispwoff - (int)pixNeeded) / 2;
                dispwoff = (int)pixNeeded;
            }
        }
        else {
            if (displayAspect > XJ_aspect)
            {
                float pixNeeded = ((XJ_aspect / displayAspect) * (float)dispwoff) + 0.5;

                dispxoff += (dispwoff - (int)pixNeeded) / 2;
                dispwoff = (int)pixNeeded;
            }
            else
            {
                float pixNeeded = ((displayAspect / XJ_aspect) * (float)disphoff) + 0.5;

                dispyoff += (disphoff - (int)pixNeeded) / 2;
                disphoff = (int)pixNeeded;
            }
        }
    }


    if ((letterbox == kLetterbox_4_3_Zoom) ||
        (letterbox == kLetterbox_16_9_Zoom))
    {
        // Zoom mode
        //printf("Before zoom: %dx%d%+d%+d\n", dispwoff, disphoff,
        //dispxoff, dispyoff);
        // Expand by 4/3 and overscan
        dispxoff -= (dispwoff/6); // 1/6 of original is 1/8 of new
        dispwoff = dispwoff*4/3;
        dispyoff -= (disphoff/6);
        disphoff = disphoff*4/3;
    }

    if (letterbox == kLetterbox_16_9_Stretch)
    {
        //Stretch mode
        //Should be used to eliminate side bars on 4:3 material
        //encoded to 16:9. Basically crop a 16:9 to 4:3
        //printf("Before zoom: %dx%d%+d%+d\n", dispwoff, disphoff,
        //dispxoff, dispyoff);
        dispxoff -= (dispwoff/6); // 1/6 of original is 1/8 of new
        dispwoff = dispwoff*4/3;
    }

    if ((double(abs(disphoff - imgh)) / disphoff) < 0.05){
      /* Calculated height is within 5% of the source height. Accept
        slight distortion/overscan/underscan to avoid scaling */

      dispyoff += (disphoff - imgh) / 2;
      disphoff = imgh;

      VERBOSE(VB_PLAYBACK,
             QString("Snapping height to avoid scaling: disphoff %1, dispyoff: %2")
             .arg(disphoff).arg(dispyoff));
    }

    if ((double(abs(dispwoff - imgw)) / dispwoff) < 0.05){
      /* Calculated width is within 5% of the source width. Accept
        slight distortion/overscan/underscan to avoid scaling */

      dispxoff += (dispwoff - imgw) / 2;
      dispwoff = imgw;

      VERBOSE(VB_PLAYBACK,
             QString("Snapping width to avoid scaling: dispwoff %1, dispxoff: %2")
             .arg(dispwoff).arg(dispxoff));
    }

    if (ZoomedIn)
    {
        int oldDW = dispwoff;
        int oldDH = disphoff;
        dispwoff = (int)(dispwoff * (1 + (ZoomedIn * .01)));
        disphoff = (int)(disphoff * (1 + (ZoomedIn * .01)));

        dispxoff += (oldDW - dispwoff) / 2;
        dispyoff += (oldDH - disphoff) / 2;
    }

    if (ZoomedUp)
    {
        dispyoff = (int)(dispyoff * (1 - (ZoomedUp * .01)));
    }

    if (ZoomedRight)
    {
        dispxoff = (int)(dispxoff * (1 - (ZoomedRight * .01)));
    }

    //printf("After: %dx%d%+d%+d\n", dispwoff, disphoff, dispxoff, 
    //dispyoff);

#if 0
    printf("VideoOutput::MoveResize:\n");
    printf("Img(%d,%d %d,%d)\n", imgx, imgy, imgw, imgh);
    printf("Disp(%d,%d %d,%d)\n", dispxoff, dispyoff, dispwoff, disphoff);
    printf("Offset(%d,%d)\n", xoff, yoff);
    printf("Vscan(%f, %f)\n", img_vscanf, img_vscanf);
    printf("DisplayAspect: %f\n", GetDisplayAspect());
    printf("VideoAspect(%f)\n", videoAspect);
    printf("XJ_aspect(%f)\n", XJ_aspect);
    printf("CDisplayAspect: %f\n", displayAspect);
    printf("Letterbox: %d\n", letterbox);
#endif
 
    VERBOSE(VB_PLAYBACK,
            QString("Image size. dispxoff %1, dispyoff: %2, dispwoff: %3, "
                    "disphoff: %4")
            .arg(dispxoff).arg(dispyoff).arg(dispwoff).arg(disphoff));

    VERBOSE(VB_PLAYBACK,
            QString("Image size. imgx %1, imgy: %2, imgw: %3, imgh: %4")
           .arg(imgx).arg(imgy).arg(imgw).arg(imgh));

    needrepaint = true;
}

/**
 * \fn VideoOutput::Zoom(int)
 * \brief Sets up zooming into to different parts of the video, the zoom
 *        is actually applied in MoveResize().
 * \sa ToggleLetterbox(int), GetLetterbox()
 */
void VideoOutput::Zoom(int direction)
{
    switch (direction)
    {
        case kZoomHome:
                ZoomedIn = 0;
                ZoomedUp = 0;
                ZoomedRight = 0;
                break;
        case kZoomIn:
                if (ZoomedIn < 200)
                    ZoomedIn += 10;
                else
                    ZoomedIn = 0;
                break;
        case kZoomOut:
                if (ZoomedIn > -50)
                    ZoomedIn -= 10;
                else
                    ZoomedIn = 0;
                break;
        case kZoomUp:
                if (ZoomedUp < 100)
                    ZoomedUp += 10;
                break;
        case kZoomDown:
                if (ZoomedUp > -100)
                    ZoomedUp -= 10;
                break;
        case kZoomLeft:
                if (ZoomedRight < 100)
                    ZoomedRight += 10;
                break;
        case kZoomRight:
                if (ZoomedRight > -100)
                    ZoomedRight -= 10;
                break;
    }
}

/**
 * \fn VideoOutput::ToggleLetterbox(int)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimentions. These are actually applied in MoveResize().
 * \sa Zoom(int), 
 */
void VideoOutput::ToggleLetterbox(int letterboxMode)
{
    if (letterboxMode == kLetterbox_Toggle)
    {
        if (++letterbox >= kLetterbox_END)
            letterbox = kLetterbox_Off;
    }
    else
    {
        letterbox = letterboxMode;
    }

    AspectChanged(videoAspect);
}

/**
 * \fn VideoOutput::ChangePictureAttribute(int, int)
 * \brief Sets a specified picture attribute.
 * \param attribute Picture attribute to set.
 * \param newValue  Value to set attribute to.
 * \return Set value if it succeeds, -1 if it does not.
 */
int VideoOutput::ChangePictureAttribute(int attribute, int newValue)
{
    (void)attribute;
    (void)newValue;
    return -1;
}

/**
 * \fn VideoOutput::ChangeBrightness(bool)
 * \brief Increases brightenss if "up" is true, decreases it otherwise.
 * \return new value if it succeeds, old value if it does not.
 */
int VideoOutput::ChangeBrightness(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Brightness, 
                                          brightness + ((up) ? 1 : -1) );

    brightness = (result == -1) ? brightness : result;

    return brightness;
}

/**
 * \fn VideoOutput::ChangeContrast(bool)
 * \brief Increases contrast if "up" is true, decreases it otherwise.
 * \return new value if it succeeds, old value if it does not.
 */
int VideoOutput::ChangeContrast(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Contrast, 
                                          contrast + ((up) ? 1 : -1) );

    contrast = (result == -1) ? contrast : result;

    return contrast;
}

/**
 * \fn VideoOutput::ChangeColour(bool)
 * \brief Increases colour phase if "up" is true, decreases it otherwise.
 * \return new value if it succeeds, old value if it does not.
 */
int VideoOutput::ChangeColour(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Colour, 
                                          colour + ((up) ? 1 : -1) );
    colour = (result == -1) ? colour : result;

    return colour;
}

/**
 * \fn VideoOutput::ChangeHue(bool)
 * \brief Increases Hue phase if "up" is true, decreases it otherwise.
 * \return new value if it succeeds, old value if it does not.
 */
int VideoOutput::ChangeHue(bool up)
{
    int result;

    result = this->ChangePictureAttribute(kPictureAttribute_Hue, 
                                          hue + ((up) ? 1 : -1) );
    hue = (result == -1) ? hue : result;

    return hue;
}

/**
 * \fn VideoOutput::DoPipResize(int,int)
 * \brief Sets up Picture in Picture image resampler.
 * \param pipwidth  input width
 * \param pipheight input height
 * \sa ShutdownPipResize(), ShowPip(VideoFrame*,NuppelVideoPlayer*)
 */
void VideoOutput::DoPipResize(int pipwidth, int pipheight)
{
    if (pipwidth != desired_piph || pipheight != desired_pipw)
    {
        ShutdownPipResize();

        piptmpbuf = new unsigned char[desired_piph * desired_pipw * 3 / 2];

        piph_in = pipheight;
        pipw_in = pipwidth;

        piph_out = desired_piph;
        pipw_out = desired_pipw;

        pipscontext = img_resample_init(pipw_out, piph_out, pipw_in, piph_in);
    }
}

/**
 * \fn VideoOutput::ShutdownPipResize()
 * \brief Shuts down Picture in Picture image resampler.
 * \sa VideoOutput::DoPipResize(int,int),
 *     ShowPip(VideoFrame*,NuppelVideoPlayer*)
 */
void VideoOutput::ShutdownPipResize(void)
{
    if (piptmpbuf)
        delete [] piptmpbuf;
    if (pipscontext)
        img_resample_close(pipscontext);

    piptmpbuf = NULL;
    pipscontext = NULL;

    piph_in = piph_out = -1;
    pipw_in = pipw_out = -1;
}

/**
 * \fn VideoOutput::ShowPip(VideoFrame*,NuppelVideoPlayer*)
 * \brief Composites PiP image onto a video frame.
 * Note: This only works with memory backed VideoFrames,
 *       that is not XvMC.
 * \param frame     Frame to composite PiP onto.
 * \param pipplayer Picture-in-Picture NVP.
 */
void VideoOutput::ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer)
{
    if (!pipplayer)
        return;

    int pipw, piph;

    VideoFrame *pipimage = pipplayer->GetCurrentFrame(pipw, piph);

    if (!pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    int xoff;
    int yoff;

    unsigned char *pipbuf = pipimage->buf;

    if (pipw != desired_pipw || piph != desired_piph)
    {
        DoPipResize(pipw, piph);

        if (piptmpbuf && pipscontext)
        {
            AVPicture img_in, img_out;

            avpicture_fill(&img_out, (uint8_t *)piptmpbuf, PIX_FMT_YUV420P,
                           pipw_out, piph_out);
            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            img_resample(pipscontext, &img_out, &img_in);

            pipw = pipw_out;
            piph = piph_out;

            pipbuf = piptmpbuf;
        }
    }

    switch (PIPLocation)
    {
        default:
        case kPIPTopLeft:
                xoff = 30;
                yoff = 40;
                break;
        case kPIPBottomLeft:
                xoff = 30;
                yoff = frame->height - piph - 40;
                break;
        case kPIPTopRight:
                xoff = frame->width - pipw - 30;
                yoff = 40;
                break;
        case kPIPBottomRight:
                xoff = frame->width - pipw - 30;
                yoff = frame->height - piph - 40;
                break;
    }

    for (int i = 0; i < piph; i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               pipbuf + i * pipw, pipw);
    }

    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *pipuptr = pipbuf + pipw * piph;
    unsigned char *pipvptr = pipbuf + pipw * piph * 5 / 4;
    pipw /= 2;

    for (int i = 0; i < piph / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, pipuptr + i * pipw, pipw);
        memcpy(vptr + (i + yoff) * vidw + xoff, pipvptr + i * pipw, pipw);
    }

    pipplayer->ReleaseCurrentFrame(pipimage);
}

/**
 * \fn VideoOutput::DisplayOSD(VideoFrame*,OSD *,int,int)
 * \brief If the OSD has changed, this will convert the OSD buffer
 *        to the OSDSurface's color format.
 *
 *  If the destination format is either IA44 or AI44 the osd is
 *  converted to greyscale.
 */ 
int VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd, int stride,
                            int revision)
{
    if (!osd)
        return -1;

    OSDSurface *surface = osd->Display();
    if (!surface)
        return -1;

    bool changed = (-1 == revision) ?
        surface->Changed() : (surface->GetRevision()!=revision);

    switch (frame->codec)
    {
        case FMT_YV12:
        {
            surface->BlendToYV12(frame->buf);
            break;
        }
        case FMT_AI44:
        {
            if (stride < 0)
                stride = XJ_width; // 8 bits per pixel
            if (changed)
                surface->DitherToAI44(frame->buf, stride, XJ_height);
            break;
        }
        case FMT_IA44:
        {
            if (stride < 0)
                    stride = XJ_width; // 8 bits per pixel
            if (changed)
                surface->DitherToIA44(frame->buf, stride, XJ_height);
            break;
        }
        case FMT_ARGB32:
        {
            if (stride < 0)
                stride = XJ_width*4; // 32 bits per pixel
            if (changed)
                surface->BlendToARGB(frame->buf, stride, XJ_height);
            break;
        }
        default:
            break;
    }
    return (changed) ? 1 : 0;
}

/**
 * \fn VideoOutput::CopyFrame(VideoFrame *, VideoFrame *)
 * \brief Copies frame data from one VideoFrame to another.
 * 
 *  Note: The frames must have the same width, height, and format.
 * \param to   The destination frame.
 * \param from The source frame 
 */
void VideoOutput::CopyFrame(VideoFrame *to, const VideoFrame *from)
{
    if (to == NULL || from == NULL)
        return;

    to->frameNumber = from->frameNumber;

    // guaranteed to be correct sizes.
    memcpy(to->buf, from->buf, from->size);

/* XXX: Broken.
    if (from->qstride > 0 && from->qscale_table != NULL)
    {
        int tablesize = from->qstride * ((from->height + 15) / 16);

        if (to->qstride != from->qstride || to->qscale_table == NULL)
        {
            to->qstride = from->qstride;
            if (to->qscale_table)
                delete [] to->qscale_table;

            to->qscale_table = new unsigned char[tablesize];
        }

        memcpy(to->qscale_table, from->qscale_table, tablesize);
    }
*/
}

