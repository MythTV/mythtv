#ifndef UTIL_VDPAU_H_
#define UTIL_VDPAU_H_

extern "C" {
#include "../libavcodec/vdpau_render.h"
}

#include "videobuffers.h"

class VDPAUContext
{
  public:
    VDPAUContext();
   ~VDPAUContext();

    bool Init(Display *disp, Screen *screen, Window win,
              QSize screen_size, bool color_control,
              MythCodecID mcodecid);
    void Deinit(void);
    bool IsErrored(void) { return errored; }

    bool InitBuffers(int width, int height, int numbufs);
    void FreeBuffers(void);
    void *GetRenderData(int i) 
    { if (i < numSurfaces && i >= 0) return (void*)&(surface_render[i]); 
      return NULL;
    }
    int GetNumBufs(void) { return numSurfaces; }

    bool InitOutput(QSize size);
    void FreeOutput(void);

    void Decode(VideoFrame *frame);

    void PrepareVideo(VideoFrame *frame, QRect video_rect,
                      QRect display_video_rect,
                      QSize screen_size, FrameScanType scan);
    void DisplayNextFrame(void);
    void SetNextFrameDisplayTimeOffset(int delayus);
    bool InitOSD(QSize osd_size);
    void UpdateOSD(void* const planes[3], QSize size,
                   void* const alpha[1]);
    void DisableOSD(void) { osdReady = false; }
    void DeinitOSD(void);

    bool SetDeinterlacer(const QString &deint);
    bool SetDeinterlacing(bool interlaced);
    QString GetDeinterlacer(void) const
            { return deinterlacer; }
    bool IsBeingUsed(VideoFrame * frame);
    void ClearReferenceFrames(void) { referenceFrames.clear(); }

    static bool CheckCodecSupported(MythCodecID myth_codec_id);
    PictureAttributeSupported  GetSupportedPictureAttributes(void) const;
    int SetPictureAttribute(PictureAttribute attributeType, int newValue);

    bool ShowPiP(VideoFrame * frame, QRect position);
    void CopyFrame(VideoFrame *dst, const VideoFrame *src, QSize size);

  private:
    bool InitProcs(Display *disp, Screen *screen);
    void DeinitProcs(void);
    void ClearScreen(void);

    bool InitFlipQueue(Window win);
    void DeinitFlipQueue(void);

    bool UpdateReferenceFrames(VideoFrame *frame);
    bool InitColorControl(void);
    bool SetPictureAttributes(void);

    bool InitPiP(QSize vid_size);
    void DeinitPip(void);

    int nextframedelay;
    VdpTime lastframetime;

    int pix_fmt;

    uint maxVideoWidth;
    uint maxVideoHeight;
    VdpVideoSurface *videoSurfaces;
    vdpau_render_state_t *surface_render;
    int numSurfaces;

    VdpOutputSurface outputSurfaces[2];
    VdpVideoSurface videoSurface;
    VdpOutputSurface outputSurface;

    VdpDecoder decoder;
    VdpVideoMixer videoMixer;

    VdpRect outRect;
    VdpRect outRectVid;

    int surfaceNum;

    VdpVideoSurface   osdVideoSurface;
    VdpOutputSurface  osdOutputSurface;
    VdpVideoMixer     osdVideoMixer;
    VdpBitmapSurface  osdAlpha;
    VdpLayer          osdLayer;
    QSize             osdSize;
    VdpRect           osdRect;
    bool              osdReady;

    bool              deintAvail;
    QString           deinterlacer;
    bool              deinterlacing;
    long long         currentFrameNum;
    frame_queue_t     referenceFrames;
    bool              needDeintRefs;

    bool              useColorControl;
    VdpCSCMatrix      cscMatrix;
    VdpProcamp        proCamp;

    QSize             pipFrameSize;
    VdpLayer          pipLayer;
    VdpVideoSurface   pipVideoSurface;
    VdpOutputSurface  pipOutputSurface;
    VdpVideoMixer     pipVideoMixer;
    int               pipReady;
    VdpRect           pipPosition;
    VdpBitmapSurface  pipAlpha;

    VdpPresentationQueueTarget vdp_flip_target;
    VdpPresentationQueue       vdp_flip_queue;

    bool              vdpauDecode;

    VdpDevice vdp_device;
    bool      errored;

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
};

#endif

