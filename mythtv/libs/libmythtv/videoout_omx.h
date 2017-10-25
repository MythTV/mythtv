#ifndef VIDEOOUT_OMX_H
#define VIDEOOUT_OMX_H

#ifdef USING_OPENGLES
#define OSD_EGL // OSD with EGL
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
    virtual bool Init(const QSize&, const QSize&, float, WId, const QRect&, MythCodecID);
    virtual bool InputChanged(const QSize&, const QSize&, float, MythCodecID, void*, bool&);
    virtual void Zoom(ZoomDirection);
    virtual void EmbedInWidget(const QRect&);
    virtual void StopEmbedding(void);
    virtual bool ApproveDeintFilter(const QString&) const;
    virtual bool SetDeinterlacingEnabled(bool interlaced);
    virtual bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    virtual QString GetName(void) const { return kName; } // = "openmax"
    virtual bool IsPIPSupported(void) const { return true; }
    virtual bool IsPBPSupported(void) const { return true; }
    virtual QRect GetPIPRect(PIPLocation, MythPlayer* = NULL, bool = true) const;
    virtual MythPainter *GetOSDPainter(void);

    // VideoOutput implementation
    virtual void PrepareFrame(VideoFrame*, FrameScanType, OSD*);
    virtual void ProcessFrame(VideoFrame*, OSD*, FilterChain*, const PIPMap&, FrameScanType);
    virtual void Show(FrameScanType);
    virtual void MoveResizeWindow(QRect);
    virtual void DrawUnusedRects(bool);
    virtual void UpdatePauseFrame(int64_t &);

  protected:
    // VideoOutput overrides
    virtual bool hasFullScreenOSD(void) const;
    virtual bool DisplayOSD(VideoFrame *frame, OSD *osd);
    virtual bool CanVisualise(AudioPlayer*, MythRender*);
    virtual bool SetupVisualisation(AudioPlayer*, MythRender*, const QString&);
    virtual QStringList GetVisualiserList(void);

  private:
    // OMXComponentCtx implementation
    virtual OMX_ERRORTYPE EmptyBufferDone(OMXComponent&, OMX_BUFFERHEADERTYPE*);
    virtual void ReleaseBuffers(OMXComponent&);

  private:
    // OMXComponentCB actions
    typedef OMX_ERRORTYPE ComponentCB();
    ComponentCB FreeBuffersCB, UseBuffersCB;

    void CreatePauseFrame(void);
    bool SetVideoRect(const QRect &disp_rect, const QRect &vid_rect);
    bool CreateBuffers(const QSize&, const QSize&, WId = 0);
    void DeleteBuffers();
    bool Start();
    OMX_ERRORTYPE SetImageFilter(OMX_IMAGEFILTERTYPE);

  private:
    OMXComponent m_render, m_imagefx;
    VideoFrame av_pause_frame;
    QRect m_disp_rect, m_vid_rect;
    QVector<void*> m_bufs;
    MythRenderEGL *m_context;
#ifdef OSD_EGL   
    MythOpenGLPainter *m_osdpainter;
    MythPainter *m_threaded_osdpainter;
#endif
    MythScreenType *m_backgroundscreen;
    GlOsdThread *m_glOsdThread;
    bool m_changed;
    bool m_videoPaused;
};

#endif // ndef VIDEOOUT_OMX_H
