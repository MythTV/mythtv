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
    VideoOutputOpenGL();
   ~VideoOutputOpenGL();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh,
              MythCodecID codec_id, WId embedid = 0);
    void TearDown(void);

    void PrepareFrame(VideoFrame *buffer, FrameScanType, OSD *osd);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void Show(FrameScanType );
    bool InputChanged(const QSize &input_size, float aspect,
                      MythCodecID  av_codec_id, void *codec_private,
                      bool &aspect_only);
    void UpdatePauseFrame(void);
    void DrawUnusedRects(bool) { }
    void Zoom(ZoomDirection direction);
    void MoveResize(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                           const QSize &video_dim);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    bool SetDeinterlacingEnabled(bool);
    bool SetupDeinterlace(bool i, const QString& ovrf="");
    void ShowPIP(VideoFrame        *frame,
                 NuppelVideoPlayer *pipplayer,
                 PIPLocation        loc);
    void MoveResizeWindow(QRect new_rect);

    virtual void RemovePIP(NuppelVideoPlayer *pipplayer);
    virtual bool IsPIPSupported(void) const   { return true; }
    virtual bool hasFullScreenOSD(void) const { return true; }
    virtual bool IsSyncLocked(void) const     { return true; }
    virtual bool ApproveDeintFilter(const QString& filtername) const;
    virtual MythPainter *GetOSDPainter(void)  { return (MythPainter*)gl_painter; }

  private:
    bool CreateBuffers(void);
    bool SetupContext(void);
    bool SetupOpenGL(void);
    void InitOSD(void);

    QMutex            gl_context_lock;
    MythRenderOpenGL *gl_context;
    OpenGLVideo      *gl_videochain;
    QMap<NuppelVideoPlayer*,OpenGLVideo*> gl_pipchains;
    QMap<NuppelVideoPlayer*,bool>         gl_pip_ready;
    OpenGLVideo      *gl_pipchain_active;
    WId               gl_parent_win;
    WId               gl_embed_win;
    VideoFrame        av_pause_frame;

    MythOpenGLPainter *gl_painter;
};

#endif
