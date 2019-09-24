#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV
#include "videooutbase.h"
#include "openglvideo.h"

class MythRenderOpenGL;
class MythOpenGLPainter;
class MythOpenGLPerf;

class VideoOutputOpenGL : public VideoOutput
{
  public:
    static void GetRenderOptions(RenderOptions &Options, QStringList &SoftwareDeinteralacers);
    static QStringList GetAllowedRenderers(MythCodecID CodecId, const QSize &VideoDim);

    explicit VideoOutputOpenGL(const QString &Profile = QString());
    ~VideoOutputOpenGL() override;

    // VideoOutput
    bool Init(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect,
              WId WinId, const QRect &DisplayVisibleRect, MythCodecID CodecId) override;

    void PrepareFrame(VideoFrame *Frame, FrameScanType, OSD *Osd) override;
    void ProcessFrame(VideoFrame *Frame, OSD *Osd,
                      const PIPMap &PiPPlayers, FrameScanType Scan) override;
    void Show(FrameScanType ) override;
    void ClearAfterSeek(void) override;
    bool InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                      float Aspect, MythCodecID CodecId, bool &AspectOnly,
                      MythMultiLocker* Locks, int ReferenceFrames) override;
    void UpdatePauseFrame(int64_t &DisplayTimecode) override;
    void InitPictureAttributes(void) override;
    void EmbedInWidget(const QRect &Rect) override;
    void StopEmbedding(void) override;
    void ShowPIP(VideoFrame *Frame, MythPlayer *PiPPlayer, PIPLocation Location) override;
    void MoveResizeWindow(QRect NewRect) override;
    void RemovePIP(MythPlayer *PiPPlayer) override;
    bool IsPIPSupported(void) const override  { return true; }
    MythPainter *GetOSDPainter(void) override;
    bool CanVisualise(AudioPlayer *Audio, MythRender *Render) override;
    bool SetupVisualisation(AudioPlayer *Audio, MythRender *Render, const QString &Name) override;
    QStringList GetVisualiserList(void) override;
    bool StereoscopicModesAllowed(void) const override { return true; }
    void DoneDisplayingFrame(VideoFrame *Frame) override;
    VideoFrameType* DirectRenderFormats(void) override;

  protected:
    void DestroyBuffers(void);
    bool CreateBuffers(MythCodecID CodecID, QSize Size);
    QRect GetDisplayVisibleRect(void);

    MythRenderOpenGL      *m_render;
    bool                   m_isGLES2;
    OpenGLVideo           *m_openGLVideo;
    QMap<MythPlayer*,OpenGLVideo*> m_openGLVideoPiPs;
    QMap<MythPlayer*,bool> m_openGLVideoPiPsReady;
    OpenGLVideo           *m_openGLVideoPiPActive;
    MythOpenGLPainter     *m_openGLPainter;
    QString                m_videoProfile;
    MythCodecID            m_newCodecId;
    QSize                  m_newVideoDim;
    QSize                  m_newVideoDispDim;
    float                  m_newAspect;
    bool                   m_buffersCreated;

    // performance monitoring (-v gpu)
    MythOpenGLPerf        *m_openGLPerf;
};

#endif
