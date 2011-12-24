#include <QSize>

#include <strings.h>
#include <dlfcn.h>

#include "mythmainwindow.h"
#include "mythrender_vdpau.h"

#include "openglsupport.h"
#include "mythlogging.h"
#include "mythxdisplay.h"
#include "vdpauvideodecoder.h"
#include "mythcodecid.h"
#include "videoconsumer.h"
#include "openclinterface.h"
#include "mythuihelper.h"
#include "videosurface.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/vdpau.h"
#include "libavutil/avutil.h"
#include "libavformat/avformat.h"
}

#include <CL/opencl.h>


static const char* dummy_get_error_string(VdpStatus status);

static const char* dummy_get_error_string(VdpStatus status)
{
    static const char dummy[] = "Unknown";
    return &dummy[0];
}

static AVCodec *find_vdpau_decoder(AVCodec *c, enum CodecID id)
{
    AVCodec *codec = c;
    while (codec)
    {
        if (codec->id == id && CODEC_IS_VDPAU(codec))
            return codec;

        codec = codec->next;
    }

    return c;
}

#define LOCK_RENDER QMutexLocker locker1(&m_renderLock)
#define LOCK_DECODE QMutexLocker locker2(&m_decodeLock)
#define LOCK_ALL    LOCK_RENDER; LOCK_DECODE

#define INIT_ST \
  VdpStatus vdp_st; \
  bool ok = true

#define CHECK_ST \
  ok &= (vdp_st == VDP_STATUS_OK); \
  if (!ok) \
  { \
      LOG(VB_GENERAL, LOG_ERR, QString("VDPAU: Error (#%1, %2)") \
          .arg(vdp_st) .arg(vdp_get_error_string(vdp_st))); \
  } \
  (void)(ok)

#define GET_PROC(FUNC_ID, PROC) \
    vdp_st = vdp_get_proc_address(m_device, (FUNC_ID), (void **)&(PROC)); \
    CHECK_ST

#define MIN_REFERENCE_FRAMES 2
#define MAX_REFERENCE_FRAMES 16

#define CHECK_ERROR(Loc) \
  if (IsErrored()) \
  { \
      LOG(VB_GENERAL, LOG_ERR, QString("VDPAU: IsErrored() in %1").arg(Loc)); \
      return; \
  } while(0)


bool VDPAUVideoDecoder::Initialize(void)
{
    m_display = OpenMythXDisplay();
    if (!m_display)
    {
        LOG(VB_GENERAL, LOG_ERR, "Can't open X display");
        return false;
    }

    INIT_ST;

    vdp_get_error_string = &dummy_get_error_string;
    vdp_st = vdp_device_create_x11(m_display->GetDisplay(),
                                   m_display->GetScreen(), &m_device,
                                   &vdp_get_proc_address);
    if (vdp_st != VDP_STATUS_OK)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("Error creating VDPAU device (#%1, %2)")
            .arg(vdp_st) .arg(vdp_get_error_string(vdp_st)));
        return false;
    }

    vdp_st = vdp_get_proc_address(m_device, VDP_FUNC_ID_GET_ERROR_STRING,
                                  (void **)&vdp_get_error_string);
    if (vdp_st != VDP_STATUS_OK)
    {
        // Carry on but have "Unknown" error strings
        vdp_get_error_string = &dummy_get_error_string;
    }

    GET_PROC(VDP_FUNC_ID_DEVICE_DESTROY, vdp_device_destroy);
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_CREATE,  vdp_video_surface_create);
    GET_PROC(VDP_FUNC_ID_VIDEO_SURFACE_DESTROY, vdp_video_surface_destroy);
    GET_PROC(VDP_FUNC_ID_DECODER_CREATE,  vdp_decoder_create);
    GET_PROC(VDP_FUNC_ID_DECODER_DESTROY, vdp_decoder_destroy);
    GET_PROC(VDP_FUNC_ID_DECODER_RENDER,  vdp_decoder_render);
    GET_PROC(VDP_FUNC_ID_DECODER_QUERY_CAPABILITIES,
             vdp_decoder_query_capabilities);

    if (!ok)
        return false;

    GET_PROC(VDP_FUNC_ID_GET_API_VERSION, vdp_get_api_version);
    GET_PROC(VDP_FUNC_ID_GET_INFORMATION_STRING, vdp_get_information_string);

    ok = true;

    if (vdp_get_api_version)
    {   
        uint version;
        vdp_get_api_version(&version);
        LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: Version: %1").arg(version));
    }

    if (vdp_get_information_string)
    {   
        const char * info;
        vdp_get_information_string(&info);
        LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: Information: %2").arg(info));
    }

    uint32_t tmp1, tmp2, tmp3, tmp4;
    VdpBool supported;
    vdp_st = vdp_decoder_query_capabilities(m_device,
                VDP_DECODER_PROFILE_MPEG4_PART2_ASP, &supported, &tmp1, &tmp2,
                &tmp3, &tmp4);
    CHECK_ST;

    m_accelMpeg4 = (bool)supported;
    LOG(VB_GENERAL, LOG_INFO,
        QString("VDPAU: MPEG4 hardware acceleration %1supported.")
        .arg(m_accelMpeg4 ? "" : "not"));

    vdp_st = vdp_decoder_query_capabilities(m_device,
                VDP_DECODER_PROFILE_H264_HIGH, &supported, &tmp1, &tmp2,
                &tmp3, &tmp4);
    CHECK_ST;

    m_accelH264 = (bool)supported;
    LOG(VB_GENERAL, LOG_INFO,
        QString("VDPAU: H.264 hardware acceleration %1supported.")
        .arg(m_accelH264 ? "" : "not"));

    glVDPAUInitNV((void *)(size_t)m_device,
                  (void *)(uintptr_t)vdp_get_proc_address);

    return true;
}

bool openGLInitialize(void)
{
    // initialize GLUT 
    QString x11display = MythUIHelper::GetX11Display();
    int argc = 3;
    const char *argv[] = { "client", "-display", NULL };
    argv[2] = x11display.toLocal8Bit().constData();

    glutInit(&argc, (char **)argv);
    // glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    int glutWindow = glutCreateWindow("OpenCL/GL Interop");
    (void)glutWindow;

    LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: OpenGL Version %1")
        .arg((char *)glGetString(GL_VERSION)));
    LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: OpenGL Vendor: %1")
        .arg((char *)glGetString(GL_VENDOR)));
    LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: OpenGL Renderer: %1")
        .arg((char *)glGetString(GL_RENDERER)));

    // initialize necessary OpenGL extensions
    glewInit();

    LOG(VB_GENERAL, LOG_INFO, QString("VDPAU: GLEW Version %1")
        .arg((char *)glewGetString(GLEW_VERSION)));

    if (!GLEW_EXT_framebuffer_blit)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "VDPAU: OpenGL framebuffer blit not supported");
        return false;
    }

    if (!GLEW_EXT_framebuffer_object)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "VDPAU: OpenGL framebuffer objects not supported");
        return false;
    }

    if (!GLEW_ARB_texture_non_power_of_two)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "VDPAU: OpenGL non-power-of-two textures not supported");
        return false;
    }

    if (!GLEW_ARB_texture_rectangle)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "VDPAU: OpenGL rectangle textures not supported");
        return false;
    }

    if (!GLEW_NV_vdpau_interop)
    {
        LOG(VB_GENERAL, LOG_ERR, "VDPAU: OpenGL-VDPAU interop not supported");
        return false;
    }

    return true;
}


VideoSurface *VDPAUVideoDecoder::DecodeFrame(AVFrame *frame)
{
    VideoSurface *surface = (VideoSurface *)frame->opaque;
    cl_int ciErrNum;

    // Copy data from VDPAU -> OpenCL
    surface->Bind();

    ciErrNum = clEnqueueAcquireGLObjects(m_dev->m_commandQ, 4,
                                         surface->m_clBuffer, 0, NULL, NULL);
    if (ciErrNum != CL_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR,
            QString("VDPAU: OpenCL acquire failed: %2 (%3)")
            .arg(ciErrNum) .arg(openCLErrorString(ciErrNum)));
        return NULL;
    }

    return surface;
}

void VDPAUVideoDecoder::DiscardFrame(VideoSurface *frame)
{
    VideoSurface *surface = (VideoSurface *)frame;
    cl_int ciErrNum;

    ciErrNum = clEnqueueReleaseGLObjects(m_dev->m_commandQ, 4,
                                         surface->m_clBuffer, 0, NULL, NULL);
}

void VDPAUVideoDecoder::Shutdown(void)
{
    if (vdp_video_surface_destroy)
    {
        for (VDPAUSurfaceRevHash::iterator it = m_reverseSurfaces.begin();
             it != m_reverseSurfaces.end(); )
        {
            VdpVideoSurface vdpSurface = it.key();
            uint id = it.value();
            VideoSurface *surface = m_videoSurfaces[id];

            it = m_reverseSurfaces.erase(it);
            m_videoSurfaces.remove(id);

            delete surface;
            vdp_video_surface_destroy(vdpSurface);
        }
    }

    if (vdp_decoder_destroy && m_decoder)
        vdp_decoder_destroy(m_decoder);

    glVDPAUFiniNV();

    if (vdp_device_destroy && m_device)
        vdp_device_destroy(m_device);

    if (m_display)
    {
        delete m_display;
        m_display = NULL;
    }
}

void VDPAUVideoDecoder::SetCodec(AVCodec *codec)
{
    bool m_useCPU = false;
    uint stream_type = mpeg_version(codec->id);
    MythCodecID test_cid = (MythCodecID)(kCodec_MPEG1_VDPAU + (stream_type-1));
    m_useCPU |= !codec_is_vdpau_hw(test_cid);
    if (test_cid == kCodec_MPEG4_VDPAU)
        m_useCPU |= !m_accelMpeg4;
    if (test_cid == kCodec_H264_VDPAU)
        m_useCPU |= !H264DecoderSizeSupported();

    if (test_cid == kCodec_H263_VDPAU)
    {
        LOG(VB_GENERAL, LOG_ERR, "VDPAU: Video format not supported");
        m_useCPU = true;
    }
    if (m_useCPU)
        test_cid = (MythCodecID)(kCodec_MPEG1 + (stream_type-1));

    m_codecId = (CodecID) myth2av_codecid(test_cid);
    
    if (!m_useCPU)
    {
        m_codec = avcodec_find_decoder(m_codecId);
        if (m_codec)
            m_codec = find_vdpau_decoder(m_codec, m_codecId);
    }
}

bool VDPAUVideoDecoder::H264DecoderSizeSupported(void)
{
    if (!m_accelH264)
        return false;

    int mbs = (int)((m_width + 15) >> 4);
    // see Appendix H of the NVIDIA proprietary driver README
    int check = (mbs == 49 ) || (mbs == 54 ) || (mbs == 59 ) || (mbs == 64) ||
                (mbs == 113) || (mbs == 118) || (mbs == 123) || (mbs == 128);
    if (!check)
        return true;

    LOG(VB_PLAYBACK, LOG_INFO,
        QString("VDPAU: Checking support for H.264 video with width %1")
        .arg(m_width));

    INIT_ST;
    VdpDecoder tmp;

    vdp_st = vdp_decoder_create(m_device, VDP_DECODER_PROFILE_H264_HIGH,
                                m_width, m_height, 2, &tmp);
    CHECK_ST;

    bool supported = true;
    if (!ok || !tmp)
        supported = false;

    vdp_st = vdp_decoder_destroy(tmp);
    CHECK_ST;

    LOG(VB_GENERAL, (supported ? LOG_INFO : LOG_WARNING),
        QString("VDPAU: Hardware decoding of this H.264 video is %1supported "
                "on this video card.").arg(supported ? "" : "NOT "));

    return supported;
}

static unsigned char *ffmpeg_hack = (unsigned char*)
    "avlib should not use this private data";

int get_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic)
{
    VDPAUVideoDecoder *nd = (VDPAUVideoDecoder *)(c->opaque);

    if (nd->m_unusedSurfaces.isEmpty())
    {
        int created = 0;
        for (int i = 0; i < MIN_REFERENCE_FRAMES; i++)
        {
            uint tmp = nd->CreateVideoSurface(nd->m_width, nd->m_height);
            if (tmp)
            {
                nd->m_unusedSurfaces.append(tmp);
                created++;
            }
        }
        nd->m_decoderBufferSize += created;
    }

    uint id = nd->m_unusedSurfaces.takeFirst();
    nd->m_usedSurfaces.append(id);
    VideoSurface *surface = nd->m_videoSurfaces[id];

    pic->data[0] = (uint8_t *)&surface->m_render;
    pic->data[1] = ffmpeg_hack;
    pic->data[2] = ffmpeg_hack;

    pic->linesize[0] = 0;
    pic->linesize[1] = 0;
    pic->linesize[2] = 0;

    pic->opaque = (char *)surface;
    pic->type = FF_BUFFER_TYPE_USER;

    pic->age = 256 * 256 * 256 * 64;

    surface->m_pixFmt = c->pix_fmt;

    surface->m_render.state |= FF_VDPAU_STATE_USED_FOR_REFERENCE;

    pic->reordered_opaque = c->reordered_opaque;

    return 0;
}

void release_avf_buffer_vdpau(struct AVCodecContext *c, AVFrame *pic)
{
    if (pic->type != FF_BUFFER_TYPE_USER);
        return;

#ifdef USING_VDPAU
    struct vdpau_render_state *render =
        (struct vdpau_render_state *)pic->data[0];
    render->state &= ~FF_VDPAU_STATE_USED_FOR_REFERENCE;
#endif

    VDPAUVideoDecoder *nd = (VDPAUVideoDecoder *)(c->opaque);
    if (nd)
    {
        VideoSurface *surface = (VideoSurface *)pic->opaque;
        uint id = surface->m_id;
        nd->m_usedSurfaces.removeOne(id);
        nd->m_unusedSurfaces.append(id);
    }

    for (uint i = 0; i < 4; i++)
        pic->data[i] = NULL;
}

void render_slice_vdpau(struct AVCodecContext *s, const AVFrame *src,
                        int offset[4], int y, int type, int height)
{
    if (!src)
        return;

    (void)offset;
    (void)type;

    if (s && src && s->opaque && src->opaque)
    {
        VDPAUVideoDecoder *nd = (VDPAUVideoDecoder *)(s->opaque);

        int width = s->width;

        VideoSurface *surface = (VideoSurface *)src->opaque;
        nd->DrawSlice(surface, 0, y, width, height);
    }
    else
    {
        LOG(VB_GENERAL, LOG_ERR,
            "render_slice_vdpau called with bad avctx or src");
    }
}

static bool IS_VDPAU_PIX_FMT(enum PixelFormat fmt)
{
    return
        fmt == PIX_FMT_VDPAU_H264  ||
        fmt == PIX_FMT_VDPAU_MPEG1 ||
        fmt == PIX_FMT_VDPAU_MPEG2 ||
        fmt == PIX_FMT_VDPAU_MPEG4 ||
        fmt == PIX_FMT_VDPAU_WMV3  ||
        fmt == PIX_FMT_VDPAU_VC1;
}

enum PixelFormat get_format_vdpau(struct AVCodecContext *avctx,
                                  const enum PixelFormat *fmt)
{
    int i = 0;

    for(i=0; fmt[i]!=PIX_FMT_NONE; i++)
        if (IS_VDPAU_PIX_FMT(fmt[i]))
            break;

    return fmt[i];
}

void VDPAUVideoDecoder::DrawSlice(VideoSurface *surface, int x, int y, int w,
                                  int h)
{
    (void)x;
    (void)y;
    (void)w;
    (void)h;

    CHECK_ERROR("DrawSlice");

    struct vdpau_render_state *render = &surface->m_render;
    if (!render)
    {
        LOG(VB_GENERAL, LOG_ERR, "VDPAU: No video surface to decode to.");
        m_errorState = kError_Unknown;
        return;
    }

    if (surface->m_pixFmt != m_pixFmt)
    {
        if (m_decoder && m_pixFmt != PIX_FMT_NONE)
        {
            LOG(VB_GENERAL, LOG_ERR,
                QString("VDPAU: Picture format has changed. (%1 -> %2)")
                .arg(m_pixFmt) .arg(surface->m_pixFmt));
            m_errorState = kError_Unknown;
            return;
        }

        uint max_refs = MIN_REFERENCE_FRAMES;
        if (surface->m_pixFmt == PIX_FMT_VDPAU_H264)
        {
            max_refs = render->info.h264.num_ref_frames;
            if (max_refs < 1 || max_refs > MAX_REFERENCE_FRAMES)
            {
                uint32_t round_width  = (m_width + 15) & ~15;
                uint32_t round_height = (m_height + 15) & ~15;
                uint32_t surf_size    = (round_width * round_height * 3) / 2;
                max_refs = (12 * 1024 * 1024) / surf_size;
            }
            if (max_refs > MAX_REFERENCE_FRAMES)
                max_refs = MAX_REFERENCE_FRAMES;

            // Add extra buffers as necessary
            int needed = max_refs - m_decoderBufferSize;
            if (needed > 0)
            {
                QMutexLocker locker(&m_lock);
                uint created = 0;
                for (int i = 0; i < needed; i++)
                {
                    uint tmp = CreateVideoSurface(m_width, m_height);
                    if (tmp)
                    {
                        m_unusedSurfaces.append(tmp);
                        created++;
                    }
                }
                m_decoderBufferSize += created;
                LOG(VB_GENERAL, LOG_INFO,
                    QString("VDPAU: Added %1 new buffers. New buffer size: "
                            "(%2 decode and %3 process)")
                                .arg(created)
                                .arg(m_decoderBufferSize)
                                .arg(m_processBufferSize));
            }
        }

        switch (surface->m_pixFmt)
        {
            case PIX_FMT_VDPAU_MPEG1:
                m_profile = VDP_DECODER_PROFILE_MPEG1;
                break;
            case PIX_FMT_VDPAU_MPEG2:
                m_profile = VDP_DECODER_PROFILE_MPEG2_MAIN;
                break;
            case PIX_FMT_VDPAU_MPEG4:
                m_profile = VDP_DECODER_PROFILE_MPEG4_PART2_ASP;
                break;
            case PIX_FMT_VDPAU_H264:
                m_profile = VDP_DECODER_PROFILE_H264_HIGH;
                break;
            case PIX_FMT_VDPAU_WMV3:
                m_profile = VDP_DECODER_PROFILE_VC1_MAIN;
                break;
            case PIX_FMT_VDPAU_VC1:
                m_profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
                break;
            default:
                LOG(VB_GENERAL, LOG_ERR,
                    "VDPAU: Picture format is not supported.");
                m_errorState = kError_Unknown;
                return;
        }

        bool ok = CreateDecoder(m_width, m_height, m_profile, max_refs);
        if (ok)
        {
            m_pixFmt = surface->m_pixFmt;
            LOG(VB_PLAYBACK, LOG_INFO,
                QString("VDPAU: Created VDPAU decoder (%1 ref frames)")
                    .arg(max_refs));
        }
        else
        {
            LOG(VB_GENERAL, LOG_ERR, "VDPAU: Failed to create decoder.");
            m_errorState = kError_Unknown;
            return;
        }
    }
    else if (!m_decoder)
    {
        LOG(VB_GENERAL, LOG_ERR,
            "VDPAU: Pix format already set but no VDPAU decoder.");
        m_errorState = kError_Unknown;
        return;
    }

    Decode(&surface->m_render);
}

uint VDPAUVideoDecoder::CreateVideoSurface(uint32_t width, uint32_t height,
                                           VdpChromaType type)
{
    LOCK_RENDER;

    if (IsErrored())
        return 0;

    INIT_ST;

    VdpVideoSurface tmp;
    vdp_st = vdp_video_surface_create(m_device, type, width, height, &tmp);
    CHECK_ST;

    if (!ok || !tmp)
    {
        LOG(VB_PLAYBACK, LOG_ERR, "VDPAU: Failed to create video surface.");
        return 0;
    }

    QMutexLocker idlock(&m_idLock);

    while (m_videoSurfaces.contains(m_nextId))
        if ((++m_nextId) == 0)
            m_nextId = 1;

    uint id = m_nextId;
    VideoSurface *surface = new VideoSurface(m_dev, width, height, id, tmp);
    m_videoSurfaces.insert(id, surface);
    m_reverseSurfaces[tmp] = id;

    return id;
}

bool VDPAUVideoDecoder::CreateDecoder(uint32_t width, uint32_t height,
                                      VdpDecoderProfile profile,
                                      uint references)
{
    LOCK_DECODE;

    if (IsErrored())
        return false;

    INIT_ST;

    if (references < 1)
        return false;

    VdpDecoder tmp;
    vdp_st = vdp_decoder_create(m_device, profile, width, height, references,
                                &tmp);
    CHECK_ST;

    if (!ok || !tmp)
    {
        LOG(VB_PLAYBACK, LOG_ERR, "VDPAU: Failed to create decoder.");
        return false;
    }

    m_decoder = tmp;

    return true;
}

bool VDPAUVideoDecoder::Decode(struct vdpau_render_state *render)
{
    INIT_ST;
    vdp_st = vdp_decoder_render(m_decoder, render->surface,
                                (VdpPictureInfo const *)&(render->info),
                                render->bitstream_buffers_used,
                                render->bitstream_buffers);
    CHECK_ST;
    return ok;
}


/*
 * vim:ts=4:sw=4:ai:et:si:sts=4
 */
