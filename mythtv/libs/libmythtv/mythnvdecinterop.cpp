// MythTV
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythnvdecinterop.h"

#define LOC QString("NVDECInterop: ")

#define CUDA_CHECK(CUDA_CALL) \
{ \
    CUresult res = CUDA_CALL; \
    if (res != CUDA_SUCCESS) { \
        const char * desc; \
        m_cudaFuncs->cuGetErrorString(res, &desc); \
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("CUDA error %1 (%2)").arg(res).arg(desc)); \
    } \
}

MythNVDECInterop::MythNVDECInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, NVDEC),
    m_cudaContext(),
    m_cudaFuncs(nullptr)
{
    InitialiseCuda();
}

MythNVDECInterop::~MythNVDECInterop()
{
    CUcontext dummy;
    if (m_cudaContext && m_cudaFuncs)
        CUDA_CHECK(m_cudaFuncs->cuCtxPushCurrent(m_cudaContext));

    if (!m_openglTextures.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting CUDA resources");
        QHash<unsigned long long, vector<MythVideoTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            vector<MythVideoTexture*>::iterator it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                QPair<CUarray,CUgraphicsResource> *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>((*it2)->m_data);
                // Don't error check here - for some reason the context is deemed destroyed but pop/destroy below
                // work fine
                if (m_cudaFuncs && data && data->second)
                    m_cudaFuncs->cuGraphicsUnregisterResource(&(data->second));
                delete data;
                (*it2)->m_data = nullptr;
            }
        }
    }

    if (m_cudaFuncs)
    {
        if (m_cudaContext)
        {
            CUDA_CHECK(m_cudaFuncs->cuCtxPopCurrent(&dummy));
            CUDA_CHECK(m_cudaFuncs->cuCtxDestroy(m_cudaContext));
        }
        cuda_free_functions(&m_cudaFuncs);
    }
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

MythOpenGLInterop::Type MythNVDECInterop::GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context)
{
    if (!codec_is_nvdec(CodecId) || !gCoreContext->IsUIThread())
        return Unsupported;
    if (!Context)
        Context = MythRenderOpenGL::GetOpenGLRender();
    if (!Context)
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
                                                    FrameScanType)
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
            LOG(VB_GENERAL, LOG_WARNING, LOC + "Video texture size changed!");
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

    CUdeviceptr cudabuffer = reinterpret_cast<CUdeviceptr>(Frame->buf);
    if (!cudabuffer)
        return result;

    // make the CUDA context current
    CUcontext dummy;
    CUDA_CHECK(m_cudaFuncs->cuCtxPushCurrent(m_cudaContext));

    // create and map textures for a new buffer
    VideoFrameType type = (Frame->sw_pix_fmt == AV_PIX_FMT_NONE) ? FMT_NV12 :
                PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->sw_pix_fmt));
    bool hdr = ColorDepth(type) > 8;
    if (!m_openglTextures.contains(cudabuffer))
    {
        vector<QSize> sizes;
        sizes.push_back(QSize(Frame->width, Frame->height));
        sizes.push_back(QSize(Frame->width, Frame->height >> 1));
        vector<MythVideoTexture*> textures = MythVideoTexture::CreateTextures(
                    m_context, FMT_NVDEC, type, sizes, QOpenGLTexture::Target2D);
        if (textures.empty())
        {
            CUDA_CHECK(m_cudaFuncs->cuCtxPopCurrent(&dummy));
            return result;
        }

        for (uint plane = 0; plane < textures.size(); ++plane)
        {
            MythVideoTexture *tex = textures[plane];
            m_context->glBindTexture(tex->m_target, tex->m_textureId);
            QOpenGLTexture::PixelFormat pixformat  = hdr ? QOpenGLTexture::RG : QOpenGLTexture::Red;
            QOpenGLTexture::PixelType   pixtype    = QOpenGLTexture::UInt8;
            QOpenGLTexture::TextureFormat internal = hdr ? QOpenGLTexture::RG8_UNorm : QOpenGLTexture::R8_UNorm;
            int width = tex->m_size.width();

            if (plane)
            {
                pixformat = hdr ? QOpenGLTexture::RGBA : QOpenGLTexture::RG;
                internal  = hdr ? QOpenGLTexture::RGBA8_UNorm : QOpenGLTexture::RG8_UNorm;
                width /= 2;
            }

            m_context->glTexImage2D(tex->m_target, 0, internal, width, tex->m_size.height(),
                                    0, pixformat, pixtype, nullptr);

            CUarray array;
            CUgraphicsResource graphicsResource = nullptr;
            CUDA_CHECK(m_cudaFuncs->cuGraphicsGLRegisterImage(&graphicsResource, tex->m_textureId,
                                          QOpenGLTexture::Target2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
            CUDA_CHECK(m_cudaFuncs->cuGraphicsMapResources(1, &graphicsResource, nullptr));
            CUDA_CHECK(m_cudaFuncs->cuGraphicsSubResourceGetMappedArray(&array, graphicsResource, 0, 0));
            CUDA_CHECK(m_cudaFuncs->cuGraphicsUnmapResources(1, &graphicsResource, nullptr));
            tex->m_data = reinterpret_cast<unsigned char*>(new QPair<CUarray,CUgraphicsResource>(array, graphicsResource));
        }
        MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        m_openglTextures.insert(cudabuffer, textures);
    }

    if (!m_openglTextures.contains(cudabuffer))
    {
        CUDA_CHECK(m_cudaFuncs->cuCtxPopCurrent(&dummy));
        return result;
    }

    // Copy device data to array data (i.e. texture) - surely this can be avoided?
    // N.B. we don't use cuMemcpy2DAsync as it gives occasional 'green screen' flashes,
    // even with subsequent synchronisation
    result = m_openglTextures[cudabuffer];
    for (uint i = 0; i < result.size(); ++i)
    {
        QPair<CUarray,CUgraphicsResource> *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>(result[i]->m_data);
        CUDA_MEMCPY2D cpy;
        memset(&cpy, 0, sizeof(cpy));
        cpy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        cpy.srcDevice     = cudabuffer + static_cast<CUdeviceptr>(Frame->offsets[i]);
        cpy.srcPitch      = static_cast<size_t>(Frame->pitches[i]);
        cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        cpy.dstArray      = data->first;
        cpy.WidthInBytes  = static_cast<size_t>(result[i]->m_size.width()) * (hdr ? 2 : 1);
        cpy.Height        = static_cast<size_t>(result[i]->m_size.height());
        CUDA_CHECK(m_cudaFuncs->cuMemcpy2D(&cpy));
    }

    CUDA_CHECK(m_cudaFuncs->cuCtxPopCurrent(&dummy));
    return result;
}

/*! \brief Initialise a CUDA context
 * \note We do not use the FFmpeg internal context creation as the created context
 * is deleted before we have a chance to cleanup our own CUDA resources.
*/
bool MythNVDECInterop::InitialiseCuda(void)
{
    if (!m_context)
        return false;

    OpenGLLocker locker(m_context);

    // retrieve CUDA entry points
    if (cuda_load_functions(&m_cudaFuncs, nullptr) != 0)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to load functions");
        return false;
    }

    // create a CUDA context for the current device
    CUdevice cudevice;
    CUcontext dummy;
    CUresult res = m_cudaFuncs->cuInit(0);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise CUDA API");
        return false;
    }

    unsigned int devicecount;
    res = m_cudaFuncs->cuGLGetDevices(&devicecount, &cudevice, 1, CU_GL_DEVICE_LIST_ALL);
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

    res = m_cudaFuncs->cuCtxCreate(&m_cudaContext, CU_CTX_SCHED_BLOCKING_SYNC, cudevice);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create CUDA context");
        return false;
    }

    m_cudaFuncs->cuCtxPopCurrent(&dummy);
    LOG(VB_GENERAL, LOG_INFO, LOC + "Created CUDA context");
    return true;
}
