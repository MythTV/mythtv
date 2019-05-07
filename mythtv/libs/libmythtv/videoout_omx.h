#ifndef VIDEOOUT_OMX_H
#define VIDEOOUT_OMX_H

#ifdef USING_OPENGLES
#ifdef USING_BROADCOM
#define OSD_EGL // OSD with EGL
#endif
#endif

#include <OMX_Types.h>
#include <OMX_Core.h>

#include <QStringList>
#include <QVector>
#include <QRect>

#include "videooutbase.h"
#include "omxcontext.h"

class MythRenderEGL;
class GlOsdThread;
#ifdef OSD_EGL
class MythOpenGLPainter;
#endif
class MythScreenType;

class VideoOutputOMX : public VideoOutput, private OMXComponentCtx
{
  public:
    static QString const kName; // ="openmax"
    static void GetRenderOptions(render_opts &opts, QStringList &cpudeints);
    static QStringList GetAllowedRenderers(MythCodecID, const QSize&);

    VideoOutputOMX();
    virtual ~VideoOutputOMX();

    // VideoOutput overrides
    bool Init(const QSize&, const QSize&, float, WId, const QRect&, MythCodecID) override;
    bool InputChanged(const QSize&, const QSize&, float, MythCodecID, void*, bool&) override;
    void Zoom(ZoomDirection) override;
    void EmbedInWidget(const QRect&) override;
    void StopEmbedding(void) override;
    bool ApproveDeintFilter(const QString&) const override;
    bool SetDeinterlacingEnabled(bool interlaced) override;
    bool SetupDeinterlace(bool interlaced, const QString& overridefilter="") override;
    QString GetName(void) const override { return kName; } // = "openmax"
    bool IsPIPSupported(void) const override { return true; }
    bool IsPBPSupported(void) const override { return true; }
    QRect GetPIPRect(PIPLocation, MythPlayer* = nullptr, bool = true) const override;
    MythPainter *GetOSDPainter(void) override;

    // VideoOutput implementation
    void PrepareFrame(VideoFrame*, FrameScanType, OSD*) override;
    void ProcessFrame(VideoFrame*, OSD*, FilterChain*, const PIPMap&, FrameScanType) override;
    void Show(FrameScanType) override;
    void MoveResizeWindow(QRect) override;
    void DrawUnusedRects(bool) override;
    void UpdatePauseFrame(int64_t &) override;

  protected:
    // VideoOutput overrides
    bool hasFullScreenOSD(void) const override;
    bool DisplayOSD(VideoFrame *frame, OSD *osd) override;
    bool CanVisualise(AudioPlayer*, MythRender*) override;
    bool SetupVisualisation(AudioPlayer*, MythRender*, const QString&) override;
    QStringList GetVisualiserList(void) override;

  private:
    // OMXComponentCtx implementation
    OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*) override;
    void ReleaseBuffers(OMXComponent&) override;

  private:
    // OMXComponentCB actions
    typedef OMX_ERRORTYPE ComponentCB();
    ComponentCB FreeBuffersCB, UseBuffersCB;

    void CreatePauseFrame(void);
    bool SetVideoRect(const QRect &d_rect, const QRect &vid_rect);
    bool CreateBuffers(const QSize&, const QSize&);
    void DeleteBuffers();
    bool Start();
    OMX_ERRORTYPE SetImageFilter(OMX_IMAGEFILTERTYPE);

  private:
    OMXComponent m_render, m_imagefx;
    VideoFrame av_pause_frame;
    QRect m_disp_rect, m_vid_rect;
    QVector<void*> m_bufs;
#ifdef OSD_EGL
    MythRenderEGL *m_context;
    MythOpenGLPainter *m_osdpainter;
    MythPainter *m_threaded_osdpainter;
    GlOsdThread *m_glOsdThread;
    bool m_changed;
#endif
    MythScreenType *m_backgroundscreen;
    bool m_videoPaused;
};

#endif // ndef VIDEOOUT_OMX_H
