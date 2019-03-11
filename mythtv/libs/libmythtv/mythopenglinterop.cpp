// MythTV
#include "videooutbase.h"
#include "videocolourspace.h"
#include "mythrender_opengl.h"
#include "mythopenglinterop.h"

#ifdef USING_GLVAAPI
#include "mythvaapiinterop.h"
#endif
#ifdef USING_VTB
#include "mythvtbinterop.h"
#endif
#ifdef USING_MEDIACODEC
#include "mythmediacodeccontext.h"
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
    return "Unsupported";
}

QStringList MythOpenGLInterop::GetAllowedRenderers(MythCodecID CodecId)
{
    QStringList result;
    if (codec_sw_copy(CodecId))
        return result;
#ifdef USING_GLVAAPI
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
    return result;
}

MythOpenGLInterop::Type MythOpenGLInterop::GetInteropType(MythCodecID CodecId)
{
    Type supported = Unsupported;
    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "GetInteropType called from the wrong thread");
        return supported;
    }

#ifdef USING_VTB
    if (codec_is_vtb(CodecId))
        supported = MythVTBInterop::GetInteropType(CodecId);
#endif
#ifdef USING_GLVAAPI
    if (codec_is_vaapi(CodecId))
        supported = MythVAAPIInterop::GetInteropType(CodecId);
#endif
#ifdef USING_MEDIACODEC
    if (codec_is_mediacodec(CodecId))
        supported = MEDIACODEC;
#endif

    if (Unsupported == supported)
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No render support for codec '%1'").arg(toString(CodecId)));
    else
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Rendering supported for codec '%1' with %2")
            .arg(toString(CodecId)).arg(TypeToString(supported)));
    return supported;
}

vector<MythGLTexture*> MythOpenGLInterop::Retrieve(MythRenderOpenGL *Context,
                                                   VideoColourSpace *ColourSpace,
                                                   VideoFrame       *Frame,
                                                   FrameScanType     Scan)
{
    vector<MythGLTexture*> result;
    if (!(Context && Frame))
        return result;

    MythOpenGLInterop* interop = nullptr;
    bool validhwframe = Frame->priv[1];
    bool validhwcodec = false;
#ifdef USING_VTB
    if ((Frame->codec == FMT_VTB) && (Frame->pix_fmt == AV_PIX_FMT_VIDEOTOOLBOX))
        validhwcodec = true;
#endif
#ifdef USING_GLVAAPI
    if ((Frame->codec == FMT_VAAPI) && (Frame->pix_fmt == AV_PIX_FMT_VAAPI))
        validhwcodec = true;
#endif
#ifdef USING_MEDIACODEC
    if ((Frame->codec == FMT_MEDIACODEC) && (Frame->pix_fmt == AV_PIX_FMT_MEDIACODEC))
        validhwcodec = true;
#endif

    if (!(validhwframe && validhwcodec))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not a valid hardware frame");
        return result;
    }

    if ((Frame->codec == FMT_VTB) || (Frame->codec == FMT_MEDIACODEC))
    {
        interop = reinterpret_cast<MythOpenGLInterop*>(Frame->priv[1]);
    }
    else
    {
        // Unpick
        AVBufferRef* buffer = reinterpret_cast<AVBufferRef*>(Frame->priv[1]);
        if (!buffer || (buffer && !buffer->data))
            return result;
        AVHWFramesContext* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
        if (!frames || (frames && !frames->user_opaque))
            return result;
        interop = reinterpret_cast<MythOpenGLInterop*>(frames->user_opaque);
    }

    if (interop)
        return interop->Acquire(Context, ColourSpace, Frame, Scan);
    LOG(VB_GENERAL, LOG_ERR, LOC + "No OpenGL interop found");
    return result;
}

MythOpenGLInterop::MythOpenGLInterop(MythRenderOpenGL *Context, Type InteropType)
  : ReferenceCounter("GLInterop", true),
    m_context(Context),
    m_type(InteropType),
    m_openglTextures(),
    m_openglTextureSize()
{
    m_context->IncrRef();
}

MythOpenGLInterop::~MythOpenGLInterop()
{
    if (!m_openglTextures.isEmpty())
    {
        OpenGLLocker locker(m_context);
        LOG(VB_GENERAL, LOG_INFO, LOC + "Deleting textures");
        QHash<GLuint, vector<MythGLTexture*> >::const_iterator it = m_openglTextures.constBegin();
        for ( ; it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythGLTexture*> textures = it.value();
            vector<MythGLTexture*>::iterator it2 = textures.begin();
            for ( ; it2 != textures.end(); ++it2)
            {
                if ((*it2)->m_textureId)
                    m_context->glDeleteTextures(1, &(*it2)->m_textureId);
                m_context->DeleteTexture(*it2);
            }
            textures.clear();
        }
        m_openglTextures.clear();
    }
    m_context->DecrRef();
}

MythOpenGLInterop::Type MythOpenGLInterop::GetType(void)
{
    return m_type;
}
