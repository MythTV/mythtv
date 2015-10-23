#ifndef VIDEOOUT_OMX_H
#define VIDEOOUT_OMX_H

#include <IL/OMX_Types.h>
#include <IL/OMX_Core.h>

#include <QStringList>
#include <QVector>
#include <QRect>

#include "videooutbase.h"
#include "omxcontext.h"

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

    // VideoOutput implementation
    virtual void PrepareFrame(VideoFrame*, FrameScanType, OSD*);
    virtual void ProcessFrame(VideoFrame*, OSD*, FilterChain*, const PIPMap&, FrameScanType);
    virtual void Show(FrameScanType);
    virtual void MoveResizeWindow(QRect);
    virtual void DrawUnusedRects(bool);
    virtual void UpdatePauseFrame(int64_t &);

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
};

#endif // ndef VIDEOOUT_OMX_H
