// -*- Mode: c++ -*-

#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

extern "C" {
#include "frame.h"
#include "filter.h"
}

#include <qframe.h>
#include <qptrqueue.h>
#include <qptrlist.h>
#include "videobuffers.h"

using namespace std;

class NuppelVideoPlayer;
class OSD;
class OSDSurface;
class FilterChain;
class FilterManager;

extern "C" {
struct ImgReSampleContext;
}

enum VideoOutputType
{
    kVideoOutput_Default = 0,
    kVideoOutput_VIA,
    kVideoOutput_IVTV,
    kVideoOutput_Directfb
};

enum MythCodecID
{
// if you add anything to this list please update
// myth2av_codecid in videoout_xv.cpp
    kCodec_NONE = 0,

    kCodec_MPEG1,
    kCodec_MPEG2,
    kCodec_H263,
    kCodec_MPEG4,
    kCodec_H264,
    
    kCodec_NORMAL_END,

    kCodec_MPEG1_XVMC,
    kCodec_MPEG2_XVMC,
    kCodec_H263_XVMC,
    kCodec_MPEG4_XVMC,
    kCodec_H264_XVMC,

    kCodec_MPEG1_IDCT,
    kCodec_MPEG2_IDCT,
    kCodec_H263_IDCT,
    kCodec_MPEG4_IDCT,
    kCodec_H264_IDCT,

    kCodec_STD_XVMC_END,

    kCodec_MPEG1_VLD,
    kCodec_MPEG2_VLD,
    kCodec_H263_VLD,
    kCodec_MPEG4_VLD,
    kCodec_H264_VLD,

    kCodec_SPECIAL_END,
};

enum PictureAttribute
{
    kPictureAttribute_None = 0,
    kPictureAttribute_MIN = 1,
    kPictureAttribute_Brightness = 1,
    kPictureAttribute_Contrast,
    kPictureAttribute_Colour,
    kPictureAttribute_Hue,
    kPictureAttribute_Volume,
    kPictureAttribute_MAX
};

enum PIPLocations
{
    kPIPTopLeft = 0,
    kPIPBottomLeft,
    kPIPTopRight,
    kPIPBottomRight
};

enum ZoomDirections
{
    kZoomHome = 0,
    kZoomIn,
    kZoomOut,
    kZoomUp,
    kZoomDown,
    kZoomLeft,
    kZoomRight
};

enum LetterboxModes
{
    kLetterbox_Toggle = -1,
    kLetterbox_Off = 0,
    kLetterbox_4_3,
    kLetterbox_16_9,
    kLetterbox_4_3_Zoom,
    kLetterbox_16_9_Zoom,
    kLetterbox_16_9_Stretch,
    kLetterbox_Fill,
    kLetterbox_END
};

enum FrameScanType
{
    kScan_Ignore      = -1,
    kScan_Detect      =  0,
    kScan_Interlaced  =  1, // == XVMC_TOP_PICTURE
    kScan_Intr2ndField=  2, // == XVMC_BOTTOM_PICTURE
    kScan_Progressive =  3, // == XVMC_FRAME_PICTURE
};

static inline bool is_interlaced(FrameScanType scan)
{
    return (kScan_Interlaced == scan) || (kScan_Intr2ndField == scan);
}

static inline bool is_progressive(FrameScanType scan)
{
    return (kScan_Progressive == scan);
}

static inline QString frame_scan_to_string(FrameScanType scan,
                                           bool brief = false)
{
    QString ret = QObject::tr("Unknown");
    switch (scan)
    {
        case kScan_Ignore:
            ret = QObject::tr("Ignore"); break;
        case kScan_Detect:
            ret = QObject::tr("Detect"); break;
        case kScan_Interlaced:
            if (brief)
                ret = QObject::tr("Interlaced");
            else
                ret = QObject::tr("Interlaced (Normal)");
            break;
        case kScan_Intr2ndField:
            if (brief)
                ret = QObject::tr("Interlaced");
            else
                ret = QObject::tr("Interlaced (Reversed)");
            break;
        case kScan_Progressive:
            ret = QObject::tr("Progressive"); break;
        default:
            break;
    }
    return ret;
}

class VideoOutput
{
  public:
    static VideoOutput *InitVideoOut(VideoOutputType type,
                                     MythCodecID av_codec_id);

    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(int width, int height, float aspect,
                      WId winid, int winx, int winy, int winw, 
                      int winh, WId embedid = 0);

    virtual bool SetDeinterlacingEnabled(bool);
    virtual bool SetupDeinterlace(bool i, const QString& ovrf="");
    virtual bool NeedsDoubleFramerate(void) const;
    virtual bool ApproveDeintFilter(const QString& filtername) const;

    virtual void PrepareFrame(VideoFrame *buffer, FrameScanType) = 0;
    virtual void Show(FrameScanType) = 0;

    virtual void InputChanged(int width, int height, float aspect,
                              MythCodecID av_codec_id);
    virtual void VideoAspectRatioChanged(float aspect);

    virtual void EmbedInWidget(WId wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);

    virtual void MoveResize(void);
    virtual void Zoom(int direction);
 
    virtual void GetDrawSize(int &xoff, int &yoff, int &width, int &height);
    virtual void GetOSDBounds(QRect &visible, QRect &total,
                              float &pixelAspect, float &fontScale) const;

    virtual int GetRefreshRate(void) = 0;

    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    /// \brief Draws non-video portions of the screen
    /// \param sync if set any queued up draws are sent immediately to the
    ///             graphics context and we block until they have completed.
    virtual void DrawUnusedRects(bool sync = true) = 0;

    /// \brief Returns current display aspect ratio.
    virtual float GetDisplayAspect(void) const { return display_aspect; }

    /// \brief Returns current letterboxing mode
    /// \sa ToggleLetterbox(int)
    int GetLetterbox(void) { return letterbox; }
    void ToggleLetterbox(int letterboxMode = kLetterbox_Toggle);

    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              NuppelVideoPlayer *pipPlayer) = 0;

    /// \brief Tells video output that a full repaint is needed.
    void ExposeEvent(void) { needrepaint = true; }

    int ChangeBrightness(bool up);
    int ChangeContrast(bool up);
    int ChangeColour(bool up);
    int ChangeHue(bool up);

    /// \brief Returns current playback brightness
    int GetCurrentBrightness(void) { return brightness; }
    /// \brief Returns current playback contrast
    int GetCurrentContrast(void) { return contrast; }
    /// \brief Returns current playback colour
    int GetCurrentColour(void) { return colour; }
    /// \brief Returns current playback hue
    int GetCurrentHue(void) { return hue; }

    bool AllowPreviewEPG(void) { return allowpreviewepg; }

    /// \brief Returns true iff Motion Compensation acceleration is available.
    virtual bool hasMCAcceleration() const { return false; }
    /// \brief Returns true iff Inverse Discrete Cosine Transform acceleration
    ///        is available.
    virtual bool hasIDCTAcceleration() const { return false; }
    /// \brief Returns true iff VLD acceleration is available.
    virtual bool hasVLDAcceleration() const { return false; }

    /// \brief Sets the number of frames played
    virtual void SetFramesPlayed(long long fp) { framesPlayed = fp; };
    /// \brief Returns the number of frames played
    virtual long long GetFramesPlayed(void) { return framesPlayed; };

    /// \brief Returns true if a fatal error has been encountered.
    bool IsErrored() { return errored; }

    // Video Buffer Management
    /// \brief Sets whether to use a normal number of buffers or fewer buffers.
    void SetPrebuffering(bool normal) { vbuffers.SetPrebuffering(normal); }
    /// \brief Tells video output to toss decoded buffers due to a seek
    virtual void ClearAfterSeek(void) { vbuffers.ClearAfterSeek(); }
    /// \brief Blocks until a frame is available for decoding onto.
    bool WaitForAvailable(uint w) { return vbuffers.WaitForAvailable(w); }

    /// \brief Returns number of frames that are fully decoded.
    virtual int ValidVideoFrames(void) const
        { return vbuffers.ValidVideoFrames(); }
    /// \brief Returns number of frames available for decoding onto.
    int FreeVideoFrames(void) { return vbuffers.FreeVideoFrames(); }
    /// \brief Returns true iff enough frames are available to decode onto.
    bool EnoughFreeFrames(void) { return vbuffers.EnoughFreeFrames(); }
    /// \brief Returns true iff there are plenty of decoded frames ready
    ///        for display.
    bool EnoughDecodedFrames(void) { return vbuffers.EnoughDecodedFrames(); }
    /// \brief Returns true iff we have at least the minimum number of
    ///        decoded frames ready for display.
    bool EnoughPrebufferedFrames(void) { return vbuffers.EnoughPrebufferedFrames(); }
    
    /**
     * \brief Blocks until it is possible to return a frame for decoding onto.
     * \param with_lock if true frames are properly locked, but this means you
     *        must unlock them when you are done, so this is disabled by default.
     * \param allow_unsafe if true then that are queued for display can be
     *       returned as frames to decode onto, this defaults to false.
     */
    virtual VideoFrame *GetNextFreeFrame(bool with_lock = false,
                                         bool allow_unsafe = false)
        { return vbuffers.GetNextFreeFrame(with_lock, allow_unsafe); }
    /// \brief Releases a frame from the ready for decoding queue onto the
    ///        queue of frames ready for display.
    virtual void ReleaseFrame(VideoFrame *frame) { vbuffers.ReleaseFrame(frame); }
    /// \brief Releases a frame for reuse if it is in limbo.
    virtual void DeLimboFrame(VideoFrame *frame) { vbuffers.DeLimboFrame(frame); }
    /// \brief Tell GetLastShownFrame() to return the next frame from the head
    ///        of the queue of frames to display.
    virtual void StartDisplayingFrame(void) { vbuffers.StartDisplayingFrame(); }
    /// \brief Releases frame returned from GetLastShownFrame() onto the 
    ///        queue of frames ready for decoding onto.
    virtual void DoneDisplayingFrame(void) { vbuffers.DoneDisplayingFrame(); }
    /// \brief Releases frame from any queue onto the 
    ///        queue of frames ready for decoding onto.
    virtual void DiscardFrame(VideoFrame *frame) { vbuffers.DiscardFrame(frame); }
    /// \brief Releases all frames not being actively displayed from any queue
    ///        onto the queue of frames ready for decoding onto.
    virtual void DiscardFrames(bool kf) { vbuffers.DiscardFrames(kf); }

    virtual void CheckFrameStates(void) { }

    /// \bug not implemented correctly. vpos is not updated.
    VideoFrame *GetLastDecodedFrame(void) { return vbuffers.GetLastDecodedFrame(); }

    /// \brief Returns frame from the head of the ready to be displayed queue,
    ///        if StartDisplayingFrame has been called.
    VideoFrame *GetLastShownFrame(void)  { return vbuffers.GetLastShownFrame(); }

    /// \brief Returns string with status of each frame for debugging.
    QString GetFrameStatus(void) const { return vbuffers.GetStatus(); }

    /// \brief Updates frame displayed when video is paused.
    virtual void UpdatePauseFrame(void) = 0;

    /// \brief Tells the player to resize the video frame (used for ITV)
    void SetVideoResize(const QRect &videoRect);

  protected:
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer_normal, int needprebuffer_small,
                     int keepprebuffer);

    virtual void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    int DisplayOSD(VideoFrame *frame, OSD *osd, int stride = -1, int revision = -1);

    virtual int ChangePictureAttribute(int attributeType, int newValue);
    virtual QRect GetVisibleOSDBounds(float&, float&) const;
    virtual QRect GetTotalOSDBounds(void) const;

    static void CopyFrame(VideoFrame* to, const VideoFrame* from);

    void DoPipResize(int pipwidth, int pipheight);
    void ShutdownPipResize(void);

    void ResizeVideo(VideoFrame *frame);
    void DoVideoResize(const QSize &inDim, const QSize &outDim);
    void ShutdownVideoResize(void);

    void SetVideoAspectRatio(float aspect);

    void ApplyManualScaleAndMove(void);
    void ApplyDBScaleAndMove(void);
    void ApplyLetterboxing(void);
    void ApplySnapToVideoRect(void);
    void PrintMoveResizeDebug(void);

    QSize   db_display_dim;   ///< Screen dimensions in millimeters from DB
    QPoint  db_move;          ///< Percentage move from database
    float   db_scale_horiz;   ///< Horizontal Overscan/Underscan percentage
    float   db_scale_vert;    ///< Vertical Overscan/Underscan percentage
    int     db_pip_location;
    int     db_pip_size;      ///< percentage of full window to use for PiP
    int     db_pict_brightness;
    int     db_pict_contrast;
    int     db_pict_colour;
    int     db_pict_hue;
    int     db_letterbox;
    QString db_deint_filtername;

    // Manual Zoom
    int     mz_scale;         ///< Manually applied percentage scaling.
    QPoint  mz_move;          ///< Manually applied percentage move.

    // Physical dimensions
    QSize   display_dim;      ///< Screen dimensions of playback window in mm
    float   display_aspect;   ///< Physical aspect ratio of playback window

    // Video dimensions
    QSize   video_dim;        ///< Pixel dimensions of video
    float   video_aspect;     ///< Physical aspect ratio of video

    /// Normally this is the same as videoAspect, but may not be
    /// if the user has toggled to a different "letterbox" mode.
    float   letterboxed_video_aspect;
    /// LetterboxMode to use to modify letterboxed_video_aspect
    int     letterbox;

    /// Pixel rectangle in video frame to display
    QRect   video_rect;
    /// Pixel rectangle in display window into which video_rect maps to
    QRect   display_video_rect;
    /// Visible portion of display window in pixels.
    /// This may be bigger or smaller than display_video_rect.
    QRect   display_visible_rect;
    /// Used to save the display_visible_rect for
    /// restoration after video embedding ends.
    QRect   tmp_display_visible_rect;

    // Picture settings
    int     brightness;
    int     contrast;
    int     colour;
    int     hue;

    // Picture-in-Picture
    QSize   pip_desired_display_size;
    QSize   pip_display_size;
    QSize   pip_video_size;
    unsigned char      *pip_tmp_buf;
    ImgReSampleContext *pip_scaling_context;

    // Video resizing (for ITV)
    bool    vsz_enabled;
    QRect   vsz_desired_display_rect;
    QSize   vsz_display_size;
    QSize   vsz_video_size;
    unsigned char      *vsz_tmp_buf;
    ImgReSampleContext *vsz_scale_context;

    // Deinterlacing
    bool           m_deinterlacing;
    QString        m_deintfiltername;
    FilterManager *m_deintFiltMan;
    FilterChain   *m_deintFilter;
    bool           m_deinterlaceBeforeOSD;;

    /// VideoBuffers instance used to track video output buffers.
    VideoBuffers vbuffers;

    // Various state variables
    bool    embedding;
    bool    needrepaint;
    bool    allowpreviewepg;
    bool    errored;
    long long framesPlayed;
};

#endif
