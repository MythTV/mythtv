// MythTV
#include "libmythbase/mythconfig.h"
#include "libmythbase/mythcorecontext.h"
#include "mythvideocolourspace.h"
#include "opengl/mythnvdecinterop.h"

// Std
#include <chrono>
#include <thread>

extern "C" {
#include "libavutil/log.h"
#define FFNV_LOG_FUNC(logctx, msg, ...) av_log(logctx, AV_LOG_ERROR, msg,  __VA_ARGS__)
#define FFNV_DEBUG_LOG_FUNC(logctx, msg, ...) av_log(logctx, AV_LOG_DEBUG, msg,  __VA_ARGS__)
#include <ffnvcodec/dynlink_loader.h>
}

#define LOC QString("NVDECInterop: ")

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CUDA_CHECK(CUDA_FUNCS, CUDA_CALL) \
{ \
    CUresult res = (CUDA_FUNCS)->CUDA_CALL;          \
    if (res != CUDA_SUCCESS) { \
        const char * desc; \
        (CUDA_FUNCS)->cuGetErrorString(res, &desc);                      \
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("CUDA error %1 (%2)").arg(res).arg(desc)); \
    } \
}

MythNVDECInterop::MythNVDECInterop(MythPlayerUI* Player, MythRenderOpenGL* Context)
  : MythOpenGLInterop(Context, GL_NVDEC, Player)
{
    InitialiseCuda();
}

MythNVDECInterop::~MythNVDECInterop()
{
    m_referenceFrames.clear();
    MythNVDECInterop::DeleteTextures();
    CleanupContext(m_openglContext, m_cudaFuncs, m_cudaContext);
}

void MythNVDECInterop::DeleteTextures()
{
    if (!(m_cudaContext && m_cudaFuncs))
        return;

    OpenGLLocker locker(m_openglContext);
    CUDA_CHECK(m_cudaFuncs, cuCtxPushCurrent(m_cudaContext))

    if (!m_openglTextures.isEmpty())
    {
        LOG(VB_PLAYBACK, LOG_INFO, LOC + "Deleting CUDA resources");
        for (auto it = m_openglTextures.constBegin(); it != m_openglTextures.constEnd(); ++it)
        {
            std::vector<MythVideoTextureOpenGL*> textures = it.value();
            for (auto & texture : textures)
            {
                auto *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>(texture->m_data);
                if (data && data->second)
                    CUDA_CHECK(m_cudaFuncs, cuGraphicsUnregisterResource(data->second))
                delete data;
                texture->m_data = nullptr;
            }
        }
    }

    CUcontext dummy = nullptr;
    CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy))

    MythOpenGLInterop::DeleteTextures();
}

bool MythNVDECInterop::IsValid()
{
    return m_cudaFuncs && m_cudaContext;
}

CUcontext MythNVDECInterop::GetCUDAContext()
{
    return m_cudaContext;
}

MythNVDECInterop* MythNVDECInterop::CreateNVDEC(MythPlayerUI* Player, MythRenderOpenGL* Context)
{
    if (!(Context && Player))
        return nullptr;

    MythInteropGPU::InteropMap types;
    GetNVDECTypes(Context, types);
    if (auto nvdec = types.find(FMT_NVDEC); nvdec != types.end())
    {
        auto matchType = [](auto type){ return (type == GL_NVDEC); };
        if (std::any_of(nvdec->second.cbegin(), nvdec->second.cend(), matchType))
            return new MythNVDECInterop(Player, Context);
    }
    return nullptr;
}

void MythNVDECInterop::GetNVDECTypes(MythRenderOpenGL* Render, MythInteropGPU::InteropMap& Types)
{
    if (Render)
        Types[FMT_NVDEC] = { GL_NVDEC };
}

/*! \brief Map CUDA video memory to OpenGL textures.
 *
 * \note This is not zero copy - although the copy will be extremely fast.
 * It may be marginally quicker to implement a custom FFmpeg buffer pool that allocates
 * textures and maps the texture storage to a CUdeviceptr (if that is possible). Alternatively
 * EGL interopability may also be useful.
*/
std::vector<MythVideoTextureOpenGL*>
MythNVDECInterop::Acquire(MythRenderOpenGL* Context,
                          MythVideoColourSpace* ColourSpace,
                          MythVideoFrame* Frame,
                          FrameScanType Scan)
{
    std::vector<MythVideoTextureOpenGL*> result;
    if (!Frame || !m_cudaContext || !m_cudaFuncs)
        return result;

    if (Context && (Context != m_openglContext))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Mismatched OpenGL contexts");

    // Check size
    QSize surfacesize(Frame->m_width, Frame->m_height);
    if (m_textureSize != surfacesize)
    {
        if (!m_textureSize.isEmpty())
        {
            LOG(VB_GENERAL, LOG_WARNING, LOC + QString("Video texture size changed! %1x%2->%3x%4")
                .arg(m_textureSize.width()).arg(m_textureSize.height())
                .arg(Frame->m_width).arg(Frame->m_height));
        }
        DeleteTextures();
        m_textureSize = surfacesize;
    }

    // Lock
    OpenGLLocker locker(m_openglContext);

    // Update colourspace and initialise on first frame
    if (ColourSpace)
    {
        if (m_openglTextures.isEmpty())
            ColourSpace->SetSupportedAttributes(ALL_PICTURE_ATTRIBUTES);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve hardware frames context and AVCUDADeviceContext
    if ((Frame->m_pixFmt != AV_PIX_FMT_CUDA) || (Frame->m_type != FMT_NVDEC) ||
        !Frame->m_buffer || !Frame->m_priv[0] || !Frame->m_priv[1])
    {
        return result;
    }

    auto cudabuffer = reinterpret_cast<CUdeviceptr>(Frame->m_buffer);
    if (!cudabuffer)
        return result;

    // make the CUDA context current
    CUcontext dummy = nullptr;
    CUDA_CHECK(m_cudaFuncs, cuCtxPushCurrent(m_cudaContext))

    // create and map textures for a new buffer
    VideoFrameType type = (Frame->m_swPixFmt == AV_PIX_FMT_NONE) ? FMT_NV12 :
                MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_swPixFmt));
    bool p010 = MythVideoFrame::ColorDepth(type) > 8;
    if (!m_openglTextures.contains(cudabuffer))
    {
        std::vector<QSize> sizes;
        sizes.emplace_back(Frame->m_width, Frame->m_height);
        sizes.emplace_back(Frame->m_width, Frame->m_height >> 1);
        std::vector<MythVideoTextureOpenGL*> textures =
            MythVideoTextureOpenGL::CreateTextures(m_openglContext, FMT_NVDEC, type, sizes);
        if (textures.empty())
        {
            CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy))
            return result;
        }

        bool success = true;
        for (uint plane = 0; plane < textures.size(); ++plane)
        {
            // N.B. I think the texture formats for P010 are not strictly compliant
            // with OpenGL ES 3.X but the Nvidia driver does not complain.
            MythVideoTextureOpenGL *tex = textures[plane];
            tex->m_allowGLSLDeint = true;
            m_openglContext->glBindTexture(tex->m_target, tex->m_textureId);
            QOpenGLTexture::PixelFormat format     = QOpenGLTexture::Red;
            QOpenGLTexture::PixelType   pixtype    = p010 ? QOpenGLTexture::UInt16 : QOpenGLTexture::UInt8;
            QOpenGLTexture::TextureFormat internal = p010 ? QOpenGLTexture::R16_UNorm : QOpenGLTexture::R8_UNorm;
            int width = tex->m_size.width();

            if (plane)
            {
                internal = p010 ? QOpenGLTexture::RG16_UNorm : QOpenGLTexture::RG8_UNorm;
                format   = QOpenGLTexture::RG;
                width /= 2;
            }

            m_openglContext->glTexImage2D(tex->m_target, 0, internal, width, tex->m_size.height(),
                                    0, format, pixtype, nullptr);

            CUarray array = nullptr;
            CUgraphicsResource graphicsResource = nullptr;
            CUDA_CHECK(m_cudaFuncs, cuGraphicsGLRegisterImage(&graphicsResource, tex->m_textureId,
                                          QOpenGLTexture::Target2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD))
            if (graphicsResource)
            {
                CUDA_CHECK(m_cudaFuncs, cuGraphicsMapResources(1, &graphicsResource, nullptr))
                CUDA_CHECK(m_cudaFuncs, cuGraphicsSubResourceGetMappedArray(&array, graphicsResource, 0, 0))
                CUDA_CHECK(m_cudaFuncs, cuGraphicsUnmapResources(1, &graphicsResource, nullptr))
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
            for (auto & texture : textures)
            {
                auto *data = reinterpret_cast<QPair<CUarray,CUgraphicsResource>*>(texture->m_data);
                if (data && data->second)
                    CUDA_CHECK(m_cudaFuncs, cuGraphicsUnregisterResource(data->second))
                delete data;
                texture->m_data = nullptr;
                if (texture->m_textureId)
                    m_openglContext->glDeleteTextures(1, &texture->m_textureId);
                MythVideoTextureOpenGL::DeleteTexture(m_openglContext, texture);
            }
        }
    }

    if (!m_openglTextures.contains(cudabuffer))
    {
        CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy))
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
        cpy.srcDevice     = cudabuffer + static_cast<CUdeviceptr>(Frame->m_offsets[i]);
        cpy.srcPitch      = static_cast<size_t>(Frame->m_pitches[i]);
        cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        cpy.dstArray      = data->first;
        cpy.WidthInBytes  = static_cast<size_t>(result[i]->m_size.width()) * (p010 ? 2 : 1);
        cpy.Height        = static_cast<size_t>(result[i]->m_size.height());
        CUDA_CHECK(m_cudaFuncs, cuMemcpy2DAsync(&cpy, nullptr))
    }

    CUDA_CHECK(m_cudaFuncs, cuStreamSynchronize(nullptr))
    CUDA_CHECK(m_cudaFuncs, cuCtxPopCurrent(&dummy))

    // GLSL deinterlacing. The decoder will pick up any CPU or driver preference
    // and return a stream of deinterlaced frames. Just check for GLSL here.
    bool needreferences = false;
    if (is_interlaced(Scan) && !Frame->m_alreadyDeinterlaced)
    {
        MythDeintType shader = Frame->GetDoubleRateOption(DEINT_SHADER);
        if (shader)
            needreferences = shader == DEINT_HIGH;
        else
            needreferences = Frame->GetSingleRateOption(DEINT_SHADER) == DEINT_HIGH;
    }

    if (needreferences)
    {
        if (qAbs(Frame->m_frameCounter - m_discontinuityCounter) > 1)
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
        std::copy(m_openglTextures[current].cbegin(), m_openglTextures[current].cend(), std::back_inserter(result));
        std::copy(m_openglTextures[next].cbegin(), m_openglTextures[next].cend(), std::back_inserter(result));
        return result;
    }
    m_referenceFrames.clear();
    m_discontinuityCounter = Frame->m_frameCounter;

    return result;
}

/*! \brief Initialise a CUDA context
 * \note We do not use the FFmpeg internal context creation as the created context
 * is deleted before we have a chance to cleanup our own CUDA resources.
*/
bool MythNVDECInterop::InitialiseCuda()
{
    return CreateCUDAContext(m_openglContext, m_cudaFuncs, m_cudaContext);
}

bool MythNVDECInterop::CreateCUDAPriv(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                                      CUcontext& CudaContext, bool& Retry)
{
    Retry = false;
    if (!GLContext)
        return false;

    // Make OpenGL context current
    OpenGLLocker locker(GLContext);

    // retrieve CUDA entry points
    if (cuda_load_functions(&CudaFuncs, nullptr) != 0)
    {
        LOG(VB_PLAYBACK, LOG_ERR, LOC + "Failed to load functions");
        return false;
    }

    // create a CUDA context for the current device
    CUdevice cudevice = 0;
    CUcontext dummy = nullptr;
    CUresult res = CudaFuncs->cuInit(0);
    if (res != CUDA_SUCCESS)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to initialise CUDA API");
        return false;
    }

    unsigned int devicecount = 0;
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
        LOG(VB_GENERAL, LOG_ERR, LOC + QString("Failed to create CUDA context (Err: %1)")
            .arg(res));
        Retry = true;
        return false;
    }

    CudaFuncs->cuCtxPopCurrent(&dummy);
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Created CUDA context");
    return true;
}

bool MythNVDECInterop::CreateCUDAContext(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                                         CUcontext& CudaContext)
{
    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Must create CUDA context from main thread");
        return false;
    }

    int retries = 0;
    bool retry = false;
    while (retries++ < 5)
    {
        if (CreateCUDAPriv(GLContext, CudaFuncs, CudaContext, retry))
            return true;
        CleanupContext(GLContext, CudaFuncs, CudaContext);
        if (!retry)
            break;
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Will retry in 50ms");
        std::this_thread::sleep_for(50ms);
    }
    return false;
}

void MythNVDECInterop::CleanupContext(MythRenderOpenGL* GLContext, CudaFunctions*& CudaFuncs,
                                      CUcontext& CudaContext)
{
    if (!GLContext)
        return;

    OpenGLLocker locker(GLContext);
    if (CudaFuncs)
    {
        if (CudaContext)
            CUDA_CHECK(CudaFuncs, cuCtxDestroy(CudaContext))
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
