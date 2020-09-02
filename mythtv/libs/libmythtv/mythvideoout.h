#ifndef MYTH_VIDEOOUT_H_
#define MYTH_VIDEOOUT_H_

// Qt
#include <QSize>
#include <QRect>
#include <QString>
#include <QPoint>
#include <QMap>
#include <qwindowdefs.h>

// MythTV
#include "mythframe.h"
#include "videobuffers.h"
#include "mythcodecid.h"
#include "videoouttypes.h"
#include "mythvideobounds.h"
#include "mythdisplay.h"
#include "videodisplayprofile.h"
#include "mythvideocolourspace.h"
#include "visualisations/videovisual.h"
#include "mythavutil.h"
#include "mythdeinterlacer.h"

using namespace std;

class MythPlayer;
class OSD;
class AudioPlayer;
class MythRender;

using PIPMap = QMap<MythPlayer*,PIPLocation>;

class MythMultiLocker;

class MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions& Options);
    static MythVideoOutput* Create(const QString& Decoder,    MythCodecID CodecID,
                                   PIPState PiPState,         const QSize& VideoDim,
                                   const QSize& VideoDispDim, float VideoAspect,
                                   QWidget* ParentWidget,     const QRect& EmbedRect,
                                   float FrameRate,           uint  PlayerFlags,
                                   QString& Codec,            int ReferenceFrames);

    MythVideoOutput();
    virtual ~MythVideoOutput();

    virtual bool Init(const QSize& VideoDim, const QSize& VideoDispDim,
                      float VideoAspect, MythDisplay* Display,
                      const QRect& WindowRect, MythCodecID CodecID);
    virtual void SetVideoFrameRate(float playback_fps);
    virtual void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE);
    virtual void ProcessFrame(VideoFrame* Frame, OSD* Osd, const PIPMap& PipPlayers,
                              FrameScanType Scan = kScan_Ignore) = 0;
    virtual void PrepareFrame(VideoFrame* Frame, FrameScanType, OSD* Osd) = 0;
    virtual void Show(FrameScanType) = 0;
    void         SetReferenceFrames(int ReferenceFrames);
    VideoDisplayProfile* GetProfile() { return m_dbDisplayProfile; }
    virtual void WindowResized(const QSize& /*size*/) {}
    virtual bool InputChanged(const QSize& VideoDim, const QSize& VideoDispDim,
                              float VideoAspect, MythCodecID  CodecID,
                              bool& AspectChanged, MythMultiLocker* Locks,
                              int   ReferenceFrames, bool ForceChange);
    virtual void VideoAspectRatioChanged(float VideoAspect);
    virtual void ResizeDisplayWindow(const QRect& Rect, bool SaveVisible);
    virtual void EmbedInWidget(const QRect& EmbedRect);
    bool         IsEmbedding();
    virtual void StopEmbedding();
    virtual void ResizeForVideo(QSize Size = QSize());
    virtual void Zoom(ZoomDirection Direction);
    virtual void ToggleMoveBottomLine();
    virtual void SaveBottomLine();
    virtual void GetOSDBounds(QRect& Total, QRect& Visible,
                              float& VisibleAspect, float& FontScaling,
                              float ThemeAspect) const;
    QRect        GetMHEGBounds();
    AspectOverrideMode GetAspectOverride() const;
    virtual void ToggleAspectOverride(AspectOverrideMode AspectMode = kAspect_Toggle);
    AdjustFillMode GetAdjustFill() const;
    virtual void ToggleAdjustFill(AdjustFillMode FillMode = kAdjustFill_Toggle);
    QString      GetZoomString() const;
    PictureAttributeSupported GetSupportedPictureAttributes();
    int          ChangePictureAttribute(PictureAttribute AttributeType, bool Direction);
    virtual int  SetPictureAttribute(PictureAttribute Attribute, int NewValue);
    int          GetPictureAttribute(PictureAttribute AttributeType);
    virtual void InitPictureAttributes() { }
    virtual bool IsPIPSupported() const { return false; }
    virtual bool IsPBPSupported() const { return false; }
    bool         HasSoftwareFrames() const { return codec_sw_copy(m_videoCodecID); }
    virtual void SetFramesPlayed(long long FramesPlayed);
    virtual long long GetFramesPlayed();
    bool         IsErrored() const;
    VideoErrorState GetError() const;

    void         SetPrebuffering(bool Normal);
    virtual void ClearAfterSeek();
    virtual int  ValidVideoFrames() const;
    int          FreeVideoFrames();
    bool         EnoughFreeFrames();
    bool         EnoughDecodedFrames();
    virtual VideoFrameVec DirectRenderFormats();
    virtual VideoFrame* GetNextFreeFrame();
    virtual void ReleaseFrame(VideoFrame* Frame);
    virtual void DeLimboFrame(VideoFrame* Frame);
    virtual void StartDisplayingFrame();
    virtual void DoneDisplayingFrame(VideoFrame* Frame);
    virtual void DiscardFrame(VideoFrame* frame);
    virtual void DiscardFrames(bool KeyFrame, bool Flushed);
    virtual void CheckFrameStates() { }
    virtual VideoFrame* GetLastDecodedFrame();
    virtual VideoFrame* GetLastShownFrame();
    QString      GetFrameStatus() const;
    virtual void UpdatePauseFrame(int64_t& DisplayTimecode, FrameScanType Scan = kScan_Progressive) = 0;

    void         SetVideoResize(const QRect& VideoRect);
    void         SetVideoScalingAllowed(bool Allow);
    virtual QRect GetPIPRect(PIPLocation Location,
                             MythPlayer* PiPPlayer = nullptr,
                             bool DoPixelAdj = true) const;
    virtual void RemovePIP(MythPlayer* /*pipplayer*/) { }
    virtual void SetPIPState(PIPState Setting);
    virtual MythPainter* GetOSDPainter() { return nullptr; }

    QRect        GetImageRect(const QRect& Rect, QRect* DisplayRect = nullptr);
    QRect        GetSafeRect();

    bool         EnableVisualisation(AudioPlayer* Audio, bool Enable,
                                     const QString& Name = QString(""));
    virtual bool CanVisualise(AudioPlayer* Audio, MythRender* Render);
    virtual bool SetupVisualisation(AudioPlayer* Audio, MythRender* Render,
                                    const QString& Name);
    VideoVisual* GetVisualisation() { return m_visual; }
    QString      GetVisualiserName();
    virtual QStringList GetVisualiserList();
    void         DestroyVisualisation();

    static MythDeintType ParseDeinterlacer(const QString& Deinterlacer);
    virtual bool StereoscopicModesAllowed() const { return false; }
    void SetStereoscopicMode(StereoscopicMode mode) { m_stereo = mode; }
    StereoscopicMode GetStereoscopicMode() const { return m_stereo; }

  protected:
    virtual void MoveResize();
    virtual void ShowPIPs(VideoFrame* Frame, const PIPMap& PiPPlayers);
    virtual void ShowPIP(VideoFrame* /*Frame*/, MythPlayer* /*PiPPlayer*/, PIPLocation /*Location*/) { }

    QRect        GetVisibleOSDBounds(float& VisibleAspect,
                                     float& FontScaling,
                                     float ThemeAspect) const;
    QRect        GetTotalOSDBounds() const;

    static void  CopyFrame(VideoFrame* To, const VideoFrame* From);

    MythDisplay*         m_display            { nullptr };
    MythVideoBounds       m_window;
    MythVideoColourSpace m_videoColourSpace;
    AspectOverrideMode   m_dbAspectOverride   { kAspect_Off };
    AdjustFillMode       m_dbAdjustFill       { kAdjustFill_Off };
    LetterBoxColour      m_dbLetterboxColour  { kLetterBoxColour_Black };
    MythCodecID          m_videoCodecID       { kCodec_NONE };
    int                  m_maxReferenceFrames { 16 };
    VideoDisplayProfile* m_dbDisplayProfile   { nullptr };
    VideoBuffers         m_videoBuffers;
    VideoErrorState      m_errorState         { kError_None };
    long long            m_framesPlayed       { 0 };
    VideoVisual*         m_visual             { nullptr };
    StereoscopicMode     m_stereo             { kStereoscopicModeNone };
    MythAVCopy           m_copyFrame;
    MythDeinterlacer     m_deinterlacer;
};

#endif // MYTH_VIDEOOUT_H_
