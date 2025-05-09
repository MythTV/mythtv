// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythlogging.h"
#include "libmyth/mythavframe.h"

#if CONFIG_DRM_VIDEO
#include "libmythui/platforms/mythdisplaydrm.h"
#endif

#include "mythvideoout.h"
#include "mythplayerui.h"
#include "mythvideocolourspace.h"
#include "fourcc.h"
#include "mythvaapiinterop.h"
#include "mythvaapidrminterop.h"
#include "mythvaapiglxinterop.h"

extern "C" {
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/hwcontext_vaapi.h"
}

#define LOC QString("VAAPIInterop: ")

/*! \brief Return a list of interops that are supported by the current render device.
 *
 * DRM interop is the preferred option as it is copy free but requires EGL.
 * DRM returns raw YUV frames which gives us full colourspace and deinterlacing control.
 *
 * GLX Pixmap interop will copy the frame to an RGBA texture. VAAPI functionality is
 * used for colourspace conversion and deinterlacing. It is not supported
 * when EGL and/or Wayland are running.
 *
 * GLX Copy is only available when GLX is running (no OpenGLES or Wayland) and,
 * under the hood, performs the same Pixmap copy as GLXPixmap support plus an
 * additional render to texture via a FramebufferObject. As it is less performant
 * and less widely available than GLX Pixmap, it may be removed in the future.
 *
 * \note The returned list is in priority order (i.e. most preferable first).
*/
void MythVAAPIInterop::GetVAAPITypes(MythRenderOpenGL* Context, MythInteropGPU::InteropMap& Types)
{
    if (!Context)
        return;

    OpenGLLocker locker(Context);
    bool egl = Context->IsEGL();
    bool opengles = Context->isOpenGLES();
    bool wayland = qgetenv("XDG_SESSION_TYPE").contains("wayland");

    // best first
    MythInteropGPU::InteropTypes vaapitypes;

#if CONFIG_DRM_VIDEO
    if (MythDisplayDRM::DirectRenderingAvailable())
        vaapitypes.emplace_back(DRM_DRMPRIME);
#endif

#if CONFIG_EGL
    // zero copy
    if (egl && MythVAAPIInteropDRM::IsSupported(Context))
        vaapitypes.emplace_back(GL_VAAPIEGLDRM);
#endif
    // 1x copy
    if (!egl && !wayland && MythVAAPIInteropGLXPixmap::IsSupported(Context))
        vaapitypes.emplace_back(GL_VAAPIGLXPIX);
    // 2x copy
    if (!egl && !opengles && !wayland)
        vaapitypes.emplace_back(GL_VAAPIGLXCOPY);

    if (!vaapitypes.empty())
        Types[FMT_VAAPI] = vaapitypes;
}

MythVAAPIInterop* MythVAAPIInterop::CreateVAAPI(MythPlayerUI *Player, MythRenderOpenGL* Context)
{
    if (!(Player && Context))
        return nullptr;

    const auto & types = Player->GetInteropTypes();
    if (const auto & vaapi = types.find(FMT_VAAPI); vaapi != types.cend())
    {
        for (auto type : vaapi->second)
        {
#if CONFIG_EGL
            if ((type == GL_VAAPIEGLDRM) || (type == DRM_DRMPRIME))
                return new MythVAAPIInteropDRM(Player, Context, type);
#endif
            if (type == GL_VAAPIGLXPIX)
                return new MythVAAPIInteropGLXPixmap(Player, Context);
            if (type == GL_VAAPIGLXCOPY)
                return new MythVAAPIInteropGLXCopy(Player, Context);
        }
    }
    return nullptr;
}

/*! \class MythVAAPIInterop
 *
 * \todo Fix pause frame deinterlacing
 * \todo Deinterlacing 'breaks' after certain stream changes (e.g. aspect ratio)
 * \todo Scaling of some 1080 H.264 material (garbage line at bottom - presumably
 * scaling from 1088 to 1080 - but only some files). Same effect on all VAAPI interop types.
*/
MythVAAPIInterop::MythVAAPIInterop(MythPlayerUI* Player, MythRenderOpenGL *Context, InteropType Type)
  : MythOpenGLInterop(Context, Type, Player)
{
}

MythVAAPIInterop::~MythVAAPIInterop()
{
    MythVAAPIInterop::DestroyDeinterlacer();
    if (m_vaDisplay)
        if (vaTerminate(m_vaDisplay) != VA_STATUS_SUCCESS)
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Error closing VAAPI display");
}

VADisplay MythVAAPIInterop::GetDisplay(void)
{
    return m_vaDisplay;
}

QString MythVAAPIInterop::GetVendor(void)
{
    return m_vaVendor;
}

void MythVAAPIInterop::InitaliseDisplay(void)
{
    if (!m_vaDisplay)
        return;
    int major = 0;
    int minor = 0;
    if (vaInitialize(m_vaDisplay, &major, &minor) != VA_STATUS_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise VAAPI display");
        vaTerminate(m_vaDisplay);
        m_vaDisplay = nullptr;
    }
    else
    {
        m_vaVendor = vaQueryVendorString(m_vaDisplay);
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created VAAPI %1.%2 display for %3 (%4)")
            .arg(major).arg(minor).arg(TypeToString(m_type), m_vaVendor));
    }
}

void MythVAAPIInterop::DestroyDeinterlacer(void)
{
    if (m_filterGraph)
        LOG(VB_GENERAL, LOG_INFO, LOC + "Destroying VAAPI deinterlacer");
    avfilter_graph_free(&m_filterGraph);
    m_filterGraph = nullptr;
    m_filterSink = nullptr;
    m_filterSource = nullptr;
    m_deinterlacer = DEINT_NONE;
    m_deinterlacer2x = false;
    m_firstField = true;
    m_lastFilteredFrame = 0;
    m_lastFilteredFrameCount = 0;
    av_buffer_unref(&m_vppFramesContext);
}

VASurfaceID MythVAAPIInterop::VerifySurface(MythRenderOpenGL *Context, MythVideoFrame *Frame)
{
    VASurfaceID result = 0;
    if (!Frame)
        return result;

    if ((Frame->m_pixFmt != AV_PIX_FMT_VAAPI) || (Frame->m_type != FMT_VAAPI) ||
        !Frame->m_buffer || !Frame->m_priv[1])
        return result;

    // Sanity check the context
    if (m_openglContext != Context)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Mismatched OpenGL contexts!");
        return result;
    }

    // Check size
    QSize surfacesize(Frame->m_width, Frame->m_height);
    if (m_textureSize != surfacesize)
    {
        if (!m_textureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
        m_textureSize = surfacesize;
    }

    // Retrieve surface
    auto id = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(Frame->m_buffer));
    if (id)
        result = id;
    return result;
}

bool MythVAAPIInterop::SetupDeinterlacer(MythDeintType Deinterlacer, bool DoubleRate,
                                         AVBufferRef *FramesContext,
                                         int Width, int Height,
                                         // Outputs
                                         AVFilterGraph *&Graph,
                                         AVFilterContext *&Source,
                                         AVFilterContext *&Sink)
{
    if (!FramesContext)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No hardware frames context");
        return false;
    }

    int ret = 0;
    QString args;
    QString deinterlacer = "bob";
    if (DEINT_MEDIUM == Deinterlacer)
        deinterlacer = "motion_adaptive";
    else if (DEINT_HIGH == Deinterlacer)
        deinterlacer = "motion_compensated";

    // N.B. set auto to 0 otherwise we confuse playback if VAAPI does not deinterlace
    QString filters = QString("deinterlace_vaapi=mode=%1:rate=%2:auto=0")
            .arg(deinterlacer, DoubleRate ? "field" : "frame");
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVBufferSrcParameters* params = nullptr;

    // Automatically clean up memory allocation at function exit
    auto cleanup_fn = [&](int */*x*/) {
        if (ret < 0) {
            avfilter_graph_free(&Graph);
            Graph = nullptr;
        }
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
    };
    std::unique_ptr<int,decltype(cleanup_fn)> cleanup { &ret, cleanup_fn };

    Graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !Graph)
    {
        ret = AVERROR(ENOMEM);
        return false;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    args = QString("video_size=%1x%2:pix_fmt=%3:time_base=1/1")
                .arg(Width).arg(Height).arg(AV_PIX_FMT_VAAPI);

    ret = avfilter_graph_create_filter(&Source, buffersrc, "in",
                                       args.toLocal8Bit().constData(), nullptr, Graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer source");
        return false;
    }

    params = av_buffersrc_parameters_alloc();
    params->hw_frames_ctx = FramesContext;
    ret = av_buffersrc_parameters_set(Source, params);

    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "av_buffersrc_parameters_set failed");
        return false;
    }
    av_freep(reinterpret_cast<void*>(&params));

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&Sink, buffersink, "out",
                                       nullptr, nullptr, Graph);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "avfilter_graph_create_filter failed for buffer sink");
        return false;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = Source;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = Sink;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    ret = avfilter_graph_parse_ptr(Graph, filters.toLocal8Bit(),
                                   &inputs, &outputs, nullptr);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("avfilter_graph_parse_ptr failed for %1")
            .arg(filters));
        return false;
    }

    ret = avfilter_graph_config(Graph, nullptr);
    if (ret < 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC +
            QString("VAAPI deinterlacer config failed - '%1' unsupported?").arg(deinterlacer));
        return false;
    }

    LOG(VB_GENERAL, LOG_INFO, LOC + QString("Created deinterlacer '%1'")
        .arg(MythVideoFrame::DeinterlacerName(Deinterlacer | DEINT_DRIVER, DoubleRate, FMT_VAAPI)));

    return true;
}

VASurfaceID MythVAAPIInterop::Deinterlace(MythVideoFrame *Frame, VASurfaceID Current, FrameScanType Scan)
{
    VASurfaceID result = Current;
    if (!Frame)
        return result;

    while (!m_filterError && is_interlaced(Scan))
    {
        // N.B. for DRM the use of a shader deint is checked before we get here
        MythDeintType deinterlacer = DEINT_NONE;
        bool doublerate = true;
        // no CPU or GLSL deinterlacing so pick up these options as well
        // N.B. Override deinterlacer_allowed to pick up any preference
        MythDeintType doublepref = Frame->GetDoubleRateOption(DEINT_DRIVER | DEINT_SHADER | DEINT_CPU, DEINT_ALL);
        MythDeintType singlepref = Frame->GetSingleRateOption(DEINT_DRIVER | DEINT_SHADER | DEINT_CPU, DEINT_ALL);

        if (doublepref)
        {
            deinterlacer = doublepref;
        }
        else if (singlepref)
        {
            deinterlacer = singlepref;
            doublerate = false;
        }

        if ((m_deinterlacer == deinterlacer) && (m_deinterlacer2x == doublerate))
            break;

        DestroyDeinterlacer();

        if (deinterlacer != DEINT_NONE)
        {
            auto* frames = reinterpret_cast<AVBufferRef*>(Frame->m_priv[1]);
            if (!frames)
                break;

            AVBufferRef* hwdeviceref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
            if (!hwdeviceref)
                break;

            auto* hwdevicecontext  = reinterpret_cast<AVHWDeviceContext*>(hwdeviceref->data);
            hwdevicecontext->free = [](AVHWDeviceContext* /*unused*/) { LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI VPP device context finished"); };

            auto *vaapidevicectx = reinterpret_cast<AVVAAPIDeviceContext*>(hwdevicecontext->hwctx);
            vaapidevicectx->display = m_vaDisplay; // re-use the existing display

            if (av_hwdevice_ctx_init(hwdeviceref) < 0)
            {
                av_buffer_unref(&hwdeviceref);
                m_filterError = true;
                break;
            }

            AVBufferRef *newframes = av_hwframe_ctx_alloc(hwdeviceref);
            if (!newframes)
            {
                m_filterError = true;
                av_buffer_unref(&hwdeviceref);
                break;
            }

            auto* dstframes = reinterpret_cast<AVHWFramesContext*>(newframes->data);
            auto* srcframes = reinterpret_cast<AVHWFramesContext*>(frames->data);

            m_filterWidth = srcframes->width;
            m_filterHeight = srcframes->height;
            static constexpr int kVppPoolSize = 2; // seems to be enough
            dstframes->sw_format = srcframes->sw_format;
            dstframes->width = m_filterWidth;
            dstframes->height = m_filterHeight;
            dstframes->initial_pool_size = kVppPoolSize;
            dstframes->format = AV_PIX_FMT_VAAPI;
            dstframes->free = [](AVHWFramesContext* /*unused*/) { LOG(VB_PLAYBACK, LOG_INFO, LOC + "VAAPI VPP frames context finished"); };

            if (av_hwframe_ctx_init(newframes) < 0)
            {
                m_filterError = true;
                av_buffer_unref(&hwdeviceref);
                av_buffer_unref(&newframes);
                break;
            }

            m_vppFramesContext = newframes;
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("New VAAPI frame pool with %1 %2x%3 surfaces")
                .arg(kVppPoolSize).arg(m_filterWidth).arg(m_filterHeight));
            av_buffer_unref(&hwdeviceref);

            if (!MythVAAPIInterop::SetupDeinterlacer(deinterlacer, doublerate, m_vppFramesContext,
                                                     m_filterWidth, m_filterHeight,
                                                     m_filterGraph, m_filterSource, m_filterSink))
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create VAAPI deinterlacer %1 - disabling")
                    .arg(MythVideoFrame::DeinterlacerName(deinterlacer | DEINT_DRIVER, doublerate, FMT_VAAPI)));
                DestroyDeinterlacer();
                m_filterError = true;
            }
            else
            {
                m_deinterlacer = deinterlacer;
                m_deinterlacer2x = doublerate;
                PostInitDeinterlacer();
                break;
            }
        }
        break;
    }

    if (!is_interlaced(Scan) && m_deinterlacer)
        DestroyDeinterlacer();

    if (m_deinterlacer)
    {
        // deinterlacing the pause frame repeatedly is unnecessary and, due to the
        // buffering in the VAAPI frames context, causes the image to 'jiggle' when using
        // double rate deinterlacing. If we are confident this is a pause frame we have seen,
        // return the last deinterlaced frame.
        if (Frame->m_pauseFrame && m_lastFilteredFrame && (m_lastFilteredFrameCount == Frame->m_frameCounter))
            return m_lastFilteredFrame;

        Frame->m_deinterlaceInuse = m_deinterlacer | DEINT_DRIVER;
        Frame->m_deinterlaceInuse2x = m_deinterlacer2x;

        // 'pump' the filter with frames until it starts returning usefull output.
        // This minimises discontinuities at start up (where we would otherwise
        // show a couple of progressive frames first) and memory consumption for the DRM
        // interop as we only cache OpenGL textures for the deinterlacer's frame
        // pool.
        int retries = 3;
        while ((result == Current) && retries--)
        {
            while (true)
            {
                int ret = 0;
                MythAVFrame sinkframe;
                sinkframe->format =  AV_PIX_FMT_VAAPI;

                // only ask for a frame if we are expecting another
                if (m_deinterlacer2x && !m_firstField)
                {
                    ret = av_buffersink_get_frame(m_filterSink, sinkframe);
                    if  (ret >= 0)
                    {
                        // we have a filtered frame
                        result = m_lastFilteredFrame = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(sinkframe->data[3]));
                        m_lastFilteredFrameCount = Frame->m_frameCounter;
                        m_firstField = true;
                        break;
                    }
                    if (ret != AVERROR(EAGAIN))
                        break;
                }

                // add another frame
                MythAVFrame sourceframe;
                sourceframe->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
                if (Frame->m_interlacedReverse ^ Frame->m_topFieldFirst)
                {
                    sourceframe->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
                }
                sourceframe->flags |= AV_FRAME_FLAG_INTERLACED;
                sourceframe->data[3] = Frame->m_buffer;
                auto* buffer = reinterpret_cast<AVBufferRef*>(Frame->m_priv[0]);
                sourceframe->buf[0] = buffer ? av_buffer_ref(buffer) : nullptr;
                sourceframe->width  = m_filterWidth;
                sourceframe->height = m_filterHeight;
                sourceframe->format = AV_PIX_FMT_VAAPI;
                // N.B. This is required after changes in FFmpeg. Using the vpp
                // frames context doesn't seem correct - but using the 'master'
                // context does not work
                sourceframe->hw_frames_ctx = av_buffer_ref(m_vppFramesContext);
                ret = av_buffersrc_add_frame(m_filterSource, sourceframe);
                sourceframe->data[3] = nullptr;
                sourceframe->buf[0] = nullptr;
                if (ret < 0)
                    break;

                // try again
                ret = av_buffersink_get_frame(m_filterSink, sinkframe);
                if  (ret >= 0)
                {
                    // we have a filtered frame
                    result = m_lastFilteredFrame = static_cast<VASurfaceID>(reinterpret_cast<uintptr_t>(sinkframe->data[3]));
                    m_lastFilteredFrameCount = Frame->m_frameCounter;
                    m_firstField = false;
                    break;
                }
                break;
            }
        }
    }
    return result;
}
