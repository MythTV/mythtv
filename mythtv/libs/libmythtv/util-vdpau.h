#ifndef UTIL_VDPAU_H_
#define UTIL_VDPAU_H_

extern "C" {
#include "libavcodec/vdpau.h"
}

#include "videobuffers.h"
#include "mythxdisplay.h"
#include "mythcodecid.h"
#include "videoouttypes.h"

#define MAX_VDPAU_ERRORS 10

enum VDPAUFeatures
{
    kVDP_FEAT_NONE      = 0x00,
    kVDP_FEAT_TEMPORAL  = 0x01,
    kVDP_FEAT_SPATIAL   = 0x02,
    kVDP_FEAT_IVTC      = 0x04,
    kVDP_FEAT_DENOISE   = 0x08,
    kVDP_FEAT_SHARPNESS = 0x10,
};

struct vdpauPIP
{
    QSize           videoSize;
    VdpVideoSurface videoSurface;
    VdpVideoMixer   videoMixer;
};

struct video_surface
{
    VdpVideoSurface      surface;
    struct vdpau_render_state render;
};

class NuppelVideoPlayer;

class VDPAUContext
{
  public:
    VDPAUContext();
   ~VDPAUContext();

    bool Init(MythXDisplay *disp, Window win,
              QSize screen_size, bool color_control,
              int color_key, MythCodecID mcodecid,
              QString options);
    void Deinit(void);
    VideoErrorState GetError(void) { return errorState; }
    bool IsErrored(void)
    {
        return (errorCount > MAX_VDPAU_ERRORS && errorState != kError_None);
    }
    void SetErrored(VideoErrorState error, int errors = 1)
    {
        if (errorState == kError_None || errorCount < 1)
            errorState = error;
        errorCount += errors;
    }

    bool InitBuffers(int width, int height, int numbufs,
                     LetterBoxColour letterbox_colour);
    int  AddBuffer(int width, int height);
    void FreeBuffers(void);
    void *GetRenderData(int i)
    { if (i < GetNumBufs() && i >= 0) return (void*)&(videoSurfaces[i].render);
      return NULL;
    }
    int GetNumBufs(void) { return videoSurfaces.size(); }

    bool InitOutput(QSize size);
    void FreeOutput(void);

    void Decode(VideoFrame *frame);

    void UpdatePauseFrame(VideoFrame *frame);
    void PrepareVideo(VideoFrame *frame, QRect video_rect,
                      QRect display_video_rect,
                      QSize screen_size, FrameScanType scan);
    void DisplayNextFrame(void);
    void SetNextFrameDisplayTimeOffset(int delayus);
    bool InitOSD(QSize size);
    void UpdateOSD(void* const planes[3], QSize size,
                   void* const alpha[1]);
    void DisableOSD(void) { osdReady = false; }
    void DeinitOSD(void);

    bool SetDeinterlacer(const QString &deint);
    bool SetDeinterlacing(bool interlaced);
    QString GetDeinterlacer(void) const
            { return deinterlacer; }
    bool IsBeingUsed(VideoFrame * frame);
    void ClearReferenceFrames(void);
    PictureAttributeSupported  GetSupportedPictureAttributes(void) const;
    int SetPictureAttribute(PictureAttribute attributeType, int newValue);

    bool InitPIPLayer(QSize screen_size);
    bool ShowPIP(NuppelVideoPlayer *pipplayer,
                 VideoFrame * frame, QRect position,
                 bool pip_is_active);
    void DeinitPIP(NuppelVideoPlayer *pipplayer, bool check_layer = true);

  private:
    bool InitProcs(MythXDisplay *disp);
    void DeinitProcs(void);
    void ClearScreen(void);
    VdpVideoMixer CreateMasterMixer(int width, int height);
    VdpVideoMixer CreateMixer(int width, int height,  uint32_t layers = 0,
                              int feats = 0);
    bool InitFlipQueue(Window win, int color_key);
    void DeinitFlipQueue(void);
    void AddOutputSurfaces(void);
    bool UpdateReferenceFrames(VideoFrame *frame);
    bool InitColorControl(void);
    bool SetPictureAttributes(void);
    void DeinitPIPLayer(void);
    bool InitPIP(NuppelVideoPlayer *pipplayer, QSize vid_size);
    void ParseOptions(QString options);

    int nextframedelay;
    VdpTime lastframetime;

    int pix_fmt;

    vector<video_surface> videoSurfaces;
    int checkVideoSurfaces;
    VdpVideoSurface pause_surface;

    vector<VdpOutputSurface> outputSurfaces;
    VdpOutputSurface outputSurface;
    bool             checkOutputSurfaces;
    QSize            outputSize;

    VdpDecoder    decoder;
    uint32_t      maxReferences;
    VdpVideoMixer videoMixer;
    int           mixerFeatures;
    QSize         videoSize;
    LetterBoxColour letterboxColour;

    VdpRect outRect;
    VdpRect outRectVid;

    int surfaceNum;

    VdpVideoSurface   osdVideoSurface;
    VdpOutputSurface  osdOutputSurface;
    VdpVideoMixer     osdVideoMixer;
    VdpBitmapSurface  osdAlpha;
    VdpLayer          osdLayer;
    VdpRect           osdRect;
    bool              osdReady;
    QSize             osdSize;

    QString           deinterlacer;
    bool              deinterlacing;
    long long         currentFrameNum;
    frame_queue_t     referenceFrames;
    bool              needDeintRefs;
    QMutex            deintLock;
    uint8_t           skipChroma;
    int               numRefs;
    float             denoise;
    float             sharpen;

    bool              useColorControl;
    VdpCSCMatrix      cscMatrix;
    VdpProcamp        proCamp;

    VdpLayer          pipLayer;
    VdpOutputSurface  pipOutputSurface;
    VdpBitmapSurface  pipAlpha;
    VdpBitmapSurface  pipBorder;
    VdpBitmapSurface  pipClear;
    QMap<NuppelVideoPlayer*,vdpauPIP> pips;
    int               pipReady;
    bool              pipNeedsClear;

    VdpPresentationQueueTarget vdp_flip_target;
    VdpPresentationQueue       vdp_flip_queue;

    bool              vdpauDecode;
    VdpDevice         vdp_device;

    int               errorCount;
    VideoErrorState   errorState;

    VdpGetProcAddress * vdp_get_proc_address;
    VdpDeviceDestroy * vdp_device_destroy;
    VdpGetErrorString * vdp_get_error_string;
    VdpGetApiVersion * vdp_get_api_version;
    VdpGetInformationString * vdp_get_information_string;

    VdpVideoSurfaceCreate * vdp_video_surface_create;
    VdpVideoSurfaceDestroy * vdp_video_surface_destroy;
    VdpVideoSurfaceGetBitsYCbCr * vdp_video_surface_put_bits_y_cb_cr;
    VdpVideoSurfacePutBitsYCbCr * vdp_video_surface_get_bits_y_cb_cr;
    VdpVideoSurfaceQueryGetPutBitsYCbCrCapabilities *
                vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities;
    VdpVideoSurfaceQueryCapabilities * vdp_video_surface_query_capabilities;

    VdpOutputSurfacePutBitsYCbCr * vdp_output_surface_put_bits_y_cb_cr;
    VdpOutputSurfacePutBitsNative * vdp_output_surface_put_bits_native;
    VdpOutputSurfaceCreate * vdp_output_surface_create;
    VdpOutputSurfaceDestroy * vdp_output_surface_destroy;
    VdpOutputSurfaceRenderBitmapSurface * vdp_output_surface_render_bitmap_surface;
    VdpOutputSurfaceQueryCapabilities * vdp_output_surface_query_capabilities;

    /* videoMixer puts videoSurface data to displayble ouputSurface. */
    VdpVideoMixerCreate * vdp_video_mixer_create;
    VdpVideoMixerSetFeatureEnables * vdp_video_mixer_set_feature_enables;
    VdpVideoMixerDestroy * vdp_video_mixer_destroy;
    VdpVideoMixerRender * vdp_video_mixer_render;
    VdpVideoMixerSetAttributeValues * vdp_video_mixer_set_attribute_values;
    VdpVideoMixerQueryFeatureSupport * vdp_video_mixer_query_feature_support;
    VdpVideoMixerQueryAttributeSupport * vdp_video_mixer_query_attribute_support;
    VdpVideoMixerQueryParameterSupport * vdp_video_mixer_query_parameter_support;
    VdpGenerateCSCMatrix * vdp_generate_csc_matrix;

    VdpPresentationQueueTargetDestroy * vdp_presentation_queue_target_destroy;
    VdpPresentationQueueCreate * vdp_presentation_queue_create;
    VdpPresentationQueueDestroy * vdp_presentation_queue_destroy;
    VdpPresentationQueueDisplay * vdp_presentation_queue_display;
    VdpPresentationQueueBlockUntilSurfaceIdle * vdp_presentation_queue_block_until_surface_idle;
    VdpPresentationQueueTargetCreateX11 * vdp_presentation_queue_target_create_x11;
    VdpPresentationQueueQuerySurfaceStatus * vdp_presentation_queue_query_surface_status;
    VdpPresentationQueueGetTime * vdp_presentation_queue_get_time;
    VdpPresentationQueueSetBackgroundColor * vdp_presentation_queue_set_background_color;

    VdpDecoderCreate * vdp_decoder_create;
    VdpDecoderDestroy * vdp_decoder_destroy;
    VdpDecoderRender * vdp_decoder_render;

    VdpBitmapSurfaceCreate * vdp_bitmap_surface_create;
    VdpBitmapSurfaceDestroy * vdp_bitmap_surface_destroy;
    VdpBitmapSurfacePutBitsNative * vdp_bitmap_surface_put_bits_native;
    VdpBitmapSurfaceQueryCapabilities * vdp_bitmap_surface_query_capabilities;

    VdpPreemptionCallbackRegister * vdp_preemption_callback_register;
};

#endif

