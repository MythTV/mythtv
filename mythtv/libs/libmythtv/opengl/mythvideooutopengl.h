#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV
#include "mythvideoout.h"
#include "mythopenglvideo.h"

class MythRenderOpenGL;
class MythOpenGLPainter;
class MythOpenGLPerf;

class MythVideoOutputOpenGL : public MythVideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options);
    static QStringList GetAllowedRenderers(MythCodecID CodecId, const QSize &VideoDim);

    explicit MythVideoOutputOpenGL(QString Profile = QString());
    ~MythVideoOutputOpenGL() override;

    // VideoOutput
    bool Init(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect,
              MythDisplay *Display, const QRect &DisplayVisibleRect, MythCodecID CodecId) override;

    void PrepareFrame(VideoFrame *Frame, FrameScanType, OSD *Osd) override;
    void ProcessFrame(VideoFrame *Frame, OSD *Osd,
                      const PIPMap &PiPPlayers, FrameScanType Scan) override;
    void Show(FrameScanType ) override;
    void ClearAfterSeek(void) override;
    bool InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                      float Aspect, MythCodecID CodecId, bool &AspectOnly,
                      MythMultiLocker* Locks, int ReferenceFrames,
                      bool ForceChange) override;
    void UpdatePauseFrame(int64_t &DisplayTimecode) override;
    void InitPictureAttributes(void) override;
    void ShowPIP(VideoFrame *Frame, MythPlayer *PiPPlayer, PIPLocation Location) override;
    void RemovePIP(MythPlayer *PiPPlayer) override;
    bool IsPIPSupported(void) const override  { return true; }
    MythPainter *GetOSDPainter(void) override;
    bool CanVisualise(AudioPlayer *Audio, MythRender *Render) override;
    bool SetupVisualisation(AudioPlayer *Audio, MythRender *Render, const QString &Name) override;
    QStringList GetVisualiserList(void) override;
    bool StereoscopicModesAllowed(void) const override { return true; }
    void DoneDisplayingFrame(VideoFrame *Frame) override;
    void DiscardFrames(bool KeyFrame, bool Flushed) override;
    VideoFrameType* DirectRenderFormats(void) override;
    void WindowResized(const QSize &Size) override;

  protected:
    void DestroyBuffers(void);
    bool CreateBuffers(MythCodecID CodecID, QSize Size);
    QRect GetDisplayVisibleRect(void);

    MythRenderOpenGL      *m_render               { nullptr };
    bool                   m_legacyTextureFormats { false };
    MythOpenGLVideo       *m_openGLVideo          { nullptr };
    QMap<MythPlayer*,MythOpenGLVideo*> m_openGLVideoPiPs;
    QMap<MythPlayer*,bool> m_openGLVideoPiPsReady;
    MythOpenGLVideo       *m_openGLVideoPiPActive { nullptr };
    MythOpenGLPainter     *m_openGLPainter        { nullptr };
    QString                m_videoProfile;
    MythCodecID            m_newCodecId           { kCodec_NONE };
    QSize                  m_newVideoDim;
    QSize                  m_newVideoDispDim;
    float                  m_newAspect            { 0.0F };
    bool                   m_buffersCreated       { false };

    // performance monitoring (-v gpu)
    MythOpenGLPerf        *m_openGLPerf           { nullptr };
};

#endif
