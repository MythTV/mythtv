#ifndef MYTHVIDEOOUTGPU_H
#define MYTHVIDEOOUTGPU_H

// MythTV
#include "mythvideoout.h"

class MythMainWindow;
class MythVideoGPU;
class MythPainterGPU;

class MythVideoOutputGPU : public MythVideoOutput
{
    Q_OBJECT

  public:
    static MythVideoOutputGPU* Create(MythMainWindow* MainWindow, const QString& Decoder,
                                      MythCodecID CodecID,       QSize VideoDim,
                                      QSize VideoDispDim,        float VideoAspect,
                                      float FrameRate,           uint  PlayerFlags,
                                      const QString& Codec,      int ReferenceFrames,
                                      const VideoFrameTypes*& RenderFormats);
   ~MythVideoOutputGPU() override;

  signals:
    void            ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value);
    void            PictureAttributeChanged(PictureAttribute Attribute, int Value);

  public slots:
    void            WindowResized         (QSize Size);

  public:
    void            InitPictureAttributes () override;
    void            SetVideoFrameRate     (float NewRate) override;
    void            DiscardFrames         (bool KeyFrame, bool Flushed) override;
    void            DoneDisplayingFrame   (MythVideoFrame* Frame) override;
    void            UpdatePauseFrame      (int64_t& DisplayTimecode, FrameScanType Scan = kScan_Progressive) override;
    bool            InputChanged          (QSize VideoDim, QSize VideoDispDim,
                                           float Aspect, MythCodecID CodecId, bool& AspectOnly,
                                           int ReferenceFrames, bool ForceChange) override;
    void            EndFrame              () override;
    void            ClearAfterSeek        () override;
    void            ResizeForVideo        (QSize Size = QSize()) override;

  protected:
    MythVideoOutputGPU(MythRender* Render, MythVideoProfilePtr VideoProfile, QString &Profile);
    virtual QRect   GetDisplayVisibleRectAdj();
    bool            Init                  (QSize VideoDim, QSize VideoDispDim, float Aspect,
                                           QRect DisplayVisibleRect, MythCodecID CodecId) override;
    void            PrepareFrame          (MythVideoFrame* Frame, FrameScanType Scan) override;
    void            RenderFrame           (MythVideoFrame* Frame, FrameScanType Scan) override;
    void            RenderOverlays        (OSD& Osd) override;
    bool            CreateBuffers         (MythCodecID CodecID, QSize Size);
    void            DestroyBuffers        ();
    bool            ProcessInputChange    ();
    void            InitDisplayMeasurements();
    void            SetReferenceFrames    (int ReferenceFrames);

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

  private:
    Q_DISABLE_COPY(MythVideoOutputGPU)
};

#endif
