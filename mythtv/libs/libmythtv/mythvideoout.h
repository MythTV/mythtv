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

using PIPMap = QHash<MythPlayer*,PIPLocation>;

class MythMultiLocker;

class MythVideoOutput : public MythVideoBounds
{
    Q_OBJECT

  public:
    static void GetRenderOptions(RenderOptions& Options);
    static MythVideoOutput* Create(const QString& Decoder,    MythCodecID CodecID,
                                   PIPState PiPState,         const QSize& VideoDim,
                                   const QSize& VideoDispDim, float VideoAspect,
                                   QWidget* ParentWidget,     const QRect& EmbedRect,
                                   float FrameRate,           uint  PlayerFlags,
                                   QString& Codec,            int ReferenceFrames);

    MythVideoOutput();
    ~MythVideoOutput() override;

    virtual bool Init(const QSize& VideoDim, const QSize& VideoDispDim,
                      float VideoAspect, MythDisplay* Display,
                      const QRect& WindowRect, MythCodecID CodecID);
    virtual void SetVideoFrameRate(float playback_fps);
    virtual void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE);
    virtual void PrepareFrame (VideoFrame* Frame, const PIPMap& PipPlayers,
                               FrameScanType Scan = kScan_Ignore) = 0;
    virtual void RenderFrame  (VideoFrame* Frame, FrameScanType, OSD* Osd) = 0;
    virtual void EndFrame     () = 0;
    void         SetReferenceFrames(int ReferenceFrames);
    VideoDisplayProfile* GetProfile() { return m_dbDisplayProfile; }
    virtual void WindowResized(const QSize& /*size*/) {}
    virtual bool InputChanged(const QSize& VideoDim, const QSize& VideoDispDim,
                              float VideoAspect, MythCodecID  CodecID,
                              bool& AspectChanged, MythMultiLocker* Locks,
                              int   ReferenceFrames, bool ForceChange);
    virtual void ResizeForVideo(QSize Size = QSize());
    virtual void GetOSDBounds(QRect& Total, QRect& Visible,
                              float& VisibleAspect, float& FontScaling,
                              float ThemeAspect) const;
    PictureAttributeSupported GetSupportedPictureAttributes();
    int          ChangePictureAttribute(PictureAttribute AttributeType, bool Direction);
    virtual int  SetPictureAttribute(PictureAttribute Attribute, int NewValue);
    int          GetPictureAttribute(PictureAttribute AttributeType);
    virtual void InitPictureAttributes() { }
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

    QRect        GetImageRect(const QRect& Rect, QRect* DisplayRect = nullptr);
    QRect        GetSafeRect();
    static MythDeintType ParseDeinterlacer(const QString& Deinterlacer);

    virtual MythPainter* GetOSDPainter       () { return nullptr; }
    virtual void         RemovePIP           (MythPlayer* /*PiPPlayer*/) { }
    virtual bool         IsPIPSupported      () const { return false; }
    virtual bool         IsPBPSupported      () const { return false; }
    virtual bool         EnableVisualisation (AudioPlayer*/*Audio*/, bool /*Enable*/, const QString& /*Name*/ = QString("")) { return false; }
    virtual bool         CanVisualise        (AudioPlayer*/*Audio*/) { return false; }
    virtual bool         SetupVisualisation  (AudioPlayer*/*Audio*/, const QString& /*Name*/) { return false; }
    virtual VideoVisual* GetVisualisation    () { return nullptr; }
    virtual QString      GetVisualiserName   () { return QString {}; }
    virtual QStringList  GetVisualiserList   () { return QStringList {}; }
    virtual void         DestroyVisualisation() { }
    virtual bool         StereoscopicModesAllowed() const { return false; }
    virtual void         SetStereoscopicMode (StereoscopicMode /*Mode*/) { }
    virtual StereoscopicMode GetStereoscopicMode() const { return kStereoscopicModeNone; }

  protected:
    virtual void ShowPIPs (const PIPMap& /*PiPPlayers*/) { }
    virtual void ShowPIP  (MythPlayer* /*PiPPlayer*/, PIPLocation /*Location*/) { }

    QRect        GetVisibleOSDBounds(float& VisibleAspect, float& FontScaling, float ThemeAspect) const;
    QRect        GetTotalOSDBounds() const;
    static void  CopyFrame(VideoFrame* To, const VideoFrame* From);

    MythDisplay*         m_display            { nullptr };
    MythVideoColourSpace m_videoColourSpace;
    LetterBoxColour      m_dbLetterboxColour  { kLetterBoxColour_Black };
    MythCodecID          m_videoCodecID       { kCodec_NONE };
    int                  m_maxReferenceFrames { 16 };
    VideoDisplayProfile* m_dbDisplayProfile   { nullptr };
    VideoBuffers         m_videoBuffers;
    VideoErrorState      m_errorState         { kError_None };
    long long            m_framesPlayed       { 0 };
    MythAVCopy           m_copyFrame;
    MythDeinterlacer     m_deinterlacer;
};

#endif // MYTH_VIDEOOUT_H_
