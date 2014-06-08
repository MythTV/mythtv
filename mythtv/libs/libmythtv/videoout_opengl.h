#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV headers
#include "videooutbase.h"
#include "openglvideo.h"
#include "mythpainter_ogl.h"

class VideoOutputOpenGL : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    VideoOutputOpenGL(const QString &profile = QString());
    virtual ~VideoOutputOpenGL();

    virtual bool Init(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float aspect,
                      WId winid, const QRect &win_rect, MythCodecID codec_id);
    virtual void SetProfile(void);
    virtual void TearDown(void);

    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd);
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              const PIPMap &pipPlayers,
                              FrameScanType scan);
    virtual void Show(FrameScanType );
    virtual bool InputChanged(const QSize &video_dim_buf,
                              const QSize &video_dim_disp,
                              float aspect,
                              MythCodecID  av_codec_id, void *codec_private,
                              bool &aspect_only);
    virtual void UpdatePauseFrame(int64_t &disp_timecode);
    virtual void DrawUnusedRects(bool) { } // VideoOutput
    void Zoom(ZoomDirection direction);
    void MoveResize(void);
    virtual int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    virtual void InitPictureAttributes(void);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    void EmbedInWidget(const QRect &rect);
    void StopEmbedding(void);
    virtual bool SetDeinterlacingEnabled(bool);
    virtual bool SetupDeinterlace(bool i, const QString& ovrf="");
    void ShowPIP(VideoFrame  *frame,
                 MythPlayer  *pipplayer,
                 PIPLocation  loc);
    void MoveResizeWindow(QRect new_rect);

    virtual void RemovePIP(MythPlayer *pipplayer);
    virtual bool IsPIPSupported(void) const   { return true; }
    virtual bool hasFullScreenOSD(void) const { return true; }
    virtual bool ApproveDeintFilter(const QString& filtername) const;
    virtual MythPainter *GetOSDPainter(void)  { return gl_painter; }

    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render)
        { return VideoOutput::CanVisualise(audio, gl_context);       }
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name)
        { return VideoOutput::SetupVisualisation(audio, gl_context, name); }
    virtual QStringList GetVisualiserList(void);

    virtual bool StereoscopicModesAllowed(void) const { return true; }

  protected:
    bool CreateCPUResources(void);
    bool CreateGPUResources(void);
    bool CreateVideoResources(void);
    void DestroyCPUResources(void);
    void DestroyVideoResources(void);
    void DestroyGPUResources(void);
    virtual bool CreateBuffers(void);
    bool CreatePauseFrame(void);
    virtual bool SetupContext(void);
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
    bool               gl_opengl_lite;
};

#endif
