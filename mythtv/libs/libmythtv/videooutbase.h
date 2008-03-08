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
#include "mythcodecid.h"
#include "videoouttypes.h"

using namespace std;

class NuppelVideoPlayer;
class OSD;
class OSDSurface;
class FilterChain;
class FilterManager;
class VideoDisplayProfile;

extern "C" {
struct ImgReSampleContext;
}

class VideoOutput
{
  public:
    static VideoOutput *Create(
        const QString &decoder,   MythCodecID  codec_id,
        void          *codec_priv,
        const QSize   &video_dim, float        video_aspect,
        WId            win_id,    const QRect &display_rect,
        WId            embed_id);

    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(int width, int height, float aspect,
                      WId winid, int winx, int winy, int winw, 
                      int winh, WId embedid = 0);
    virtual void InitOSD(OSD *osd);
    virtual void SetVideoFrameRate(float);

    virtual bool SetDeinterlacingEnabled(bool);
    virtual bool SetupDeinterlace(bool i, const QString& ovrf="");
    virtual void FallbackDeint(void);
    virtual void BestDeint(void);
    virtual bool NeedsDoubleFramerate(void) const;
    virtual bool IsBobDeint(void) const;
    virtual bool IsExtraProcessingRequired(void) const;
    virtual bool ApproveDeintFilter(const QString& filtername) const;

    virtual void PrepareFrame(VideoFrame *buffer, FrameScanType) = 0;
    virtual void Show(FrameScanType) = 0;

    virtual bool InputChanged(const QSize &input_size,
                              float        aspect,
                              MythCodecID  myth_codec_id,
                              void        *codec_private);
    virtual void VideoAspectRatioChanged(float aspect);

    virtual void EmbedInWidget(WId wid, int x, int y, int w, int h);
    virtual void StopEmbedding(void);
    virtual void ResizeForGui(void) {;} 
    virtual void ResizeForVideo(void) {;}

    virtual void MoveResize(void);
    virtual void Zoom(ZoomDirection direction);
 
    virtual void GetDrawSize(int &xoff, int &yoff, int &width, int &height);
    virtual void GetOSDBounds(QRect &visible, QRect &total,
                              float &pixelAspect, float &fontScale,
                              float themeaspect) const;

    /// \brief Returns current display's frame refresh period in microseconds.
    ///        e.g. 1000000 / frame_rate_in_Hz
    virtual int GetRefreshRate(void) = 0;

    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    /// \brief Draws non-video portions of the screen
    /// \param sync if set any queued up draws are sent immediately to the
    ///             graphics context and we block until they have completed.
    virtual void DrawUnusedRects(bool sync = true) = 0;

    /// \brief Returns current display aspect ratio.
    virtual float GetDisplayAspect(void) const { return display_aspect; }

    /// \brief Returns current aspect override mode
    /// \sa ToggleAspectOverride(AspectOverrideMode)
    AspectOverrideMode GetAspectOverride(void) { return aspectoverride; }
    void ToggleAspectOverride(
        AspectOverrideMode aspectOverrideMode = kAspect_Toggle);

    /// \brief Returns current adjust fill mode
    /// \sa ToggleAdjustFill(AdjustFillMode)
    AdjustFillMode GetAdjustFill(void) { return adjustfill; }
    void ToggleAdjustFill(AdjustFillMode adjustFillMode = kAdjustFill_Toggle);

    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              NuppelVideoPlayer *pipPlayer) = 0;

    /// \brief Tells video output that a full repaint is needed.
    void ExposeEvent(void) { needrepaint = true; }

    PictureAttributeSupported GetSupportedPictureAttributes(void) const
        { return supported_attributes; }
    int         ChangePictureAttribute(PictureAttribute, bool direction);
    virtual int SetPictureAttribute(PictureAttribute, int newValue);
    int         GetPictureAttribute(PictureAttribute) const;
    virtual void InitPictureAttributes(void);

    bool AllowPreviewEPG(void) { return allowpreviewepg; }

    /// \brief Returns true iff Motion Compensation acceleration is available.
    virtual bool hasMCAcceleration(void) const { return false; }
    /// \brief Returns true iff Inverse Discrete Cosine Transform acceleration
    ///        is available.
    virtual bool hasIDCTAcceleration(void) const { return false; }
    /// \brief Returns true iff VLD acceleration is available.
    virtual bool hasVLDAcceleration(void) const { return false; }

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

    /// \brief Returns if videooutput is embedding
    bool IsEmbedding(void) { return embedding; }

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

    void SetVideoScalingAllowed(bool change); 

    /// \brief check if video underscan/overscan is allowed
    bool IsVideoScalingAllowed(void) { return db_scaling_allowed; }

    /// \brief returns QRect of PIP based on PIPLocation
    virtual QRect GetPIPRect(int location, NuppelVideoPlayer *pipplayer = NULL);

    QString GetFilters(void) const;

  protected:
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer_normal, int needprebuffer_small,
                     int keepprebuffer);

    virtual void ShowPip(VideoFrame *frame, NuppelVideoPlayer *pipplayer);
    virtual int DisplayOSD(VideoFrame *frame, OSD *osd, int stride = -1, int revision = -1);

    virtual void SetPictureAttributeDBValue(
        PictureAttribute attributeType, int newValue);
    virtual QRect GetVisibleOSDBounds(float&, float&, float) const;
    virtual QRect GetTotalOSDBounds(void) const;

    static void CopyFrame(VideoFrame* to, const VideoFrame* from);

    void DoPipResize(int pipwidth, int pipheight);
    void ShutdownPipResize(void);

    void ResizeVideo(VideoFrame *frame);
    void DoVideoResize(const QSize &inDim, const QSize &outDim);
    virtual void ShutdownVideoResize(void);

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
    PIPLocation db_pip_location;
    int     db_pip_size;      ///< percentage of full window to use for PiP
    typedef QMap<PictureAttribute,int> PictureSettingMap;
    PictureSettingMap  db_pict_attr; ///< Picture settings
    AspectOverrideMode db_aspectoverride;
    AdjustFillMode     db_adjustfill;
    LetterBoxColour    db_letterbox_colour;
    QString db_deint_filtername;
    bool    db_use_picture_controls;

    VideoDisplayProfile *db_vdisp_profile;

    // Manual Zoom
    float   mz_scale_v;       ///< Manually applied vertical scaling.
    float   mz_scale_h;       ///< Manually applied horizontal scaling.
    QPoint  mz_move;          ///< Manually applied percentage move.

    // Physical dimensions
    QSize   display_dim;      ///< Screen dimensions of playback window in mm
    float   display_aspect;   ///< Physical aspect ratio of playback window

    // Video dimensions
    QSize   video_dim;        ///< Pixel dimensions of video buffer
    QSize   video_disp_dim;   ///< Pixel dimensions of video display area
    float   video_aspect;     ///< Physical aspect ratio of video

    /// Normally this is the same as videoAspect, but may not be
    /// if the user has toggled the aspect override mode.
    float   overriden_video_aspect;
    /// AspectOverrideMode to use to modify overriden_video_aspect
    AspectOverrideMode aspectoverride;
    /// Zoom mode
    AdjustFillMode adjustfill;

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

    // Picture-in-Picture
    QSize   pip_desired_display_size;
    QSize   pip_display_size;
    QSize   pip_video_size;
    unsigned char      *pip_tmp_buf;
    ImgReSampleContext *pip_scaling_context;
    VideoFrame pip_tmp_image;

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
    bool           m_deinterlaceBeforeOSD;

    /// VideoBuffers instance used to track video output buffers.
    VideoBuffers vbuffers;

    // Various state variables
    bool    embedding;
    bool    needrepaint;
    bool    allowpreviewepg;
    bool    errored;
    long long framesPlayed;
    bool    db_scaling_allowed; ///< disable this to prevent overscan/underscan
    PictureAttributeSupported supported_attributes;

    // Constants
    static const float kManualZoomMaxHorizontalZoom;
    static const float kManualZoomMaxVerticalZoom;
    static const float kManualZoomMinHorizontalZoom;
    static const float kManualZoomMinVerticalZoom;
    static const int   kManualZoomMaxMove;
};

#endif
