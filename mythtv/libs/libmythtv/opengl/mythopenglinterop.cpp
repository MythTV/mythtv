// Qt
#include <QWaitCondition>

// MythTV
#include "mythplayerui.h"
#include "mythcorecontext.h"
#include "mythvideocolourspace.h"
#include "opengl/mythrenderopengl.h"
#include "opengl/mythopenglinterop.h"

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
#ifdef USING_EGL
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

QStringList MythOpenGLInterop::GetAllowedRenderers(VideoFrameType Format)
{
    QStringList result;
    if (GetInteropType(Format, nullptr) != Unsupported)
        result << "opengl-hw";
    return result;
}

void MythOpenGLInterop::GetInteropTypeCallback(void *Wait, void *Format, void *Result)
{
    LOG(VB_PLAYBACK, LOG_INFO, LOC + "Check interop callback");
    auto *wait = reinterpret_cast<QWaitCondition*>(Wait);
    auto *format = reinterpret_cast<VideoFrameType*>(Format);
    auto *result = reinterpret_cast<MythOpenGLInterop::Type*>(Result);

    if (format && result)
        *result = MythOpenGLInterop::GetInteropType(*format, nullptr);
    if (wait)
        wait->wakeAll();
}

/*! \brief Check whether we support direct rendering for the given VideoFrameType.
*/
MythOpenGLInterop::Type MythOpenGLInterop::GetInteropType(VideoFrameType Format, MythPlayerUI *Player)
{
    // cache supported formats to avoid potentially expensive callbacks
    static QMutex s_lock(QMutex::Recursive);
    static QHash<VideoFrameType,Type> s_supported;

    Type supported = Unsupported;
    bool alreadychecked = false;

    // have we checked this format already
    s_lock.lock();
    if (s_supported.contains(Format))
    {
        supported = s_supported.value(Format);
        alreadychecked = true;
    }
    s_lock.unlock();

    // need to check
    if (!alreadychecked)
    {
        if (!gCoreContext->IsUIThread())
        {
            if (!Player)
            {
                LOG(VB_GENERAL, LOG_ERR, LOC +
                    "GetInteropType called from another thread without player");
            }
            else
            {
                // Note - this is a callback into the same function from the main thread
                Player->HandleDecoderCallback("interop check",
                                              MythOpenGLInterop::GetInteropTypeCallback,
                                              &Format, &supported);
            }
            return supported;
        }

        LOG(VB_PLAYBACK, LOG_INFO, LOC + QString("Checking interop support for %1")
            .arg(MythVideoFrame::FormatDescription(Format)));

#ifdef USING_VTB
        if (FMT_VTB == Format)
            supported = MythVTBInterop::GetInteropType(Format);
#endif
#ifdef USING_VAAPI
        if (FMT_VAAPI == Format)
            supported = MythVAAPIInterop::GetInteropType(Format);
#endif
#ifdef USING_MEDIACODEC
        if (FMT_MEDIACODEC == Format)
            supported = MEDIACODEC;
#endif
#ifdef USING_VDPAU
        if (FMT_VDPAU == Format)
            supported = MythVDPAUInterop::GetInteropType(Format);
#endif
#ifdef USING_NVDEC
        if (FMT_NVDEC == Format)
            supported = MythNVDECInterop::GetInteropType(Format);
#endif
#ifdef USING_MMAL
        if (FMT_MMAL == Format)
            supported = MythMMALInterop::GetInteropType(Format);
#endif
#ifdef USING_EGL
        if (FMT_DRMPRIME == Format)
            supported = MythDRMPRIMEInterop::GetInteropType(Format);
#endif
        // update supported formats
        s_lock.lock();
        s_supported.insert(Format, supported);
        s_lock.unlock();
    }

    if (Unsupported == supported)
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + QString("No render support for frame type '%1'")
            .arg(MythVideoFrame::FormatDescription(Format)));
    }
    else
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Rendering supported for frame type '%1' with %2")
            .arg(MythVideoFrame::FormatDescription(Format)).arg(TypeToString(supported)));
    }
    return supported;
}

vector<MythVideoTextureOpenGL*> MythOpenGLInterop::Retrieve(MythRenderOpenGL *Context,
                                                            MythVideoColourSpace *ColourSpace,
                                                            MythVideoFrame       *Frame,
                                                            FrameScanType     Scan)
{
    vector<MythVideoTextureOpenGL*> result;
    if (!(Context && Frame))
        return result;

    if (!(Frame->m_priv[1] && MythVideoFrame::HardwareFormat(Frame->m_type) &&
         (Frame->m_type == MythAVUtil::PixelFormatToFrameType(static_cast<AVPixelFormat>(Frame->m_pixFmt)))))
    {
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Not a valid hardware frame");
        return result;
    }

    MythOpenGLInterop* interop = nullptr;
    if ((Frame->m_type == FMT_VTB)  || (Frame->m_type == FMT_MEDIACODEC) ||
        (Frame->m_type == FMT_MMAL) || (Frame->m_type == FMT_DRMPRIME))
    {
        interop = reinterpret_cast<MythOpenGLInterop*>(Frame->m_priv[1]);
    }
    else
    {
        // Unpick
        auto* buffer = reinterpret_cast<AVBufferRef*>(Frame->m_priv[1]);
        if (!buffer || !buffer->data)
            return result;
        if (Frame->m_type == FMT_NVDEC)
        {
            auto* context = reinterpret_cast<AVHWDeviceContext*>(buffer->data);
            if (!context || !context->user_opaque)
                return result;
            interop = reinterpret_cast<MythOpenGLInterop*>(context->user_opaque);
        }
        else
        {
            auto* frames = reinterpret_cast<AVHWFramesContext*>(buffer->data);
            if (!frames || !frames->user_opaque)
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
  : ReferenceCounter("GLInterop", true),
    m_context(Context),
    m_type(InteropType)
{
    if (m_context)
        m_context->IncrRef();
}

MythOpenGLInterop::~MythOpenGLInterop()
{
    MythOpenGLInterop::DeleteTextures();
    if (m_context)
        m_context->DecrRef();
}

MythOpenGLInterop* MythOpenGLInterop::CreateDummy()
{
    // This is used to store AVHWDeviceContext free and user_opaque when
    // set by the decoder in use. This usually applies to VAAPI and VDPAU
    // and we do not always want or need to use MythRenderOpenGL e.g. when
    // checking functionality only.
    return new MythOpenGLInterop(nullptr, DUMMY);
}

vector<MythVideoTextureOpenGL*> MythOpenGLInterop::Acquire(MythRenderOpenGL* /*Context*/,
                                                           MythVideoColourSpace* /*ColourSpace*/,
                                                           MythVideoFrame* /*Frame*/, FrameScanType /*Scan*/)
{
    return vector<MythVideoTextureOpenGL*>();
}

void MythOpenGLInterop::DeleteTextures()
{
    if (m_context && !m_openglTextures.isEmpty())
    {
        OpenGLLocker locker(m_context);
        int count = 0;
        for (auto it = m_openglTextures.constBegin(); it != m_openglTextures.constEnd(); ++it)
        {
            vector<MythVideoTextureOpenGL*> textures = it.value();
            for (auto & texture : textures)
            {
                if (texture->m_textureId)
                    m_context->glDeleteTextures(1, &texture->m_textureId);
                MythVideoTextureOpenGL::DeleteTexture(m_context, texture);
                count++;
            }
            textures.clear();
        }
        LOG(VB_GENERAL, LOG_INFO, LOC + QString("Deleted %1 textures in %2 groups")
            .arg(count).arg(m_openglTextures.size()));
        m_openglTextures.clear();
    }
}

MythOpenGLInterop::Type MythOpenGLInterop::GetType()
{
    return m_type;
}

MythPlayerUI* MythOpenGLInterop::GetPlayer()
{
    return m_player;
}

void MythOpenGLInterop::SetPlayer(MythPlayerUI* Player)
{
    m_player = Player;
}

void MythOpenGLInterop::SetDefaultFree(FreeAVHWDeviceContext FreeContext)
{
    m_defaultFree = FreeContext;
}

void MythOpenGLInterop::SetDefaultUserOpaque(void* UserOpaque)
{
    m_defaultUserOpaque = UserOpaque;
}

FreeAVHWDeviceContext MythOpenGLInterop::GetDefaultFree()
{
    return m_defaultFree;
}

void* MythOpenGLInterop::GetDefaultUserOpaque()
{
    return m_defaultUserOpaque;
}
