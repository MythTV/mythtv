// Qt
#include <QWaitCondition>

// MythTV
#include "mythmainwindow.h"
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "opengl/mythrenderopengl.h"
#include "mythopenglinterop.h"

#ifdef USING_VAAPI
#include "mythvaapiinterop.h"
#endif
#ifdef USING_VTB
#include "mythvtbinterop.h"
#endif
#ifdef USING_MEDIACODEC
#include "mythmediacodeccontext.h"
#endif
#ifdef USING_VDPAU
#include "mythvdpauinterop.h"
#endif
#ifdef USING_NVDEC
#include "mythnvdecinterop.h"
#endif
#ifdef USING_MMAL
#include "mythmmalinterop.h"
#endif
#ifdef USING_V4L2PRIME
#include "mythdrmprimeinterop.h"
#endif

#define LOC QString("OpenGLInterop: ")

QString MythOpenGLInterop::TypeToString(Type InteropType)
{
    if (InteropType == VAAPIEGLDRM)  return "VAAPI DRM";
    if (InteropType == VAAPIGLXPIX)  return "VAAPI GLX Pixmap";
    if (InteropType == VAAPIGLXCOPY) return "VAAPI GLX Copy";
    if (InteropType == VTBOPENGL)    return "VTB OpenGL";
    if (InteropType == VTBSURFACE)   return "VTB IOSurface";
    if (InteropType == MEDIACODEC)   return "MediaCodec Surface";
    if (InteropType == VDPAU)        return "VDPAU";
    if (InteropType == NVDEC)        return "NVDEC";
    if (InteropType == MMAL)         return "MMAL";
    if (InteropType == DRMPRIME)     return "DRM PRIME";
    if (InteropType == DUMMY)        return "DUMMY";
    return "Unsupported";
}

QStringList MythOpenGLInterop::GetAllowedRenderers(MythCodecID CodecId)
{
    QStringList result;
    if (codec_sw_copy(CodecId))
        return result;
#ifdef USING_VAAPI
    else if (codec_is_vaapi(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
#ifdef USING_VTB
    else if (codec_is_vtb(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
#ifdef USING_MEDIACODEC
    else if (codec_is_mediacodec(CodecId) /*&& (GetInteropType(CodecId) != Unsupported)*/)
        result << "opengl-hw";
#endif
#ifdef USING_VDPAU
    else if (codec_is_vdpau_hw(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
#ifdef USING_NVDEC
    else if (codec_is_nvdec(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
#ifdef USING_MMAL
    else if (codec_is_mmal(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
#ifdef USING_V4L2PRIME
    else if (codec_is_v4l2(CodecId) && (GetInteropType(CodecId) != Unsupported))
        result << "opengl-hw";
#endif
    return result;
}

void MythOpenGLInterop::GetInteropTypeCallback(void *Wait, void *CodecId, void *Result)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Check interop callback");
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    MythCodecID *codecid = reinterpret_cast<MythCodecID*>(CodecId);
    MythOpenGLInterop::Type *result = reinterpret_cast<MythOpenGLInterop::Type*>(Result);

    if (codecid && result)
        *result = MythOpenGLInterop::GetInteropType(*codecid);
    if (wait)
        wait->wakeAll();
}

MythOpenGLInterop::Type MythOpenGLInterop::GetInteropType(MythCodecID CodecId)
{
    Type supported = Unsupported;
    if (!gCoreContext->IsUIThread())
    {
        MythMainWindow::HandleCallback("interop check", MythOpenGLInterop::GetInteropTypeCallback,
                                       &CodecId, &supported);
        return supported;
    }

#ifdef USING_VTB
    if (codec_is_vtb(CodecId))
        supported = MythVTBInterop::GetInteropType(CodecId);
#endif
#ifdef USING_VAAPI
    if (codec_is_vaapi(CodecId))
        supported = MythVAAPIInterop::GetInteropType(CodecId);
#endif
#ifdef USING_MEDIACODEC
    if (codec_is_mediacodec(CodecId))
        supported = MEDIACODEC;
#endif
#ifdef USING_VDPAU
    if (codec_is_vdpau_hw(CodecId))
        supported = MythVDPAUInterop::GetInteropType(CodecId);
#endif
#ifdef USING_NVDEC
    if (codec_is_nvdec(CodecId))
        supported = MythNVDECInterop::GetInteropType(CodecId);
#endif
#ifdef USING_MMAL
    if (codec_is_mmal(CodecId))
        supported = MythMMALInterop::GetInteropType(CodecId);
#endif
#ifdef USING_V4L2PRIME
    if (codec_is_v4l2(CodecId))
        supported = MythDRMPRIMEInterop::GetInteropType(CodecId);
#endif

    if (Unsupported == supported)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No render support for codec '%1'").arg(toString(CodecId)));
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Rendering supported for codec '%1' with %2")
            .arg(toString(CodecId)).arg(TypeToString(supported)));
    return supported;
}

vector<MythVideoTexture*> MythOpenGLInterop::Retrieve(MythRenderOpenGL *Context,
                                                      VideoColourSpace *ColourSpace,
                                                      VideoFrame       *Frame,
                                                      FrameScanType     Scan)
{
    vector<MythVideoTexture*> result;
    if (!(Context && Frame))
        return result;

    MythOpenGLInterop* interop = nullptr;
    bool validhwframe = Frame->priv[1];
    bool validhwcodec = false;
#ifdef USING_VTB
    if ((Frame->codec == FMT_VTB) && (Frame->pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX))
        validhwcodec = true;
#endif
#ifdef USING_VAAPI
    if ((Frame->codec == FMT_VAAPI) && (Frame->pix_fmt == AV_PIX_FMT_VAAPI))
        validhwcodec = true;
#endif
#ifdef USING_MEDIACODEC
    if ((Frame->codec == FMT_MEDIACODEC) && (Frame->pix_fmt == AV_PIX_FMT_MEDIACODEC))
        validhwcodec = true;
#endif
#ifdef USING_VDPAU
    if ((Frame->codec == FMT_VDPAU) && (Frame->pix_fmt == AV_PIX_FMT_VDPAU))
        validhwcodec = true;
#endif
#ifdef USING_NVDEC
    if ((Frame->codec == FMT_NVDEC) && (Frame->pix_fmt == AV_PIX_FMT_CUDA))
        validhwcodec = true;
#endif
#ifdef USING_MMAL
    if ((Frame->codec == FMT_MMAL) && (Frame->pix_fmt == AV_PIX_FMT_MMAL))
        validhwcodec = true;
#endif
#ifdef USING_V4L2PRIME
    if ((Frame->codec == FMT_DRMPRIME) && (Frame->pix_fmt == AV_PIX_FMT_DRM_PRIME))
        validhwcodec = true;
#endif

    if (!(validhwframe && validhwcodec))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not a valid hardware frame");
        return result;
    }

    if ((Frame->codec == FMT_VTB)  || (Frame->codec == FMT_MEDIACODEC) ||
        (Frame->codec == FMT_MMAL) || (Frame->codec == FMT_DRMPRIME))
    {
        interop = reinterpret_cast<MythOpenGLInterop*>(Frame->priv[1]);
    }
    else
    {
        // Unpick
        AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
        if (!buffer || (buffer && !buffer->data))
            return result;
        if (Frame->codec == FMT_NVDEC)
        {
            AVHWDeviceContext* context = reinterpret_cast<AVHWDeviceContext*>(buffer->data);
            if (!context || (context && !context->user_opaque))
                return result;
            interop = reinterpret_cast<MythOpenGLInterop*>(context->user_opaque);
        }
        else
        {
            AVHWFramesContext* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
            if (!frames || (frames && !frames->user_opaque))
                return result;
            interop = reinterpret_cast<MythOpenGLInterop*>(frames->user_opaque);
        }
    }

    if (interop)
        return interop->Acquire(Context, ColourSpace, Frame, Scan);
    LOG(VB_GENERAL, LOG_ERR, LOC + "No OpenGL interop found");
    return result;
}

MythOpenGLInterop::MythOpenGLInterop(MythRenderOpenGL *Context, Type InteropType)
  : QObject(),
    ReferenceCounter("GLInterop", true),
    m_context(Context),
    m_type(InteropType),
    m_openglTextures(),
    m_openglTextureSize(),
    m_discontinuityCounter(0),
    m_defaultFree(nullptr),
    m_defaultUserOpaque(nullptr)
{
    if (m_context)
        m_context->IncrRef();
}

MythOpenGLInterop::~MythOpenGLInterop()
{
    DeleteTextures();
    if (m_context)
        m_context->DecrRef();
}

MythOpenGLInterop* MythOpenGLInterop::CreateDummy(void)
{
    // This is used to store AVHWDeviceContext free and user_opaque when
    // set by the decoder in use. This usually applies to VAAPI and VDPAU
    // and we do not always want or need to use MythRenderOpenGL e.g. when
    // checking functionality only.
    return new MythOpenGLInterop(nullptr, DUMMY);
}

vector<MythVideoTexture*> MythOpenGLInterop::Acquire(MythRenderOpenGL*, VideoColourSpace*,
                                                     VideoFrame*, FrameScanType)
{
    return vector<MythVideoTexture*>();
}

void MythOpenGLInterop::DeleteTextures(void)
{
    if (m_context && !m_openglTextures.isEmpty())
    {
        OpenGLLocker locker(m_context);
        int count = 0;
        QHash<unsigned long long, vector<MythVideoTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTexture*> textures = it.value();
            vector<MythVideoTexture*>::iterator it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                if ((*it2)->m_textureId)
                    m_context->glDeleteTextures(1, &(*it2)->m_textureId);
                MythVideoTexture::DeleteTexture(m_context, *it2);
                count++;
            }
            textures.clear();
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Deleted %1 textures in %2 groups")
            .arg(count).arg(m_openglTextures.size()));
        m_openglTextures.clear();
    }
}

MythOpenGLInterop::Type MythOpenGLInterop::GetType(void)
{
    return m_type;
}

void MythOpenGLInterop::SetDefaultFree(FreeAVHWDeviceContext FreeContext)
{
    m_defaultFree = FreeContext;
}

void MythOpenGLInterop::SetDefaultUserOpaque(void* UserOpaque)
{
    m_defaultUserOpaque = UserOpaque;
}

FreeAVHWDeviceContext MythOpenGLInterop::GetDefaultFree(void)
{
    return m_defaultFree;
}

void* MythOpenGLInterop::GetDefaultUserOpaque(void)
{
    return m_defaultUserOpaque;
}
