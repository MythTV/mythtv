// MythTV
#include "mythconfig.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythnvdecinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_cuda_internal.h"
#include "libavutil/hwcontext_cuda.h"
}

#define LOC QString("NVDECInterop: ")

#define CUDA_CHECK(CUDA_CALL) { CUresult res = CUDA_CALL; if (res != CUDA_SUCCESS) { \
    LOG(VB_GENERAL, LOG_ERR, LOC + QString("CUDA error: %1").arg(res)); }}

/*! \brief A simple wrapper around resources allocated by the given CUDA context.
 *
 * \note This does not currently release m_resource as the CUDA context is released
 * before AVHWDeviceContext::free is called. This may be the reason why OpenGL
 * rendering is broken after using NVDEC direct rendering - but seems unlikely.
 */
class NVDECData
{
  public:
    NVDECData(CUarray Array, CUgraphicsResource Resource, AVCUDADeviceContext* Context)
      : m_array(Array),
        m_resource(Resource),
        m_context(Context)
    {
    }

   ~NVDECData()
    {
        if (m_context && m_context->cuda_ctx && m_context->internal && m_context->internal->cuda_dl)
        {
            CUcontext dummy;
            CUDA_CHECK(m_context->internal->cuda_dl->cuCtxPushCurrent(m_context->cuda_ctx));
            CUDA_CHECK(m_context->internal->cuda_dl->cuGraphicsUnregisterResource(m_resource));
            CUDA_CHECK(m_context->internal->cuda_dl->cuCtxPopCurrent(&dummy));
        }
    }

    CUarray m_array;
    CUgraphicsResource m_resource;
    AVCUDADeviceContext* m_context;
};

MythNVDECInterop::MythNVDECInterop(MythRenderOpenGL *Context)
  : MythOpenGLInterop(Context, NVDEC)
{
}

MythNVDECInterop::~MythNVDECInterop()
{
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
                // see comment in NVDECData
                NVDECData *data = reinterpret_cast<NVDECData*>((*it2)->m_data);
                if (data)
                    delete data;
                (*it2)->m_data = nullptr;
            }
        }
    }
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
 * textures and maps the texture storage to a CUdeviceptr (if that is possible). Alternativel
 * EGL interopability may also be useful.
*/
vector<MythVideoTexture*> MythNVDECInterop::Acquire(MythRenderOpenGL *Context,
                                                    VideoColourSpace *ColourSpace,
                                                    VideoFrame *Frame,
                                                    FrameScanType)
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

    AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
    if (!buffer || (buffer && !buffer->data))
        return result;
    AVHWDeviceContext* context = reinterpret_cast<AVHWDeviceContext*>(buffer->data);
    AVCUDADeviceContext *devicecontext = reinterpret_cast<AVCUDADeviceContext*>(context->hwctx);
    if (!devicecontext)
        return result;

    CudaFunctions *cu = devicecontext->internal->cuda_dl;
    CUdeviceptr cudabuffer = reinterpret_cast<CUdeviceptr>(Frame->buf);
    if (!cu || !cudabuffer)
        return result;

    // make the CUDA context current
    CUcontext dummy;
    CUDA_CHECK(cu->cuCtxPushCurrent(devicecontext->cuda_ctx));

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
            CUDA_CHECK(cu->cuCtxPopCurrent(&dummy));
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
            CUDA_CHECK(cu->cuGraphicsGLRegisterImage(&graphicsResource, tex->m_textureId,
                                          QOpenGLTexture::Target2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
            CUDA_CHECK(cu->cuGraphicsMapResources(1, &graphicsResource, nullptr));
            CUDA_CHECK(cu->cuGraphicsSubResourceGetMappedArray(&array, graphicsResource, 0, 0));
            CUDA_CHECK(cu->cuGraphicsUnmapResources(1, &graphicsResource, nullptr));
            tex->m_data = reinterpret_cast<unsigned char*>(new NVDECData(array, graphicsResource, devicecontext));
        }
        MythVideoTexture::SetTextureFilters(m_context, textures, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
        m_openglTextures.insert(cudabuffer, textures);
    }

    if (!m_openglTextures.contains(cudabuffer))
    {
        CUDA_CHECK(cu->cuCtxPopCurrent(&dummy));
        return result;
    }

    // Copy device data to array data (i.e. texture) - surely this can be avoided?
    // N.B. we don't use cuMemcpy2DAsync as it gives occasional 'green screen' flashes,
    // even with subsequent synchronisation
    result = m_openglTextures[cudabuffer];
    for (uint i = 0; i < result.size(); ++i)
    {
        NVDECData *data = reinterpret_cast<NVDECData*>(result[i]->m_data);
        CUDA_MEMCPY2D cpy;
        memset(&cpy, 0, sizeof(cpy));
        cpy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
        cpy.srcDevice     = cudabuffer + static_cast<CUdeviceptr>(Frame->offsets[i]);
        cpy.srcPitch      = static_cast<size_t>(Frame->pitches[i]);
        cpy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
        cpy.dstArray      = data->m_array;
        cpy.WidthInBytes  = static_cast<size_t>(result[i]->m_size.width()) * (hdr ? 2 : 1);
        cpy.Height        = static_cast<size_t>(result[i]->m_size.height());
        CUDA_CHECK(cu->cuMemcpy2D(&cpy));
    }

    CUDA_CHECK(cu->cuCtxPopCurrent(&dummy));
    return result;
}

