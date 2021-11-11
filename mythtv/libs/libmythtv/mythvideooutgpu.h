#ifndef MYTHVIDEOOUTGPU_H
#define MYTHVIDEOOUTGPU_H

// MythTV
#include "mythvideoout.h"
#include "mythhdrtracker.h"

class MythMainWindow;
class MythVideoGPU;
class MythPainterGPU;

class MythVideoOutputGPU : public MythVideoOutput
{
    Q_OBJECT

  public:
    static void GetRenderOptions(RenderOptions& Options, MythRender* Render);
    static MythVideoOutputGPU* Create(MythMainWindow* MainWindow, MythRender* Render,
                                      MythPainter* Painter, MythDisplay* Display,
                                      const QString& Decoder,
                                      MythCodecID CodecID,       QSize VideoDim,
                                      QSize VideoDispDim,        float VideoAspect,
                                      float FrameRate,           uint  PlayerFlags,
                                      const QString& Codec,      int ReferenceFrames,
                                      const VideoFrameTypes*& RenderFormats);
    static VideoFrameType FrameTypeForCodec(MythCodecID CodecId);
   ~MythVideoOutputGPU() override;

  signals:
    void            ChangePictureAttribute(PictureAttribute Attribute, bool Direction, int Value);
    void            PictureAttributeChanged(PictureAttribute Attribute, int Value);
    void            SupportedAttributesChanged(PictureAttributeSupported Supported);
    void            PictureAttributesUpdated(const std::map<PictureAttribute,int>& Values);
    void            RefreshState();
    void            DoRefreshState();

  public slots:
    void            WindowResized         (QSize Size);
    void            ResizeForVideo        (QSize Size = QSize());

  public:
    void            InitPictureAttributes () override;
    void            SetVideoFrameRate     (float NewRate) override;
    void            DiscardFrames         (bool KeyFrame, bool Flushed) override;
    void            DoneDisplayingFrame   (MythVideoFrame* Frame) override;
    void            UpdatePauseFrame      (std::chrono::milliseconds& DisplayTimecode, FrameScanType Scan = kScan_Progressive) override;
    bool            InputChanged          (QSize VideoDim, QSize VideoDispDim,
                                           float VideoAspect, MythCodecID CodecId, bool& AspectOnly,
                                           int ReferenceFrames, bool ForceChange) override;
    void            EndFrame              () override;
    void            ClearAfterSeek        () override;

  protected:
    MythVideoOutputGPU(MythMainWindow* MainWindow, MythRender* Render,
                       MythPainterGPU* Painter, MythDisplay* Display,
                       MythVideoProfilePtr VideoProfile, QString& Profile);
    virtual QRect   GetDisplayVisibleRectAdj();
    bool            Init                  (QSize VideoDim, QSize VideoDispDim, float Aspect,
                                           QRect DisplayVisibleRect, MythCodecID CodecId) override;
    void            PrepareFrame          (MythVideoFrame* Frame, FrameScanType Scan) override;
    void            RenderFrame           (MythVideoFrame* Frame, FrameScanType Scan) override;
    bool            CreateBuffers         (MythCodecID CodecID, QSize Size);
    void            DestroyBuffers        ();
    bool            ProcessInputChange    ();
    void            InitDisplayMeasurements();
    void            SetReferenceFrames    (int ReferenceFrames);

    MythMainWindow* m_mainWindow          { nullptr };
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
    bool            m_needFullClear       { false };
    HDRTracker      m_hdrTracker          { nullptr };

  private:
    Q_DISABLE_COPY(MythVideoOutputGPU)
};

#endif
