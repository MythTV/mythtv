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
#define NUM_REFERENCE_FRAMES 3

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

static const VdpOutputSurfaceRenderBlendState pip_blend =
    {
        VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ONE,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_FACTOR_ZERO,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD,
        VDP_OUTPUT_SURFACE_RENDER_BLEND_EQUATION_ADD
    };

VDPAUContext::VDPAUContext()
  : nextframedelay(0),      lastframetime(0),
    pix_fmt(-1),            maxVideoWidth(0),  maxVideoHeight(0),
    videoSurfaces(0),       surface_render(0), numSurfaces(0),
    videoSurface(0),        outputSurface(0),  decoder(0),
    videoMixer(0),          surfaceNum(0),     osdVideoSurface(0),
    osdOutputSurface(0),    osdVideoMixer(0),  osdAlpha(0),
    osdSize(QSize(0,0)),    osdReady(false),   deintAvail(false),
    deinterlacer("notset"), deinterlacing(false), currentFrameNum(-1),
    needDeintRefs(false),   useColorControl(false),
    pipOutputSurface(0),    pipAlpha(0),       pipBorder(0),
    pipClear(0),            pipReady(0),       pipLayerSize(QSize(0,0)),
    pipNeedsClear(false),   vdp_flip_target(NULL), vdp_flip_queue(NULL),
    vdpauDecode(false),     vdp_device(NULL),  errored(false)
{
    memset(outputSurfaces, 0, sizeof(outputSurfaces));
}

VDPAUContext::~VDPAUContext()
{
}

bool VDPAUContext::Init(Display *disp, Screen *screen,
                        Window win, QSize screen_size,
                        bool color_control, MythCodecID mcodecid)
{
    if ((kCodec_VDPAU_BEGIN < mcodecid) && (mcodecid < kCodec_VDPAU_END))
        vdpauDecode = true;

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
    DeinitPIPLayer();
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
        VDP_FUNC_ID_PRESENTATION_QUEUE_GET_TIME,
        (void **)&vdp_presentation_queue_get_time
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
        VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR,
        (void **)&vdp_presentation_queue_set_background_color
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

    float tmp = 2.0 / 255.0;
    VdpColor background;
    background.red = tmp;
    background.green = tmp;
    background.blue = tmp;
    background.alpha = 1.0f;

    if (ok)
    {
        vdp_st = vdp_presentation_queue_set_background_color(
            vdp_flip_queue,
            &background
        );
        CHECK_ST
    }

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
    int num_bufs = numbufs;

    // for software decode, create enough surfaces for deinterlacing
    // TODO only create when actually deinterlacing
    if (!vdpauDecode)
        num_bufs = NUM_REFERENCE_FRAMES;

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

    videoSurfaces = (VdpVideoSurface *)malloc(sizeof(VdpVideoSurface) * num_bufs);
    if (vdpauDecode)
    {
        surface_render = (vdpau_render_state_t*)malloc(sizeof(vdpau_render_state_t) * num_bufs);
        memset(surface_render, 0, sizeof(vdpau_render_state_t) * num_bufs);
    }

    numSurfaces = num_bufs;

    for (i = 0; i < num_bufs; i++)
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

        if (vdpauDecode)
        {
            surface_render[i].magic = MP_VDPAU_RENDER_MAGIC;
            surface_render[i].state = 0;
            surface_render[i].surface = videoSurfaces[i];
        }
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
            for (i = 0; i < num_bufs; i++)
            {
                vdp_video_surface_put_bits_y_cb_cr(
                    videoSurfaces[i],
                    VDP_YCBCR_FORMAT_YV12,
                    planes,
                    pitches
                );
            }
            delete [] tmp;
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

    if (videoSurfaces)
    {
        for (i = 0; i < numSurfaces; i++)
        {
            if (videoSurfaces[i])
            {
                vdp_st = vdp_video_surface_destroy(
                    videoSurfaces[i]);
                CHECK_ST
            }
        }
        free(videoSurfaces);
        videoSurfaces = NULL;
    }

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
    if (!vdp_output_surface_destroy)
        return;

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
    if (!vdpauDecode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("VDPAUContext::Decode called for cpu decode."));
        return;
    }

    VdpStatus vdp_st;
    bool ok = true;
    vdpau_render_state_t *render;

    if (frame->pix_fmt != pix_fmt)
    {
        uint32_t max_references = 2;

        if (frame->pix_fmt == PIX_FMT_VDPAU_H264_MAIN ||
            frame->pix_fmt == PIX_FMT_VDPAU_H264_HIGH)
        {
            uint32_t round_width = (frame->width + 15) & ~15;
            uint32_t round_height = (frame->height + 15) & ~15;
            uint32_t surf_size = (round_width * round_height * 3) / 2;
            max_references = (12 * 1024 * 1024) / surf_size;
            if (max_references > 16)
                max_references = 16;
        }

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
            max_references,
            &decoder
        );
        CHECK_ST

        if (ok)
        {
            pix_fmt = frame->pix_fmt;
            VERBOSE(VB_PLAYBACK, LOC +
                QString("Created VDPAU decoder (%1 ref frames)")
                .arg(max_references));
        }
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
                                QSize screen_size, FrameScanType scan,
                                bool pause_frame)
{
    VdpStatus vdp_st;
    bool ok = true;
    VdpTime dummy;
    vdpau_render_state_t *render;

    bool new_frame = true;
    bool deint = (deinterlacing && needDeintRefs && !pause_frame);
    if (deint && frame)
    {
        new_frame = UpdateReferenceFrames(frame);
        if (vdpauDecode && (referenceFrames.size() != NUM_REFERENCE_FRAMES))
            deint = false;
    }

    if (vdpauDecode && frame)
    {
        render = (vdpau_render_state_t *)frame->buf;
        if (!render)
            return;

        videoSurface = render->surface;
    }
    else if (new_frame && frame)
    {
        int surf = 0;
        if (deint)
            surf = (currentFrameNum + 1) % NUM_REFERENCE_FRAMES;

        videoSurface = videoSurfaces[surf];

        uint32_t pitches[3] = {
            frame->pitches[0],
            frame->pitches[2],
            frame->pitches[1]
        };
        void* const planes[3] = {
            frame->buf,
            frame->buf + frame->offsets[2],
            frame->buf + frame->offsets[1]
        };
        vdp_st = vdp_video_surface_put_bits_y_cb_cr(
            videoSurface,
            VDP_YCBCR_FORMAT_YV12,
            planes,
            pitches);
        CHECK_ST;
        if (!ok)
            return;
    }
    else if (!frame)
    {
        deint = false;
        if (!videoSurface)
            videoSurface = videoSurfaces[0];
    }

    if (outRect.x1 != (uint)screen_size.width() ||
        outRect.y1 != (uint)screen_size.height())
    {
        FreeOutput();
        InitOutput(screen_size);
    }

    // fix broken/missing negative rect clipping in vdpau
    if (display_video_rect.top() < 0 && display_video_rect.height() > 0)
    {
        float yscale = (float)video_rect.height() /
                       (float)display_video_rect.height();
        int tmp = video_rect.top() -
                  (int)((float)display_video_rect.top() * yscale);
        video_rect.setTop(max(0, tmp));
        display_video_rect.setTop(0);
    }

    if (display_video_rect.left() < 0 && display_video_rect.width() > 0)
    {
        float xscale = (float)video_rect.width() /
                       (float)display_video_rect.width();
        int tmp = video_rect.left() -
                  (int)((float)display_video_rect.left() * xscale);
        video_rect.setLeft(max(0, tmp));
        display_video_rect.setLeft(0);
    }

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

    VdpVideoSurface past_surfaces[2] = { VDP_INVALID_HANDLE,
                                         VDP_INVALID_HANDLE };
    VdpVideoSurface future_surfaces[1] = { VDP_INVALID_HANDLE };

    if (deint)
    {
        VdpVideoSurface refs[NUM_REFERENCE_FRAMES];
        for (int i = 0; i < NUM_REFERENCE_FRAMES; i++)
        {
            if (vdpauDecode)
            {
                vdpau_render_state_t *render;
                render = (vdpau_render_state_t *)referenceFrames[i]->buf;
                refs[i] = render ? render->surface : VDP_INVALID_HANDLE;
            }
            else
            {
                int ref = (currentFrameNum + i - 1) % NUM_REFERENCE_FRAMES;
                if (ref < 0)
                    ref = 0;
                refs[i] = videoSurfaces[ref];
            }
        }

        videoSurface = refs[1];
 
        if (scan == kScan_Interlaced)
        {
            // next field is in the current frame
            future_surfaces[0] = refs[1];
            // previous two fields are in the previous frame
            past_surfaces[0] = refs[0];
            past_surfaces[1] = refs[0];
        }
        else
        {
            // next field is in the next frame
            future_surfaces[0] = refs[2];
            // previous field is in the current frame
            past_surfaces[0] = refs[1];
            // field before that is in the previous frame
            past_surfaces[1] = refs[0];
        }
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
        &srcRect,
        outputSurface,
        &outRect,
        &outRectVid,
        num_layers,
        num_layers ? layers : NULL
    );
    CHECK_ST

    if (pipReady)
        pipReady--;
    pipNeedsClear = true;
}

void VDPAUContext::DisplayNextFrame(void)
{
    if (!outputSurface)
        return;

    VdpStatus vdp_st;
    bool ok = true;
    VdpTime now = 0;

    if (nextframedelay > 0)
    {
        vdp_st = vdp_presentation_queue_get_time(
            vdp_flip_queue,
            &now
        );
        CHECK_ST

        if (lastframetime == 0)
            lastframetime = now;

        now += nextframedelay * 1000;
    }

    vdp_st = vdp_presentation_queue_display(
        vdp_flip_queue,
        outputSurface,
        outRect.x1,
        outRect.y1,
        now
    );
    CHECK_ST

    surfaceNum = surfaceNum ^ 1;
}

void VDPAUContext::SetNextFrameDisplayTimeOffset(int delayus)
{
    nextframedelay = delayus;
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
        vdp_output_surface_destroy(osdOutputSurface);
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

    VdpVideoMixerFeature features[] = {
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL,
        VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL,
    };

    VdpBool temporal = interlaced;
    VdpBool spatial  = interlaced && deinterlacer.contains("advanced");
    const VdpBool * feature_values[] = {
        &temporal,
        &spatial,
    };

    vdp_st = vdp_video_mixer_set_feature_enables(
        videoMixer,
        ARSIZE(features),
        features,
        *feature_values
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
        if (deinterlacer.contains("advanced") ||
            deinterlacer.contains("basic"))
            needDeintRefs = true;
    }
    return deinterlacing;
}

bool VDPAUContext::UpdateReferenceFrames(VideoFrame *frame)
{
    if (frame->frameNumber == currentFrameNum)
        return false;

    currentFrameNum = frame->frameNumber;

    if (vdpauDecode)
    {
        while (referenceFrames.size() > (NUM_REFERENCE_FRAMES - 1))
            referenceFrames.pop_front();
        referenceFrames.push_back(frame);
    }

    return true;
}

bool VDPAUContext::IsBeingUsed(VideoFrame *frame)
{
    if (!frame || !vdpauDecode)
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
            VERBOSE(VB_IMPORTANT,
                QString("VDPAU WARNING: %1 GPU decode not fully supported"
                        " - playback may fail.")
                        .arg(toString(myth_codec_id)));
        }
        else if (!support)
        {
            VERBOSE(VB_PLAYBACK, LOC +
                QString("%1 GPU decode not supported")
                .arg(toString(myth_codec_id)));
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

bool VDPAUContext::SetPictureAttributes(void)
{
    bool ok = true;
    VdpStatus vdp_st;

    if (!videoMixer || !useColorControl)
        return false;

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

void VDPAUContext::DeinitPIPLayer(void)
{
    while (!pips.empty())
    {
        DeinitPIP(pips.begin().key());
        pips.erase(pips.begin());
    }

    if (pipOutputSurface)
    {
        vdp_output_surface_destroy(pipOutputSurface);
        pipOutputSurface = 0;
    }

    if (pipAlpha)
    {
        vdp_bitmap_surface_destroy(pipAlpha);
        pipAlpha = 0;
    }

    if (pipBorder)
    {
        vdp_bitmap_surface_destroy(pipBorder);
        pipBorder = 0;
    }

    if (pipClear)
    {
        vdp_bitmap_surface_destroy(pipClear);
        pipClear = 0;
    }

    pipLayerSize = QSize(0, 0);
    pipReady     = 0;
}

bool VDPAUContext::InitPIPLayer(QSize screen_size)
{
    if (pipLayerSize != screen_size)
        DeinitPIPLayer();

    bool ok = true;
    VdpStatus vdp_st;

    if (!pipOutputSurface)
    {
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            screen_size.width(),
            screen_size.height(),
            &pipOutputSurface
        );
        CHECK_ST

        pipLayer.struct_version = VDP_LAYER_VERSION;
        pipLayer.source_surface = pipOutputSurface;
        pipLayer.source_rect    = NULL;
        pipLayer.destination_rect = NULL;
    }

    if (!pipAlpha && ok)
    {
        vdp_st = vdp_bitmap_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_A8,
            1, 1,
            false,
            &pipAlpha
        );
        CHECK_ST

        if (ok)
        {
            unsigned char alpha = 255;
            void const * alpha_ptr[] = {&alpha};
            uint32_t pitch[1] = {1};
            vdp_st = vdp_bitmap_surface_put_bits_native(
                pipAlpha,
                alpha_ptr,
                pitch,
                NULL
            );
            CHECK_ST
        }
    }

    if (!pipBorder && ok)
    {
        vdp_st = vdp_bitmap_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_R8G8B8A8,
            1, 1,
            false,
            &pipBorder
        );
        CHECK_ST

        if (ok)
        {
            unsigned char red[] = {127, 0, 0, 255};
            void const * red_ptr[] = {&red};
            uint32_t pitch[1] = {1};
            vdp_st = vdp_bitmap_surface_put_bits_native(
                pipBorder,
                red_ptr,
                pitch,
                NULL
            );
            CHECK_ST
        }
    }

    if (!pipClear && ok)
    {
        vdp_st = vdp_bitmap_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_R8G8B8A8,
            1, 1,
            false,
            &pipClear
        );
        CHECK_ST

        if (ok)
        {
            unsigned char blank[] = {0, 0, 0, 0};
            void const * blank_ptr[] = {&blank};
            uint32_t pitch[1] = {1};
            vdp_st = vdp_bitmap_surface_put_bits_native(
                pipClear,
                blank_ptr,
                pitch,
                NULL
            );
            CHECK_ST
        }
    }

    ok &= (pipBorder && pipAlpha && pipOutputSurface && pipClear);

    if (ok && pipNeedsClear)
    {
        pipLayerSize = screen_size;
        vdp_st = vdp_output_surface_render_bitmap_surface(
            pipOutputSurface,
            NULL,
            pipClear,
            NULL,
            NULL,
            &pip_blend,
            0
        );
        CHECK_ST
        pipNeedsClear = false;
    }

    return ok;
}

void VDPAUContext::DeinitPIP(NuppelVideoPlayer *pipplayer)
{
    if (!pips.contains(pipplayer))
        return;

    if (pips[pipplayer].videoSurface)
        vdp_video_surface_destroy(pips[pipplayer].videoSurface);

    if (pips[pipplayer].videoMixer)
        vdp_video_mixer_destroy(pips[pipplayer].videoMixer);

    pips.remove(pipplayer);
    VERBOSE(VB_PLAYBACK, LOC + "Removed 1 PIP");
}

bool VDPAUContext::InitPIP(NuppelVideoPlayer *pipplayer,
                           QSize vid_size)
{
    if (pips.contains(pipplayer))
        return false;

    bool ok = true;
    VdpStatus vdp_st;

    VdpVideoSurface tmp_surface;
    VdpVideoMixer   tmp_mixer;

    vdp_st = vdp_video_surface_create(
        vdp_device,
        vdp_chroma_type,
        vid_size.width(),
        vid_size.height(),
        &tmp_surface
    );
    CHECK_ST

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
            &tmp_mixer
        );
        CHECK_ST
    }

    if (ok && tmp_mixer && tmp_surface)
    {
        vdpauPIP tmp = {vid_size, tmp_surface, tmp_mixer};
        pips[pipplayer] = tmp;
        VERBOSE(VB_PLAYBACK, LOC + QString("Created VDPAU PIP (%1x%2)")
                .arg(vid_size.width()).arg(vid_size.height()));
    }
    else
    {
        DeinitPIP(pipplayer);
        ok = false;
    }

    return ok;
}

bool VDPAUContext::ShowPIP(NuppelVideoPlayer *pipplayer,
                           VideoFrame * frame, QRect position,
                           bool pip_is_active)
{
    if (!frame || !pipplayer)
        return false;

    bool ok = true;
    VdpStatus vdp_st;

    QSize vid_size = QSize(frame->width, frame->height);

    if (pips.contains(pipplayer) &&
        pips[pipplayer].videoSize != vid_size)
    {
        DeinitPIP(pipplayer);
    }

    if (!pips.contains(pipplayer))
    {
        ok = InitPIP(pipplayer, vid_size);
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
        pips[pipplayer].videoSurface,
        VDP_YCBCR_FORMAT_YV12,
        planes,
        pitches);
    CHECK_ST;

    VdpRect pip_rect;
    pip_rect.x0 = position.left();
    pip_rect.y0 = position.top();
    pip_rect.x1 = position.left() + position.width();
    pip_rect.y1 = position.top() + position.height();

    if (pip_is_active && ok)
    {
        VdpRect pip_active_rect;
        pip_active_rect.x0 = pip_rect.x0 - 10;
        pip_active_rect.y0 = pip_rect.y0 - 10;
        pip_active_rect.x1 = pip_rect.x1 + 10;
        pip_active_rect.y1 = pip_rect.y1 + 10;

        vdp_st = vdp_output_surface_render_bitmap_surface(
            pipOutputSurface,
            &pip_active_rect,
            pipBorder,
            NULL,
            NULL,
            &pip_blend,
            0
        );
        CHECK_ST
    }

    if (ok)
    {
        vdp_st = vdp_video_mixer_render(
            pips[pipplayer].videoMixer,
            VDP_INVALID_HANDLE,
            NULL,
            VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
            0,
            NULL,
            pips[pipplayer].videoSurface,
            0,
            NULL,
            NULL,
            pipOutputSurface,
            &pip_rect,
            &pip_rect,
            0,
            NULL
        );
        CHECK_ST
    }

    if (ok)
    {
        vdp_st = vdp_output_surface_render_bitmap_surface(
            pipOutputSurface,
            &pip_rect,
            pipAlpha,
            NULL,
            NULL,
            &osd_blend,
            0
        );
        CHECK_ST
    }

    if (ok)
        pipReady = 2; // for double rate deint

    return ok;
}

