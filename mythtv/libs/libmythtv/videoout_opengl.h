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
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    explicit VideoOutputOpenGL(const QString &profile = QString());
    virtual ~VideoOutputOpenGL();

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect,
              WId winid, const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    virtual void SetProfile(void);
    virtual void TearDown(void);

    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd) override; // VideoOutput
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan) override; // VideoOutput
    virtual void Show(FrameScanType ) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float aspect,
                      MythCodecID  av_codec_id, void *codec_private,
                      bool &aspect_only) override; // VideoOutput
    void UpdatePauseFrame(int64_t &disp_timecode) override; // VideoOutput
    void DrawUnusedRects(bool) override { } // VideoOutput
    void Zoom(ZoomDirection direction) override; // VideoOutput
    void MoveResize(void) override; // VideoOutput
    int  SetPictureAttribute(PictureAttribute attribute, int newValue) override; // VideoOutput
    void InitPictureAttributes(void) override; // VideoOutput
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    void EmbedInWidget(const QRect &rect) override; // VideoOutput
    void StopEmbedding(void) override; // VideoOutput
    bool SetDeinterlacingEnabled(bool) override; // VideoOutput
    bool SetupDeinterlace(bool interlaced,
                          const QString& overridefilter="") override; // VideoOutput
    void ShowPIP(VideoFrame  *frame,
                 MythPlayer  *pipplayer,
                 PIPLocation  loc) override; // VideoOutput
    void MoveResizeWindow(QRect new_rect) override; // VideoOutput

    void RemovePIP(MythPlayer *pipplayer) override; // VideoOutput
    bool IsPIPSupported(void) const override  { return true; } //VideoOutput
    bool hasFullScreenOSD(void) const override { return true; } // VideoOutput
    bool ApproveDeintFilter(const QString& filtername) const override; // VideoOutput
    MythPainter *GetOSDPainter(void) override; // VideoOutput

    bool CanVisualise(AudioPlayer *audio, MythRender *render) override; // VideoOutput
    bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                            const QString &name) override; // VideoOutput
    QStringList GetVisualiserList(void) override; // VideoOutput

    bool StereoscopicModesAllowed(void) const override { return true; } // VideoOutput

  protected:
    bool CreateCPUResources(void);
    bool CreateGPUResources(void);
    bool CreateVideoResources(void);
    void DestroyCPUResources(void);
    void DestroyVideoResources(void);
    void DestroyGPUResources(void);
    virtual bool CreateBuffers(void);
    bool CreatePauseFrame(void);
    bool SetupContext(void);
    bool SetupOpenGL(void);
    void CreatePainter(void);

    QMutex            gl_context_lock;
    MythRenderOpenGL *gl_context;
    bool              gl_valid;
    OpenGLVideo      *gl_videochain;
    QMap<MythPlayer*,OpenGLVideo*> gl_pipchains;
    QMap<MythPlayer*,bool>         gl_pip_ready;
    OpenGLVideo      *gl_pipchain_active;
    WId               gl_parent_win;
    VideoFrame        av_pause_frame;

    MythOpenGLPainter *gl_painter;
    bool               gl_created_painter;
    QString            gl_opengl_profile;
    OpenGLVideo::VideoType gl_opengl_type;
};

#endif
