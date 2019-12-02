// MythTV
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythnvdecinterop.h"

#define LOC QString("NVDECInterop: ")

#define CUDA_CHECK(CUDA_FUNCS, CUDA_CALL) \
{ \
    CUresult res = (CUDA_FUNCS)->CUDA_CALL;          \
    if (res != CUDA_SUCCESS) { \
        const char * desc; \
        (CUDA_FUNCS)->cuGetErrorString(res, &desc);                      \
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("CUDA error %1 (%2)").arg(res).arg(desc)); \
    } \
}

MythNVDECInterop::MythNVDECInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, NVDEC),
    m_cudaContext()
{
    InitialiseCuda();
}

MythNVDECInterop::~MythNVDECInterop()
{
    m_referenceFrames.clear();
    DeleteTextures();
    CleanupContext(m_context, m_cudaFuncs, m_cudaContext);
}

void MythNVDECInterop::DeleteTextures(void)
{
    CUcontext dummy;
    if (!(m_cudaContext && m_cudaFuncs))
        return;

    OpenGLLocker locker(m_context);
    CUDA_CHECK(m_cudaFuncs, cuCtxPushCurrent(m_cudaContext));

    if (!m_openglTextures.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting CUDA resources");
        QHash<unsigned long long, vector<MythVideoTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            auto it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                auto *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>((*it2)->m_data);
                if (data && data->second)
                    CUDA_CHECK(m_cudaFuncs, cuGraphicsUnregisterResource(data->second));
                delete data;
                (*it2)->m_data = nullptr;
            }
        }
    }

    CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy));

    MythOpenGLInterop::DeleteTextures();
}

bool MythNVDECInterop::IsValid(void)
{
    return m_cudaFuncs && m_cudaContext;
}

CUcontext MythNVDECInterop::GetCUDAContext(void)
{
    return m_cudaContext;
}

MythNVDECInterop* MythNVDECInterop::Create(MythRenderOpenGL *Context)
{
    if (Context)
        return new MythNVDECInterop(Context);
    return nullptr;
}

MythOpenGLInterop::Type MythNVDECInterop::GetInteropType(VideoFrameType Format)
{
    if ((FMT_NVDEC != Format) || !gCoreContext->IsUIThread())
        return Unsupported;

    if (!MythRenderOpenGL::GetOpenGLRender())
        return Unsupported;
    return NVDEC;
}

/*! \brief Map CUDA video memory to OpenGL textures.
 *
 * \note This is not zero copy - although the copy will be extremely fast.
 * It may be marginally quicker to implement a custom FFmpeg buffer pool that allocates
 * textures and maps the texture storage to a CUdeviceptr (if that is possible). Alternatively
 * EGL interopability may also be useful.
*/
vector<MythVideoTexture*> MythNVDECInterop::Acquire(MythRenderOpenGL *Context,
                                                    VideoColourSpace *ColourSpace,
                                                    VideoFrame *Frame,
                                                    FrameScanType Scan)
{
    vector<MythVideoTexture*> result;
    if (!Frame || !m_cudaContext || !m_cudaFuncs)
        return result;

    if (Context && (Context != m_context))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Check size
    QSize surfacesize(Frame->width, Frame->height);
    if (m_openglTextureSize != surfacesize)
    {
        if (!m_openglTextureSize.isEmpty())
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Video texture size changed! %1x%2->%3x%4")
                .arg(m_openglTextureSize.width()).arg(m_openglTextureSize.height())
                .arg(Frame->width).arg(Frame->height));
        DeleteTextures();
        m_openglTextureSize = surfacesize;
    }

    // Lock
    OpenGLLocker locker(m_context);

    // Update colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve hardware frames context and AVCUDADeviceContext
    if ((Frame->pix_fmt != AV_PIX_FMT_CUDA) || (Frame->codec != FMT_NVDEC) ||
        !Frame->buf || !Frame->priv[0] || !Frame->priv[1])
    {
        return result;
    }

    auto cudabuffer = reinterpret_cast<CUdeviceptr>(Frame->buf);
    if (!cudabuffer)
        return result;

    // make the CUDA context current
    CUcontext dummy;
    CUDA_CHECK(m_cudaFuncs, cuCtxPushCurrent(m_cudaContext));

    // create and map textures for a new buffer
    VideoFrameType type = (Frame->sw_pix_fmt == AV_PIX_FMT_NONE) ? FMT_NV12 :
                PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));
    bool hdr = ColorDepth(type) > 8;
    if (!m_openglTextures.contains(cudabuffer))
    {
        vector<QSize> sizes;
        sizes.emplace_back(QSize(Frame->width, Frame->height));
        sizes.emplace_back(QSize(Frame->width, Frame->height >> 1));
        vector<MythVideoTexture*> textures =
                MythVideoTexture::CreateTextures(m_context, FMT_NVDEC, type, sizes);
        if (textures.empty())
        {
            CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy));
            return result;
        }

        bool success = true;
        for (uint plane = 0; plane < textures.size(); ++plane)
        {
            MythVideoTexture *tex = textures[plane];
            tex->m_allowGLSLDeint = true;
            m_context->glBindTexture(tex->m_target, tex->m_textureId);
            QOpenGLTexture::PixelType   pixtype    = hdr ? QOpenGLTexture::UInt16 : QOpenGLTexture::UInt8;
            QOpenGLTexture::TextureFormat internal = hdr ? QOpenGLTexture::R16_UNorm : QOpenGLTexture::R8_UNorm;
            int width = tex->m_size.width();

            if (plane)
            {
                pixtype   = hdr ? QOpenGLTexture::UInt32 : QOpenGLTexture::UInt16;
                internal  = hdr ? QOpenGLTexture::RG16_UNorm : QOpenGLTexture::RG8_UNorm;
                width /= 2;
            }

            m_context->glTexImage2D(tex->m_target, 0, internal, width, tex->m_size.height(),
                                    0, QOpenGLTexture::Red, pixtype, nullptr);

            CUarray array;
            CUgraphicsResource graphicsResource = nullptr;
            CUDA_CHECK(m_cudaFuncs, cuGraphicsGLRegisterImage(&graphicsResource, tex->m_textureId,
                                          QOpenGLTexture::Target2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
            if (graphicsResource)
            {
                CUDA_CHECK(m_cudaFuncs, cuGraphicsMapResources(1, &graphicsResource, nullptr));
                CUDA_CHECK(m_cudaFuncs, cuGraphicsSubResourceGetMappedArray(&array, graphicsResource, 0, 0));
                CUDA_CHECK(m_cudaFuncs, cuGraphicsUnmapResources(1, &graphicsResource, nullptr));
                tex->m_data = reinterpret_cast<unsigned char*>(new QPair<CUarray,CUgraphicsResource>(array, graphicsResource));
            }
            else
            {
                success = false;
                break;
            }
        }

        if (success)
        {
            m_openglTextures.insert(cudabuffer, textures);
        }
        else
        {
            auto it = textures.begin();
            for ( ; it != textures.end(); ++it)
            {
                auto *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>((*it)->m_data);
                if (data && data->second)
                    CUDA_CHECK(m_cudaFuncs, cuGraphicsUnregisterResource(data->second));
                delete data;
                (*it)->m_data = nullptr;
                if ((*it)->m_textureId)
                    m_context->glDeleteTextures(1, &(*it)->m_textureId);
                MythVideoTexture::DeleteTexture(m_context, *it);
            }
        }
    }

    if (!m_openglTextures.contains(cudabuffer))
    {
        CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy));
        return result;
    }

    // Copy device data to array data (i.e. texture) - surely this can be avoided?
    // In theory, asynchronous copies should not be required but we use async
    // followed by stream synchronisation to ensure CUDA and OpenGL are in sync
    // which avoids presenting old/stale frames when the GPU is under load.
    result = m_openglTextures[cudabuffer];
    for (uint i = 0; i < result.size(); ++i)
    {
        auto *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>(result[i]->m_data);
        CUDA_MEMCPY2D cpy;
        memset(&cpy, 0, sizeof(cpy));
        cpy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        cpy.srcDevice     = cudabuffer + static_cast<CUdeviceptr>(Frame->offsets[i]);
        cpy.srcPitch      = static_cast<size_t>(Frame->pitches[i]);
        cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        cpy.dstArray      = data->first;
        cpy.WidthInBytes  = static_cast<size_t>(result[i]->m_size.width()) * (hdr ? 2 : 1);
        cpy.Height        = static_cast<size_t>(result[i]->m_size.height());
        CUDA_CHECK(m_cudaFuncs, cuMemcpy2DAsync(&cpy, nullptr));
    }

    CUDA_CHECK(m_cudaFuncs, cuStreamSynchronize(nullptr));
    CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy));

    // GLSL deinterlacing. The decoder will pick up any CPU or driver preference
    // and return a stream of deinterlaced frames. Just check for GLSL here.
    bool needreferences = false;
    if (is_interlaced(Scan))
    {
        MythDeintType shader = GetDoubleRateOption(Frame, DEINT_SHADER);
        if (shader)
            needreferences = shader == DEINT_HIGH;
        else
            needreferences = GetSingleRateOption(Frame, DEINT_SHADER) == DEINT_HIGH;
    }

    if (needreferences)
    {
        if (abs(Frame->frameCounter - m_discontinuityCounter) > 1)
            m_referenceFrames.clear();

        RotateReferenceFrames(cudabuffer);
        int size = m_referenceFrames.size();

        CUdeviceptr next    = m_referenceFrames[0];
        CUdeviceptr current = m_referenceFrames[size > 1 ? 1 : 0];
        CUdeviceptr last    = m_referenceFrames[size > 2 ? 2 : 0];

        if (!m_openglTextures.contains(next) || !m_openglTextures.contains(current) ||
            !m_openglTextures.contains(last))
        {
            LOG(VB_GENERAL, LOG_ERR, LOC + "Reference frame error");
            return result;
        }

        result = m_openglTextures[last];
        foreach (MythVideoTexture* tex, m_openglTextures[current])
            result.push_back(tex);
        foreach (MythVideoTexture* tex, m_openglTextures[next])
            result.push_back(tex);
        return result;
    }
    m_referenceFrames.clear();
    m_discontinuityCounter = Frame->frameCounter;

    return result;
}

/*! \brief Initialise a CUDA context
 * \note We do not use the FFmpeg internal context creation as the created context
 * is deleted before we have a chance to cleanup our own CUDA resources.
*/
bool MythNVDECInterop::InitialiseCuda(void)
{
    return CreateCUDAContext(m_context, m_cudaFuncs, m_cudaContext);
}

bool MythNVDECInterop::CreateCUDAContext(MythRenderOpenGL *GLContext, CudaFunctions *&CudaFuncs,
                                         CUcontext &CudaContext)
{
    if (!GLContext)
        return false;

    // Make OpenGL context current
    OpenGLLocker locker(GLContext);

    // retrieve CUDA entry points
    if (cuda_load_functions(&CudaFuncs, nullptr) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load functions");
        return false;
    }

    // create a CUDA context for the current device
    CUdevice cudevice;
    CUcontext dummy;
    CUresult res = CudaFuncs->cuInit(0);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise CUDA API");
        return false;
    }

    unsigned int devicecount;
    res = CudaFuncs->cuGLGetDevices(&devicecount, &cudevice, 1, CU_GL_DEVICE_LIST_ALL);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to get CUDA device");
        return false;
    }

    if (devicecount < 1)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No CUDA devices");
        return false;
    }

    res = CudaFuncs->cuCtxCreate(&CudaContext, CU_CTX_SCHED_BLOCKING_SYNC, cudevice);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create CUDA context");
        return false;
    }

    CudaFuncs->cuCtxPopCurrent(&dummy);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created CUDA context");
    return true;

}

void MythNVDECInterop::CleanupContext(MythRenderOpenGL *GLContext, CudaFunctions *&CudaFuncs,
                                      CUcontext &CudaContext)
{
    OpenGLLocker locker(GLContext);
    if (CudaFuncs)
    {
        if (CudaContext)
            CUDA_CHECK(CudaFuncs, cuCtxDestroy(CudaContext));
        cuda_free_functions(&CudaFuncs);
    }
}

void MythNVDECInterop::RotateReferenceFrames(CUdeviceptr Buffer)
{
    if (!Buffer)
        return;

    // don't retain twice for double rate
    if (!m_referenceFrames.empty() && (m_referenceFrames[0] == Buffer))
        return;

    m_referenceFrames.push_front(Buffer);

    // release old frames
    while (m_referenceFrames.size() > 3)
        m_referenceFrames.pop_back();
}
