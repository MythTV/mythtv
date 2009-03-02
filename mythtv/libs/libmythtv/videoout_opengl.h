#ifndef VIDEOOUT_OPENGL_H_
#define VIDEOOUT_OPENGL_H_

// MythTV headers
#include "videooutbase.h"
#include "openglvideo.h"
#include "openglcontext.h"
#include "DisplayRes.h"

class VideoOutputOpenGL : public VideoOutput
{
  public:
    VideoOutputOpenGL();
   ~VideoOutputOpenGL();

    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    void TearDown(void);

    void PrepareFrame(VideoFrame *buffer, FrameScanType);
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers);
    void Show(FrameScanType );
    bool InputChanged(const QSize &input_size, float aspect,
                      MythCodecID  av_codec_id, void *codec_private);
    int  GetRefreshRate(void);
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
    int  DisplayOSD(VideoFrame *frame, OSD *osd,
                    int stride = -1, int revision = -1);
    void ShowPIP(VideoFrame        *frame,
                 NuppelVideoPlayer *pipplayer,
                 PIPLocation        loc);
    void ResizeForGui(void);
    void ResizeForVideo(void);

    virtual void RemovePIP(NuppelVideoPlayer *pipplayer);
    virtual bool hasOpenGLAcceleration(void) const { return true; }
    virtual bool hasFullScreenOSD(void) const { return gl_osd; }
    virtual void ShutdownVideoResize(void);
    virtual bool ApproveDeintFilter(const QString& filtername) const;

  private:
    bool CreateBuffers(void);
    bool SetupContext(void);
    bool SetupOpenGL(void);
    void InitOSD(void);
    void InitDisplayMeasurements(void);

    QMutex         gl_context_lock;
    OpenGLContext *gl_context;
    OpenGLVideo   *gl_videochain;
    OpenGLVideo   *gl_osdchain;
    QMap<NuppelVideoPlayer*,OpenGLVideo*> gl_pipchains;
    QMap<NuppelVideoPlayer*,bool>         gl_pip_ready;
    OpenGLVideo   *gl_pipchain_active;
    bool           gl_osd;
    bool           gl_osd_ready;
    WId            gl_parent_win;
    WId            gl_embed_win;
    DisplayRes    *display_res;
    VideoFrame     av_pause_frame;
};

#endif
