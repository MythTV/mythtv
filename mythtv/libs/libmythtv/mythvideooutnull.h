#ifndef MYTH_VIDEOOUT_NULL_H_
#define MYTH_VIDEOOUT_NULL_H_

// MythTV
#include "mythvideoout.h"

class MythVideoOutputNull : public MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions& Options);
    MythVideoOutputNull();
   ~MythVideoOutputNull() override;

    bool Init(const QSize& VideoDim, const QSize& VideoDispDim,
              float Aspect, const QRect& DisplayVisibleRect, MythCodecID CodecID) override;
    void SetDeinterlacing(bool Enable, bool DoubleRate, MythDeintType Force = DEINT_NONE) override;

    void PrepareFrame (VideoFrame* Frame, FrameScanType Scan) override;
    void RenderFrame  (VideoFrame* Frame, FrameScanType Scan, OSD* /*Osd*/) override;
    void EndFrame     () override;
    bool InputChanged(const QSize& VideoDim,   const QSize& VideoDispDim,
                      float        Aspect,     MythCodecID  CodecID,
                      bool&        AspectOnly,
                      int          ReferenceFrames, bool ForceChange) override;
    void EmbedInWidget(const QRect& EmbedRect) override;
    void StopEmbedding(void) override;
    void UpdatePauseFrame(int64_t& DisplayTimecode, FrameScanType Scan = kScan_Progressive) override;
    void CreatePauseFrame(void);

  private:
    QMutex     m_globalLock   { QMutex::Recursive };
    VideoFrame m_avPauseFrame { };
};
#endif
