#ifndef VIDEOOUTPUTNULLVDPAU_H
#define VIDEOOUTPUTNULLVDPAU_H

// MythTV headers
#include "videooutbase.h"
#include "mythrender_vdpau.h"

/**
 * \class VideoOutputNullVDPAU
 * \brief A dummy video class that uses VDPAU to decode video frames.
 *
 * The normal VDPAU frames are held in vbuffers and a 'shadow' set of buffers
 * is held in m_shadowBuffers. These are allocated a buffer in main memory.
 *
 * GetNextFreeFrame() will always return a pointer to a GPU backed buffer.
 *
 * ReleaseFrame() musted be called with a pointer to a GPU buffer and when
 * invoked (i.e. once decoding is finished) the contents of the GPU buffer are
 * copied to the main memory buffer.
 *
 * GetLastDecodedFrame() and GetLastShownFrame() will always return a pointer
 * to the frame in main memory.
 *
 * DoneDisplayingFrame() and DiscardFrame() can be called with either version
 * of the frame.
 *
 * All buffer state tracking is handled in the main vbuffers object.
**/

class VideoOutputNullVDPAU : public VideoOutput
{
  public:
    static void GetRenderOptions(render_opts &opts);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id);
    VideoOutputNullVDPAU();
   ~VideoOutputNullVDPAU();

    virtual bool Init(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float aspect, WId winid,
                      const QRect &win_rect, MythCodecID codec_id);
    virtual void* GetDecoderContext(unsigned char* buf, uint8_t*& id);
    virtual bool InputChanged(const QSize &video_dim_buf,
                              const QSize &video_dim_disp,
                              float        aspect,
                              MythCodecID  av_codec_id,
                              void        *codec_private,
                              bool        &aspect_only);
    virtual bool SetupDeinterlace(bool, const QString &ovrf = "") { return false; }
    virtual bool SetDeinterlacingEnabled(bool)                    { return false; }
    virtual bool ApproveDeintFilter(const QString& filtername) const { return false; }
    virtual void ClearAfterSeek(void);
    virtual void ReleaseFrame(VideoFrame *frame);
    virtual void ProcessFrame(VideoFrame *frame, OSD *osd,
                              FilterChain *filterList,
                              const PIPMap &pipPlayers,
                              FrameScanType scan)         {;}
    virtual void PrepareFrame(VideoFrame *buffer,
                              FrameScanType, OSD *osd)    {;}
    virtual void Show(FrameScanType )                     {;}

    virtual void Zoom(ZoomDirection direction)            {;}
    virtual void EmbedInWidget(const QRect &rect)         {;}
    virtual void StopEmbedding(void)                      {;}
    virtual void DrawUnusedRects(bool sync = true)        {;}
    virtual void UpdatePauseFrame(int64_t &disp_timecode) {;}
    virtual void MoveResizeWindow(QRect )                 {;}
    virtual bool CanVisualise(AudioPlayer *audio, MythRender *render)
        { return false; }
    virtual bool SetupVisualisation(AudioPlayer *audio, MythRender *render,
                                    const QString &name) { return false; }
    virtual MythPainter *GetOSDPainter(void) { return NULL; }
    virtual void DrawSlice(VideoFrame *frame, int x, int y, int w, int h);

    virtual VideoFrame *GetLastDecodedFrame(void);
    virtual VideoFrame *GetLastShownFrame(void);
    virtual void DoneDisplayingFrame(VideoFrame *frame);

  private:
    void TearDown(void);
    bool InitRender(void);
    void DeleteRender(void);
    bool InitBuffers(void);
    void DeleteBuffers(void);
    bool InitShadowBuffers(void);
    void DeleteShadowBuffers(void);
    bool CreateVideoSurfaces(uint num);
    void DeleteVideoSurfaces(void);
    void ClaimVideoSurfaces(void);
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);
    void CheckFrameStates(void);
    bool BufferSizeCheck(void);

  private:
    MythRenderVDPAU *m_render;
    AVVDPAUContext   m_context;
    QMutex           m_lock;
    uint             m_decoder;
    int              m_pix_fmt;
    uint             m_decoder_buffer_size;
    QVector<uint>    m_video_surfaces;
    bool             m_checked_surface_ownership;
    VideoBuffers    *m_shadowBuffers;
    QSize            m_surfaceSize;
};

#endif // VIDEOOUTPUTNULLVDPAU_H
