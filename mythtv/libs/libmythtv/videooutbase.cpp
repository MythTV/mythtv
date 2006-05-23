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

static QSize fix_1080i(QSize raw);
static float fix_aspect(float raw);

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
 *     if (vo->EnoughPrebufferedFrames())
 *     {
 *         // Sets "Last Shown Frame" to head of "used" queue
 *         vo->StartDisplayingFrame();
 *         // Get pointer to "Last Shown Frame"
 *         frame = vo->GetLastShownFrame();
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
    // DB Settings
    db_display_dim(0,0),                db_move(0,0),
    db_scale_horiz(0.0f),               db_scale_vert(0.0f),
    db_pip_location(0),                 db_pip_size(26),
    db_letterbox(kLetterbox_Off),       db_deint_filtername(QString::null),

    // Manual Zoom
    mz_scale(0),                        mz_move(0,0),

    // Physical dimensions
    display_dim(400,300),               display_aspect(1.3333f),

    // Video dimensions
    video_dim(640,480),                 video_aspect(1.3333f),

    // Aspect override
    letterboxed_video_aspect(1.3333f),  letterbox(kLetterbox_Off),

    // Screen settings
    video_rect(0,0,0,0),                display_video_rect(0,0,0,0),
    display_visible_rect(0,0,0,0),      tmp_display_visible_rect(0,0,0,0),

    // Picture-in-Picture stuff
    pip_desired_display_size(160,128),  pip_display_size(0,0),
    pip_video_size(0,0),
    pip_tmp_buf(NULL),                  pip_scaling_context(NULL),

    // Video resizing (for ITV)
    vsz_enabled(false),
    vsz_desired_display_rect(0,0,0,0),  vsz_display_size(0,0),
    vsz_video_size(0,0),
    vsz_tmp_buf(NULL),                  vsz_scale_context(NULL),

    // Deinterlacing
    m_deinterlacing(false),             m_deintfiltername("linearblend"),
    m_deintFiltMan(NULL),               m_deintFilter(NULL),
    m_deinterlaceBeforeOSD(true),

    // Various state variables
    embedding(false),                   needrepaint(false),
    allowpreviewepg(true),              errored(false),
    framesPlayed(0)
{
    db_display_dim = QSize(gContext->GetNumSetting("DisplaySizeWidth",  0),
                           gContext->GetNumSetting("DisplaySizeHeight", 0));

    db_move        = QPoint(gContext->GetNumSetting("xScanDisplacement", 0),
                            gContext->GetNumSetting("yScanDisplacement", 0));

    db_scale_vert  = gContext->GetNumSetting("VertScanPercentage",  5) / 100.0;
    db_scale_horiz = gContext->GetNumSetting("HorizScanPercentage", 5) / 100.0;

    db_pip_location    = gContext->GetNumSetting("PIPLocation",        0);
    db_pip_size        = gContext->GetNumSetting("PIPSize",           26);

    db_pict_attr[kPictureAttribute_Brightness] =
        gContext->GetNumSetting("PlaybackBrightness", 50);
    db_pict_attr[kPictureAttribute_Contrast] =
        gContext->GetNumSetting("PlaybackContrast",   50);
    db_pict_attr[kPictureAttribute_Colour] =
        gContext->GetNumSetting("PlaybackColour",     50);
    db_pict_attr[kPictureAttribute_Hue] =
        gContext->GetNumSetting("PlaybackHue",         0);

    db_letterbox       = gContext->GetNumSetting("AspectOverride",     0);
    db_deint_filtername= gContext->GetSetting("DeinterlaceFilter",
                                              "linearblend");
}

/**
 * \fn VideoOutput::~VideoOutput()
 * \brief Shuts down video output.
 */
VideoOutput::~VideoOutput()
{
    ShutdownPipResize();

    ShutdownVideoResize();

    if (m_deintFilter)
        delete m_deintFilter;
    if (m_deintFiltMan)
        delete m_deintFiltMan;
}

/**
 * \fn VideoOutput::Init(int,int,float,WId,int,int,int,int,WId)
 * \brief Performs most of the initialization for VideoOutput.
 * \return true if successful, false otherwise.
 */
bool VideoOutput::Init(int width, int height, float aspect, WId winid,
                       int winx, int winy, int winw, int winh, WId embedid)
{
    (void)winid;
    (void)embedid;

    if (winw && winh)
    {
        VERBOSE(VB_PLAYBACK,
                QString("Over/underscan. V: %1, H: %2, XOff: %3, YOff: %4")
                .arg(db_scale_vert).arg(db_scale_horiz)
                .arg(db_move.x()).arg(db_move.y()));
    }

    display_visible_rect = QRect(0, 0, winw, winh);
    video_dim            = QSize(width, height);
    video_rect           = QRect(QPoint(winx, winy), fix_1080i(video_dim));

    letterbox  = db_letterbox;

    VideoAspectRatioChanged(aspect); // apply aspect ratio and letterbox mode

    embedding = false;

    return true;
}

/**
 * \fn VideoOutput::SetDeinterlacingEnabled(bool)
 * \brief Attempts to enable/disable deinterlacing using
 *        existing deinterlace method when enabling.
 */
bool VideoOutput::SetDeinterlacingEnabled(bool enable)
{
    if (enable && m_deinterlacing)
        return m_deinterlacing;

    // if enable and no deinterlacer allocated, attempt allocate one
    if (enable && (!m_deintFiltMan || !m_deintFilter))
        return SetupDeinterlace(enable);

    m_deinterlacing = enable;
    return m_deinterlacing;
}

/**
 * \fn VideoOutput::SetupDeinterlace(bool,const QString&)
 * \brief Attempts to enable or disable deinterlacing.
 * \return true if successful, false otherwise.
 * \param overridefilter optional, explicitly use this nondefault deint filter
 */
bool VideoOutput::SetupDeinterlace(bool interlaced, 
                                   const QString& overridefilter)
{
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
        
        m_deintfiltername = QDeepCopy<QString>(
            overridefilter.isEmpty() ? db_deint_filtername : overridefilter);

        m_deintFiltMan = new FilterManager;
        m_deintFilter = NULL;

        if (ApproveDeintFilter(m_deintfiltername))
        {
            int width  = video_dim.width();
            int height = video_dim.height();
            m_deintFilter = m_deintFiltMan->LoadFilters(
                m_deintfiltername, itmp, otmp, width, height, btmp);
            video_dim = QSize(width, height);
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
 * \fn VideoOutput::SetVideoAspectRatio(float aspect)
 * \brief Sets VideoOutput::video_aspect to aspect, and sets 
 *        VideoOutput::letterboxed_video_aspect the letterbox type, unless
 *        the letterbox type is kLetterbox_Fill.
 * 
 * \param aspect video aspect ratio to use
 */
void VideoOutput::SetVideoAspectRatio(float aspect)
{
    letterboxed_video_aspect = video_aspect = aspect;
    
    // Override video's aspect if configured to do so
    if ((kLetterbox_4_3      == letterbox) ||
        (kLetterbox_4_3_Zoom == letterbox))
    {
        letterboxed_video_aspect = 4.0f / 3.0f;
    }
    else if ((kLetterbox_16_9      == letterbox) ||
             (kLetterbox_16_9_Zoom == letterbox))
    {
        letterboxed_video_aspect = 16.0f / 9.0f;
    }
}

/**
 * \fn VideoOutput::VideoAspectRatioChanged(float aspect)
 * \brief Calls SetVideoAspectRatio(float aspect),
 *        then calls MoveResize() to apply changes.
 * \param aspect video aspect ratio to use
 */
void VideoOutput::VideoAspectRatioChanged(float aspect)
{
    SetVideoAspectRatio(aspect);
    MoveResize();
}

/**
 * \fn VideoOutput::InputChanged(int, int, float, MythCodecID)
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
void VideoOutput::InputChanged(int width, int height, float aspect,
                               MythCodecID av_codec_id)
{
    (void)av_codec_id;

    video_dim = QSize(width, height);
    SetVideoAspectRatio(aspect);
    
    DiscardFrames(true);
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

    embedding = true;

    tmp_display_visible_rect = display_visible_rect;
    display_visible_rect     = QRect(x, y, w, h);
    display_video_rect       = QRect(x, y, w, h);

    MoveResize();
}

/**
 * \fn VideoOutput::StopEmbedding(void)
 * \brief Tells video output to stop embedding video in an existing window.
 * \sa EmbedInWidget(WId, int, int, int, int)
 */ 
void VideoOutput::StopEmbedding(void)
{
    display_visible_rect = tmp_display_visible_rect;

    MoveResize();

    embedding = false;
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
    xoff   = video_rect.left();
    yoff   = video_rect.top();
    width  = video_rect.width();
    height = video_rect.height();
}

void VideoOutput::GetOSDBounds(QRect &total, QRect &visible,
                               float &visible_aspect,
                               float &font_scaling) const
{
    total   = GetTotalOSDBounds();
    visible = GetVisibleOSDBounds(visible_aspect, font_scaling);
}

static float sq(float a) { return a*a; }
//static float lerp(float r, float a, float b)
//    { return ((1.0f - r) * a) + (r * b); }

/**
 * \fn VideoOutput::GetVisibleOSDBounds(float&,float&) const
 * \brief Returns visible portions of total OSD bounds
 * \param visible_aspect physical aspect ratio of bounds returned
 * \param font_scaling   scaling to apply to fonts
 */
QRect VideoOutput::GetVisibleOSDBounds(
    float &visible_aspect, float &font_scaling) const
{
    float dv_w = ((float)video_dim.width())  / display_video_rect.width();
    float dv_h = ((float)video_dim.height()) / display_video_rect.height();

    uint right_overflow = max((display_video_rect.width() + display_video_rect.left()) - display_visible_rect.width(), 0);
    uint lower_overflow = max((display_video_rect.height() + display_video_rect.top()) - display_visible_rect.height(), 0);

    // top left and bottom right corners respecting letterboxing
    QPoint tl = QPoint((uint) ceil(max(-display_video_rect.left(),0)*dv_w),
                       (uint) ceil(max(-display_video_rect.top(),0)*dv_h));
    QPoint br = QPoint((uint) floor(video_dim.width()-(right_overflow*dv_w)),
                       (uint) floor(video_dim.height()-(lower_overflow*dv_h)));
    // adjust for overscan
    if ((db_scale_vert > 0.0f) || (db_scale_horiz > 0.0f))
    {
        QRect v(tl, br);
        float xs = (db_scale_horiz > 0.0f) ? db_scale_horiz : 0.0f;
        float ys = (db_scale_vert > 0.0f) ? db_scale_vert : 0.0f;
        QPoint s((int)(v.width() * xs), (int)(v.height() * ys));
        tl += s*0.5f;
        br -= s*1.5f;
    }
    QRect vb(tl, br);

    // The calculation is completely bogus if the video is not centered
    // which happens in the EPG, where we don't actually care about the OSD.
    // So we just make sure the width and height are positive numbers
    vb = QRect(vb.x(), vb.y(), abs(vb.width()), abs(vb.height()));

    // set the physical aspect ratio of the displayable area
    float dispPixelAdj = (GetDisplayAspect() * display_visible_rect.height()) / display_visible_rect.width();
    // now adjust for scaling of the video on the aspect ratio
    float vs = ((float)vb.width())/vb.height();
    visible_aspect = 1.3333f * (vs/letterboxed_video_aspect) * dispPixelAdj;

    // now adjust for scaling of the video on the size
    font_scaling = 1.0f/sqrtf(2.0/(sq(visible_aspect / 1.3333f) + 1.0f));
    // now adjust for aspect ratio effect on font size (should be in osd.cpp?)
    font_scaling *= sqrtf(letterboxed_video_aspect/1.3333f);
    return vb;
}

/**
 * \fn VideoOutput::GetTotalOSDBounds(void) const
 * \brief Returns total OSD bounds
 */
QRect VideoOutput::GetTotalOSDBounds(void) const
{
    return QRect(QPoint(0, 0), video_dim);
}

/** \fn VideoOutput::ApplyDBScaleAndMove(void)
 *  \brief Apply scales and moves from "Zoom Mode" settings.
 */
void VideoOutput::ApplyManualScaleAndMove(void)
{
    if (mz_scale)
    {
        QSize  newsz  = display_video_rect.size() * (1.0f + (mz_scale*0.01f));
        QSize  tmp    = (display_video_rect.size() - newsz) / 2;
        QPoint chgloc = QPoint(tmp.width(), tmp.height());
        QPoint newloc = display_video_rect.topLeft() + chgloc;

        display_video_rect = QRect(newloc, newsz);
    }

    if (mz_move.y())
    {
        int move_vert = mz_move.y() * display_video_rect.height() / 100;
        display_video_rect.moveTop(display_video_rect.top() + move_vert);
    }

    if (mz_move.x())
    {
        int move_horiz = mz_move.x() * display_video_rect.width() / 100;
        display_video_rect.moveLeft(display_video_rect.left() + move_horiz);
    }
}

/** \fn VideoOutput::ApplyDBScaleAndMove(void)
 *  \brief Apply scales and moves for "Overscan" and "Underscan" DB settings.
 *
 *  It doesn't make any sense to me to offset an image such that it is clipped.
 *  Therefore, we only apply offsets if there is an underscan or overscan which
 *  creates "room" to move the image around. That is, if we overscan, we can
 *  move the "viewport". If we underscan, we change where we place the image
 *  into the display window. If no over/underscanning is performed, you just
 *  get the full original image scaled into the full display area.
 */
void VideoOutput::ApplyDBScaleAndMove(void)
{
    if (db_scale_vert > 0) 
    {
        // Veritcal overscan. Move the Y start point in original image.
        float tmp = 1.0f - 2.0f * db_scale_vert;
        video_rect.moveTop((int) round(video_rect.height() * db_scale_vert));
        video_rect.setHeight((int) round(video_rect.height() * tmp));

        // If there is an offset, apply it now that we have a room.
        int yoff = db_move.y();
        if (yoff > 0)
        {
            // To move the image down, move the start point up.
            // Don't offset the image more than we have overscanned.
            yoff = min(video_rect.top(), yoff);
            video_rect.moveTop(video_rect.top() - yoff);
        }
        else if (yoff < 0)
        {
            // To move the image up, move the start point down.
            // Don't offset the image more than we have overscanned.
            if (abs(yoff) > video_rect.top())
                yoff = 0 - video_rect.top();
            video_rect.moveTop(video_rect.top() - yoff);
        }
    }
    else if (db_scale_vert < 0) 
    {
        // Vertical underscan. Move the starting Y point in the display window.
        // Use the abolute value of scan factor.
        float vscanf = fabs(db_scale_vert);
        float tmp = 1.0f - 2.0f * vscanf;

        display_video_rect.moveTop(
            (int) round(display_visible_rect.height() * vscanf) +
            display_visible_rect.top());

        display_video_rect.setHeight(
            (int) round(display_visible_rect.height() * tmp));

        // Now offset the image within the extra blank space created by
        // underscanning. To move the image down, increase the Y offset
        // inside the display window.
        int yoff = db_move.y();
        if (yoff > 0) 
        {
            // Don't offset more than we have underscanned.
            yoff = min(display_video_rect.top(), yoff);
            display_video_rect.moveTop(display_video_rect.top() + yoff);
        }
        else if (yoff < 0) 
        {
            // Don't offset more than we have underscanned.
            if (abs(yoff) > display_video_rect.top()) 
                yoff = 0 - display_video_rect.top();
            display_video_rect.moveTop(display_video_rect.top() + yoff);
        }
    }

    // Horizontal.. comments, same as vertical...
    if (db_scale_horiz > 0) 
    {
        float tmp = 1.0f - 2.0f * db_scale_horiz;
        video_rect.moveLeft((int) round(video_dim.width() * db_scale_horiz));
        video_rect.setWidth((int) round(video_dim.width() * tmp));

        int xoff = db_move.x();
        if (xoff > 0) 
        {
            xoff = min(video_rect.left(), xoff);
            video_rect.moveLeft(video_rect.left() - xoff);
        }
        else if (xoff < 0) 
        {
            if (abs(xoff) > video_rect.left()) 
                xoff = 0 - video_rect.left();
            video_rect.moveLeft(video_rect.left() - xoff);
        }
    }
    else if (db_scale_horiz < 0) 
    {
        float hscanf = fabs(db_scale_horiz);
        float tmp = 1.0f - 2.0f * hscanf;

        display_video_rect.moveLeft(
            (int) round(display_visible_rect.width() * hscanf) +
            display_visible_rect.left());

        display_video_rect.setWidth(
            (int) round(display_visible_rect.width() * tmp));

        int xoff = db_move.x();
        if (xoff > 0) 
        {
            xoff = min(display_video_rect.left(), xoff);
            display_video_rect.moveLeft(display_video_rect.left() + xoff);
        }
        else if (xoff < 0) 
        {
            if (abs(xoff) > display_video_rect.left()) 
                xoff = 0 - display_video_rect.left();
            display_video_rect.moveLeft(display_video_rect.left() + xoff);
        }
    }

}

// Code should take into account the aspect ratios of both the video as
// well as the actual screen to allow proper letterboxing to take place.
void VideoOutput::ApplyLetterboxing(void)
{
    float disp_aspect = fix_aspect(GetDisplayAspect());
    float aspect_diff = disp_aspect - letterboxed_video_aspect;
    bool  aspects_match = abs(aspect_diff / disp_aspect) <= 0.1f;
    bool  nomatch_with_fill = !aspects_match && (letterbox == kLetterbox_Fill);
    bool  nomatch_without_fill = (!aspects_match) && !nomatch_with_fill;

    // Adjust for video/display aspect ratio mismatch
    if (nomatch_with_fill && (disp_aspect > letterboxed_video_aspect))
    {
        float pixNeeded =
            ((disp_aspect / letterboxed_video_aspect) *
             (float)display_video_rect.height()) + 0.5f;

        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - (int)pixNeeded) / 2);
        display_video_rect.setHeight((int)pixNeeded);
    }
    else if (nomatch_with_fill)
    {
        float pixNeeded =
            ((letterboxed_video_aspect / disp_aspect) * 
             (float)display_video_rect.width()) + 0.5f;

        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - (int)pixNeeded) / 2);

        display_video_rect.setWidth((int)pixNeeded);
    }
    else if (nomatch_without_fill && (disp_aspect > letterboxed_video_aspect))
    {
        float pixNeeded =
            ((letterboxed_video_aspect / disp_aspect) *
             (float)display_video_rect.width()) + 0.5f;

        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - (int)pixNeeded) / 2);

        display_video_rect.setWidth((int)pixNeeded);
    }
    else if (nomatch_without_fill)
    {
        float pixNeeded =
            ((disp_aspect / letterboxed_video_aspect) *
             (float)display_video_rect.height()) + 0.5f;

        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - (int)pixNeeded) / 2);

        display_video_rect.setHeight((int)pixNeeded);
    }

    // Process letterbox zoom modes
    if ((letterbox == kLetterbox_4_3_Zoom) ||
        (letterbox == kLetterbox_16_9_Zoom))
    {
        // Zoom mode -- Expand by 4/3 and overscan.
        // 1/6 of original is 1/8 of new
        display_video_rect = QRect(
            display_video_rect.left() - (display_video_rect.width()  / 6),
            display_video_rect.top()  - (display_video_rect.height() / 6),
            display_video_rect.width()  * 4 / 3,
            display_video_rect.height() * 4 / 3);
    }
    else if (letterbox == kLetterbox_16_9_Stretch)
    {
        // Stretch mode -- intended to be used to eliminate side
        //                 bars on 4:3 material encoded to 16:9.
        // 1/6 of original is 1/8 of new
        display_video_rect.moveLeft(
            display_video_rect.left() - (display_video_rect.width() / 6));
        
        display_video_rect.setWidth(display_video_rect.width() * 4 / 3);
    }
}

/** \fn VideoOutput::ApplySnapToVideoRect(void)
 *  \brief Snap displayed rectagle to video rectange if they are close.
 *
 *  If our display rectangle is within 5% of the video rectangle in
 *  either dimension then snap the display rectangle in that dimension
 *  to the video rectangle. The idea is to avoid scaling if it will
 *  result in only moderate distortion.
 */
void VideoOutput::ApplySnapToVideoRect(void)
{
    float ydiff = abs(display_video_rect.height() - video_rect.height());
    if ((ydiff / display_video_rect.height()) < 0.05)
    {
        display_video_rect.moveTop(
            display_video_rect.top() +
            (display_video_rect.height() - video_rect.height()) / 2);

        display_video_rect.setHeight(video_rect.height());

        VERBOSE(VB_PLAYBACK,
                QString("Snapping height to avoid scaling: "
                        "height: %1, top: %2")
                .arg(display_video_rect.height())
                .arg(display_video_rect.top()));
    }

    float xdiff = abs(display_video_rect.width() - video_rect.width());
    if ((xdiff / display_video_rect.width()) < 0.05)
    {
        display_video_rect.moveLeft(
            display_video_rect.left() +
            (display_video_rect.width() - video_rect.width()) / 2);

        display_video_rect.setWidth(video_rect.width());

        VERBOSE(VB_PLAYBACK,
                QString("Snapping width to avoid scaling: "
                        "width: %1, left: %2")
                .arg(display_video_rect.width())
                .arg(display_video_rect.left()));
    }
}

void VideoOutput::PrintMoveResizeDebug(void)
{
#if 0
    printf("VideoOutput::MoveResize:\n");
    printf("Img(%d,%d %d,%d)\n",
           video_rect.left(), video_rect.top(),
           video_rect.width(), video_rect.height());
    printf("Disp(%d,%d %d,%d)\n",
           display_video_rect.left(), display_video_rect.top(),
           display_video_rect.width(), display_video_rect.height());
    printf("Offset(%d,%d)\n", xoff, yoff);
    printf("Vscan(%f, %f)\n", db_scale_vert, db_scale_vert);
    printf("DisplayAspect: %f\n", GetDisplayAspect());
    printf("VideoAspect(%f)\n", video_aspect);
    printf("letterboxed_video_aspect(%f)\n", letterboxed_video_aspect);
    printf("CDisplayAspect: %f\n", fix_aspect(GetDisplayAspect()));
    printf("Letterbox: %d\n", letterbox);
#endif

    VERBOSE(VB_PLAYBACK,
            QString("Display Rect  left: %1, top: %2, width: %3, "
                    "height: %4, aspect: %5")
            .arg(display_video_rect.left()).arg(display_video_rect.top())
            .arg(display_video_rect.width()).arg(display_video_rect.height())
            .arg(fix_aspect(GetDisplayAspect())));

    VERBOSE(VB_PLAYBACK,
            QString("Video Rect    left: %1, top: %2, width: %3, "
                    "height: %4, aspect: %5")
            .arg(video_rect.left()).arg(video_rect.top())
            .arg(video_rect.width()).arg(video_rect.height())
            .arg(letterboxed_video_aspect));

}

/**
 * \fn VideoOutput::MoveResize(void)
 * \brief performs all the calculations for video framing and any resizing.
 *
 * First we apply playback over/underscanning and offsetting,
 * then we letterbox settings, and finally we apply manual
 * scale & move properties for "Zoom Mode".
 *
 * \sa Zoom(int), ToggleLetterbox(int)
 */
void VideoOutput::MoveResize(void)
{
    // Preset all image placement and sizing variables.
    video_rect         = QRect(QPoint(0, 0), fix_1080i(video_dim));
    display_video_rect = display_visible_rect;

    // Apply various modifications
    ApplyDBScaleAndMove();
    ApplyLetterboxing();
    ApplyManualScaleAndMove();
    if ((db_scale_vert == 0) && (db_scale_horiz == 0) && (mz_scale == 0))
        ApplySnapToVideoRect();

    PrintMoveResizeDebug();
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
    if (kZoomHome == direction)
    {
        mz_scale = 0;
        mz_move = QPoint(0,0);
    }        
    else if (kZoomIn == direction)
    {
        if (mz_scale < 200)
            mz_scale += 10;
        else
            mz_scale = 0;
    }
    else if (kZoomOut == direction)
    {
        if (mz_scale > -50)
            mz_scale -= 10;
        else
            mz_scale = 0;
    }
    else if (kZoomUp == direction && (mz_move.y() <= 50))
        mz_move.setY(mz_move.y() + 2);
    else if (kZoomDown == direction && (mz_move.y() >= -50))
        mz_move.setY(mz_move.y() - 2);
    else if (kZoomLeft == direction && (mz_move.x() <= 50))
        mz_move.setX(mz_move.x() + 2);
    else if (kZoomRight == direction && (mz_move.x() >= -50))
        mz_move.setX(mz_move.x() - 2);
}

/**
 * \fn VideoOutput::ToggleLetterbox(int)
 * \brief Sets up letterboxing for various standard video frame and
 *        monitor dimensions, then calls VideoAspectRatioChanged(float)
 *        to apply them.
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

    VideoAspectRatioChanged(video_aspect);
}

int VideoOutput::ChangePictureAttribute(int attributeType, bool direction)
{
    int curVal = GetPictureAttribute(attributeType);
    if (curVal < 0)
        return -1;

    int newVal = curVal + (direction) ? +1 : -1;

    if (kPictureAttribute_Hue == attributeType)
        newVal = newVal % 100;

    newVal = min(max(newVal, 0), 100);

    return SetPictureAttribute(attributeType, newVal);
}

/**
 * \fn VideoOutput::SetPictureAttribute(int, int)
 * \brief Sets a specified picture attribute.
 * \param attribute Picture attribute to set.
 * \param newValue  Value to set attribute to.
 * \return Set value if it succeeds, -1 if it does not.
 */
int VideoOutput::SetPictureAttribute(int attribute, int newValue)
{
    (void)attribute;
    (void)newValue;
    return -1;
}

int VideoOutput::GetPictureAttribute(int attributeType) const
{
    QMap<int,int>::const_iterator it = db_pict_attr.find(attributeType);
    if (it == db_pict_attr.end())
        return -1;
    return *it;
}

void VideoOutput::InitPictureAttributes(void)
{
    QMap<int,int>::const_iterator it = db_pict_attr.begin();
    for (; it != db_pict_attr.end(); ++it)
        SetPictureAttribute(it.key(), *it);
}

void VideoOutput::SetPictureAttributeDBValue(int attributeType, int newValue)
{
    QString dbName = QString::null;
    if (kPictureAttribute_Brightness == attributeType)
        dbName = "PlaybackBrightness";
    else if (kPictureAttribute_Contrast == attributeType)
        dbName = "PlaybackContrast";
    else if (kPictureAttribute_Colour == attributeType)
        dbName = "PlaybackColour";
    else if (kPictureAttribute_Hue == attributeType)
        dbName = "PlaybackHue";

    if (!dbName.isEmpty())
        gContext->SaveSetting(dbName, newValue);

    db_pict_attr[attributeType] = newValue;
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
    QSize vid_size = QSize(pipwidth, pipheight);
    if (vid_size == pip_desired_display_size)
        return;

    ShutdownPipResize();

    pip_video_size   = vid_size;
    pip_display_size = pip_desired_display_size;

    int sz = pip_display_size.height() * pip_display_size.width() * 3 / 2;
    pip_tmp_buf = new unsigned char[sz];

    pip_scaling_context = img_resample_init(
        pip_display_size.width(), pip_display_size.height(),
        pip_video_size.width(),   pip_video_size.height());
}

/**
 * \fn VideoOutput::ShutdownPipResize()
 * \brief Shuts down Picture in Picture image resampler.
 * \sa VideoOutput::DoPipResize(int,int),
 *     ShowPip(VideoFrame*,NuppelVideoPlayer*)
 */
void VideoOutput::ShutdownPipResize(void)
{
    if (pip_tmp_buf)
    {
        delete [] pip_tmp_buf;
        pip_tmp_buf   = NULL;
    }

    if (pip_scaling_context)
    {
        img_resample_close(pip_scaling_context);
        pip_scaling_context = NULL;
    }

    pip_video_size   = QSize(0,0);
    pip_display_size = QSize(0,0);
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
    float pipVideoAspect = pipplayer->GetVideoAspect();
    uint  pipVideoWidth  = pipplayer->GetVideoWidth();
    uint  pipVideoHeight = pipplayer->GetVideoHeight();

    // If PiP is not initialized to values we like, silently ignore the frame.
    if ((video_aspect <= 0) || (pipVideoAspect <= 0) || 
        (frame->height <= 0) || (frame->width <= 0) ||
        !pipimage || !pipimage->buf || pipimage->codec != FMT_YV12)
    {
        pipplayer->ReleaseCurrentFrame(pipimage);
        return;
    }

    // set height
    int tmph = (frame->height * db_pip_size) / 100;
    pip_desired_display_size.setHeight((tmph >> 1) << 1);

    // adjust for letterbox modes...
    int letterXadj = 0;
    int letterYadj = 0;
    float letterAdj = 1.0f;
    if (letterbox != kLetterbox_Off)
    {
        letterXadj = max(-display_video_rect.left(), 0);
        float xadj = (float) video_rect.width() / display_visible_rect.width();
        letterXadj = (int) (letterXadj * xadj);

        float yadj = (float)video_rect.height() /display_visible_rect.height();
        letterYadj = max(-display_video_rect.top(), 0);
        letterYadj = (int) (letterYadj * yadj);

        letterAdj  = video_aspect / letterboxed_video_aspect;
    }

    // adjust for non-square pixels on screen
    float dispPixelAdj = (GetDisplayAspect() * video_dim.height()) /
        video_dim.width();

    // adjust for non-square pixels in video
    float vidPixelAdj  = pipVideoWidth / (pipVideoAspect * pipVideoHeight);

    // set width
    int tmpw = (int) (pip_desired_display_size.height() * pipVideoAspect *
                      vidPixelAdj * dispPixelAdj * letterAdj);
    pip_desired_display_size.setWidth((tmpw >> 1) << 1);

    // Scale the image if we have to...
    unsigned char *pipbuf = pipimage->buf;
    if (pipw != pip_desired_display_size.width() ||
        piph != pip_desired_display_size.height())
    {
        DoPipResize(pipw, piph);

        if (pip_tmp_buf && pip_scaling_context)
        {
            AVPicture img_in, img_out;

            avpicture_fill(
                &img_out, (uint8_t *)pip_tmp_buf, PIX_FMT_YUV420P,
                pip_display_size.width(), pip_display_size.height());

            avpicture_fill(&img_in, (uint8_t *)pipimage->buf, PIX_FMT_YUV420P,
                           pipw, piph);

            img_resample(pip_scaling_context, &img_out, &img_in);

            pipw = pip_display_size.width();
            piph = pip_display_size.height();

            pipbuf = pip_tmp_buf;
        }
    }


    // Figure out where to put the Picture-in-Picture window
    int xoff = 0;
    int yoff = 0;
    switch (db_pip_location)
    {
        default:
        case kPIPTopLeft:
                xoff = 30 + letterXadj;
                yoff = 40 + letterYadj;
                break;
        case kPIPBottomLeft:
                xoff = 30 + letterXadj;
                yoff = frame->height - piph - 40 - letterYadj;
                break;
        case kPIPTopRight:
                xoff = frame->width  - pipw - 30 - letterXadj;
                yoff = 40 + letterYadj;
                break;
        case kPIPBottomRight:
                xoff = frame->width  - pipw - 30 - letterXadj;
                yoff = frame->height - piph - 40 - letterYadj;
                break;
    }

    // Copy Y (intensity values)
    for (int i = 0; i < piph; i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               pipbuf + i * pipw, pipw);
    }

    // Copy U & V (half plane chroma values)
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

    // we're done with the frame, release it
    pipplayer->ReleaseCurrentFrame(pipimage);
}

/**
 * \fn VideoOutput::DoVideoResize(int,int)
 * \brief Sets up Picture in Picture image resampler.
 * \param pipwidth  input width
 * \param pipheight input height
 * \sa ShutdownPipResize(), ShowPip(VideoFrame*,NuppelVideoPlayer*)
 */
void VideoOutput::DoVideoResize(const QSize &inDim, const QSize &outDim)
{
    if ((inDim == vsz_video_size) && (outDim == vsz_display_size))
        return;

    ShutdownVideoResize();

    vsz_video_size   = inDim;
    vsz_display_size = outDim;

    int sz = vsz_display_size.height() * vsz_display_size.width() * 3 / 2;
    vsz_tmp_buf = new unsigned char[sz];

    vsz_scale_context = img_resample_init(
        vsz_display_size.width(), vsz_display_size.height(),
        vsz_video_size.width(),   vsz_video_size.height());
}

void VideoOutput::ResizeVideo(VideoFrame *frame)
{
    if (vsz_desired_display_rect.isNull() || frame->codec !=  FMT_YV12)
        return;

    QRect resize = vsz_desired_display_rect;
    QSize frameDim(frame->width, frame->height);

    // if resize is outside existing frame, abort
    bool abort =
        (resize.right() > frame->width || resize.bottom() > frame->height ||
         resize.width() > frame->width || resize.height() > frame->height);
    // if resize == existing frame, no need to carry on
    abort |= !resize.left() && !resize.top() && (resize.size() == frameDim);

    if (abort)
    {
        vsz_enabled = false;
        ShutdownVideoResize();
        vsz_desired_display_rect.setRect(0,0,0,0);
        return;
    }

    DoVideoResize(frameDim, resize.size());

    if (vsz_tmp_buf && vsz_scale_context)
    {
        AVPicture img_in, img_out;

        avpicture_fill(&img_out, (uint8_t *)vsz_tmp_buf, PIX_FMT_YUV420P,
                       resize.width(), resize.height());
        avpicture_fill(&img_in, (uint8_t *)frame->buf, PIX_FMT_YUV420P,
                       frame->width, frame->height);
        img_resample(vsz_scale_context, &img_out, &img_in);
    }

    int xoff = resize.left();
    int yoff = resize.top();
    int resw = resize.width();

    // Copy Y (intensity values)
    for (int i = 0; i < resize.height(); i++)
    {
        memcpy(frame->buf + (i + yoff) * frame->width + xoff,
               vsz_tmp_buf + i * resw, resw);
    }

    // Copy U & V (half plane chroma values)
    xoff /= 2;
    yoff /= 2;

    unsigned char *uptr = frame->buf + frame->width * frame->height;
    unsigned char *vptr = frame->buf + frame->width * frame->height * 5 / 4;
    int vidw = frame->width / 2;

    unsigned char *videouptr = vsz_tmp_buf + resw * resize.height();
    unsigned char *videovptr = vsz_tmp_buf + resw * resize.height() * 5 / 4;
    resw /= 2;
    for (int i = 0; i < resize.height() / 2; i ++)
    {
        memcpy(uptr + (i + yoff) * vidw + xoff, videouptr + i * resw, resw);
        memcpy(vptr + (i + yoff) * vidw + xoff, videovptr + i * resw, resw);
    }
}

void VideoOutput::ShutdownVideoResize(void)
{
    if (vsz_tmp_buf)
    {
        delete [] vsz_tmp_buf;
        vsz_tmp_buf = NULL;
    }

    if (vsz_scale_context)
    {
        img_resample_close(vsz_scale_context);
        vsz_scale_context = NULL;
    }

    vsz_video_size   = QSize(0,0);
    vsz_display_size = QSize(0,0);
    vsz_enabled      = false;
}

void VideoOutput::SetVideoResize(const QRect &videoRect)
{
    if (!videoRect.isValid()    ||
         videoRect.width()  < 1 || videoRect.height() < 1 ||
         videoRect.left()   < 0 || videoRect.top()    < 0)
    {
        vsz_enabled = false;
        ShutdownVideoResize();
        vsz_desired_display_rect.setRect(0,0,0,0);
    }
    else
    {
        vsz_enabled = true;
        vsz_desired_display_rect = videoRect;
    }
}

/**
 * \fn VideoOutput::DisplayOSD(VideoFrame*,OSD *,int,int)
 * \brief If the OSD has changed, this will convert the OSD buffer
 *        to the OSDSurface's color format.
 *
 *  If the destination format is either IA44 or AI44 the osd is
 *  converted to greyscale.
 *
 * \return 1 if changed, -1 on error and 0 otherwise
 */ 
int VideoOutput::DisplayOSD(VideoFrame *frame, OSD *osd, int stride,
                            int revision)
{
    if (!osd)
        return -1;

    if (vsz_enabled)
        ResizeVideo(frame);

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
                stride = video_dim.width(); // 8 bits per pixel
            if (changed)
                surface->DitherToAI44(frame->buf, stride, video_dim.height());
            break;
        }
        case FMT_IA44:
        {
            if (stride < 0)
                    stride = video_dim.width(); // 8 bits per pixel
            if (changed)
                surface->DitherToIA44(frame->buf, stride, video_dim.height());
            break;
        }
        case FMT_ARGB32:
        {
            if (stride < 0)
                stride = video_dim.width()*4; // 32 bits per pixel
            if (changed)
                surface->BlendToARGB(frame->buf, stride, video_dim.height());
            break;
        }
        default:
            break;
    }
    return (changed) ? 1 : 0;
}

/**
 * \fn VideoOutput::CopyFrame(VideoFrame*, const VideoFrame*)
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
    if (from->size == to->size)
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

/// Correct for a 1920x1080 frames reported as 1920x1088
static QSize fix_1080i(QSize raw)
{
    if (QSize(1920, 1088) == raw)
        return QSize(1920, 1080);
    return raw;
}

/// Correct for rounding errors
static float fix_aspect(float raw)
{
    // Check if close to 4:3
    if (fabs(raw - 1.333333f) < 0.05f)
        raw = 1.333333f;

    // Check if close to 16:9
    if (fabs(raw - 1.777777f) < 0.05f)
        raw = 1.777777f;

    return raw;
}
