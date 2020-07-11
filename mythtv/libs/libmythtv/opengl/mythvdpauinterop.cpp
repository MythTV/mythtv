// MythTV
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythvdpauhelper.h"
#include "mythvdpauinterop.h"

#define LOC QString("VDPAUInterop: ")

MythVDPAUInterop* MythVDPAUInterop::Create(MythRenderOpenGL *Context, MythCodecID CodecId)
{
    if (Context)
        return new MythVDPAUInterop(Context, CodecId);
    return nullptr;
}

MythOpenGLInterop::Type MythVDPAUInterop::GetInteropType(VideoFrameType Format)
{
    if ((FMT_VDPAU != Format) || !gCoreContext->IsUIThread())
        return Unsupported;

    MythRenderOpenGL* context = MythRenderOpenGL::GetOpenGLRender();
    if (!context)
        return Unsupported;

    if (context->hasExtension("GL_NV_vdpau_interop") && MythVDPAUHelper::HaveVDPAU())
        return VDPAU;
    return Unsupported;
}

MythVDPAUInterop::MythVDPAUInterop(MythRenderOpenGL *Context, MythCodecID CodecId)
  : MythOpenGLInterop(Context, VDPAU),
    m_codec(CodecId)
{
}

MythVDPAUInterop::~MythVDPAUInterop()
{
    if (!m_context)
        return;

    if (m_colourSpace)
        m_colourSpace->DecrRef();

    OpenGLLocker locker(m_context);
    CleanupDeinterlacer();
    Cleanup();
    delete m_helper;
}

void MythVDPAUInterop::Cleanup(void)
{
    OpenGLLocker locker(m_context);

    // per the spec, this should automatically release any registered
    // and mapped surfaces
    if (m_finiNV)
        m_finiNV();

    if (m_helper && !m_preempted)
    {
        m_helper->DeleteOutputSurface(m_outputSurface);
        m_helper->DeleteMixer(m_mixer);
    }

    m_mixer = 0;
    m_outputSurface = 0;
    m_outputSurfaceReg = 0;
    m_deinterlacer = DEINT_NONE;
    m_mixerSize = QSize();
    m_mixerChroma = VDP_CHROMA_TYPE_420;

    DeleteTextures();
}

void MythVDPAUInterop::CleanupDeinterlacer(void)
{
    while (!m_referenceFrames.isEmpty())
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

void MythVDPAUInterop::RotateReferenceFrames(AVBufferRef *Buffer)
{
    if (!Buffer)
        return;

    // don't retain twice for double rate
    if (!m_referenceFrames.empty() &&
            (static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(m_referenceFrames[0]->data)) ==
             static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Buffer->data))))
    {
        return;
    }

    m_referenceFrames.push_front(av_buffer_ref(Buffer));

    // release old frames
    while (m_referenceFrames.size() > 3)
    {
        AVBufferRef* ref = m_referenceFrames.takeLast();
        av_buffer_unref(&ref);
    }
}

bool MythVDPAUInterop::InitNV(AVVDPAUDeviceContext* DeviceContext)
{
    if (!DeviceContext || !m_context)
        return false;

    if (m_initNV && m_finiNV && m_registerNV && m_accessNV && m_mapNV && m_unmapNV &&
        m_helper && m_helper->IsValid())
        return true;

    OpenGLLocker locker(m_context);
    m_initNV     = reinterpret_cast<MYTH_VDPAUINITNV>(m_context->GetProcAddress("glVDPAUInitNV"));
    m_finiNV     = reinterpret_cast<MYTH_VDPAUFININV>(m_context->GetProcAddress("glVDPAUFiniNV"));
    m_registerNV = reinterpret_cast<MYTH_VDPAUREGOUTSURFNV>(m_context->GetProcAddress("glVDPAURegisterOutputSurfaceNV"));
    m_accessNV   = reinterpret_cast<MYTH_VDPAUSURFACCESSNV>(m_context->GetProcAddress("glVDPAUSurfaceAccessNV"));
    m_mapNV      = reinterpret_cast<MYTH_VDPAUMAPSURFNV>(m_context->GetProcAddress("glVDPAUMapSurfacesNV"));
    m_unmapNV    = reinterpret_cast<MYTH_VDPAUMAPSURFNV>(m_context->GetProcAddress("glVDPAUUnmapSurfacesNV"));

    delete m_helper;
    m_helper = nullptr;

    if (m_initNV && m_finiNV && m_registerNV && m_accessNV && m_mapNV && m_unmapNV)
    {
        m_helper = new MythVDPAUHelper(DeviceContext);
        if (m_helper->IsValid())
        {
            connect(m_helper, &MythVDPAUHelper::DisplayPreempted, this, &MythVDPAUInterop::DisplayPreempted, Qt::DirectConnection);
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Ready");
            return true;
        }
        delete m_helper;
        m_helper = nullptr;
    }

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to retrieve procs");
    return false;
}

bool MythVDPAUInterop::InitVDPAU(AVVDPAUDeviceContext* DeviceContext, VdpVideoSurface Surface,
                                 MythDeintType Deint, bool DoubleRate)
{
    if (!m_helper || !m_context || !Surface || !DeviceContext)
        return false;

    VdpChromaType chroma = VDP_CHROMA_TYPE_420;
    QSize size = m_helper->GetSurfaceParameters(Surface, chroma);

    if (m_mixer && (chroma != m_mixerChroma || size != m_mixerSize || Deint != m_deinterlacer))
        Cleanup();

    if (!m_mixer)
    {
        m_mixer = m_helper->CreateMixer(size, chroma, Deint);
        m_deinterlacer = Deint;
        m_mixerChroma = chroma;
        m_mixerSize = size;
        if (DEINT_NONE != m_deinterlacer)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Setup deinterlacer '%1'")
                .arg(DeinterlacerName(m_deinterlacer | DEINT_DRIVER, DoubleRate, FMT_VDPAU)));
    }

    if (!m_outputSurface)
    {
        m_outputSurface = m_helper->CreateOutputSurface(size);
        if (m_outputSurface)
        {
            vector<QSize> sizes;
            sizes.push_back(size);
            vector<MythVideoTexture*> textures =
                    MythVideoTexture::CreateTextures(m_context, FMT_VDPAU, FMT_ARGB32, sizes);
            if (textures.empty())
                return false;
            m_openglTextures.insert(DUMMY_INTEROP_ID, textures);
        }
    }

    if (m_mixer && m_outputSurface)
    {
        if (!m_outputSurfaceReg && !m_openglTextures.empty())
        {
            // This may fail if another interop is registered (but should not happen if
            // decoder creataion is working properly). Subsequent surface
            // registration will then fail and we will try again on the next pass
            m_initNV(reinterpret_cast<void*>(static_cast<uintptr_t>(DeviceContext->device)),
                     reinterpret_cast<const void*>(DeviceContext->get_proc_address));
            GLuint texid = m_openglTextures[DUMMY_INTEROP_ID][0]->m_textureId;
            m_outputSurfaceReg = m_registerNV(reinterpret_cast<void*>(static_cast<uintptr_t>(m_outputSurface)),
                                              QOpenGLTexture::Target2D, 1, &texid);
            // this happens if there is another interop registered to this OpenGL context
            if (!m_outputSurfaceReg)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to register VdpOutputSurface. Will retry.");
            }
            else
            {
                m_accessNV(m_outputSurfaceReg, QOpenGLBuffer::ReadOnly);
            }
        }
        return true;
    }

    return m_mixer && m_outputSurface;
}

/*! \brief Map VDPAU video surfaces to an OpenGL texture.
 *
 * \note There can only be one VDPAU context mapped to an OpenGL context.
 *
 * \note We use a VdpVideoMixer to complete the conversion from YUV to RGB. Hence the returned
 * texture is RGB... We could use GL_NV_vdpau_interop2 to return raw YUV frames.
*/
vector<MythVideoTexture*> MythVDPAUInterop::Acquire(MythRenderOpenGL *Context,
                                                    VideoColourSpace *ColourSpace,
                                                    VideoFrame *Frame,
                                                    FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    if (m_preempted)
    {
        // Don't spam the logs with this warning
        if (!m_preemptedWarning)
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Display preempted. Decoder needs to be reset");
        m_preemptedWarning = true;
        return result;
    }

    if (Context && (Context != m_context))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Check size
    QSize surfacesize(Frame->width, Frame->height);
    if (m_openglTextureSize != surfacesize)
    {
        if (!m_openglTextureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
        m_openglTextureSize = surfacesize;
    }

    // Lock
    OpenGLLocker locker(m_context);

    // Retrieve hardware frames context and AVVDPAUDeviceContext
    if ((Frame->pix_fmt != AV_PIX_FMT_VDPAU) || (Frame->codec != FMT_VDPAU) ||
        !Frame->buf || !Frame->priv[1])
        return result;

    auto* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
    if (!buffer || (buffer && !buffer->data))
        return result;
    auto* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
    if (!frames || (frames && !frames->device_ctx))
        return result;
    auto *devicecontext = reinterpret_cast<AVVDPAUDeviceContext*>(frames->device_ctx->hwctx);
    if (!devicecontext)
        return result;

    // Initialise
    if (!InitNV(devicecontext))
        return result;

    // Retrieve surface - we need its size to create the mixer and output surface
    auto surface = static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Frame->buf));
    if (!surface)
        return result;

    // Workaround HEVC interlaced bug
    // VDPAU driver hangs if we try to render progressive HEVC as interlaced (tested with version 418.56)
    // FFmpeg clearly currently has issues with interlaced HEVC (https://trac.ffmpeg.org/ticket/4141).
    // Streams are always return with the field height.
    // Deinterlacing does work with (some?) HEVC material flagged as interlaced.
    if ((kCodec_HEVC_VDPAU == m_codec) && is_interlaced(Scan) && !Frame->interlaced_frame)
    {
        // This should only be logged a couple of times before the scan is detected as progressive
        LOG(VB_GENERAL, LOG_INFO, LOC + "Ignoring scan for non-interlaced HEVC frame");
        Scan = kScan_Progressive;
    }

    // Check for deinterlacing - VDPAU deinterlacers trump all others as we can only
    // deinterlace VDPAU frames here. So accept any deinterlacer.
    // N.B. basic deinterlacing requires no additional setup and is managed with
    // the field/frame parameter
    bool doublerate = true;
    MythDeintType deinterlacer = DEINT_BASIC;
    if (is_interlaced(Scan))
    {
        MythDeintType driverdeint = GetDoubleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER,
                                                        DEINT_ALL);
        if (!driverdeint)
        {
            doublerate = false;
            driverdeint = GetSingleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER, DEINT_ALL);
        }

        if (driverdeint)
        {
            Frame->deinterlace_inuse = driverdeint | DEINT_DRIVER;
            Frame->deinterlace_inuse2x = doublerate;
            deinterlacer = driverdeint;
        }
    }

    if ((deinterlacer == DEINT_HIGH) || (deinterlacer == DEINT_MEDIUM))
    {
        if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
            CleanupDeinterlacer();
        RotateReferenceFrames(reinterpret_cast<AVBufferRef*>(Frame->priv[0]));
    }
    else
    {
        CleanupDeinterlacer();
    }
    m_discontinuityCounter = Frame->frameCounter;

    // We need a mixer, an output surface and mapped texture
    if (!InitVDPAU(devicecontext, surface, deinterlacer, doublerate))
        return result;

    // Update colourspace and initialise on first frame - after mixer is created
    if (ColourSpace)
    {
        if (!m_colourSpace)
        {
            if (m_helper->IsFeatureAvailable(VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX))
            {
                ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
                connect(ColourSpace, &VideoColourSpace::Updated, this, &MythVDPAUInterop::UpdateColourSpace);
            }
            else
            {
                // N.B. CSC matrix support should always be available so there is no fallback.
                ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
                LOG(VB_GENERAL, LOG_WARNING, LOC + "No VDPAU CSC matrix support");
            }

            ColourSpace->IncrRef();
            m_colourSpace = ColourSpace;
        }
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Render surface
    m_unmapNV(1, &m_outputSurfaceReg);
    m_helper->MixerRender(m_mixer, surface, m_outputSurface, Scan,
                          Frame->interlaced_reversed ? !Frame->top_field_first :
                          Frame->top_field_first, m_referenceFrames);
    m_mapNV(1, &m_outputSurfaceReg);
    return m_openglTextures[DUMMY_INTEROP_ID];
}

void MythVDPAUInterop::UpdateColourSpace(bool /*PrimariesChanged*/)
{
    if (!m_mixer || !m_context || !m_colourSpace || !m_helper)
        return;

    OpenGLLocker locker(m_context);
    m_helper->SetCSCMatrix(m_mixer, m_colourSpace);
}

void MythVDPAUInterop::DisplayPreempted(void)
{
    // N.B. Pre-emption is irrecoverable here. We ensure the error state is recorded
    // and when AvFormatDecoder/MythCodecContext hit a problem, IsPreempted is checked.
    // The decoder context is then released, along with the associated interop
    // class (i.e. this) and a new interop is created.
    LOG(VB_GENERAL, LOG_INFO, LOC + "VDPAU display preempted");
    m_preempted = true;
}

bool MythVDPAUInterop::IsPreempted(void)
{
    return m_preempted;
}
