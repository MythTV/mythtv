#ifndef _VDPAUVIDEODECODER_H
#define _VDPAUVIDEODECODER_H

#include <QMutex>
#include <QHash>
#include <QList>

#include "mythxdisplay.h"
#include "videodecoder.h"
#include "videoouttypes.h"
#include "openclinterface.h"
#include "videosurface.h"

extern "C" {
#include "libavcodec/vdpau.h" 
#include "libavcodec/avcodec.h"
}

typedef QHash<VdpVideoSurface, uint> VDPAUSurfaceRevHash;

class VDPAUVideoDecoder : public VideoDecoder
{
  public:
    VDPAUVideoDecoder(OpenCLDevice *dev) : VideoDecoder(dev, false),
        m_display(NULL), m_errorState(kError_None), m_pixFmt(PIX_FMT_NONE),
        m_processBufferSize(0), m_decoderInit(false) {};

    ~VDPAUVideoDecoder() { Shutdown(); };
    const char *Name(void) { return "VDPAU"; };
    bool Initialize(void);
    VideoSurface *DecodeFrame(AVFrame *frame);
    void DiscardFrame(VideoSurface *frame);
    void Shutdown(void);
    void SetCodec(AVCodec *codec);
    bool H264DecoderSizeSupported(void);
    void DrawSlice(VideoSurface *surface, int x, int y, int w, int h);
    uint CreateVideoSurface(uint32_t width, uint32_t height,
                            VdpChromaType type = VDP_CHROMA_TYPE_420);
    bool CreateDecoder(uint32_t width, uint32_t height,
                       VdpDecoderProfile profile, uint references);
    bool Decode(struct vdpau_render_state *render);
    bool IsErrored() const { return m_errorState != kError_None; };

    MythXDisplay *m_display;
    VdpDevice m_device;
    VdpDecoder m_decoder;
    VdpDecoderProfile m_profile;

    VideoErrorState m_errorState;
    PixelFormat m_pixFmt;

    VDPAUSurfaceRevHash m_reverseSurfaces;

    QMutex m_lock;
    QMutex m_decodeLock;

    int m_processBufferSize;

    bool m_accelMpeg4;
    bool m_accelH264;
    bool m_decoderInit;

    VdpGetProcAddress *vdp_get_proc_address;
    VdpGetErrorString *vdp_get_error_string;
    VdpDeviceDestroy *vdp_device_destroy;
    VdpVideoSurfaceCreate *vdp_video_surface_create;
    VdpVideoSurfaceDestroy *vdp_video_surface_destroy;
    VdpDecoderCreate *vdp_decoder_create;
    VdpDecoderDestroy *vdp_decoder_destroy;
    VdpDecoderRender *vdp_decoder_render;
    VdpDecoderQueryCapabilities *vdp_decoder_query_capabilities;
    VdpGetApiVersion *vdp_get_api_version;
    VdpGetInformationString *vdp_get_information_string;
};

int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void release_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic);
void render_slice_vdpau(struct AVCodecContext *s, const AVFrame *src,
                        int offset[4], int y, int type, int height);
enum PixelFormat get_format_vdpau(struct AVCodecContext *avctx,
                                  const enum PixelFormat *fmt);

#endif

/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
