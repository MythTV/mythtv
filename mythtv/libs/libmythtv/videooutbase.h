// -*- Mode: c++ -*-

#ifndef VIDEOOUTBASE_H_
#define VIDEOOUTBASE_H_

#include "mythframe.h"

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
#include "mythdisplay.h"
#include "DisplayRes.h"
#include "videodisplayprofile.h"
#include "videocolourspace.h"
#include "visualisations/videovisual.h"
#include "mythavutil.h"
#include "mythdeinterlacer.h"

using namespace std;

class MythPlayer;
class OSD;
class AudioPlayer;
class MythRender;

typedef QMap<MythPlayer*,PIPLocation> PIPMap;

class MythMultiLocker;

class VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts);
    static VideoOutput *Create(
        const QString &decoder, MythCodecID  codec_id,
        PIPState pipState,      const QSize &video_dim_buf,
        const QSize &video_dim_disp, float video_aspect,
        QWidget *parentwidget,  const QRect &embed_rect,   float video_prate,
        uint playerFlags, QString &codecName);

    VideoOutput();
    virtual ~VideoOutput();

    virtual bool Init(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float aspect,
                      WId winid, const QRect &win_rect, MythCodecID codec_id);
    virtual void InitOSD(OSD *osd);
    virtual void SetVideoFrameRate(float);
    virtual void SetDeinterlacing(bool Enable, bool DoubleRate);
    virtual void ProcessFrame(VideoFrame *Frame, OSD *Osd, const PIPMap &PipPlayers,
                              FrameScanType Scan = kScan_Ignore) = 0;
    virtual void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd) = 0;
    virtual void Show(FrameScanType) = 0;
    VideoDisplayProfile *GetProfile() { return m_dbDisplayProfile; }


    virtual void WindowResized(const QSize &) {}

    /** \fn VideoOutput::InputChanged()
     *
     *  \param video_dim_buf  The size of the video buffer.
     *  \param video_dim_disp The size of the video display.
     *  \param aspect         The width/height of the presented video.
     *  \param myth_codec_id  The video codec ID.
     *  \param aspect_changed An output parameter indicating that only
     *                        the aspect ratio has changed. It must be
     *                        initialized to false before calling this
     *                        function.
     */
    virtual bool InputChanged(const QSize &video_dim_buf,
                              const QSize &video_dim_disp,
                              float        aspect,
                              MythCodecID  myth_codec_id,
                              bool        &aspect_changed,
                              MythMultiLocker* Locks);

    virtual void VideoAspectRatioChanged(float aspect);

    virtual void ResizeDisplayWindow(const QRect&, bool);
    virtual void EmbedInWidget(const QRect &rect);
    virtual void StopEmbedding(void);
    virtual void ResizeForGui(void);
    virtual void ResizeForVideo(int width = 0, int height = 0);

    virtual void Zoom(ZoomDirection direction);
    virtual void ToggleMoveBottomLine(void);
    virtual void SaveBottomLine(void);

    virtual void GetOSDBounds(QRect &total, QRect &visible,
                              float &visible_aspect, float &font_scaling,
                              float themeAspect) const;
    QRect        GetMHEGBounds(void);

    /// \brief Returns current display aspect ratio.
    virtual float GetDisplayAspect(void) const;

    /// \brief Returns current aspect override mode
    /// \sa ToggleAspectOverride(AspectOverrideMode)
    AspectOverrideMode GetAspectOverride(void) const;
    virtual void ToggleAspectOverride(
        AspectOverrideMode aspectMode = kAspect_Toggle);

    /// \brief Returns current adjust fill mode
    /// \sa ToggleAdjustFill(AdjustFillMode)
    AdjustFillMode GetAdjustFill(void) const;
    virtual void ToggleAdjustFill(
        AdjustFillMode adjustFillMode = kAdjustFill_Toggle);

    QString GetZoomString(void) const { return m_window.GetZoomString(); }
    PictureAttributeSupported GetSupportedPictureAttributes(void)
        { return m_videoColourSpace.SupportedAttributes(); }
    int          ChangePictureAttribute(PictureAttribute, bool direction);
    virtual int  SetPictureAttribute(PictureAttribute, int newValue);
    int          GetPictureAttribute(PictureAttribute);
    virtual void InitPictureAttributes(void) { }

    bool AllowPreviewEPG(void) const;

    virtual QString GetName(void) const { return QString(); }
    virtual bool IsPIPSupported(void) const { return false; }
    virtual bool IsPBPSupported(void) const { return false; }
    virtual bool NeedExtraAudioDecode(void) const { return false; }

    /// \brief Return true if HW Acceleration is running
    virtual bool hasHWAcceleration(void) const { return false; }
    virtual void* GetDecoderContext(unsigned char*, uint8_t*&) { return nullptr; }

    /// \brief Sets the number of frames played
    virtual void SetFramesPlayed(long long fp) { m_framesPlayed = fp; }
    /// \brief Returns the number of frames played
    virtual long long GetFramesPlayed(void) { return m_framesPlayed; }

    /// \brief Returns true if a fatal error has been encountered.
    bool IsErrored() const { return m_errorState != kError_None; }
    /// \brief Returns error type
    VideoErrorState GetError(void) const { return m_errorState; }
    // Video Buffer Management
    /// \brief Sets whether to use a normal number of buffers or fewer buffers.
    void SetPrebuffering(bool normal) { m_videoBuffers.SetPrebuffering(normal); }
    /// \brief Tells video output to toss decoded buffers due to a seek
    virtual void ClearAfterSeek(void) { m_videoBuffers.ClearAfterSeek(); }
    /// \brief Returns number of frames that are fully decoded.
    virtual int ValidVideoFrames(void) const
        { return static_cast<int>(m_videoBuffers.ValidVideoFrames()); }
    /// \brief Returns number of frames available for decoding onto.
    int FreeVideoFrames(void) { return static_cast<int>(m_videoBuffers.FreeVideoFrames()); }
    /// \brief Returns true iff enough frames are available to decode onto.
    bool EnoughFreeFrames(void) { return m_videoBuffers.EnoughFreeFrames(); }
    /// \brief Returns true iff there are plenty of decoded frames ready
    ///        for display.
    bool EnoughDecodedFrames(void) { return m_videoBuffers.EnoughDecodedFrames(); }
    /// \brief Returns true iff we have at least the minimum number of
    ///        decoded frames ready for display.
    bool EnoughPrebufferedFrames(void) { return m_videoBuffers.EnoughPrebufferedFrames(); }
    virtual VideoFrameType* DirectRenderFormats(void);
    bool ReAllocateFrame(VideoFrame *Frame, VideoFrameType Type);
    /// \brief Returns if videooutput is embedding
    bool IsEmbedding(void);

    /**
     * \brief Blocks until it is possible to return a frame for decoding onto.
     */
    virtual VideoFrame *GetNextFreeFrame(void)
        { return m_videoBuffers.GetNextFreeFrame(); }
    /// \brief Releases a frame from the ready for decoding queue onto the
    ///        queue of frames ready for display.
    virtual void ReleaseFrame(VideoFrame *frame) { m_videoBuffers.ReleaseFrame(frame); }
    /// \brief Releases a frame for reuse if it is in limbo.
    virtual void DeLimboFrame(VideoFrame *frame) { m_videoBuffers.DeLimboFrame(frame); }
    /// \brief Tell GetLastShownFrame() to return the next frame from the head
    ///        of the queue of frames to display.
    virtual void StartDisplayingFrame(void) { m_videoBuffers.StartDisplayingFrame(); }
    /// \brief Releases frame returned from GetLastShownFrame() onto the
    ///        queue of frames ready for decoding onto.
    virtual void DoneDisplayingFrame(VideoFrame *frame)
        { m_videoBuffers.DoneDisplayingFrame(frame); }
    /// \brief Releases frame from any queue onto the
    ///        queue of frames ready for decoding onto.
    virtual void DiscardFrame(VideoFrame *frame) { m_videoBuffers.DiscardFrame(frame); }
    /// \brief Releases all frames not being actively displayed from any queue
    ///        onto the queue of frames ready for decoding onto.
    virtual void DiscardFrames(bool kf) { m_videoBuffers.DiscardFrames(kf); }
    /// \brief Clears the frame to black. Subclasses may choose
    ///        to mark the frame as a dummy and act appropriately
    virtual void ClearDummyFrame(VideoFrame* frame);
    virtual void CheckFrameStates(void) { }

    /// \bug not implemented correctly. vpos is not updated.
    virtual VideoFrame *GetLastDecodedFrame(void)
        { return m_videoBuffers.GetLastDecodedFrame(); }

    /// \brief Returns frame from the head of the ready to be displayed queue,
    ///        if StartDisplayingFrame has been called.
    virtual VideoFrame *GetLastShownFrame(void)
        { return m_videoBuffers.GetLastShownFrame(); }

    /// \brief Returns string with status of each frame for debugging.
    QString GetFrameStatus(void) const { return m_videoBuffers.GetStatus(); }

    /// \brief Updates frame displayed when video is paused.
    virtual void UpdatePauseFrame(int64_t &disp_timecode) = 0;

    /// \brief Tells the player to resize the video frame (used for ITV)
    void SetVideoResize(const QRect &VideoRect);

    void SetVideoScalingAllowed(bool change);

    /// \brief Tells the player to flip the video frames for proper display
    virtual void SetVideoFlip(void) { }

    /// \brief returns QRect of PIP based on PIPLocation
    virtual QRect GetPIPRect(PIPLocation location,
                             MythPlayer *pipplayer = nullptr,
                             bool do_pixel_adj = true) const;
    virtual void RemovePIP(MythPlayer *) { }

    virtual void SetPIPState(PIPState setting);

    virtual QString GetOSDRenderer(void) const;
    virtual MythPainter *GetOSDPainter(void) { return nullptr; }

    QString GetFilters(void) const;
    /// \brief translates caption/dvd button rectangle into 'screen' space
    QRect   GetImageRect(const QRect &rect, QRect *display = nullptr);
    QRect   GetSafeRect(void);

    // Visualisations
    bool EnableVisualisation(AudioPlayer *audio, bool enable,
                             const QString &name = QString(""));
    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render);
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name);
    VideoVisual* GetVisualisation(void) { return m_visual; }
    QString GetVisualiserName(void);
    virtual QStringList GetVisualiserList(void);
    void DestroyVisualisation(void);

    // Hue adjustment for certain vendors (mostly ATI)
    static int CalcHueBase(const QString &adaptor_name);

    // 3D TV
    virtual bool StereoscopicModesAllowed(void) const { return false; }
    void SetStereoscopicMode(StereoscopicMode mode) { m_stereo = mode; }
    StereoscopicMode GetStereoscopicMode(void) const { return m_stereo; }

  protected:
    virtual void MoveResize(void);
    virtual void MoveResizeWindow(QRect new_rect) = 0;
    void InitBuffers(int numdecode, bool extra_for_pause, int need_free,
                     int needprebuffer_normal, int needprebuffer_small,
                     int keepprebuffer);
    void InitDisplayMeasurements(int width, int height, bool resize);
    virtual void ShowPIPs(VideoFrame *frame, const PIPMap &pipPlayers);
    virtual void ShowPIP(VideoFrame*, MythPlayer*, PIPLocation) { }

    QRect GetVisibleOSDBounds(float&, float&, float) const;
    QRect GetTotalOSDBounds(void) const;

    static void CopyFrame(VideoFrame* to, const VideoFrame* from);

    VideoOutWindow     m_window;
    QSize              m_dbDisplayDimensions;   ///< Screen dimensions in millimeters from DB
    VideoColourSpace   m_videoColourSpace;
    AspectOverrideMode m_dbAspectOverride;
    AdjustFillMode     m_dbAdjustFill;
    LetterBoxColour    m_dbLetterboxColour;

    // Video parameters
    MythCodecID          m_videoCodecID;
    VideoDisplayProfile *m_dbDisplayProfile;

    /// VideoBuffers instance used to track video output buffers.
    VideoBuffers m_videoBuffers;

    // Various state variables
    VideoErrorState m_errorState;
    long long m_framesPlayed;

    // Custom display resolutions
    DisplayRes *m_displayRes;

    // Display information
    QSize m_monitorSize;
    QSize m_monitorDimensions;

    // Visualisation
    VideoVisual     *m_visual;

    // 3D TV mode
    StereoscopicMode m_stereo;

    MythAVCopy m_copyFrame;

    // Software deinterlacer
    MythDeinterlacer m_deinterlacer;
};

#endif
