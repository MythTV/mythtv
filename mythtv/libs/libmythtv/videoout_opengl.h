#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV headers
#include "videooutbase.h"
#include "openglvideo.h"

class MythRenderOpenGL;
class MythOpenGLPainter;

class VideoOutputOpenGL : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &Options, QStringList &SoftwareDeinteralacers);
    static QStringList GetAllowedRenderers(MythCodecID CodecId, const QSize &VideoDim);

    explicit VideoOutputOpenGL(const QString &Profile = QString());
    virtual ~VideoOutputOpenGL() override;

    virtual void SetProfile(void);
    virtual void TearDown(void);

    // VideoOutput
    bool Init(const QSize &VideoDim, const QSize &VideoDispDim, float Aspect,
              WId WinId, const QRect &DisplayVisibleRect, MythCodecID CodecId) override;

    void PrepareFrame(VideoFrame *Frame, FrameScanType, OSD *Osd) override;
    void ProcessFrame(VideoFrame *Frame, OSD *Osd, FilterChain *FilterList,
                      const PIPMap &PiPPlayers, FrameScanType Scan) override;
    virtual void Show(FrameScanType ) override;
    bool InputChanged(const QSize &VideoDim, const QSize &VideoDispDim,
                      float Aspect, MythCodecID CodecId, bool &AspectOnly) override;
    void UpdatePauseFrame(int64_t &DisplayTimecode) override;
    void DrawUnusedRects(bool) override { }
    void Zoom(ZoomDirection Direction) override;
    void MoveResize(void) override;
    void InitPictureAttributes(void) override;
    void EmbedInWidget(const QRect &Rect) override;
    void StopEmbedding(void) override;
    bool SetDeinterlacingEnabled(bool Enable) override;
    bool SetupDeinterlace(bool Interlaced, const QString &OverrideFilter = QString()) override;
    void ShowPIP(VideoFrame *Frame, MythPlayer *PiPPlayer, PIPLocation Location) override;
    void MoveResizeWindow(QRect NewRect) override;
    void RemovePIP(MythPlayer *PiPPlayer) override;
    bool IsPIPSupported(void) const override  { return true; }
    bool hasFullScreenOSD(void) const override { return true; }
    bool ApproveDeintFilter(const QString& Deinterlacer) const override;
    MythPainter *GetOSDPainter(void) override;
    bool CanVisualise(AudioPlayer *Audio, MythRender *Render) override;
    bool SetupVisualisation(AudioPlayer *Audio, MythRender *Render, const QString &Name) override;
    QStringList GetVisualiserList(void) override;
    bool StereoscopicModesAllowed(void) const override { return true; }
    void DoneDisplayingFrame(VideoFrame *Frame) override;

  protected:
    bool CreateGPUResources(void);
    bool CreateVideoResources(void);
    void DestroyCPUResources(void);
    void DestroyVideoResources(void);
    void DestroyGPUResources(void);
    bool CreateBuffers(void);
    bool SetupContext(void);
    bool SetupOpenGL(void);
    void CreatePainter(void);

    MythRenderOpenGL      *m_render;
    bool                   m_renderValid;
    OpenGLVideo           *m_openGLVideo;
    QMap<MythPlayer*,OpenGLVideo*> m_openGLVideoPiPs;
    QMap<MythPlayer*,bool> m_openGLVideoPiPsReady;
    OpenGLVideo           *m_openGLVideoPiPActive;
    WId                    m_window;
    VideoFrame             m_pauseFrame;
    MythOpenGLPainter     *m_openGLPainter;
    QString                m_videoProfile;
    OpenGLVideo::FrameType m_openGLVideoType;
};

#endif
