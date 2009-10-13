#include <cstdio>
#include <cstdlib>

#include <algorithm>
using namespace std;

#include <QSize>
#include <QRect>

#include "mythcontext.h"
extern "C" {
#include "frame.h"
#include "avutil.h"
#include "libavcodec/vdpau.h"
}

#include "videoouttypes.h"
#include "mythcodecid.h"
#include "mythxdisplay.h"
#include "util-vdpau.h"

#define LOC QString("VDPAU: ")
#define LOC_ERR QString("VDPAU Error: ")

#define MIN_OUTPUT_SURFACES 2
#define MAX_OUTPUT_SURFACES 4
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

static void vdpau_preemption_callback(VdpDevice device, void *vdpau_ctx)
{
    (void)device;
    VERBOSE(VB_IMPORTANT, LOC_ERR + QString("DISPLAY PRE-EMPTED. Aborting playback."));
    VDPAUContext *ctx = (VDPAUContext*)vdpau_ctx;
    if (ctx)
        ctx->SetErrored(kError_Preempt, 1000);
}

VDPAUContext::VDPAUContext()
  : nextframedelay(0),      lastframetime(0),
    pix_fmt(-1),            checkVideoSurfaces(8), pause_surface(0),
    outputSurface(0),       checkOutputSurfaces(false),
    outputSize(QSize(0,0)), decoder(0),        maxReferences(2),
    videoMixer(0),          mixerFeatures(0),  videoSize(QSize(0,0)),
    letterboxColour(kLetterBoxColour_Black),      surfaceNum(0),
    osdVideoSurface(0),     osdOutputSurface(0),  osdVideoMixer(0),
    osdAlpha(0),            osdReady(false),      osdSize(QSize(0,0)),
    deinterlacer("notset"), deinterlacing(false), currentFrameNum(-1),
    needDeintRefs(false),   deintLock(QMutex::Recursive), skipChroma(0),
    numRefs(0),             denoise(0.0f),          sharpen(0.0f),
    useColorControl(false), pipOutputSurface(0),
    pipAlpha(0),            pipBorder(0),
    pipClear(0),            pipReady(0),       pipNeedsClear(false),
    vdp_flip_target(0),     vdp_flip_queue(0), vdpauDecode(false),
    vdp_device(0),          errorCount(0),     errorState(kError_None),
    vdp_get_proc_address(NULL),       vdp_device_destroy(NULL),
    vdp_get_error_string(NULL),       vdp_get_api_version(NULL),
    vdp_get_information_string(NULL), vdp_video_surface_create(NULL),
    vdp_video_surface_destroy(NULL),  vdp_video_surface_put_bits_y_cb_cr(NULL),
    vdp_video_surface_get_bits_y_cb_cr(NULL),
    vdp_video_surface_query_get_put_bits_y_cb_cr_capabilities(NULL),
    vdp_video_surface_query_capabilities(NULL),
    vdp_output_surface_put_bits_y_cb_cr(NULL),
    vdp_output_surface_put_bits_native(NULL), vdp_output_surface_create(NULL),
    vdp_output_surface_destroy(NULL),
    vdp_output_surface_render_bitmap_surface(NULL),
    vdp_output_surface_query_capabilities(NULL), vdp_video_mixer_create(NULL),
    vdp_video_mixer_set_feature_enables(NULL), vdp_video_mixer_destroy(NULL),
    vdp_video_mixer_render(NULL), vdp_video_mixer_set_attribute_values(NULL),
    vdp_video_mixer_query_feature_support(NULL),
    vdp_video_mixer_query_attribute_support(NULL),
    vdp_video_mixer_query_parameter_support(NULL),
    vdp_generate_csc_matrix(NULL),
    vdp_presentation_queue_target_destroy(NULL),
    vdp_presentation_queue_create(NULL),
    vdp_presentation_queue_destroy(NULL), vdp_presentation_queue_display(NULL),
    vdp_presentation_queue_block_until_surface_idle(NULL),
    vdp_presentation_queue_target_create_x11(NULL),
    vdp_presentation_queue_query_surface_status(NULL),
    vdp_presentation_queue_get_time(NULL),
    vdp_presentation_queue_set_background_color(NULL),
    vdp_decoder_create(NULL), vdp_decoder_destroy(NULL),
    vdp_decoder_render(NULL), vdp_bitmap_surface_create(NULL),
    vdp_bitmap_surface_destroy(NULL), vdp_bitmap_surface_put_bits_native(NULL),
    vdp_bitmap_surface_query_capabilities(NULL),
    vdp_preemption_callback_register(NULL)
{
    memset(&outRect, 0, sizeof(outRect));
    memset(&outRectVid, 0, sizeof(outRectVid));
    memset(&osdLayer, 0, sizeof(osdLayer));
    memset(&osdRect, 0, sizeof(osdRect));
    memset(&proCamp, 0, sizeof(proCamp));
    memset(&pipLayer, 0, sizeof(pipLayer));
}

VDPAUContext::~VDPAUContext()
{
}

bool VDPAUContext::Init(MythXDisplay *disp, Window win, QSize screen_size,
                        bool color_control, int color_key,
                        MythCodecID mcodecid, QString options)
{
    outputSize = screen_size;
    ParseOptions(options);

    if (codec_is_vdpau(mcodecid))
        vdpauDecode = true;

    bool ok;

    ok = InitProcs(disp);
    if (!ok)
        return ok;

    XLOCK(disp, ok = InitFlipQueue(win, color_key));
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
    outputSize =  QSize(0,0);
    errorCount = 0;
    errorState = kError_None;
}

static const char* dummy_get_error_string(VdpStatus status)
{
    static const char dummy[] = "Unknown";
    return &dummy[0];
}

bool VDPAUContext::InitProcs(MythXDisplay *disp)
{
    VdpStatus vdp_st;
    bool ok = true;
    vdp_get_error_string = &dummy_get_error_string;

    XLOCK(disp, vdp_st = vdp_device_create_x11(
        disp->GetDisplay(),
        disp->GetScreen(),
        &vdp_device,
        &vdp_get_proc_address
    ));
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

    // non-fatal callback registration
    vdp_get_proc_address(
        vdp_device,
        VDP_FUNC_ID_PREEMPTION_CALLBACK_REGISTER,
        (void **)&vdp_preemption_callback_register
    );

    if (vdp_preemption_callback_register)
    {
        vdp_preemption_callback_register(
            vdp_device,
            &vdpau_preemption_callback,
            (void*)this
        );
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
    if (vdp_device && vdp_device_destroy)
    {
        vdp_device_destroy(vdp_device);
        vdp_device = 0;
    }
}

bool VDPAUContext::InitFlipQueue(Window win, int color_key)
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

    VdpColor background;
    background.red   = (float)((color_key & 0xFF0000) >> 16) / 255.0f;
    background.green = (float)((color_key & 0xFF00) >> 8) / 255.0f;
    background.blue  = (float)(color_key & 0xFF) / 255.0f;
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

int VDPAUContext::AddBuffer(int width, int height)
{
    VdpStatus vdp_st;
    bool ok = true;

    video_surface tmp;
    tmp.surface = 0;

    vdp_st = vdp_video_surface_create(
        vdp_device,
        vdp_chroma_type,
        width,
        height,
        &(tmp.surface)
    );
    CHECK_ST

    if (!ok || !tmp.surface)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create video surface."));
        return -1;
    }

    if (vdpauDecode)
    {
        struct vdpau_render_state new_rend;
        memset(&new_rend, 0, sizeof(struct vdpau_render_state));
        new_rend.state = 0;
        new_rend.surface = tmp.surface;
        tmp.render = new_rend;
    }

    videoSurfaces.push_back(tmp);
    return GetNumBufs() - 1;
}

bool VDPAUContext::InitBuffers(int width, int height, int numbufs,
                               LetterBoxColour letterbox_colour)
{
    int num_bufs = numbufs;

    // for software decode, create enough surfaces for deinterlacing
    // TODO only create when actually deinterlacing
    if (!vdpauDecode)
        num_bufs = NUM_REFERENCE_FRAMES;

    int i;
    for (i = 0; i < num_bufs; i++)
    {
        int tmp = AddBuffer(width, height);
        if (tmp != i)
        {
            VERBOSE(VB_IMPORTANT, LOC +
                QString("Failed to add buffer %1 of %2")
                .arg(i+1).arg(num_bufs));
            return false;
        }
    }
    pause_surface = videoSurfaces[0].surface;

    // clear video surfaces to black
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
                videoSurfaces[i].surface,
                VDP_YCBCR_FORMAT_YV12,
                planes,
                pitches
            );
        }
        delete [] tmp;
    }

    letterboxColour = letterbox_colour;
    videoMixer = CreateMasterMixer(width, height);

    // minimise green screen
    if (videoMixer)
        ClearScreen();
    return videoMixer;
}

VdpVideoMixer VDPAUContext::CreateMasterMixer(int width, int height)
{
    videoSize = QSize(width, height);
    VdpVideoMixer ret = CreateMixer(width, height, 2, mixerFeatures);

    if (!ret)
        return ret;

    int count = 0;
    VdpVideoMixerAttribute attributes[4];
    void const *values[4];

    VdpColor gray;
    gray.red = 0.5f;
    gray.green = 0.5f;
    gray.blue = 0.5f;
    gray.alpha = 1.0f;

    if (letterboxColour == kLetterBoxColour_Gray25)
    {
        attributes[count] =
            VDP_VIDEO_MIXER_ATTRIBUTE_BACKGROUND_COLOR;
        values[count] = &gray;
        count++;
    }

    if (skipChroma)
    {
        attributes[count] =
            VDP_VIDEO_MIXER_ATTRIBUTE_SKIP_CHROMA_DEINTERLACE;
        values[count] = &skipChroma;
        count++;
    }

    if ((denoise != 0.0f) && (mixerFeatures & kVDP_FEAT_DENOISE))
    {
        attributes[count] =
            VDP_VIDEO_MIXER_ATTRIBUTE_NOISE_REDUCTION_LEVEL;
        values[count] = &denoise;
        count++;
    }

    if ((sharpen != 0.0f) && (mixerFeatures & kVDP_FEAT_SHARPNESS))
    {
        attributes[count] =
            VDP_VIDEO_MIXER_ATTRIBUTE_SHARPNESS_LEVEL;
        values[count] = &sharpen;
        count++;
    }

    if (count)
    {
        VdpStatus vdp_st;
        bool ok = true;
        vdp_st = vdp_video_mixer_set_attribute_values(
                    ret, count, attributes, values);
        CHECK_ST
    }

    return ret;
}

VdpVideoMixer VDPAUContext::CreateMixer(int width, int height, uint32_t layers,
                                        int feats)
{
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
        &layers
    };

    int count = 0;
    VdpVideoMixerFeature feat[5];
    VdpBool enable = true;
    const VdpBool enables[5] = { enable, enable, enable, enable, enable };
    bool temporal = (feats & kVDP_FEAT_TEMPORAL) || (feats & kVDP_FEAT_SPATIAL);

    if (temporal)
    {
        feat[count] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL;
        count++;
    }

    if (feats & kVDP_FEAT_SPATIAL)
    {
        feat[count] = VDP_VIDEO_MIXER_FEATURE_DEINTERLACE_TEMPORAL_SPATIAL;
        count++;
    }

    if ((feats & kVDP_FEAT_IVTC) && temporal)
    {
        feat[count] = VDP_VIDEO_MIXER_FEATURE_INVERSE_TELECINE;
        count++;
    }

    if (feats & kVDP_FEAT_DENOISE)
    {
        feat[count] = VDP_VIDEO_MIXER_FEATURE_NOISE_REDUCTION;
        count++;
    }

    if (feats & kVDP_FEAT_SHARPNESS)
    {
        feat[count] = VDP_VIDEO_MIXER_FEATURE_SHARPNESS;
        count++;
    }

    VdpStatus vdp_st;
    bool ok = true;
    VdpVideoMixer tmp;
    vdp_st = vdp_video_mixer_create(
        vdp_device, count, count ? feat : NULL, 4, parameters,
        parameter_values, &tmp
    );
    CHECK_ST

    if (!ok || !tmp)
        VERBOSE(VB_IMPORTANT, LOC + QString("Failed to create video mixer."));
    else
    {
        vdp_st = vdp_video_mixer_set_feature_enables(
            tmp, count, count ? feat : NULL, count ? enables : NULL);
        CHECK_ST
    }

    return tmp;
}

void VDPAUContext::FreeBuffers(void)
{
    VdpStatus vdp_st;
    bool ok = true;

    int i;

    if (videoMixer)
    {
        vdp_st = vdp_video_mixer_destroy(videoMixer);
        videoMixer = 0;
        CHECK_ST
    }

    if (videoSurfaces.size())
    {
        for (i = 0; i < GetNumBufs(); i++)
        {
            if (videoSurfaces[i].surface)
            {
                vdp_st = vdp_video_surface_destroy(
                    videoSurfaces[i].surface);
                CHECK_ST
            }
        }
        videoSurfaces.clear();
    }
    checkVideoSurfaces = 8;
    pause_surface = 0;
}

bool VDPAUContext::InitOutput(QSize size)
{
    VdpStatus vdp_st;
    bool ok = true;

    for (int i = 0; i < MIN_OUTPUT_SURFACES; i++)
    {
        VdpOutputSurface tmp;
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            size.width(),
            size.height(),
            &tmp
        );
        CHECK_ST

        if (!ok)
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR +
                QString("Failed to create output surface."));
            return false;
        }
        outputSurfaces.push_back(tmp);
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
    uint i;

    for (i = 0; i < outputSurfaces.size(); i++)
    {
        if (outputSurfaces[i])
        {
            vdp_st = vdp_output_surface_destroy(
                outputSurfaces[i]);
            CHECK_ST
        }
    }
    outputSurfaces.clear();
    checkOutputSurfaces = false;
}

void VDPAUContext::Decode(VideoFrame *frame)
{
    if (!vdpauDecode)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("VDPAUContext::Decode called for cpu decode."));
        SetErrored(kError_Unknown);
        return;
    }

    VdpStatus vdp_st;
    bool ok = true;
    struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;

    if (!render)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("No video surface to decode to."));
        SetErrored(kError_Unknown);
        return;
    }

    if (frame->pix_fmt != pix_fmt)
    {
        if (decoder)
        {
            VERBOSE(VB_IMPORTANT, LOC_ERR +
                QString("Picture format has changed."));
            SetErrored(kError_Unknown);
            return;
        }

        if (frame->pix_fmt == PIX_FMT_VDPAU_H264)
        {
            maxReferences = render->info.h264.num_ref_frames;
            if (maxReferences < 1 || maxReferences > 16)
            {
                uint32_t round_width = (frame->width + 15) & ~15;
                uint32_t round_height = (frame->height + 15) & ~15;
                uint32_t surf_size = (round_width * round_height * 3) / 2;
                maxReferences = (12 * 1024 * 1024) / surf_size;
            }
            if (maxReferences > 16)
                maxReferences = 16;
        }

        VdpDecoderProfile vdp_decoder_profile;
        switch (frame->pix_fmt)
        {
            case PIX_FMT_VDPAU_MPEG1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG1;
                break;
            case PIX_FMT_VDPAU_MPEG2:
                vdp_decoder_profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
                break;
            case PIX_FMT_VDPAU_H264:
                vdp_decoder_profile = VDP_DECODER_PROFILE_H264_HIGH;
                break;
            case PIX_FMT_VDPAU_WMV3:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_MAIN;
                break;
            case PIX_FMT_VDPAU_VC1:
                vdp_decoder_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
                break;
            default:
                VERBOSE(VB_IMPORTANT, LOC_ERR +
                    QString("Picture format is not supported."));
                SetErrored(kError_Unknown);
                return;
        }

        // generic capability pre-checked but specific profile may now fail
        vdp_st = vdp_decoder_create(
            vdp_device,
            vdp_decoder_profile,
            frame->width,
            frame->height,
            maxReferences,
            &decoder
        );
        CHECK_ST

        if (ok && decoder)
        {
            pix_fmt = frame->pix_fmt;
            VERBOSE(VB_PLAYBACK, LOC +
                QString("Created VDPAU decoder (%1 ref frames)")
                .arg(maxReferences));
        }
        else
        {
            VERBOSE(VB_PLAYBACK, LOC_ERR + QString("Failed to create decoder."));
            SetErrored(kError_Decode);
            return;
        }
    }
    else if (!decoder)
    {
        VERBOSE(VB_IMPORTANT, LOC_ERR +
            QString("Pix format already set but no VDPAU decoder."));
            SetErrored(kError_Unknown);
        return;
    }

    vdp_st = vdp_decoder_render(
        decoder,
        render->surface,
        (VdpPictureInfo const *)&(render->info),
        render->bitstream_buffers_used,
        render->bitstream_buffers
    );
    CHECK_ST

    if (ok && (errorCount > 0))
        errorCount--;
    else if (!ok)
    {
        SetErrored(kError_Unknown);
    }
}

void VDPAUContext::UpdatePauseFrame(VideoFrame *frame)
{
    if (vdpauDecode)
    {
        struct vdpau_render_state *render = (struct vdpau_render_state *)frame->buf;
        if (render)
        {
            pause_surface = render->surface;
            return;
        }
    }
    else
    {
        VdpStatus vdp_st;
        bool ok = true;
        pause_surface = videoSurfaces[0].surface;
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
            pause_surface,
            VDP_YCBCR_FORMAT_YV12,
            planes,
            pitches);
        CHECK_ST;
        if (ok)
            return;
    }

    VERBOSE(VB_PLAYBACK, LOC + QString("Failed to update pause surface."));
    return;
}

void VDPAUContext::PrepareVideo(VideoFrame *frame, QRect video_rect,
                                QRect display_video_rect,
                                QSize screen_size, FrameScanType scan)
{
    if (checkVideoSurfaces == 1)
        checkOutputSurfaces = true;

    if (checkVideoSurfaces > 0)
        checkVideoSurfaces--;

    QMutexLocker locker(&deintLock);

    VdpStatus vdp_st;
    bool ok = true;
    VdpTime dummy;
    struct vdpau_render_state *render;
    VdpVideoSurface video_surface = 0;

    // FIXME for 0.23. This should be triggered from AFD by a seek
    if (frame && abs(frame->frameNumber - currentFrameNum) > 8)
        ClearReferenceFrames();

    bool new_frame = true;
    bool deint = (deinterlacing && needDeintRefs && frame);

    if (deint)
    {
        new_frame = UpdateReferenceFrames(frame);
        if ((vdpauDecode && (referenceFrames.size() != NUM_REFERENCE_FRAMES)) ||
           (!vdpauDecode && (numRefs != NUM_REFERENCE_FRAMES)))
            deint = false;
    }

    if (vdpauDecode && frame)
    {
        render = (struct vdpau_render_state *)frame->buf;
        if (!render)
            return;

        video_surface = render->surface;
    }
    else if (new_frame && frame)
    {
        int surf = 0;
        if (deint)
            surf = (currentFrameNum + 1) % NUM_REFERENCE_FRAMES;

        video_surface = videoSurfaces[surf].surface;

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
            video_surface,
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
        video_surface = pause_surface;
    }

    if (!video_surface)
        video_surface = videoSurfaces[0].surface;

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
    {
        field = frame->top_field_first ?
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD :
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD;
    }
    else if (scan == kScan_Intr2ndField && deinterlacing)
    {
        field = frame->top_field_first ?
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_BOTTOM_FIELD :
                VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }
    else if (!frame && deinterlacing)
    {
        field = VDP_VIDEO_MIXER_PICTURE_STRUCTURE_TOP_FIELD;
    }

    outputSurface = outputSurfaces[surfaceNum];
    usleep(2000);
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
                refs[i] = VDP_INVALID_HANDLE;
                render = (struct vdpau_render_state *)referenceFrames[i]->buf;
                if (render && render->surface)
                    refs[i] = render->surface;
            }
            else
            {
                int ref = (currentFrameNum + i - 1) % NUM_REFERENCE_FRAMES;
                if (ref < 0)
                    ref = 0;
                refs[i] = videoSurfaces[ref].surface;
            }
        }

        video_surface = refs[1];

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
        video_surface,
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

    surfaceNum++;
    if (surfaceNum >= (int)(outputSurfaces.size()))
        surfaceNum = 0;;

    if (checkOutputSurfaces)
        AddOutputSurfaces();
}

void VDPAUContext::AddOutputSurfaces(void)
{
    checkOutputSurfaces = false;
    VdpStatus vdp_st;
    bool ok = true;

    int cnt = 0;
    int extra = MAX_OUTPUT_SURFACES - outputSurfaces.size();
    if (extra <= 0)
        return;

    for (int i = 0; i < extra; i++)
    {
        VdpOutputSurface tmp;
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            outputSize.width(),
            outputSize.height(),
            &tmp
        );
        // suppress non-fatal error messages
        ok &= (vdp_st == VDP_STATUS_OK);

        if (!ok)
            break;

        outputSurfaces.push_back(tmp);
        cnt++;
    }
    VERBOSE(VB_PLAYBACK, LOC + QString("Using %1 output surfaces (max %2)")
        .arg(outputSurfaces.size()).arg(MAX_OUTPUT_SURFACES));
}

void VDPAUContext::SetNextFrameDisplayTimeOffset(int delayus)
{
    nextframedelay = delayus;
}

bool VDPAUContext::InitOSD(QSize size)
{
    if (!vdp_device)
        return false;

    VdpStatus vdp_st;
    bool ok = true;

    uint width = size.width();
    uint height = size.height();

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
            true,
            &osdAlpha
        );
        CHECK_ST
    }

    if (!ok)
    {
        VERBOSE(VB_PLAYBACK, LOC_ERR +
            QString("Failed to create bitmap surface."));
    }
    else
    {
        osdVideoMixer = CreateMixer(width, height);
        ok = osdVideoMixer;
    }

    if (ok)
    {
        osdSize = size;
        osdRect.x0 = 0;
        osdRect.y0 = 0;
        osdRect.x1 = width;
        osdRect.y1 = height;
        osdLayer.struct_version = VDP_LAYER_VERSION;
        osdLayer.source_surface = osdOutputSurface;
        osdLayer.source_rect    = &osdRect;
        osdLayer.destination_rect = &osdRect;
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
    {
        DeinitOSD();
        if (!InitOSD(size))
            return;
    }

    VdpStatus vdp_st;
    bool ok = true;

    // upload OSD YV12 data
    uint32_t pitches[3] = {osdSize.width(),
                           osdSize.width()>>1,
                           osdSize.width()>>1};
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
        uint32_t pitch[1] = {osdSize.width()};
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
    osdSize = QSize(0,0);
}

bool VDPAUContext::SetDeinterlacer(const QString &deint)
{
    QMutexLocker locker(&deintLock);
    deinterlacer = deint;
    deinterlacer.detach();
    return true;
}

bool VDPAUContext::SetDeinterlacing(bool interlaced)
{
    QMutexLocker locker(&deintLock);

    if (!deinterlacer.contains("vdpau"))
        interlaced = false;

    bool spatial  = deinterlacer.contains("advanced");
    bool temporal = deinterlacer.contains("basic") || spatial;
    int features  = mixerFeatures;
    needDeintRefs = spatial || temporal;

    if (temporal)
    {
        features = interlaced ?
            features |  kVDP_FEAT_TEMPORAL :
            features & ~kVDP_FEAT_TEMPORAL;
    }

    if (spatial)
    {
        features = interlaced ?
            features |  kVDP_FEAT_SPATIAL :
            features & ~kVDP_FEAT_SPATIAL;
    }

    if (features == mixerFeatures)
    {
        deinterlacing = interlaced;
        return deinterlacing;
    }

    vdp_video_mixer_destroy(videoMixer);
    mixerFeatures = features;
    videoMixer = CreateMasterMixer(videoSize.width(), videoSize.height());
    SetPictureAttributes();
    deinterlacing = interlaced && videoMixer;

    if (!deinterlacing)
        ClearReferenceFrames();

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
    else
    {
        if (numRefs < NUM_REFERENCE_FRAMES)
            numRefs++;
    }

    return true;
}

bool VDPAUContext::IsBeingUsed(VideoFrame *frame)
{
    if (!frame || !vdpauDecode)
        return false;

    return referenceFrames.contains(frame);
}

void VDPAUContext::ClearReferenceFrames(void)
{
    deintLock.lock();
    referenceFrames.clear();
    numRefs = 0;
    deintLock.unlock();
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
    float new_val;
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
            new_val = (newValue * 0.062831853f);
            if (new_val > 3.14159265f)
                new_val -= 6.2831853f;
            proCamp.hue = new_val;
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
        videoSurfaces[0].surface,
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

    pipReady     = 0;
}

bool VDPAUContext::InitPIPLayer(QSize screen_size)
{
    if (outputSize != screen_size)
        DeinitPIPLayer();

    bool ok = true;
    VdpStatus vdp_st;

    if (!pipOutputSurface)
    {
        vdp_st = vdp_output_surface_create(
            vdp_device,
            VDP_RGBA_FORMAT_B8G8R8A8,
            outputSize.width(),
            outputSize.height(),
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
            true,
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
            true,
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
            true,
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

void VDPAUContext::DeinitPIP(NuppelVideoPlayer *pipplayer, bool check_layer)
{
    if (!pips.contains(pipplayer))
        return;

    if (pips[pipplayer].videoSurface)
        vdp_video_surface_destroy(pips[pipplayer].videoSurface);

    if (pips[pipplayer].videoMixer)
        vdp_video_mixer_destroy(pips[pipplayer].videoMixer);

    pips.remove(pipplayer);
    VERBOSE(VB_PLAYBACK, LOC + "Removed 1 PIP");

    if (pips.size() < 1 && check_layer)
    {
        DeinitPIPLayer();
        VERBOSE(VB_PLAYBACK, LOC + "Removed PIP Layer");
    }
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
        tmp_mixer = CreateMixer(vid_size.width(), vid_size.height());

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
        DeinitPIP(pipplayer, false);
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

void VDPAUContext::ParseOptions(QString options)
{
    QStringList list = options.split(",");
    if (list.empty())
        return;

    for (QStringList::Iterator i = list.begin(); i != list.end(); ++i)
    {
        QString name = (*i).section('=', 0, 0);
        QString opts = (*i).section('=', 1);

        if (name.contains("vdpauivtc"))
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Enabling VDPAU inverse telecine - "
                            "requires Basic or Advanced deinterlacer."));
            mixerFeatures |= kVDP_FEAT_IVTC;
        }

        if (name.contains("vdpauskipchroma"))
        {
            VERBOSE(VB_PLAYBACK, LOC +
                    QString("Enabling SkipChromaDeinterlace."));
            skipChroma = 1;
        }

        if (name.contains("vdpaudenoise"))
        {
            float tmp = std::max(0.0f, std::min(1.0f, opts.toFloat()));
            if (tmp != 0.0)
            {
                VERBOSE(VB_PLAYBACK, LOC +
                    QString("VDPAU Denoise %1").arg(tmp,4,'f',2,'0'));
                denoise = tmp;
                mixerFeatures |= kVDP_FEAT_DENOISE;
            }
        }

        if (name.contains("vdpausharpen"))
        {
            float tmp = std::max(-1.0f, std::min(1.0f, opts.toFloat()));
            if (tmp != 0.0)
            {
                VERBOSE(VB_PLAYBACK, LOC +
                    QString("VDPAU Sharpen %1").arg(tmp,4,'f',2,'0'));
                sharpen = tmp;
                mixerFeatures |= kVDP_FEAT_SHARPNESS;
            }
        }
    }
}
