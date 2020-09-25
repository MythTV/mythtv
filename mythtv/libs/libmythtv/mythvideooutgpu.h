#ifndef MYTHVIDEOOUTGPU_H
#define MYTHVIDEOOUTGPU_H

// MythTV
#include "mythvideoout.h"

class MythVideoGPU;
class MythPainterGPU;

class MythVideoOutputGPU : public MythVideoOutput
{
  public:
    MythVideoOutputGPU(MythRender* Render, QString &Profile);
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
                                           int ReferenceFrames, bool ForceChange) override;
    void            EndFrame              () override;
    void            ClearAfterSeek        () override;
    bool            EnableVisualisation   (AudioPlayer* Audio, bool Enable, const QString& Name = QString("")) override;
    bool            CanVisualise          (AudioPlayer* Audio) override;
    bool            SetupVisualisation    (AudioPlayer* Audio, const QString& Name) override;
    VideoVisual*    GetVisualisation      () override;
    QString         GetVisualiserName     () override;
    QStringList     GetVisualiserList     () override;
    void            DestroyVisualisation  () override;
    bool            StereoscopicModesAllowed() const override;
    void            ResizeForVideo        (QSize Size = QSize()) override;

  protected:
    virtual QRect   GetDisplayVisibleRectAdj();
    bool            Init                  (const QSize& VideoDim, const QSize& VideoDispDim, float Aspect,
                                           const QRect& DisplayVisibleRect, MythCodecID CodecId) override;
    void            PrepareFrame          (VideoFrame* Frame, FrameScanType Scan) override;
    void            RenderFrame           (VideoFrame* Frame, FrameScanType Scan, OSD* Osd) override;
    bool            CreateBuffers         (MythCodecID CodecID, QSize Size);
    void            DestroyBuffers        ();
    bool            ProcessInputChange    ();
    void            InitDisplayMeasurements();

    MythRender*     m_render              { nullptr };
    MythVideoGPU*   m_video               { nullptr };
    MythPainterGPU* m_painter             { nullptr };
    MythCodecID     m_newCodecId          { kCodec_NONE };
    QSize           m_newVideoDim         { 0, 0 };
    QSize           m_newVideoDispDim     { 0, 0 };
    float           m_newAspect           { 0.0F };
    bool            m_newFrameRate        { false };
    bool            m_buffersCreated      { false };
    QString         m_profile;
    VideoVisual*    m_visual              { nullptr };

  private:
    Q_DISABLE_COPY(MythVideoOutputGPU)
};

#endif
