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
}

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
    static bool IsMPEG4Available(void);
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
    uint CreateLayer(uint surface, const QRect *src = NULL,
                     const QRect *dst = NULL);

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
                    int alpha = 0, int red = 0, int blue = 0, int green = 0);
    bool DrawLayer(uint id, uint target);

    int   GetBitmapSize(uint id);
    void* GetRender(uint id);
    uint  GetSurfaceOwner(VdpVideoSurface surface);
    QSize GetSurfaceSize(uint id);
    void  ClearVideoSurface(uint id);
    void  ChangeVideoSurfaceOwner(uint id);

    void  Decode(uint id, struct vdpau_render_state *render,
                 AVVDPAUContext *context);
    void  SetVideoFlip(void) { m_flipFrames = true; }

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
    bool                             m_preempted;
    bool                             m_recreating;
    bool                             m_recreated;
    bool                             m_reset_video_surfaces;
    QMutex                           m_render_lock;
    QMutex                           m_decode_lock;
    MythXDisplay                    *m_display;
    WId                              m_window;
    VdpDevice                        m_device;
    uint                             m_surface;
    VdpPresentationQueue             m_flipQueue;
    VdpPresentationQueueTarget       m_flipTarget;
    bool                             m_flipReady;
    uint                             m_colorKey;
    bool                             m_flipFrames;

    QVector<uint>                    m_surfaces;
    QHash<uint, VDPAUOutputSurface>  m_outputSurfaces;
    QHash<uint, VDPAUBitmapSurface>  m_bitmapSurfaces;
    QHash<uint, VDPAUDecoder>        m_decoders;
    QHash<uint, VDPAUVideoMixer>     m_videoMixers;
    QHash<uint, VDPAUVideoSurface>   m_videoSurfaces;
    QHash<VdpVideoSurface, uint>     m_videoSurfaceHash;
    QHash<uint, VDPAULayer>          m_layers;

    VdpGetProcAddress               *vdp_get_proc_address;
    VdpGetErrorString               *vdp_get_error_string;
    VdpDeviceDestroy                *vdp_device_destroy;
    VdpGetApiVersion                *vdp_get_api_version;
    VdpGetInformationString         *vdp_get_information_string;
    VdpVideoSurfaceCreate           *vdp_video_surface_create;
    VdpVideoSurfaceDestroy          *vdp_video_surface_destroy;
    VdpVideoSurfaceGetBitsYCbCr     *vdp_video_surface_put_bits_y_cb_cr;
    VdpVideoSurfaceGetParameters    *vdp_video_surface_get_parameters;
    VdpVideoSurfacePutBitsYCbCr     *vdp_video_surface_get_bits_y_cb_cr;
    VdpOutputSurfacePutBitsNative   *vdp_output_surface_put_bits_native;
    VdpOutputSurfaceCreate          *vdp_output_surface_create;
    VdpOutputSurfaceDestroy         *vdp_output_surface_destroy;
    VdpOutputSurfaceRenderBitmapSurface *vdp_output_surface_render_bitmap_surface;
    VdpOutputSurfaceGetParameters   *vdp_output_surface_get_parameters;
    VdpOutputSurfaceGetBitsNative   *vdp_output_surface_get_bits_native;
    VdpOutputSurfaceRenderOutputSurface *vdp_output_surface_render_output_surface;
    VdpVideoMixerCreate             *vdp_video_mixer_create;
    VdpVideoMixerSetFeatureEnables  *vdp_video_mixer_set_feature_enables;
    VdpVideoMixerDestroy            *vdp_video_mixer_destroy;
    VdpVideoMixerRender             *vdp_video_mixer_render;
    VdpVideoMixerSetAttributeValues *vdp_video_mixer_set_attribute_values;
    VdpGenerateCSCMatrix            *vdp_generate_csc_matrix;
    VdpVideoMixerQueryFeatureSupport *vdp_video_mixer_query_feature_support;
    VdpPresentationQueueTargetDestroy *vdp_presentation_queue_target_destroy;
    VdpPresentationQueueCreate      *vdp_presentation_queue_create;
    VdpPresentationQueueDestroy     *vdp_presentation_queue_destroy;
    VdpPresentationQueueDisplay     *vdp_presentation_queue_display;
    VdpPresentationQueueBlockUntilSurfaceIdle
                                    *vdp_presentation_queue_block_until_surface_idle;
    VdpPresentationQueueTargetCreateX11
                                    *vdp_presentation_queue_target_create_x11;
    VdpPresentationQueueGetTime     *vdp_presentation_queue_get_time;
    VdpPresentationQueueSetBackgroundColor
                                    *vdp_presentation_queue_set_background_color;
    VdpDecoderCreate                *vdp_decoder_create;
    VdpDecoderDestroy               *vdp_decoder_destroy;
    VdpDecoderRender                *vdp_decoder_render;
    VdpDecoderQueryCapabilities     *vdp_decoder_query_capabilities;
    VdpBitmapSurfaceCreate          *vdp_bitmap_surface_create;
    VdpBitmapSurfaceDestroy         *vdp_bitmap_surface_destroy;
    VdpBitmapSurfacePutBitsNative   *vdp_bitmap_surface_put_bits_native;
    VdpPreemptionCallbackRegister   *vdp_preemption_callback_register;
};

#endif
