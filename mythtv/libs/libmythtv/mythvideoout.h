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
#include "videooutwindow.h"
#include "mythdisplay.h"
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

using PIPMap = QMap<MythPlayer*,PIPLocation>;

class MythMultiLocker;

class MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    static MythVideoOutput *Create(const QString &Decoder,    MythCodecID CodecID,
                                   PIPState PiPState,         const QSize &VideoDim,
                                   const QSize &VideoDispDim, float VideoAspect,
                                   QWidget *ParentWidget,     const QRect &EmbedRect,
                                   float FrameRate,           uint  PlayerFlags,
                                   QString &Codec,            int ReferenceFrames);

    MythVideoOutput();
    virtual ~MythVideoOutput();

    virtual bool Init(const QSize &VideoDim, const QSize &VideoDispDim,
                      float VideoAspect, MythDisplay *Display,
                      const QRect &WindowRect, MythCodecID CodecID);
    virtual void SetVideoFrameRate(float);
    virtual void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE);
    virtual void ProcessFrame(VideoFrame *Frame, OSD *Osd, const PIPMap &PipPlayers,
                              FrameScanType Scan = kScan_Ignore) = 0;
    virtual void PrepareFrame(VideoFrame *Frame, FrameScanType, OSD *Osd) = 0;
    virtual void Show(FrameScanType) = 0;
    void         SetReferenceFrames(int ReferenceFrames);
    VideoDisplayProfile *GetProfile() { return m_dbDisplayProfile; }
    virtual void WindowResized(const QSize &) {}
    virtual bool InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                              float VideoAspect, MythCodecID  CodecID,
                              bool &AspectChanged, MythMultiLocker* Locks,
                              int   ReferenceFrames, bool ForceChange);
    virtual void VideoAspectRatioChanged(float VideoAspect);
    virtual void ResizeDisplayWindow(const QRect& Rect, bool SaveVisible);
    virtual void EmbedInWidget(const QRect &EmbedRect);
    bool         IsEmbedding(void);
    virtual void StopEmbedding(void);
    virtual void ResizeForVideo(int Width = 0, int Height = 0);
    virtual void Zoom(ZoomDirection Direction);
    virtual void ToggleMoveBottomLine(void);
    virtual void SaveBottomLine(void);
    virtual void GetOSDBounds(QRect &Total, QRect &Visible,
                              float &VisibleAspect, float &FontScaling,
                              float ThemeAspect) const;
    QRect        GetMHEGBounds(void);
    virtual float GetDisplayAspect(void) const;
    AspectOverrideMode GetAspectOverride(void) const;
    virtual void ToggleAspectOverride(AspectOverrideMode AspectMode = kAspect_Toggle);
    AdjustFillMode GetAdjustFill(void) const;
    virtual void ToggleAdjustFill(AdjustFillMode FillMode = kAdjustFill_Toggle);
    QString      GetZoomString(void) const;
    PictureAttributeSupported GetSupportedPictureAttributes(void);
    int          ChangePictureAttribute(PictureAttribute AttributeType, bool Direction);
    virtual int  SetPictureAttribute(PictureAttribute Attribute, int NewValue);
    int          GetPictureAttribute(PictureAttribute AttributeType);
    virtual void InitPictureAttributes(void) { }
    virtual bool IsPIPSupported(void) const { return false; }
    virtual bool IsPBPSupported(void) const { return false; }
    bool         HasSoftwareFrames(void) const { return codec_sw_copy(m_videoCodecID); }
    virtual void SetFramesPlayed(long long FramesPlayed);
    virtual long long GetFramesPlayed(void);
    bool         IsErrored() const;
    VideoErrorState GetError(void) const;

    void         SetPrebuffering(bool Normal);
    virtual void ClearAfterSeek(void);
    virtual int  ValidVideoFrames(void) const;
    int          FreeVideoFrames(void);
    bool         EnoughFreeFrames(void);
    bool         EnoughDecodedFrames(void);
    virtual VideoFrameType* DirectRenderFormats(void);
    virtual VideoFrame *GetNextFreeFrame(void);
    virtual void ReleaseFrame(VideoFrame *Frame);
    virtual void DeLimboFrame(VideoFrame *Frame);
    virtual void StartDisplayingFrame(void);
    virtual void DoneDisplayingFrame(VideoFrame *Frame);
    virtual void DiscardFrame(VideoFrame *frame);
    virtual void DiscardFrames(bool KeyFrame, bool);
    virtual void CheckFrameStates(void) { }
    virtual VideoFrame *GetLastDecodedFrame(void);
    virtual VideoFrame *GetLastShownFrame(void);
    QString      GetFrameStatus(void) const;
    virtual void UpdatePauseFrame(int64_t &DisplayTimecode) = 0;

    void         SetVideoResize(const QRect &VideoRect);
    void         SetVideoScalingAllowed(bool Allow);
    virtual QRect GetPIPRect(PIPLocation Location,
                             MythPlayer *PiPPlayer = nullptr,
                             bool DoPixelAdj = true) const;
    virtual void RemovePIP(MythPlayer *) { }
    virtual void SetPIPState(PIPState Setting);
    virtual MythPainter *GetOSDPainter(void) { return nullptr; }

    QRect        GetImageRect(const QRect &Rect, QRect *DisplayRect = nullptr);
    QRect        GetSafeRect(void);

    bool         EnableVisualisation(AudioPlayer *Audio, bool Enable,
                                     const QString &Name = QString(""));
    virtual bool CanVisualise(AudioPlayer *Audio, MythRender *Render);
    virtual bool SetupVisualisation(AudioPlayer *Audio, MythRender *Render,
                                    const QString &Name);
    VideoVisual* GetVisualisation(void) { return m_visual; }
    QString      GetVisualiserName(void);
    virtual QStringList GetVisualiserList(void);
    void         DestroyVisualisation(void);

    static int   CalcHueBase(const QString &AdaptorName);
    static MythDeintType ParseDeinterlacer(const QString &Deinterlacer);
    virtual bool StereoscopicModesAllowed(void) const { return false; }
    void SetStereoscopicMode(StereoscopicMode mode) { m_stereo = mode; }
    StereoscopicMode GetStereoscopicMode(void) const { return m_stereo; }

  protected:
    virtual void MoveResize(void);
    void         InitDisplayMeasurements(void);
    virtual void ShowPIPs(VideoFrame *Frame, const PIPMap &PiPPlayers);
    virtual void ShowPIP(VideoFrame*, MythPlayer*, PIPLocation) { }

    QRect        GetVisibleOSDBounds(float& VisibleAspect,
                                     float& FontScaling,
                                     float ThemeAspect) const;
    QRect        GetTotalOSDBounds(void) const;

    static void  CopyFrame(VideoFrame* To, const VideoFrame* From);

    MythDisplay*         m_display               {nullptr};
    VideoOutWindow       m_window;
    QSize                m_dbDisplayDimensionsMM {0,0};
    VideoColourSpace     m_videoColourSpace;
    AspectOverrideMode   m_dbAspectOverride      {kAspect_Off};
    AdjustFillMode       m_dbAdjustFill          {kAdjustFill_Off};
    LetterBoxColour      m_dbLetterboxColour     {kLetterBoxColour_Black};
    MythCodecID          m_videoCodecID          {kCodec_NONE};
    int                  m_maxReferenceFrames    {16};
    VideoDisplayProfile *m_dbDisplayProfile      {nullptr};
    VideoBuffers         m_videoBuffers;
    VideoErrorState      m_errorState            {kError_None};
    long long            m_framesPlayed          {0};
    QSize                m_monitorSize           {640,480};
    QSize                m_monitorDimensions     {400,300};
    VideoVisual         *m_visual                {nullptr};
    StereoscopicMode     m_stereo                {kStereoscopicModeNone};
    MythAVCopy           m_copyFrame;
    MythDeinterlacer     m_deinterlacer;
};

#endif // MYTH_VIDEOOUT_H_
