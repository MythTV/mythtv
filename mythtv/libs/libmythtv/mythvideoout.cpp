// std
#include <cmath>
#include <cstdlib>

// MythTV
#include "libmythbase/mthreadpool.h"
#include "libmythbase/mythcorecontext.h"
#include "libmythbase/mythlogging.h"
#include "libmythui/mythmainwindow.h"
#include "libmythui/mythuihelper.h"

#include "decoders/decoderbase.h"
#include "decoders/mythcodeccontext.h"
#include "mythavutil.h"
#include "mythplayer.h"
#include "mythvideoout.h"
#include "mythvideooutgpu.h"
#include "mythvideooutnull.h"
#include "mythvideoprofile.h"
#include "osd.h"

#define LOC QString("VideoOutput: ")

void MythVideoOutput::GetRenderOptions(RenderOptions& Options, MythRender* Render)
{
    MythVideoOutputNull::GetRenderOptions(Options);
    MythVideoOutputGPU::GetRenderOptions(Options, Render);
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
 *         vo->ProcessFrame(frame, osd, filters, pict-in-pict, scan);
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
 * \see VideoBuffers, MythPlayer
 */

/**
 * \fn VideoOutput::VideoOutput()
 * \brief This constructor for VideoOutput must be followed by an
 *        Init(int,int,float,WId,int,int,int,int,WId) call.
 */
MythVideoOutput::MythVideoOutput() :
    m_dbLetterboxColour(static_cast<LetterBoxColour>(gCoreContext->GetNumSetting("LetterboxColour", 0))),
    m_clearColor(m_dbLetterboxColour == kLetterBoxColour_Gray25 ? 64 : 0)
{
}

/**
 * \fn VideoOutput::Init(int,int,float,WId,int,int,int,int,WId)
 * \brief Performs most of the initialization for VideoOutput.
 * \return true if successful, false otherwise.
 */
bool MythVideoOutput::Init(const QSize VideoDim, const QSize VideoDispDim,
                           float VideoAspect, const QRect WindowRect, MythCodecID CodecID)
{
    m_videoCodecID = CodecID;
    bool wasembedding = IsEmbedding();
    QRect oldrect;
    if (wasembedding)
    {
        oldrect = GetEmbeddingRect();
        EmbedPlayback(false, {});
    }

    bool mainSuccess = InitBounds(VideoDim, VideoDispDim, VideoAspect, WindowRect);

    if (m_videoProfile)
        m_videoProfile->SetInput(GetVideoDispDim());

    if (wasembedding)
        EmbedPlayback(true, oldrect);

    VideoAspectRatioChanged(VideoAspect); // apply aspect ratio and letterbox mode

    return mainSuccess;
}

void MythVideoOutput::SetVideoFrameRate(float VideoFrameRate)
{
    if (m_videoProfile)
        m_videoProfile->SetOutput(VideoFrameRate);
}

void MythVideoOutput::SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force /*=DEINT_NONE*/)
{


    if (!Enable)
    {
        m_deinterlacing = false;
        m_deinterlacing2X = false;
        m_forcedDeinterlacer = DEINT_NONE;
        m_videoBuffers.SetDeinterlacing(DEINT_NONE, DEINT_NONE, m_videoCodecID);
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Disabled all deinterlacing");
        return;
    }

    m_deinterlacing   = Enable;
    m_deinterlacing2X = DoubleRate;
    m_forcedDeinterlacer = Force;

    MythDeintType singlerate = DEINT_NONE;
    MythDeintType doublerate = DEINT_NONE;
    if (DEINT_NONE != Force)
    {
        singlerate = Force;
        if (DoubleRate)
            doublerate = Force;
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Overriding deinterlacers");
    }
    else if (m_videoProfile)
    {
        singlerate = MythVideoFrame::ParseDeinterlacer(m_videoProfile->GetSingleRatePreferences());
        if (DoubleRate)
            doublerate = MythVideoFrame::ParseDeinterlacer(m_videoProfile->GetDoubleRatePreferences());
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("SetDeinterlacing (Doublerate %1): Single %2 Double %3")
        .arg(QString::number(DoubleRate),
             MythVideoFrame::DeinterlacerPref(singlerate),
             MythVideoFrame::DeinterlacerPref(doublerate)));
    m_videoBuffers.SetDeinterlacing(singlerate, doublerate, m_videoCodecID);
}

/**
 * \brief Tells video output to discard decoded frames and wait for new ones.
 * \bug We set the new width height and aspect ratio here, but we should
 *      do this based on the new video frames in Show().
 */
bool MythVideoOutput::InputChanged(const QSize VideoDim, const QSize VideoDispDim,
                                   float VideoAspect, MythCodecID  CodecID,
                                   bool& /*AspectOnly*/, int ReferenceFrames, bool /*ForceChange*/)
{
    SourceChanged(VideoDim, VideoDispDim, VideoAspect);
    m_maxReferenceFrames = ReferenceFrames;
    AVCodecID avCodecId = myth2av_codecid(CodecID);
    const AVCodec* codec = avcodec_find_decoder(avCodecId);
    QString codecName;
    if (codec)
        codecName = codec->name;
    if (m_videoProfile)
        m_videoProfile->SetInput(GetVideoDispDim(), 0 ,codecName);
    m_videoCodecID = CodecID;
    DiscardFrames(true, true);
    // Update deinterlacers for any input change
    SetDeinterlacing(m_deinterlacing, m_deinterlacing2X, m_forcedDeinterlacer);
    return true;
}

void MythVideoOutput::GetOSDBounds(QRect& Total, QRect& Visible,
                                   float& VisibleAspect,
                                   float& FontScaling,
                                   float ThemeAspect) const
{
    Total   = GetDisplayVisibleRect();
    Visible = GetVisibleOSDBounds(VisibleAspect, FontScaling, ThemeAspect);
}

/**
 * \brief Returns visible portions of total OSD bounds
 * \param VisibleAspect physical aspect ratio of bounds returned
 * \param FontScaling   scaling to apply to fonts
 * \param ThemeAspect   aspect ration of the theme
 */
QRect MythVideoOutput::GetVisibleOSDBounds(float& VisibleAspect,
                                           float& FontScaling,
                                           float ThemeAspect) const
{
    QRect dvr = GetDisplayVisibleRect();
    float dispPixelAdj = 1.0F;
    if (dvr.height() && dvr.width())
        dispPixelAdj = (GetDisplayAspect() * dvr.height()) / dvr.width();

    float ova = GetOverridenVideoAspect();
    QRect vr = GetVideoRect();
    float vs = vr.height() ? static_cast<float>(vr.width()) / vr.height() : 1.0F;
    VisibleAspect = ThemeAspect * (ova > 0.0F ? vs / ova : 1.F) * dispPixelAdj;

    FontScaling = 1.0F;
    return { QPoint(0,0), dvr.size() };
}

PictureAttributeSupported MythVideoOutput::GetSupportedPictureAttributes()
{
    return m_videoColourSpace.SupportedAttributes();
}

void MythVideoOutput::SetFramesPlayed(long long FramesPlayed)
{
    m_framesPlayed = FramesPlayed;
}

long long MythVideoOutput::GetFramesPlayed()
{
    return m_framesPlayed;
}

bool MythVideoOutput::IsErrored() const
{
    return m_errorState != kError_None;
}

VideoErrorState MythVideoOutput::GetError() const
{
    return m_errorState;
}

/// \brief Sets whether to use a normal number of buffers or fewer buffers.
void MythVideoOutput::SetPrebuffering(bool Normal)
{
    m_videoBuffers.SetPrebuffering(Normal);
}

/// \brief Tells video output to toss decoded buffers due to a seek
void MythVideoOutput::ClearAfterSeek()
{
    m_videoBuffers.ClearAfterSeek();
}

/// \brief Returns number of frames that are fully decoded.
int MythVideoOutput::ValidVideoFrames() const
{
    return static_cast<int>(m_videoBuffers.ValidVideoFrames());
}

/// \brief Returns number of frames available for decoding onto
int MythVideoOutput::FreeVideoFrames()
{
    return static_cast<int>(m_videoBuffers.FreeVideoFrames());
}

/// \brief Returns true iff enough frames are available to decode onto.
bool MythVideoOutput::EnoughFreeFrames()
{
    return m_videoBuffers.EnoughFreeFrames();
}

/// \brief Returns true iff there are plenty of decoded frames ready
///        for display.
bool MythVideoOutput::EnoughDecodedFrames()
{
    return m_videoBuffers.EnoughDecodedFrames();
}

/// \bug not implemented correctly. vpos is not updated.
MythVideoFrame* MythVideoOutput::GetLastDecodedFrame()
{
    return m_videoBuffers.GetLastDecodedFrame();
}

/// \brief Returns string with status of each frame for debugging.
QString MythVideoOutput::GetFrameStatus() const
{
    return m_videoBuffers.GetStatus();
}

/// \brief Returns frame from the head of the ready to be displayed queue,
///        if StartDisplayingFrame has been called.
MythVideoFrame* MythVideoOutput::GetLastShownFrame()
{
    return m_videoBuffers.GetLastShownFrame();
}

/// \brief translates caption/dvd button rectangle into 'screen' space
QRect MythVideoOutput::GetImageRect(const QRect Rect, QRect* DisplayRect)
{
    qreal hscale = 0.0;
    QSize video_size   = GetVideoDispDim();
    int image_height   = video_size.height();
    int image_width {720};
    if (image_height > 720)
        image_width = 1920;
    else if (image_height > 576)
        image_width = 1280;
    qreal image_aspect = static_cast<qreal>(image_width) / image_height;
    qreal pixel_aspect = static_cast<qreal>(video_size.width()) / video_size.height();

    QRect rect1 = Rect;
    if (DisplayRect && DisplayRect->isValid())
    {
        QTransform m0;
        m0.scale(static_cast<qreal>(image_width)  / DisplayRect->width(),
                 static_cast<qreal>(image_height) / DisplayRect->height());
        rect1 = m0.mapRect(rect1);
        rect1.translate(DisplayRect->left(), DisplayRect->top());
    }
    QRect result = rect1;

    QRect dvr_rec = GetDisplayVideoRect();
    QRect vid_rec = GetVideoRect();

    hscale = image_aspect / pixel_aspect;
    if (hscale < 0.99 || hscale > 1.01)
    {
        vid_rec.setLeft(static_cast<int>(lround(static_cast<qreal>(vid_rec.left()) * hscale)));
        vid_rec.setWidth(static_cast<int>(lround(static_cast<qreal>(vid_rec.width()) * hscale)));
    }

    qreal vscale = static_cast<qreal>(dvr_rec.width()) / image_width;
    hscale = static_cast<qreal>(dvr_rec.height()) / image_height;
    QTransform m1;
    m1.translate(dvr_rec.left(), dvr_rec.top());
    m1.scale(vscale, hscale);

    vscale = static_cast<qreal>(image_width) / vid_rec.width();
    hscale = static_cast<qreal>(image_height) / vid_rec.height();
    QTransform m2;
    m2.scale(vscale, hscale);
    m2.translate(-vid_rec.left(), -vid_rec.top());

    result = m2.mapRect(result);
    result = m1.mapRect(result);
    return result;
}

/**
 * \brief Returns a QRect describing an area of the screen on which it is
 *        'safe' to render the On Screen Display. For 'fullscreen' OSDs this
 *        will still translate to a subset of the video frame area to ensure
 *        consistency of presentation for subtitling etc.
 */
QRect MythVideoOutput::GetSafeRect()
{
    static constexpr float kSafeMargin = 0.05F;
    float dummy = NAN;
    QRect result = GetVisibleOSDBounds(dummy, dummy, 1.0F);
    int safex = static_cast<int>(static_cast<float>(result.width())  * kSafeMargin);
    int safey = static_cast<int>(static_cast<float>(result.height()) * kSafeMargin);
    return { result.left() + safex, result.top() + safey,
             result.width() - (2 * safex), result.height() - (2 * safey) };
}

/**
 * \brief Blocks until it is possible to return a frame for decoding onto.
 */
MythVideoFrame* MythVideoOutput::GetNextFreeFrame()
{
    return m_videoBuffers.GetNextFreeFrame();
}

/// \brief Releases a frame from the ready for decoding queue onto the
///        queue of frames ready for display.
void MythVideoOutput::ReleaseFrame(MythVideoFrame* Frame)
{
    m_videoBuffers.ReleaseFrame(Frame);
}

/// \brief Releases a frame for reuse if it is in limbo.
void MythVideoOutput::DeLimboFrame(MythVideoFrame* Frame)
{
    m_videoBuffers.DeLimboFrame(Frame);
}

/// \brief Tell GetLastShownFrame() to return the next frame from the head
///        of the queue of frames to display.
void MythVideoOutput::StartDisplayingFrame()
{
    m_videoBuffers.StartDisplayingFrame();
}

/// \brief Releases frame returned from GetLastShownFrame() onto the
///        queue of frames ready for decoding onto.
void MythVideoOutput::DoneDisplayingFrame(MythVideoFrame* Frame)
{
    m_videoBuffers.DoneDisplayingFrame(Frame);
}

/// \brief Releases frame from any queue onto the
///        queue of frames ready for decoding onto.
void MythVideoOutput::DiscardFrame(MythVideoFrame* Frame)
{
    m_videoBuffers.DiscardFrame(Frame);
}

/// \brief Releases all frames not being actively displayed from any queue
///        onto the queue of frames ready for decoding onto.
void MythVideoOutput::DiscardFrames(bool KeyFrame, bool /*Flushed*/)
{
    m_videoBuffers.DiscardFrames(KeyFrame);
}
