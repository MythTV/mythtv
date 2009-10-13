#ifndef VIDEOOUT_VDPAU_H
#define VIDEOOUT_VDPAU_H

// MythTV headers
#include "videooutbase.h"
#include "util-vdpau.h"
#include "mythxdisplay.h"

class VDPAUContext;
class VideoOutputVDPAU : public VideoOutput
{
  public:
    VideoOutputVDPAU(MythCodecID codec_id);
    ~VideoOutputVDPAU();
    bool Init(int width, int height, float aspect, WId winid,
              int winx, int winy, int winw, int winh, WId embedid = 0);
    bool SetDeinterlacingEnabled(bool interlaced);
    bool SetupDeinterlace(bool interlaced, const QString& ovrf="");
    bool ApproveDeintFilter(const QString& filtername) const;
    void ProcessFrame(VideoFrame *frame, OSD *osd,
                      FilterChain *filterList,
                      const PIPMap &pipPlayers,
                      FrameScanType scan);
    void PrepareFrame(VideoFrame*, FrameScanType);
    void DrawSlice(VideoFrame*, int x, int y, int w, int h);
    void Show(FrameScanType);
    void ClearAfterSeek(void);
    bool InputChanged(const QSize &input_size,
                      float        aspect,
                      MythCodecID  av_codec_id,
                      void        *codec_private);
    void Zoom(ZoomDirection direction);
    void VideoAspectRatioChanged(float aspect);
    void EmbedInWidget(int x, int y, int w, int h);
    void StopEmbedding(void);
    void MoveResizeWindow(QRect new_rect);
    void DrawUnusedRects(bool sync = true);
    void UpdatePauseFrame(void);
    int  SetPictureAttribute(PictureAttribute attribute, int newValue);
    void InitPictureAttributes(void);
    static QStringList GetAllowedRenderers(MythCodecID myth_codec_id,
                                    const QSize &video_dim);
    static MythCodecID GetBestSupportedCodec(uint width,        uint height,
                                      uint stream_type,  bool no_acceleration);
    DisplayInfo GetDisplayInfo(void);
    virtual bool IsPIPSupported(void) const { return true; }
    virtual bool IsPBPSupported(void) const { return false; }
    virtual bool NeedExtraAudioDecode(void) const
        { return codec_is_vdpau(m_codec_id); }
    virtual bool hasHWAcceleration(void) const
        { return codec_is_vdpau(m_codec_id); }
    virtual bool IsSyncLocked(void) const { return true; }
    void SetNextFrameDisplayTimeOffset(int delayus);

  private:
    virtual bool hasFullScreenOSD(void) const { return true; }
    void TearDown(void);
    bool InitContext(void);
    void DeleteContext(void);
    bool InitBuffers(void);
    void DeleteBuffers(void);
    bool InitXDisplay(WId wid);
    void DeleteXDisplay(void);
    void DiscardFrame(VideoFrame*);
    void DiscardFrames(bool next_frame_keyframe);
    void DoneDisplayingFrame(VideoFrame *frame);
    void CheckFrameStates(void);
    virtual int DisplayOSD(VideoFrame *frame, OSD *osd,
                           int stride = -1, int revision = -1);
    virtual void ShowPIP(VideoFrame        *frame,
                         NuppelVideoPlayer *pipplayer,
                         PIPLocation        loc);
    virtual void RemovePIP(NuppelVideoPlayer *pipplayer);
    void ParseBufferSize(void);

    MythCodecID          m_codec_id;
    Window               m_win;
    MythXDisplay        *m_disp;
    VDPAUContext        *m_ctx;
    int                  m_colorkey;
    QMutex               m_lock;
    bool                 m_osd_avail;
    bool                 m_pip_avail;
    uint                 m_buffer_size;
};

#endif // VIDEOOUT_VDPAU_H

