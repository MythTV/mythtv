#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <QSize>
#include <QRect>

#include "mythcontext.h"
extern "C" {
#include "frame.h"
#include "avutil.h"
#include "vdpau_render.h"
}

#include "videoouttypes.h"
#include "mythcodecid.h"
#include "util-x11.h"
#include "util-vdpau.h"

#define LOC QString("VDPAU: ")
#define LOC_ERR QString("VDPAU Error: ")

#define NUM_OUTPUT_SURFACES 2
#define NUM_REFERENCE_FRAMES 4

#define ARSIZE(x) (sizeof(x) / sizeof((x)[0]))

/* MACRO for error check */
#define CHECK_ST \
  ok &= (vdp_st == VDP_STATUS_OK); \
  if (!ok) { \
      VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Error at %1:%2 (#%3, %4)") \
              .arg(__FILE__).arg( __LINE__).arg(vdp_st) \
              .arg(vdp_get_error_string(vdp_st))); \
  }

static const VdpChromaType vdp_chroma_type = VDP_CHROMA_TYPE_420;
static const VdpOutputSurfaceRenderBlendState osd_blend =
    {
        VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD
    };        

VDPAUContext::VDPAUContext()
  : pix_fmt(-1),            maxVideoWidth(0),  maxVideoHeight(0),
    videoSurfaces(0),       surface_render(0), numSurfaces(0),
    videoSurface(0),        outputSurface(0),  decoder(0),
    videoMixer(0),          surfaceNum(0),     osdVideoSurface(0),
    osdOutputSurface(0),    osdVideoMixer(0),  osdAlpha(0),
    osdSize(QSize(0,0)),    osdReady(false),   deintAvail(false),
    deinterlacer("notset"), deinterlacing(false), currentFrameNum(-1),
    needDeintRefs(false),   useColorControl(false), pipFrameSize(QSize(0,0)),
    pipVideoSurface(0),     pipOutputSurface(0),
    pipVideoMixer(0),       pipReady(0),       pipAlpha(0),
    vdp_flip_target(NULL),  vdp_flip_queue(NULL),
    vdp_device(NULL),       errored(false)
{
}

VDPAUContext::~VDPAUContext()
{
}

bool VDPAUContext::Init(Display *disp, Screen *screen,
                        Window win, QSize screen_size,
                        bool color_control)
{
    bool ok;

    ok = InitProcs(disp, screen);
    if (!ok)
        return ok;

    ok = InitFlipQueue(win);
    if (!ok)
        return ok;

    ok = InitOutput(screen_size);
    if (!ok)
        return ok;

    if (color_control)
        useColorControl = InitColorControl();

    return ok;
}

void VDPAUContext::Deinit(void)
{
    if (decoder)
    {
        vdp_decoder_destroy(decoder);
        decoder = NULL;
        pix_fmt = -1;
    }
    ClearReferenceFrames();
    DeinitOSD();
    FreeOutput();
    DeinitFlipQueue();
    DeinitPip();
    DeinitProcs();
}

static const char* dummy_get_error_string(VdpStatus status)
{
    static const char dummy[] = "Unknown";
    return &dummy[0];
}

bool VDPAUContext::InitProcs(Display *disp, Screen *screen)
{
    VdpStatus vdp_st;
    bool ok = true;
    vdp_get_error_string = &dummy_get_error_string;

    vdp_st = vdp_device_create_x11(
        disp,
        *(int*)screen,
        &vdp_device,
        &vdp_get_proc_address
    );
    CHECK_ST
    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create VDP Device."));
        return false;
    }

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_GET_ERROR_STRING,
        (void **)&vdp_get_error_string
    );
    ok &= (vdp_st == VDP_STATUS_OK);
    if (!ok)
        vdp_get_error_string = &dummy_get_error_string;

    // non-fatal debugging info
    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_GET_API_VERSION,
        (void **)&vdp_get_api_version
    );

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_GET_INFORMATION_STRING,
        (void **)&vdp_get_information_string
    );

    static bool debugged = false;

    if (!debugged)
    {
        debugged = true;
        if (vdp_get_api_version)
        {
            uint version;
            vdp_get_api_version(&version);
            VERBOSE(VB_PLAYBACK, LOC + QString("Version %1").arg(version));
        }
        if (vdp_get_information_string)
        {
            const char * info;
            vdp_get_information_string(&info);
            VERBOSE(VB_PLAYBACK, LOC + QString("Information %2").arg(info));
        }
    }

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_DEVICE_DESTROY,
        (void **)&vdp_device_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_CREATE,
        (void **)&vdp_video_surface_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_DESTROY,
        (void **)&vdp_video_surface_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_PUT_BITS_Y_CB_CR,
        (void **)&vdp_video_surface_put_bits_y_cb_cr
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_GET_BITS_Y_CB_CR,
        (void **)&vdp_video_surface_get_bits_y_cb_cr
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_QUERY_CAPABILITIES,
        (void **)&vdp_video_surface_query_capabilities
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_Y_CB_CR,
        (void **)&vdp_output_surface_put_bits_y_cb_cr
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_PUT_BITS_NATIVE,
        (void **)&vdp_output_surface_put_bits_native
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_CREATE,
        (void **)&vdp_output_surface_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY,
        (void **)&vdp_output_surface_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_OUTPUT_SURFACE_RENDER_BITMAP_SURFACE,
        (void **)&vdp_output_surface_render_bitmap_surface
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_QUERY_CAPABILITIES,
        (void **)&vdp_output_surface_query_capabilities
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_CREATE,
        (void **)&vdp_video_mixer_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_SET_FEATURE_ENABLES,
        (void **)&vdp_video_mixer_set_feature_enables
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_DESTROY,
        (void **)&vdp_video_mixer_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_RENDER,
        (void **)&vdp_video_mixer_render
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_SET_ATTRIBUTE_VALUES,
        (void **)&vdp_video_mixer_set_attribute_values
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_QUERY_FEATURE_SUPPORT,
        (void **)&vdp_video_mixer_query_feature_support
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_QUERY_PARAMETER_SUPPORT,
        (void **)&vdp_video_mixer_query_parameter_support
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_MIXER_QUERY_ATTRIBUTE_SUPPORT,
        (void **)&vdp_video_mixer_query_attribute_support
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_GENERATE_CSC_MATRIX,
        (void **)&vdp_generate_csc_matrix
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY,
        (void **)&vdp_presentation_queue_target_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE,
        (void **)&vdp_presentation_queue_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY,
        (void **)&vdp_presentation_queue_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY,
        (void **)&vdp_presentation_queue_display
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE,
        (void **)&vdp_presentation_queue_block_until_surface_idle
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
        (void **)&vdp_presentation_queue_target_create_x11
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_DECODER_CREATE,
        (void **)&vdp_decoder_create
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_DECODER_DESTROY,
        (void **)&vdp_decoder_destroy
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_DECODER_RENDER,
        (void **)&vdp_decoder_render
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PRESENTATION_QUEUE_QUERY_SURFACE_STATUS,
        (void **)&vdp_presentation_queue_query_surface_status
    );
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_VIDEO_SURFACE_QUERY_GET_PUT_BITS_Y_CB_CR_CAPABILITIES,
        (void **)&vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_CREATE,
        (void **)&vdp_bitmap_surface_create);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_PUT_BITS_NATIVE,
        (void **)&vdp_bitmap_surface_put_bits_native);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_QUERY_CAPABILITIES,
        (void **)&vdp_bitmap_surface_query_capabilities);
    CHECK_ST

    vdp_st = vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_BITMAP_SURFACE_DESTROY,
        (void **)&vdp_bitmap_surface_destroy);
    CHECK_ST

    return ok;
}

void VDPAUContext::DeinitProcs(void)
{
    if (vdp_device)
    {
        vdp_device_destroy(vdp_device);
        vdp_device = 0;
    }
}

bool VDPAUContext::InitFlipQueue(Window win)
{
    VdpStatus vdp_st;
    bool ok = true;

    vdp_st = vdp_presentation_queue_target_create_x11(
        vdp_device,
        win,
        &vdp_flip_target
    );
    CHECK_ST

    vdp_st = vdp_presentation_queue_create(
        vdp_device,
        vdp_flip_target,
        &vdp_flip_queue
    );
    CHECK_ST

    return ok;
}

void VDPAUContext::DeinitFlipQueue(void)
{
    VdpStatus vdp_st;
    bool ok = true;

    if (vdp_flip_queue)
    {
        vdp_st = vdp_presentation_queue_destroy(
            vdp_flip_queue);
        vdp_flip_queue = 0;
        CHECK_ST
    }

    if (vdp_flip_target)
    {
        vdp_st = vdp_presentation_queue_target_destroy(
        vdp_flip_target);
        vdp_flip_target = 0;
        CHECK_ST
    }
}

bool VDPAUContext::InitBuffers(int width, int height, int numbufs)
{
    VdpStatus vdp_st;
    bool ok = true;

    int i;

    VdpBool supported;
    vdp_st = vdp_video_surface_query_capabilities(
        vdp_device,
        vdp_chroma_type,
        &supported,
        &maxVideoWidth,
        &maxVideoHeight
        );
    CHECK_ST

    if (!supported || !ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Video surface -chroma type not supported."));
        return false;
    }
    else if (maxVideoWidth  < (uint)width ||
             maxVideoHeight < (uint)height)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Video surface - too large (%1x%2 > %3x%4).")
            .arg(width).arg(height)
            .arg(maxVideoWidth).arg(maxVideoHeight));
        return false;
    }

    videoSurfaces = (VdpVideoSurface *)malloc(sizeof(VdpVideoSurface) * numbufs);
    surface_render = (vdpau_render_state_t*)malloc(sizeof(vdpau_render_state_t) * numbufs);
    memset(surface_render, 0, sizeof(vdpau_render_state_t) * numbufs);

    numSurfaces = numbufs;

    for (i = 0; i < numbufs; i++)
    {
        vdp_st = vdp_video_surface_create(
            vdp_device,
            vdp_chroma_type,
            width,
            height,
            &(videoSurfaces[i])
        );
        CHECK_ST

        if (!ok)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create video surface."));
            return false;
        }
        surface_render[i].magic = MP_VDPAU_RENDER_MAGIC;
        surface_render[i].state = 0;
        surface_render[i].surface = videoSurfaces[i];
    }

    // clear video surfaces to black
    vdp_st = vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
                vdp_device,
                vdp_chroma_type,
                VDP_YCBCR_FORMAT_YV12,
                &supported);

    if (supported && (vdp_st == VDP_STATUS_OK))
    {
        unsigned char *tmp =
            new unsigned char[(width * height * 3)>>1];
        if (tmp)
        {
            bzero(tmp, width * height);
            memset(tmp + (width * height), 127, (width * height)>>1);
            uint32_t pitches[3] = {width, width, width>>1};
            void* const planes[3] = 
                        {tmp, tmp + (width * height), tmp + (width * height)};
            for (i = 0; i < numbufs; i++)
            {
                vdp_video_surface_put_bits_y_cb_cr(
                    videoSurfaces[i],
                    VDP_YCBCR_FORMAT_YV12,
                    planes,
                    pitches
                );
            }
            delete tmp;
        }

    }

    // TODO video capability/parameter check 
    // but should just fail gracefully anyway

    uint32_t num_layers = 2; // PiP and OSD
    VdpVideoMixerParameter parameters[] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
        VDP_VIDEO_MIXER_PARAMETER_LAYERS,
    };

    void const * parameter_values[] = {
        &width,
        &height,
        &vdp_chroma_type,
        &num_layers
    };

    // check deinterlacers available
    vdp_st = vdp_video_mixer_query_parameter_support(
        vdp_device,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
        &supported
    );
    CHECK_ST
    deintAvail = (ok && supported);
    vdp_st = vdp_video_mixer_query_parameter_support(
        vdp_device,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
        &supported
    );
    CHECK_ST
    deintAvail &= (ok && supported);

    VdpVideoMixerFeature features[] = {
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
        VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION,
        VDP_VIDEO_MIXER_FEATURE_SHARPNESS
    };

    vdp_st = vdp_video_mixer_create(
        vdp_device,
        deintAvail ? ARSIZE(features) : 0,
        deintAvail ? features : NULL,
        ARSIZE(parameters),
        parameters,
        parameter_values,
        &videoMixer
    );
    CHECK_ST

    if (!ok && videoMixer)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Create video mixer - errored but returned handle."));
    }

    // minimise green screen
    if (ok)
        ClearScreen();

    return ok;
}

void VDPAUContext::FreeBuffers(void)
{
    VdpStatus vdp_st;
    bool ok = true;

    int i;

    if (videoMixer)
    {
        vdp_st = vdp_video_mixer_destroy(
            videoMixer
        );
        videoMixer = 0;
        CHECK_ST
    }

    for (i = 0; i < numSurfaces; i++)
    {
        if (videoSurfaces[i])
        {
            vdp_st = vdp_video_surface_destroy(
                videoSurfaces[i]);
            CHECK_ST
        }
    }

    if (videoSurfaces)
        free(videoSurfaces);
    videoSurfaces = NULL;

    if (surface_render)
        free(surface_render);
    surface_render = NULL;
}

bool VDPAUContext::InitOutput(QSize size)
{
    VdpStatus vdp_st;
    bool ok = true;
    int i;

    VdpBool supported;
    uint max_width, max_height;
    vdp_st = vdp_output_surface_query_capabilities(
        vdp_device,
        VDP_RGBA_FORMAT_B8G8R8A8,
        &supported,
        &max_width,
        &max_height
    );
    CHECK_ST

    if (!supported || !ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Output surface chroma format not supported."));
        return false;
    }
    else if (max_width  < (uint)size.width() ||
             max_height < (uint)size.height())
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Output surface - too large (%1x%2 > %3x%4).")
            .arg(size.width()).arg(size.height())
            .arg(max_width).arg(max_height));
        return false;
    }
    
    for (i = 0; i < NUM_OUTPUT_SURFACES; i++)
    {
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            size.width(),
            size.height(),
            &outputSurfaces[i]
        );
        CHECK_ST

        if (!ok)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create output surface."));
            return false;
        }
    }

    outRect.x0 = 0;
    outRect.y0 = 0;
    outRect.x1 = size.width();
    outRect.y1 = size.height();
    surfaceNum = 0;
    return ok;
}

void VDPAUContext::FreeOutput(void)
{
    VdpStatus vdp_st;
    bool ok = true;
    int i;

    for (i = 0; i < NUM_OUTPUT_SURFACES; i++)
    {
        if (outputSurfaces[i])
        {    
            vdp_st = vdp_output_surface_destroy(
                outputSurfaces[i]);
            CHECK_ST
        }
    }
}

void VDPAUContext::Decode(VideoFrame *frame)
{
    VdpStatus vdp_st;
    bool ok = true;
    vdpau_render_state_t *render;

    if (frame->pix_fmt != pix_fmt)
    {
        VdpDecoderProfile vdp_decoder_profile;
        switch (frame->pix_fmt)
        {
            case PIX_FMT_VDPAU_MPEG1: vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG1; break;
            case PIX_FMT_VDPAU_MPEG2_SIMPLE: vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_SIMPLE; break;
            case PIX_FMT_VDPAU_MPEG2_MAIN: vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_MAIN; break;
            case PIX_FMT_VDPAU_H264_BASELINE: vdp_decoder_profile = VDP_DECODER_PROFILE_H264_BASELINE; break;
            case PIX_FMT_VDPAU_H264_MAIN: vdp_decoder_profile = VDP_DECODER_PROFILE_H264_MAIN; break;
            case PIX_FMT_VDPAU_H264_HIGH: vdp_decoder_profile = VDP_DECODER_PROFILE_H264_HIGH; break;
            case PIX_FMT_VDPAU_VC1_SIMPLE: vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_SIMPLE; break;
            case PIX_FMT_VDPAU_VC1_MAIN: vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN; break;
            case PIX_FMT_VDPAU_VC1_ADVANCED: vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED; break;
            default:
                assert(0);
                return;
        }

        // generic capability pre-checked but specific profile may now fail
        vdp_st = vdp_decoder_create(
            vdp_device,
            vdp_decoder_profile,
            frame->width,
            frame->height,
            &decoder
        );
        CHECK_ST

        if (ok)
            pix_fmt = frame->pix_fmt;
        else
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to create decoder."));
            errored = true;
        }
    }

    render = (vdpau_render_state_t *)frame->buf;
    if (!render || !decoder)
        return;

    vdp_st = vdp_decoder_render(
        decoder,
        render->surface,
        (VdpPictureInfo const *)&(render->info),
        render->bitstreamBuffersUsed,
        render->bitstreamBuffers
    );
    CHECK_ST
}

void VDPAUContext::PrepareVideo(VideoFrame *frame, QRect video_rect,
                                QRect display_video_rect,
                                QSize screen_size, FrameScanType scan)
{
    VdpStatus vdp_st;
    bool ok = true;
    VdpTime dummy;
    vdpau_render_state_t *render;

    if (!frame)
        return;

    if (deinterlacing && needDeintRefs)
        UpdateReferenceFrames(frame);

    render = (vdpau_render_state_t *)frame->buf;
    if (!render)
        return;

    videoSurface = render->surface;

    if (outRect.x1 != (uint)screen_size.width() ||
        outRect.y1 != (uint)screen_size.height())
    {
        FreeOutput();
        InitOutput(screen_size);
    }

    QRect out_rect = QRect(QPoint(0, 0), screen_size);
    display_video_rect = out_rect.intersected(display_video_rect);

    outRect.x0 = 0;
    outRect.y0 = 0;
    outRect.x1 = screen_size.width();
    outRect.y1 = screen_size.height();

    VdpRect srcRect;
    srcRect.x0 = video_rect.left();
    srcRect.y0 = video_rect.top();
    srcRect.x1 = video_rect.left() + video_rect.width();
    srcRect.y1 = video_rect.top() + video_rect.height();

    outRectVid.x0 = display_video_rect.left();
    outRectVid.y0 = display_video_rect.top();
    outRectVid.x1 = display_video_rect.left() + display_video_rect.width();
    outRectVid.y1 = display_video_rect.top() + display_video_rect.height();

    VdpVideoMixerPictureStructure field =
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME;

    if (scan == kScan_Interlaced && deinterlacing)
        field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    else if (scan == kScan_Intr2ndField && deinterlacing)
        field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;

    outputSurface = outputSurfaces[surfaceNum];
    vdp_st = vdp_presentation_queue_block_until_surface_idle(
        vdp_flip_queue,
        outputSurface, 
        &dummy
    );
    CHECK_ST

    bool deint = false;
    if (deinterlacing && needDeintRefs &&
        referenceFrames.size() == NUM_REFERENCE_FRAMES)
    {
        deint = true;
    }

    VdpVideoSurface past_surfaces[2] = { VDP_INVALID_HANDLE, VDP_INVALID_HANDLE };
    VdpVideoSurface future_surfaces[1] = { VDP_INVALID_HANDLE };

    if (deint)
    {
#if 0
        videoSurface = render->surface;
      
        if (frame->top_field_first)
        {
            if (scan == kScan_Interlaced) // displaying top top-first
            {
                // next field (bottom) is in the current frame)
                future_surfaces[0] = videoSurface;

                // previous two fields are in the previous frame
                render = (vdpau_render_state_t *)referenceFrames[1]->buf;
                if (render)
                    past_surfaces[0] = render->surface;
                past_surfaces[1] = past_surfaces[0];
            }
            else // displaying bottom of top-first
            {
                // next field (top) is in the next frame
                render = (vdpau_render_state_t *)referenceFrames[3]->buf;
                if (render)
                    future_surfaces[0] = render->surface;

                // previous field is in the current frame
                past_surfaces[0] = videoSurface;

                // field before that is in the previous frame
                render = (vdpau_render_state_t *)referenceFrames[1]->buf;
                if (render)
                    past_surfaces[1] = render->surface;
            }
        }
        else
        {
        }
#else
        render = (vdpau_render_state_t *)referenceFrames[3]->buf;
        if (render)
            future_surfaces[0] = render->surface;
        render = (vdpau_render_state_t *)referenceFrames[2]->buf;
        if (render)
            videoSurface = render->surface;
        render = (vdpau_render_state_t *)referenceFrames[1]->buf;
        if (render)
            past_surfaces[0] = render->surface;
        render = (vdpau_render_state_t *)referenceFrames[0]->buf;
        if (render)
            past_surfaces[1] = render->surface;
#endif
    }

    uint num_layers  = 0;

    if (osdReady) { num_layers++; }
    if (pipReady) { num_layers++; }

    VdpLayer layers[2];
    
    if (num_layers == 1)
    {
        if (osdReady)
            memcpy(&(layers[0]), &osdLayer, sizeof(osdLayer));
        if (pipReady)
            memcpy(&(layers[0]), &pipLayer, sizeof(pipLayer));
    }
    else if (num_layers == 2)
    {
        memcpy(&(layers[0]), &pipLayer, sizeof(pipLayer));
        memcpy(&(layers[1]), &osdLayer, sizeof(osdLayer));
    }

    vdp_st = vdp_video_mixer_render(
        videoMixer,
        VDP_INVALID_HANDLE,
        NULL,
        field,
        deint ? ARSIZE(past_surfaces) : 0,
        deint ? past_surfaces : NULL,
        videoSurface,
        deint ? ARSIZE(future_surfaces) : 0,
        deint ? future_surfaces : NULL,
        NULL, //&srcRect,
        outputSurface,
        &outRect,
        &outRectVid,
        num_layers,
        num_layers ? layers : NULL
    );
    CHECK_ST

    if (pipReady)
        pipReady--;
}

void VDPAUContext::DisplayNextFrame(void)
{
    if (!outputSurface)
        return;

    VdpStatus vdp_st;
    bool ok = true;

    vdp_st = vdp_presentation_queue_display(
        vdp_flip_queue,
        outputSurface,
        outRect.x1,
        outRect.y1,
        0
    );
    CHECK_ST

    surfaceNum = surfaceNum ^ 1;
}

bool VDPAUContext::InitOSD(QSize osd_size)
{
    if (!vdp_device)
        return false;

    VdpStatus vdp_st;
    bool ok = true;

    uint width = osd_size.width();
    uint height = osd_size.height();
    VdpBool supported = false;

    vdp_st = vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(
        vdp_device,
        vdp_chroma_type,
        VDP_YCBCR_FORMAT_YV12,
        &supported
    );
    CHECK_ST
    if (!supported || !ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                    QString("YV12 upload to video surface not supported."));
        return false;
    }

    uint32_t max_width, max_height;
    vdp_st = vdp_bitmap_surface_query_capabilities(
        vdp_device,
        VDP_RGBA_FORMAT_A8,
        &supported,
        &max_width,
        &max_height
    );
    CHECK_ST
    if (!supported || !ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                    QString("Alpha transparency bitmaps not supported."));
        return false;
    }
    else if (max_width  < width ||
             max_height < height)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
                    QString("Alpha bitmap too large (%1x%2 > %3x%4).")
                    .arg(width).arg(height).arg(max_width).arg(max_height));
        return false;
    }

    if (maxVideoWidth  < width ||
        maxVideoHeight < height)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("OSD size too large for video surface."));
        return false;
    }

    // capability already checked in InitOutput
    vdp_st = vdp_output_surface_create(
        vdp_device,
        VDP_RGBA_FORMAT_B8G8R8A8,
        width,
        height,
        &osdOutputSurface
    );
    CHECK_ST;

    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create output surface."));
    }
    else
    {
        vdp_st = vdp_video_surface_create(
            vdp_device,
            vdp_chroma_type,
            width,
            height,
            &osdVideoSurface
        );
        CHECK_ST
    }

    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create video surface."));
    }
    else
    {
        vdp_st = vdp_bitmap_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_A8,
            width,
            height,
            false,
            &osdAlpha
        );
        CHECK_ST
    }

    VdpVideoMixerParameter parameters[] = {
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
    };

    void const * parameter_values[] = {
        &width,
        &height,
        &vdp_chroma_type
    };

    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create bitmap surface."));
    }
    else
    {
        vdp_st = vdp_video_mixer_create(
            vdp_device,
            0,
            0,
            ARSIZE(parameters),
            parameters,
            parameter_values,
            &osdVideoMixer
        );
        CHECK_ST
    }

    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create video mixer."));
    }
    else
    {
        osdSize = osd_size;
        osdRect.x0 = 0;
        osdRect.y0 = 0;
        osdRect.x1 = width;
        osdRect.y1 = height;
        osdLayer.struct_version = VDP_LAYER_VERSION;
        osdLayer.source_surface = osdOutputSurface;
        osdLayer.source_rect    = NULL;
        osdLayer.destination_rect = NULL;
        VERBOSE(VB_PLAYBACK, LOC + QString("Created OSD (%1x%2)")
                    .arg(width).arg(height));
        return ok;
    }

    osdSize = QSize(0,0);
    return ok;
}

void VDPAUContext::UpdateOSD(void* const planes[3],
                             QSize size,
                             void* const alpha[1])
{
    if (size != osdSize)
        return;

    VdpStatus vdp_st;
    bool ok = true;

    // upload OSD YV12 data
    uint32_t pitches[3] = {size.width(), size.width()>>1, size.width()>>1};
    void * const realplanes[3] = { planes[0], planes[2], planes[1] };

    vdp_st = vdp_video_surface_put_bits_y_cb_cr(osdVideoSurface,
                                                VDP_YCBCR_FORMAT_YV12,
                                                realplanes,
                                                pitches);
    CHECK_ST;

    // osd YV12 colourspace conversion
    if (ok)
    {
        vdp_st = vdp_video_mixer_render(
            osdVideoMixer,
            VDP_INVALID_HANDLE,
            NULL,
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
            0,
            NULL,
            osdVideoSurface,
            0,
            NULL,
            NULL,
            osdOutputSurface,
            &osdRect,
            &osdRect,
            0,
            NULL
        );
        CHECK_ST
    }

    // upload OSD alpha data
    if (ok)
    {
        uint32_t pitch[1] = {size.width()};
        vdp_st = vdp_bitmap_surface_put_bits_native(
            osdAlpha,
            alpha,
            pitch,
            NULL
        );
        CHECK_ST
    }

    // blend alpha into osd
    if (ok)
    {
        vdp_st = vdp_output_surface_render_bitmap_surface(
            osdOutputSurface,
            NULL,
            osdAlpha,
            NULL,
            NULL,
            &osd_blend,
            0
        );
        CHECK_ST
    }
        
    osdReady = ok;
}

void VDPAUContext::DeinitOSD(void)
{
    if (osdOutputSurface)
    {
        vdp_video_surface_destroy(osdOutputSurface);
        osdOutputSurface = 0;
    }

    if (osdVideoSurface)
    {
        vdp_video_surface_destroy(osdVideoSurface);
        osdVideoSurface = 0;
    }

    if (osdVideoMixer)
    {
        vdp_video_mixer_destroy(osdVideoMixer);
        osdVideoMixer = 0;
    }

    if (osdAlpha)
    {
        vdp_bitmap_surface_destroy(osdAlpha);
        osdAlpha = 0;
    }
}

bool VDPAUContext::SetDeinterlacer(const QString &deint)
{
    deinterlacer = deint;
    deinterlacer.detach();
    return true;
}

bool VDPAUContext::SetDeinterlacing(bool interlaced)
{
    if (!deintAvail)
        return false;

    if (!deinterlacer.contains("vdpau"))
        interlaced = false;

    VdpStatus vdp_st;
    bool ok = interlaced;

    if (interlaced)
    {
        VdpVideoMixerFeature features[] = {
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
            VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
        };

        VdpBool deint = true;
        const VdpBool * feature_values[] = {
            &deint,
            &deint,
        };

        vdp_st = vdp_video_mixer_set_feature_enables(
            videoMixer,
            ARSIZE(features),
            features,
            *feature_values
        );
        CHECK_ST
    }

    VdpVideoMixerFeature noiseReduce[] = {
        VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION,
        VDP_VIDEO_MIXER_FEATURE_SHARPNESS,
    };

    VdpBool noise = true;
    const VdpBool * noise_values[] = {
        &noise,
        &noise,
    };

    vdp_st = vdp_video_mixer_set_feature_enables(
        videoMixer,
        ARSIZE(noiseReduce),
        noiseReduce,
        *noise_values
    );
    CHECK_ST

    VdpVideoMixerAttribute noiseAttr[] = {
        VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL,
        VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL,
    };
    float noiseval = 0.5; 
    void const * noiseAttr_values[] = {
        &noiseval,
        &noiseval,
    };

    vdp_st = vdp_video_mixer_set_attribute_values(
        videoMixer,
        ARSIZE(noiseAttr),
        noiseAttr,
        noiseAttr_values
    );
    CHECK_ST

    deinterlacing = (interlaced & ok);
    needDeintRefs = false;
    if (!deinterlacing)
    {
        ClearReferenceFrames();
    }
    else
    {
        if (deinterlacer.contains("advanced"))
            needDeintRefs = true;
    }
    return deinterlacing;
}

void VDPAUContext::UpdateReferenceFrames(VideoFrame *frame)
{
    if (frame->frameNumber == currentFrameNum ||
        !needDeintRefs)
        return;

    currentFrameNum = frame->frameNumber;
    while (referenceFrames.size() > (NUM_REFERENCE_FRAMES - 1))
        referenceFrames.pop_front();

    referenceFrames.push_back(frame);
}

bool VDPAUContext::IsBeingUsed(VideoFrame *frame)
{
    if (!frame)
        return false;

    return referenceFrames.contains(frame);
}

bool VDPAUContext::CheckCodecSupported(MythCodecID myth_codec_id)
{
    bool ok = true;

    Display *disp = MythXOpenDisplay();
    if (!disp)
        return false;

    Screen *screen;
    X11S(screen = DefaultScreenOfDisplay(disp));
    if (!screen)
        ok = false;

    VdpDevice device;
    VdpGetProcAddress * vdp_proc_address;
    VdpStatus vdp_st;
    VdpGetErrorString * vdp_get_error_string;
    vdp_get_error_string = &dummy_get_error_string;

    if (ok)
    {
        vdp_st = vdp_device_create_x11(
            disp,
            *(int*)screen,
            &device,
            &vdp_proc_address
        );
        CHECK_ST
    }

    VdpDecoderQueryCapabilities * decoder_query;
    VdpDeviceDestroy * device_destroy;

    if (ok)
    {
        vdp_st = vdp_proc_address(
            device,
            VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES,
            (void **)&decoder_query
        );
        CHECK_ST
    }

    if (ok)
    {
        vdp_st = vdp_proc_address(
            device,
            VDP_FUNC_ID_DEVICE_DESTROY,
            (void **)&device_destroy
        );
        CHECK_ST
    }

    if (ok)
    {
        int support = 0;
        VdpBool supported;
        // not checked yet
        uint level, refs, width, height;
        switch (myth_codec_id)
        {
            case kCodec_MPEG1_VDPAU:
            case kCodec_MPEG2_VDPAU:
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_MPEG1,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_MPEG2_SIMPLE,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_MPEG2_MAIN,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                break;

            case kCodec_H264_VDPAU:
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_H264_BASELINE,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_H264_MAIN,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_H264_HIGH,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                break;

            case kCodec_VC1_VDPAU:
            // is this correct? (WMV3 == VC1)
            case kCodec_WMV3_VDPAU:
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_VC1_SIMPLE,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_VC1_MAIN,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                vdp_st = decoder_query(
                    device,
                    VDP_DECODER_PROFILE_VC1_ADVANCED,
                    &supported,
                    &level, &refs, &width, &height);
                CHECK_ST
                support += supported;
                break;

            default:
                ok = false;
        }
        ok = (ok && (support > 0));
        if (ok && support != 3)
        {
            VERBOSE(VB_IMPORTANT, "VDPAU WARNING: Codec not fully supported"
                                  " - playback may fail.");
        }
    }

    // tidy up
    if (device_destroy && device)
        device_destroy(device);

    if (disp)
        X11S(XCloseDisplay(disp));

    return ok;
}

PictureAttributeSupported 
VDPAUContext::GetSupportedPictureAttributes(void) const
{
    return (!useColorControl) ?
        kPictureAttributeSupported_None :
        (PictureAttributeSupported) 
        (kPictureAttributeSupported_Brightness |
         kPictureAttributeSupported_Contrast |
         kPictureAttributeSupported_Colour |
         kPictureAttributeSupported_Hue);
}

int VDPAUContext::SetPictureAttribute(
        PictureAttribute attribute, int newValue)
{
    if (!useColorControl)
        return -1;

    int ret = -1;
    switch (attribute)
    {
        case kPictureAttribute_Brightness:
            ret = newValue;
            proCamp.brightness = (newValue * 0.02f) - 1.0f;
            break;
        case kPictureAttribute_Contrast:
            ret = newValue;
            proCamp.contrast = (newValue * 0.02f);
            break;
        case kPictureAttribute_Colour:
            ret = newValue;
            proCamp.saturation = (newValue * 0.02f);
            break;
        case kPictureAttribute_Hue:
            ret = newValue;
            proCamp.hue = (newValue * 0.062831853f) - 3.14159265f;
            break;
        default:
            break;
    }

    if (ret != -1)
        SetPictureAttributes();

    return ret;
}
bool VDPAUContext::InitColorControl(void)
{
    bool ok = true;
    VdpStatus vdp_st;

    proCamp.struct_version = VDP_PROCAMP_VERSION;
    proCamp.brightness     = 0.0;
    proCamp.contrast       = 1.0;
    proCamp.saturation     = 1.0;
    proCamp.hue            = 0.0;

    VdpBool supported;
    vdp_st = vdp_video_mixer_query_attribute_support(
        vdp_device,
        VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX,
        &supported
    );
    CHECK_ST
    ok &= supported;
    return ok;
}

// incorrect chroma range
static VdpCSCMatrix itur_601 =
   {{1.164382813f, 0.000000000f, 1.596027344f,-0.870785156f},
    {1.164382813f,-0.391761718f,-0.812968750f, 0.529593650f},
    {1.164382813f, 2.017234375f, 0.000000000f,-1.081390625f}};

VdpStatus dummy_generate_csc(VdpProcamp *pro_camp,
                        VdpColorStandard standard,
                        VdpCSCMatrix *cscMatrix)
{
    (void) pro_camp;
    (void) standard;
    memcpy(cscMatrix, &itur_601, sizeof(*cscMatrix));
    return VDP_STATUS_OK;
}

bool VDPAUContext::SetPictureAttributes(void)
{
    bool ok = true;
    VdpStatus vdp_st;

    if (!videoMixer || !useColorControl)
        return false;

    // replacement csc??
    if (0)
        vdp_generate_csc_matrix = &dummy_generate_csc;

    vdp_st = vdp_generate_csc_matrix(
        &proCamp,
        VDP_COLOR_STANDARD_ITUR_BT_601, // detect?
        &cscMatrix
    );
    CHECK_ST

    VdpVideoMixerAttribute attributes[] = {
        VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX
    };
    void const * attribute_values[] = { &cscMatrix };

    if (ok)
    {
        vdp_st = vdp_video_mixer_set_attribute_values(
           videoMixer,
           ARSIZE(attributes),
           attributes,
           attribute_values
        );
        CHECK_ST
    }

    return ok;
}

void VDPAUContext::ClearScreen(void)
{
    VdpStatus vdp_st;
    bool ok = true;

    VdpRect srcRect;
    srcRect.x0 = 0;
    srcRect.y0 = 0;
    srcRect.x1 = 1;
    srcRect.y1 = 1;

    outputSurface = outputSurfaces[surfaceNum];
    vdp_st = vdp_video_mixer_render(
        videoMixer,
        VDP_INVALID_HANDLE,
        NULL,
        VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
        0,
        NULL,
        videoSurfaces[0],
        0,
        NULL,
        &srcRect,
        outputSurface,
        &outRect,
        &outRect,
        0, 
        NULL);
    CHECK_ST

    DisplayNextFrame();
}

void VDPAUContext::DeinitPip(void)
{
    pipFrameSize = QSize(0,0);
    pipReady     = 0;

    if (pipVideoSurface)
    {
        vdp_video_surface_destroy(pipVideoSurface);
        pipVideoSurface = 0;
    }

    if (pipOutputSurface)
    {
        vdp_output_surface_destroy(pipOutputSurface);
        pipOutputSurface = 0;
    }

    if (pipVideoMixer)
    {
        vdp_video_mixer_destroy(pipVideoMixer);
        pipVideoMixer = 0;
    }

    if (pipAlpha)
    {
        vdp_bitmap_surface_destroy(pipAlpha);
        pipAlpha = 0;
    }
}

bool VDPAUContext::InitPiP(QSize vid_size)
{
    // TODO capability check 
    // but should just fail gracefully anyway
    bool ok = true;
    VdpStatus vdp_st;

    pipFrameSize = vid_size;

    vdp_st = vdp_video_surface_create(
        vdp_device,
        vdp_chroma_type,
        vid_size.width(),
        vid_size.height(),
        &pipVideoSurface
    );
    CHECK_ST

    if (ok)
    {
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            vid_size.width(),
            vid_size.height(),
            &pipOutputSurface
        );
        CHECK_ST
    }

    if (ok)
    {
        vdp_st = vdp_bitmap_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_A8,
            vid_size.width(),
            vid_size.height(),
            false,
            &pipAlpha
        );
        CHECK_ST
    }

    if (ok)
    {
        unsigned char *alpha = new unsigned char[vid_size.width() * vid_size.height()];
        void const * alpha_ptr[] = {alpha};
        if (alpha)
        {
            memset(alpha, 255, vid_size.width() * vid_size.height());
            uint32_t pitch[1] = {vid_size.width()};
            vdp_st = vdp_bitmap_surface_put_bits_native(
                pipAlpha,
                alpha_ptr,
                pitch,
                NULL
            );
            CHECK_ST
            delete alpha;
        }
        else
            ok = false;
    }

    if (ok)
    {
        int width = vid_size.width();
        int height = vid_size.height();
        VdpVideoMixerParameter parameters[] = {
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
            VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE
        };

        void const * parameter_values[] = {
            &width,
            &height,
            &vdp_chroma_type
        };

        vdp_st = vdp_video_mixer_create(
            vdp_device,
            0,
            0,
            ARSIZE(parameters),
            parameters,
            parameter_values,
            &pipVideoMixer
        );
        CHECK_ST
        VERBOSE(VB_PLAYBACK, LOC + QString("Created VDPAU PiP (%1x%2)")
                .arg(width).arg(height));
    }

    pipLayer.struct_version = VDP_LAYER_VERSION;
    pipLayer.source_surface = pipOutputSurface;
    pipLayer.source_rect    = NULL;
    pipLayer.destination_rect = &pipPosition;

    return ok;
}

bool VDPAUContext::ShowPiP(VideoFrame * frame, QRect position)
{
    if (!frame)
        return false;

    bool ok = true;
    VdpStatus vdp_st;

    if (frame->width  != pipFrameSize.width() ||
        frame->height != pipFrameSize.height())
    {
        DeinitPip();
        ok = InitPiP(QSize(frame->width, frame->height));
    }

    if (!ok)
        return ok;

    uint32_t pitches[] = {
        frame->pitches[0],
        frame->pitches[2],
        frame->pitches[1]
    };
    void* const planes[] = {
        frame->buf,
        frame->buf + frame->offsets[2],
        frame->buf + frame->offsets[1]
    };
    vdp_st = vdp_video_surface_put_bits_y_cb_cr(
        pipVideoSurface,
        VDP_YCBCR_FORMAT_YV12,
        planes,
        pitches);
    CHECK_ST;

    VdpRect pip_rect;
    pip_rect.x0 = 0;
    pip_rect.y0 = 0;
    pip_rect.x1 = pipFrameSize.width();
    pip_rect.y1 = pipFrameSize.height();
    if (ok)
    {
        vdp_st = vdp_video_mixer_render(
            pipVideoMixer,
            VDP_INVALID_HANDLE,
            NULL,
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
            0,
            NULL,
            pipVideoSurface,
            0,
            NULL,
            NULL,
            pipOutputSurface,
            NULL,
            NULL,
            0,
            NULL
        );
        CHECK_ST
    }

    if (ok)
    {
        vdp_st = vdp_output_surface_render_bitmap_surface(
            pipOutputSurface,
            NULL,
            pipAlpha,
            NULL,
            NULL,
            &osd_blend,
            0
        );
        CHECK_ST
    }

    if (ok)
    {
        pipReady = 2; // for double rate deint
        pipPosition.x0 = position.left();
        pipPosition.y0 = position.top();
        pipPosition.x1 = position.left() + position.width();
        pipPosition.y1 = position.top() + position.height();
    }

    return ok;
}
