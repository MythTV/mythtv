#ifndef MYTHVIDEOOUTGPU_H
#define MYTHVIDEOOUTGPU_H

// MythTV
#include "mythvideoout.h"

class MythVideoGPU;

class MythVideoOutputGPU : public MythVideoOutput
{
  public:
    explicit MythVideoOutputGPU(QString &Profile);
   ~MythVideoOutputGPU() override;

    MythPainter*    GetOSDPainter         () override;
    void            InitPictureAttributes () override;
    void            WindowResized         (const QSize& Size) override;
    void            SetVideoFrameRate     (float NewRate) override;
    void            DiscardFrames         (bool KeyFrame, bool Flushed) override;
    void            DoneDisplayingFrame   (VideoFrame* Frame) override;
    void            UpdatePauseFrame      (int64_t& DisplayTimecode, FrameScanType Scan = kScan_Progressive) override;
    bool            InputChanged          (const QSize& VideoDim, const QSize& VideoDispDim,
                                           float Aspect, MythCodecID CodecId, bool& AspectOnly,
                                           MythMultiLocker* Locks, int ReferenceFrames,
                                           bool ForceChange) override;
    void            EndFrame              (FrameScanType /*Scan*/) override;
    void            ClearAfterSeek        () override;
    bool            IsPIPSupported        () const override  { return true; }
    void            ShowPIPs              (VideoFrame* Frame, const PIPMap& PiPPlayers) override;
    void            ShowPIP               (VideoFrame* Frame, MythPlayer* PiPPlayer, PIPLocation Location) override;
    void            RemovePIP             (MythPlayer* PiPPlayer) override;
    bool            EnableVisualisation   (AudioPlayer* /*Audio*/, bool /*Enable*/, const QString& /*Name*/ = QString("")) override;
    bool            CanVisualise          (AudioPlayer* Audio) override;
    bool            SetupVisualisation    (AudioPlayer* Audio, const QString& Name) override;
    VideoVisual*    GetVisualisation      () override;
    QString         GetVisualiserName     () override;
    QStringList     GetVisualiserList     () override;
    void            DestroyVisualisation  () override;
    bool            StereoscopicModesAllowed() const override;
    void            SetStereoscopicMode   (StereoscopicMode Mode) override;
    StereoscopicMode GetStereoscopicMode  () const override;

  protected:
    virtual MythVideoGPU* CreateSecondaryVideo(const QSize& VideoDim,
                                               const QSize& VideoDispDim,
                                               const QRect& DisplayVisibleRect,
                                               const QRect& DisplayVideoRect,
                                               const QRect& VideoRect) = 0;
    virtual QRect   GetDisplayVisibleRectAdj();

    bool            InitGPU               (const QSize& VideoDim, const QSize& VideoDispDim, float Aspect,
                                           MythDisplay* Display, const QRect& DisplayVisibleRect, MythCodecID CodecId);
    bool            CreateBuffers         (MythCodecID CodecID, QSize Size);
    void            DestroyBuffers        ();
    void            ProcessFrameGPU       (VideoFrame* Frame, const PIPMap& PiPPlayers, FrameScanType Scan);
    void            RenderFrameGPU        (VideoFrame* Frame, FrameScanType Scan, OSD* Osd,
                                           const QRect& ViewPort);
    bool            ProcessInputChange    ();
    void            InitDisplayMeasurements();

    MythRender*     m_render              { nullptr };
    MythVideoGPU*   m_video               { nullptr };
    MythPainter*    m_painter             { nullptr };
    MythCodecID     m_newCodecId          { kCodec_NONE };
    QSize           m_newVideoDim         { 0, 0 };
    QSize           m_newVideoDispDim     { 0, 0 };
    float           m_newAspect           { 0.0F };
    bool            m_newFrameRate        { false };
    bool            m_buffersCreated      { false };
    QString         m_profile;
    VideoVisual*    m_visual              { nullptr };
    StereoscopicMode m_stereo             { kStereoscopicModeNone };

    QMap<MythPlayer*,MythVideoGPU*> m_pxpVideos;
    QMap<MythPlayer*,bool>          m_pxpVideosReady;
    MythVideoGPU*                   m_pxpVideoActive { nullptr };

  private:
    Q_DISABLE_COPY(MythVideoOutputGPU)
};

#endif
