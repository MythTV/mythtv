// -*- Mode: c++ -*-

#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

extern "C" {
#include "frame.h"
#include "filter.h"
}

#include <QSize>
#include <QRect>
#include <QString>
#include <QPoint>
#include <QMap>
#include <qwindowdefs.h>

#include "videobuffers.h"
#include "mythcodecid.h"
#include "videoouttypes.h"
#include "videooutwindow.h"
#include "DisplayRes.h"
#include "videodisplayprofile.h"
using namespace std;

class NuppelVideoPlayer;
class OSD;
class OSDSurface;
class FilterChain;
class FilterManager;
class OpenGLContextGLX;

typedef QMap<NuppelVideoPlayer*,PIPLocation> PIPMap;

extern "C" {
struct ImgReSampleContext;
struct SwsContext;
}

class VideoOutput
{
    friend class OpenGLVideoSync;

  public:
    static void GetRenderOptions(render_opts &opts);
    static VideoOutput *Create(
        const QString &decoder,   MythCodecID  codec_id,
        void          *codec_priv,
        PIPState       pipState,
        const QSize   &video_dim, float        video_aspect,
        WId            win_id,    const QRect &display_rect,
        float video_prate,        WId          embed_id);

    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(int width, int height, float aspect,
                      WId winid, int winx, int winy, int winw,
                      int winh, WId embedid = 0);
    virtual void InitOSD(OSD *osd);
    virtual void SetVideoFrameRate(float);
    virtual bool IsPreferredRenderer(QSize video_size);
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

    virtual void WindowResized(const QSize &new_size) {}

    virtual bool InputChanged(const QSize &input_size,
                              float        aspect,
                              MythCodecID  myth_codec_id,
                              void        *codec_private,
                              bool        &aspect_changed);
    virtual void VideoAspectRatioChanged(float aspect);

    virtual void ResizeDisplayWindow(const QRect&, bool);
    virtual void EmbedInWidget(int x, int y, int w, int h);
    virtual void StopEmbedding(void);
    virtual void ResizeForGui(void);
    virtual void ResizeForVideo(uint width = 0, uint height = 0);
    virtual void MoveResizeWindow(QRect new_rect) = 0;

    virtual void MoveResize(void);
    virtual void Zoom(ZoomDirection direction);

    virtual void GetOSDBounds(QRect &total, QRect &visible,
                              float &visibleAspect, float &fontScale,
                              float themeAspect) const;
    QRect        GetMHEGBounds(void);
    /// \brief Returns information about the current display
    virtual DisplayInfo GetDisplayInfo(void) = 0;
    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    /// \brief Draws non-video portions of the screen
    /// \param sync if set any queued up draws are sent immediately to the
    ///             graphics context and we block until they have completed.
    virtual void DrawUnusedRects(bool sync = true) = 0;

    /// \brief Returns current display aspect ratio.
    virtual float GetDisplayAspect(void) const;

    /// \brief Returns current aspect override mode
    /// \sa ToggleAspectOverride(AspectOverrideMode)
    AspectOverrideMode GetAspectOverride(void) const;
    virtual void ToggleAspectOverride(
        AspectOverrideMode aspectOverrideMode = kAspect_Toggle);

    /// \brief Returns current adjust fill mode
    /// \sa ToggleAdjustFill(AdjustFillMode)
    AdjustFillMode GetAdjustFill(void) const;
    virtual void ToggleAdjustFill(
        AdjustFillMode adjustFillMode = kAdjustFill_Toggle);

    // pass in null to use the pause frame, if it exists.
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              const PIPMap &pipPlayers,
                              FrameScanType scan = kScan_Ignore) = 0;

    /// \brief Tells video output that a full repaint is needed.
    void ExposeEvent(void);

    PictureAttributeSupported GetSupportedPictureAttributes(void) const
        { return supported_attributes; }
    int         ChangePictureAttribute(PictureAttribute, bool direction);
    virtual int SetPictureAttribute(PictureAttribute, int newValue);
    int         GetPictureAttribute(PictureAttribute) const;
    virtual void InitPictureAttributes(void);

    bool AllowPreviewEPG(void) const;

    virtual bool IsPIPSupported(void) const { return false; }
    virtual bool IsPBPSupported(void) const { return false; }
    virtual bool NeedExtraAudioDecode(void) const { return false; }

    /// \brief Return true if HW Acceleration is running
    virtual bool hasHWAcceleration(void) const { return false; }
    /// \brief Return true if video is automatically synced to the display rate
    virtual bool IsSyncLocked(void) const      { return false; }

    /// \brief Sets the number of frames played
    virtual void SetFramesPlayed(long long fp) { framesPlayed = fp; };
    /// \brief Returns the number of frames played
    virtual long long GetFramesPlayed(void) { return framesPlayed; };

    /// \brief Returns true if a fatal error has been encountered.
    bool IsErrored() { return errorState != kError_None; }
    /// \brief Returns error type
    VideoErrorState GetError(void) { return errorState; }
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
    bool IsEmbedding(void);

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
    virtual void DoneDisplayingFrame(VideoFrame *frame)
        { vbuffers.DoneDisplayingFrame(frame); }
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
    bool IsVideoScalingAllowed(void) const;

    /// \brief returns QRect of PIP based on PIPLocation
    virtual QRect GetPIPRect(PIPLocation location,
                             NuppelVideoPlayer *pipplayer = NULL,
                             bool do_pixel_adj = true) const;
    virtual void RemovePIP(NuppelVideoPlayer *pipplayer) { }

    virtual void SetPIPState(PIPState setting);

    virtual QString GetOSDRenderer(void) const;

    QString GetFilters(void) const;
    /// \brief translates caption/dvd button rectangle into 'screen' space
    QRect   GetImageRect(const QRect &rect);

  protected:
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer_normal, int needprebuffer_small,
                     int keepprebuffer);
    void InitDisplayMeasurements(uint width, uint height, bool resize);
    virtual void ShowPIPs(VideoFrame *frame, const PIPMap &pipPlayers);
    virtual void ShowPIP(VideoFrame        *frame,
                         NuppelVideoPlayer *pipplayer,
                         PIPLocation        loc);

    virtual int DisplayOSD(VideoFrame *frame, OSD *osd, int stride = -1, int revision = -1);

    virtual void SetPictureAttributeDBValue(
        PictureAttribute attributeType, int newValue);
    QRect GetVisibleOSDBounds(float&, float&, float) const;
    QRect GetTotalOSDBounds(void) const;
    virtual bool hasFullScreenOSD(void) const { return false; }

    static void CopyFrame(VideoFrame* to, const VideoFrame* from);

    void DoPipResize(int pipwidth, int pipheight);
    void ShutdownPipResize(void);

    void ResizeVideo(VideoFrame *frame);
    void DoVideoResize(const QSize &inDim, const QSize &outDim);
    virtual void ShutdownVideoResize(void);

    void SetVideoAspectRatio(float aspect);

    vector<VideoOutWindow> windows;
    QSize   db_display_dim;   ///< Screen dimensions in millimeters from DB
    typedef QMap<PictureAttribute,int> PictureSettingMap;
    PictureSettingMap  db_pict_attr; ///< Picture settings
    AspectOverrideMode db_aspectoverride;
    AdjustFillMode     db_adjustfill;
    LetterBoxColour    db_letterbox_colour;
    QString db_deint_filtername;
    bool    db_use_picture_controls;

    VideoDisplayProfile *db_vdisp_profile;
    float   video_prate;  ///< Playback frame rate of video

    // Picture-in-Picture
    QSize   pip_desired_display_size;
    QSize   pip_display_size;
    QSize   pip_video_size;
    unsigned char      *pip_tmp_buf;
    unsigned char      *pip_tmp_buf2;
    struct SwsContext  *pip_scaling_context;
    VideoFrame pip_tmp_image;

    // Video resizing (for ITV)
    bool    vsz_enabled;
    QRect   vsz_desired_display_rect;
    QSize   vsz_display_size;
    QSize   vsz_video_size;
    unsigned char      *vsz_tmp_buf;
    struct SwsContext  *vsz_scale_context;

    // Deinterlacing
    bool           m_deinterlacing;
    QString        m_deintfiltername;
    FilterManager *m_deintFiltMan;
    FilterChain   *m_deintFilter;
    bool           m_deinterlaceBeforeOSD;

    /// VideoBuffers instance used to track video output buffers.
    VideoBuffers vbuffers;

    // Various state variables
    VideoErrorState errorState;
    long long framesPlayed;

    // PIP
    PictureAttributeSupported supported_attributes;

    // Custom display resolutions
    DisplayRes *display_res;

    // Display information
    QSize monitor_sz;
    QSize monitor_dim;
};

#endif
