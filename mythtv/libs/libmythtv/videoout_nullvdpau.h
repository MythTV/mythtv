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

    bool Init(const QSize &video_dim_buf,
              const QSize &video_dim_disp,
              float aspect, WId winid,
              const QRect &win_rect, MythCodecID codec_id) override; // VideoOutput
    void* GetDecoderContext(unsigned char* buf, uint8_t*& id) override; // VideoOutput
    bool InputChanged(const QSize &video_dim_buf,
                      const QSize &video_dim_disp,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private,
                      bool        &aspect_only) override; // VideoOutput
    bool SetupDeinterlace(bool, const QString & = "") override // VideoOutput
        { return false; }
    bool SetDeinterlacingEnabled(bool) override // VideoOutput
        { return false; }
    bool ApproveDeintFilter(const QString&) const override // VideoOutput
        { return false; }
    void ClearAfterSeek(void) override; // VideoOutput
    void ReleaseFrame(VideoFrame *frame) override; // VideoOutput
    void ProcessFrame(VideoFrame *, OSD *,
                      FilterChain *,
                      const PIPMap &,
                      FrameScanType) override {;} // VideoOutput
    void PrepareFrame(VideoFrame *,
                      FrameScanType, OSD *) override      {;} // VideoOutput
    void Show(FrameScanType ) override                    {;} // VideoOutput

    void Zoom(ZoomDirection) override                     {;} // VideoOutput
    void EmbedInWidget(const QRect &) override            {;} // VideoOutput
    void StopEmbedding(void) override                     {;} // VideoOutput
    void DrawUnusedRects(bool = true) override            {;} // VideoOutput
    void UpdatePauseFrame(int64_t &) override             {;} // VideoOutput
    void MoveResizeWindow(QRect ) override                {;} // VideoOutput
    bool CanVisualise(AudioPlayer *, MythRender *) override // VideoOutput
        { return false; }
    bool SetupVisualisation(AudioPlayer *, MythRender *,
                            const QString &) override // VideoOutput
        { return false; }
    MythPainter *GetOSDPainter(void) override // VideoOutput
        { return nullptr; }
    void DrawSlice(VideoFrame *frame, int x, int y, int w, int h) override; // VideoOutput

    VideoFrame *GetLastDecodedFrame(void) override; // VideoOutput
    VideoFrame *GetLastShownFrame(void) override; // VideoOutput
    void DoneDisplayingFrame(VideoFrame *frame) override; // VideoOutput

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
    void DiscardFrame(VideoFrame*) override; // VideoOutput
    void DiscardFrames(bool next_frame_keyframe) override; // VideoOutput
    void CheckFrameStates(void) override; // VideoOutput
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
