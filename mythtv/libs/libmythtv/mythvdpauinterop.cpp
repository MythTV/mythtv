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

MythOpenGLInterop::Type MythVDPAUInterop::GetInteropType(MythCodecID CodecId,
                                                         MythRenderOpenGL *Context)
{
    if (!codec_is_vdpau_hw(CodecId) || !gCoreContext->IsUIThread())
        return Unsupported;

    if (!Context)
        Context = MythRenderOpenGL::GetOpenGLRender();
    if (!Context)
        return Unsupported;

    if (Context->hasExtension("GL_NV_vdpau_interop"))
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
    Cleanup();
    delete m_helper;
}

void MythVDPAUInterop::Cleanup(void)
{
    OpenGLLocker locker(m_context);

    if (m_helper)
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

    if (m_finiNV)
        m_finiNV();

    DeleteTextures();
}

bool MythVDPAUInterop::InitNV(AVVDPAUDeviceContext* DeviceContext)
{
    if (!DeviceContext || !m_context)
        return false;

    if (m_initNV && m_finiNV && m_registerNV && m_accessNV && m_mapNV &&
        m_helper && m_helper->IsValid())
        return true;

    OpenGLLocker locker(m_context);
    m_initNV     = reinterpret_cast<MYTH_VDPAUINITNV>(m_context->getProcAddress("VDPAUInitNV"));
    m_finiNV     = reinterpret_cast<MYTH_VDPAUFININV>(m_context->getProcAddress("VDPAUFiniNV"));
    m_registerNV = reinterpret_cast<MYTH_VDPAUREGOUTSURFNV>(m_context->getProcAddress("VDPAURegisterOutputSurfaceNV"));
    m_accessNV   = reinterpret_cast<MYTH_VDPAUSURFACCESSNV>(m_context->getProcAddress("VDPAUSurfaceAccessNV"));
    m_mapNV      = reinterpret_cast<MYTH_VDPAUMAPSURFNV>(m_context->getProcAddress("VDPAUMapSurfacesNV"));

    if (m_initNV && m_finiNV && m_registerNV && m_accessNV && m_mapNV)
    {
        m_helper = new MythVDPAUHelper(DeviceContext);
        if (m_helper->IsValid())
        {
            LOG(VB_PLAYBACK, LOG_INFO, LOC + "Ready");
            return true;
        }
        else
        {
            delete m_helper;
            m_helper = nullptr;
        }
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
            vector<MythVideoTexture*> textures = MythVideoTexture::CreateHardwareTextures(m_context, FMT_VDPAU, FMT_ARGB32, sizes,
                                                                                 QOpenGLTexture::Target2D);
            if (textures.empty())
                return false;
            MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
            m_openglTextures.insert(DUMMY_INTEROP_ID, textures);
        }
    }

    if (m_mixer && m_outputSurface)
    {
        if (!m_outputSurfaceReg && !m_openglTextures.empty())
        {
            // if this fails because another interop is registered, subsequent surface
            // registration will fail and we will try again on the next pass - hopefully
            // when all old frames have been de-ref'd (i.e. pause frames)
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
                m_accessNV(m_outputSurfaceReg, GL_READ_ONLY);
                m_mapNV(1, &m_outputSurfaceReg);
            }
        }
        return true;
    }

    return m_mixer && m_outputSurface;
}

/*! \brief Map VDPAU video surfaces to an OpenGL texture.
 *
 * \note There can only be one VDPAU context mapped to an OpenGL context. This causes
 * a minor issue when seeking (usually with H.264) as the decoder is recreated but
 * we still have a pause frame associated with the old decoder. Hence this interop is not
 * released and remains bound to the OpenGL context. This resolves itself once the pause
 * frame is replaced (i.e. after one new frame is displayed).
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

    AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
    if (!buffer || (buffer && !buffer->data))
        return result;
    AVHWFramesContext* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
    if (!frames || (frames && !frames->device_ctx))
        return result;
    AVVDPAUDeviceContext *devicecontext = reinterpret_cast<AVVDPAUDeviceContext*>(frames->device_ctx->hwctx);
    if (!devicecontext)
        return result;

    // Initialise
    if (!InitNV(devicecontext))
        return result;

    // Retrieve surface - we need its size to create the mixer and output surface
    VdpVideoSurface surface = static_cast<VdpVideoSurface>(reinterpret_cast<uintptr_t>(Frame->buf));
    if (!surface)
        return result;

    // Workaround HEVC interlaced bug
    // VDPAU driver hangs if we try to render HEVC as interlaced (tested with version 418.56)
    // Furthermore, testing with an HEVC interlaced stream, it looks like FFmpeg does not
    // pick up the interlacing anyway - though that might be fixed with a proper HEVC stream parser
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
    if (kScan_Interlaced == Scan || kScan_Intr2ndField == Scan)
    {
        MythDeintType driverdeint = GetDoubleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        if (!driverdeint)
        {
            doublerate = false;
            driverdeint = GetSingleRateOption(Frame, DEINT_DRIVER | DEINT_CPU | DEINT_SHADER);
        }

        if (driverdeint)
        {
            Frame->deinterlace_inuse = driverdeint | DEINT_DRIVER;
            Frame->deinterlace_inuse2x = doublerate;
            deinterlacer = driverdeint;
        }
    }

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
    m_helper->MixerRender(m_mixer, surface, m_outputSurface, Scan,
                          Frame->interlaced_reversed ? !Frame->top_field_first :
                          Frame->top_field_first);
    return m_openglTextures[DUMMY_INTEROP_ID];
}

void MythVDPAUInterop::UpdateColourSpace(void)
{
    if (!m_mixer || !m_context || !m_colourSpace || !m_helper)
        return;

    OpenGLLocker locker(m_context);
    m_helper->SetCSCMatrix(m_mixer, m_colourSpace);
}
