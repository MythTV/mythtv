#ifndef MYTHRENDER_VDPAU_H_
#define MYTHRENDER_VDPAU_H_

#include <QMutex>
#include <QRect>
#include <QHash>

#include "mythuiexp.h"
#include "mythimage.h"
#include "mythxdisplay.h"
#include "mythrender_base.h"

extern "C" {
#include "libavcodec/vdpau.h"
#include "vdpau/vdpau_x11.h"
}


/**
 * Copied from earlier version of FFmpeg vdpau.h
 * @brief This structure is used as a callback between the FFmpeg
 * decoder (vd_) and presentation (vo_) module.
 * This is used for defining a video frame containing surface,
 * picture parameter, bitstream information etc which are passed
 * between the FFmpeg decoder and its clients.
 */
 union AVVDPAUPictureInfo {
    VdpPictureInfoH264        h264;
    VdpPictureInfoMPEG1Or2    mpeg;
    VdpPictureInfoVC1          vc1;
    VdpPictureInfoMPEG4Part2 mpeg4;
};

struct vdpau_render_state {
    VdpVideoSurface surface; ///< Used as rendered surface, never changed.

    int state; ///< Holds FF_VDPAU_STATE_* values.

    /** picture parameter information for all supported codecs */
    union AVVDPAUPictureInfo info;

    /** Describe size/location of the compressed video data.
        Set to 0 when freeing bitstream_buffers. */
    int bitstream_buffers_allocated;
    int bitstream_buffers_used;
    /** The user is responsible for freeing this buffer using av_freep(). */
    VdpBitstreamBuffer *bitstream_buffers;
};
#define FF_VDPAU_STATE_USED_FOR_REFERENCE 2

/* End of definitions copied from old VDPAU */

#define MIN_OUTPUT_SURFACES  2 // UI
#define MAX_OUTPUT_SURFACES  4 // Video
#define NUM_REFERENCE_FRAMES 3
#define VDPAU_COLORKEY       0x020202

typedef enum
{
    kVDPAttribNone           = 0x000,
    kVDPAttribBackground     = 0x001,
    kVDPAttribSkipChroma     = 0x002,
    kVDPAttribCSCEnd         = kVDPAttribSkipChroma,
    kVDPAttribFiltersStart   = 0x100,
    kVDPAttribNoiseReduction = kVDPAttribFiltersStart,
    kVDPAttribSharpness      = 0x200,
} VDPAUAttributes;

typedef enum
{
    kVDPFeatNone      = 0x00,
    kVDPFeatTemporal  = 0x01,
    kVDPFeatSpatial   = 0x02,
    kVDPFeatIVTC      = 0x04,
    kVDPFeatDenoise   = 0x08,
    kVDPFeatSharpness = 0x10,
    kVDPFeatHQScaling = 0x20,
} VDPAUFeatures;

typedef enum
{
    kVDPBlendNormal = 0,
    kVDPBlendPiP    = 1,
    kVDPBlendNull   = 2,
} VDPBlendType;

class VDPAUOutputSurface;
class VDPAUVideoSurface;
class VDPAUBitmapSurface;
class VDPAUDecoder;
class VDPAUVideoMixer;
class VDPAULayer;

class MUI_PUBLIC MythRenderVDPAU : public MythRender
{
  public:
    static bool gVDPAUSupportChecked;
    static uint gVDPAUBestScaling;
    static bool gVDPAUMPEG4Accel;
    static bool gVDPAUHEVCAccel;
    static bool gVDPAUNVIDIA;
    static bool IsVDPAUAvailable(void);
    static bool IsMPEG4Available(void);
    static bool IsHEVCAvailable(void);
    static bool H264DecoderSizeSupported(uint width, uint height);
    bool        CreateDummy(void);

    MythRenderVDPAU();

    uint GetColorKey(void)         { return m_colorKey;  }
    void SetPreempted(void)        { m_preempted = true; }

    bool Create(const QSize &size, WId window, uint colorkey = VDPAU_COLORKEY);
    bool CreateDecodeOnly(void);
    bool WasPreempted(void);
    bool SetColorKey(uint color);
    void WaitForFlip(void);
    void Flip(void);
    void SyncDisplay(void);
    void DrawDisplayRect(const QRect &rect, bool use_colorkey = false);
    void MoveResizeWin(QRect &rect);
    void CheckOutputSurfaces(void);
    bool GetScreenShot(int width = 0, int height = 0, QString filename = "");

    uint CreateOutputSurface(const QSize &size,
                             VdpRGBAFormat fmt = VDP_RGBA_FORMAT_B8G8R8A8,
                             uint existing = 0);
    uint CreateVideoSurface(const QSize &size,
                            VdpChromaType type = VDP_CHROMA_TYPE_420,
                            uint existing = 0);
    uint CreateBitmapSurface(const QSize &size,
                             VdpRGBAFormat fmt = VDP_RGBA_FORMAT_B8G8R8A8,
                             uint existing = 0);
    uint CreateDecoder(const QSize &size, VdpDecoderProfile profile,
                       uint references, uint existing = 0);
    uint CreateVideoMixer(const QSize &size, uint layers,
                          uint features,
                          VdpChromaType type = VDP_CHROMA_TYPE_420,
                          uint existing = 0);
    uint CreateLayer(uint surface, const QRect *src = nullptr,
                     const QRect *dst = nullptr);

    void DestroyOutputSurface(uint id);
    void DestroyVideoSurface(uint id);
    void DestroyBitmapSurface(uint id);
    void DestroyDecoder(uint id);
    void DestroyVideoMixer(uint id);
    void DestroyLayer(uint id);

    bool MixAndRend(uint id, VdpVideoMixerPictureStructure field,
                    uint vid_surface, uint out_surface,
                    const QVector<uint>* refs, bool top, QRect src,
                    const QRect &dst, QRect dst_vid,
                    uint layer1 = 0, uint layer2 = 0);
    bool SetDeinterlacing(uint id, uint deinterlacers = kVDPFeatNone);
    bool ChangeVideoMixerFeatures(uint id, uint features);
    int  SetMixerAttribute(uint id, uint attrib, int value);
    bool SetMixerAttribute(uint id, uint attrib, float value);
    void SetCSCMatrix(uint id, void* vals);

    bool UploadBitmap(uint id, void* const plane[1], uint32_t pitch[1]);
    bool UploadMythImage(uint id, MythImage *image);
    bool UploadYUVFrame(uint id, void* const planes[3], uint32_t pitches[3]);
    bool DownloadYUVFrame(uint id, void* const planes[3], uint32_t pitches[3]);
    bool DrawBitmap(uint id, uint target, const QRect *src,
                    const QRect *dst, VDPBlendType blendi = kVDPBlendNormal,
                    int alpha = 0, int red = 0, int green = 0, int blue = 0);
    bool DrawLayer(uint id, uint target);

    int   GetBitmapSize(uint id);
    void* GetRender(uint id);
    uint  GetSurfaceOwner(VdpVideoSurface surface);
    QSize GetSurfaceSize(uint id);
    void  ClearVideoSurface(uint id);
    void  ChangeVideoSurfaceOwner(uint id);

    void  Decode(uint id, struct vdpau_render_state *render);
    void  Decode(uint id, struct vdpau_render_state *render,
                 const VdpPictureInfo *info);
    void  SetVideoFlip(void) { m_flipFrames = true; }
    void BindContext(AVCodecContext *avctx);

  private:
    virtual ~MythRenderVDPAU();
    bool CreateDevice(void);
    bool GetProcs(void);
    bool CreatePresentationQueue(void);
    bool CreatePresentationSurfaces(void);
    bool RegisterCallback(bool enable = true);
    bool CheckHardwareSupport(void);
    bool IsFeatureAvailable(uint feature);

    void Destroy(void);
    void DestroyDevice(void);
    void ResetProcs(void);
    void DestroyPresentationQueue(void);
    void DestroyPresentationSurfaces(void);
    void DestroyOutputSurfaces(void);
    void DestroyVideoSurfaces(void);
    void DestroyBitmapSurfaces(void);
    void DestroyDecoders(void);
    void DestroyVideoMixers(void);
    void DestroyLayers(void);

    bool SetMixerAttribute(uint id, VdpVideoMixerAttribute attribute[1],
                           void const *value[1]);

    void Preempted(void);
    void ResetVideoSurfaces(void);

  private:
    VdpRect                          m_rect;
    bool                             m_preempted   {false};
    bool                             m_recreating  {false};
    bool                             m_recreated   {false};
    bool                             m_reset_video_surfaces {false};
    QMutex                           m_render_lock {QMutex::Recursive};
    QMutex                           m_decode_lock {QMutex::Recursive};
    MythXDisplay                    *m_display     {nullptr};
    WId                              m_window      {0};
    VdpDevice                        m_device      {0};
    uint                             m_surface     {0};
    VdpPresentationQueue             m_flipQueue   {0};
    VdpPresentationQueueTarget       m_flipTarget  {0};
    bool                             m_flipReady   {false};
    uint                             m_colorKey    {0};
    bool                             m_flipFrames  {false};

    QVector<uint>                    m_surfaces;
    QHash<uint, VDPAUOutputSurface>  m_outputSurfaces;
    QHash<uint, VDPAUBitmapSurface>  m_bitmapSurfaces;
    QHash<uint, VDPAUDecoder>        m_decoders;
    QHash<uint, VDPAUVideoMixer>     m_videoMixers;
    QHash<uint, VDPAUVideoSurface>   m_videoSurfaces;
    QHash<VdpVideoSurface, uint>     m_videoSurfaceHash;
    QHash<uint, VDPAULayer>          m_layers;

    VdpGetProcAddress               *vdp_get_proc_address {nullptr};
    VdpGetErrorString               *vdp_get_error_string {nullptr};
    VdpDeviceDestroy                *vdp_device_destroy {nullptr};
    VdpGetApiVersion                *vdp_get_api_version {nullptr};
    VdpGetInformationString         *vdp_get_information_string {nullptr};
    VdpVideoSurfaceCreate           *vdp_video_surface_create {nullptr};
    VdpVideoSurfaceDestroy          *vdp_video_surface_destroy {nullptr};
    VdpVideoSurfaceGetBitsYCbCr     *vdp_video_surface_put_bits_y_cb_cr {nullptr};
    VdpVideoSurfaceGetParameters    *vdp_video_surface_get_parameters {nullptr};
    VdpVideoSurfacePutBitsYCbCr     *vdp_video_surface_get_bits_y_cb_cr {nullptr};
    VdpOutputSurfacePutBitsNative   *vdp_output_surface_put_bits_native {nullptr};
    VdpOutputSurfaceCreate          *vdp_output_surface_create {nullptr};
    VdpOutputSurfaceDestroy         *vdp_output_surface_destroy {nullptr};
    VdpOutputSurfaceRenderBitmapSurface *vdp_output_surface_render_bitmap_surface {nullptr};
    VdpOutputSurfaceGetParameters   *vdp_output_surface_get_parameters {nullptr};
    VdpOutputSurfaceGetBitsNative   *vdp_output_surface_get_bits_native {nullptr};
    VdpOutputSurfaceRenderOutputSurface *vdp_output_surface_render_output_surface {nullptr};
    VdpVideoMixerCreate             *vdp_video_mixer_create {nullptr};
    VdpVideoMixerSetFeatureEnables  *vdp_video_mixer_set_feature_enables {nullptr};
    VdpVideoMixerDestroy            *vdp_video_mixer_destroy {nullptr};
    VdpVideoMixerRender             *vdp_video_mixer_render {nullptr};
    VdpVideoMixerSetAttributeValues *vdp_video_mixer_set_attribute_values {nullptr};
    VdpGenerateCSCMatrix            *vdp_generate_csc_matrix {nullptr};
    VdpVideoMixerQueryFeatureSupport *vdp_video_mixer_query_feature_support {nullptr};
    VdpPresentationQueueTargetDestroy *vdp_presentation_queue_target_destroy {nullptr};
    VdpPresentationQueueCreate      *vdp_presentation_queue_create {nullptr};
    VdpPresentationQueueDestroy     *vdp_presentation_queue_destroy {nullptr};
    VdpPresentationQueueDisplay     *vdp_presentation_queue_display {nullptr};
    VdpPresentationQueueBlockUntilSurfaceIdle
                                    *vdp_presentation_queue_block_until_surface_idle {nullptr};
    VdpPresentationQueueTargetCreateX11
                                    *vdp_presentation_queue_target_create_x11 {nullptr};
    VdpPresentationQueueGetTime     *vdp_presentation_queue_get_time {nullptr};
    VdpPresentationQueueSetBackgroundColor
                                    *vdp_presentation_queue_set_background_color {nullptr};
    VdpDecoderCreate                *vdp_decoder_create {nullptr};
    VdpDecoderDestroy               *vdp_decoder_destroy {nullptr};
    VdpDecoderRender                *vdp_decoder_render {nullptr};
    VdpDecoderQueryCapabilities     *vdp_decoder_query_capabilities {nullptr};
    VdpBitmapSurfaceCreate          *vdp_bitmap_surface_create {nullptr};
    VdpBitmapSurfaceDestroy         *vdp_bitmap_surface_destroy {nullptr};
    VdpBitmapSurfacePutBitsNative   *vdp_bitmap_surface_put_bits_native {nullptr};
    VdpPreemptionCallbackRegister   *vdp_preemption_callback_register {nullptr};
};

#endif
