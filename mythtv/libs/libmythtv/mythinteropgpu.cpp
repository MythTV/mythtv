// MythTV
#include "mythcorecontext.h"
#include "mythrender_base.h"
#include "mythinteropgpu.h"

#ifdef USING_OPENGL
#include "opengl/mythopenglinterop.h"
#endif

MythInteropGPU::InteropMap MythInteropGPU::GetTypes(MythRender* Render)
{
    InteropMap result;
    if (!gCoreContext->IsUIThread())
        return result;

#ifdef USING_OPENGL
    MythOpenGLInterop::GetTypes(Render, result);
#endif
    return result;
}

QString MythInteropGPU::TypeToString(InteropType Type)
{
    if (Type == VAAPIEGLDRM)  return "VAAPI-DRM";
    if (Type == VAAPIGLXPIX)  return "VAAPI-GLX-Pixmap";
    if (Type == VAAPIGLXCOPY) return "VAAPI-GLX-Copy";
    if (Type == VTBOPENGL)    return "VTB-OpenGL";
    if (Type == VTBSURFACE)   return "VTB-IOSurface";
    if (Type == MEDIACODEC)   return "MediaCodec-Surface";
    if (Type == VDPAU)        return "VDPAU";
    if (Type == NVDEC)        return "NVDEC";
    if (Type == MMAL)         return "MMAL";
    if (Type == DRMPRIME)     return "DRM-PRIME";
    if (Type == DUMMY)        return "DUMMY";
    return "Unsupported";
}

QString MythInteropGPU::TypesToString(InteropMap Types)
{
    QStringList result;
    for (auto types : Types)
        for (auto type : types.second)
            result << TypeToString(type);
    return result.join(",");
}

MythInteropGPU* MythInteropGPU::CreateDummy()
{
    // This is used to store AVHWDeviceContext free and user_opaque when
    // set by the decoder in use. This usually applies to VAAPI and VDPAU
    // and we do not always want or need to use MythRenderOpenGL e.g. when
    // checking functionality only.
    return new MythInteropGPU(nullptr, DUMMY);
}

MythInteropGPU::MythInteropGPU(MythRender *Context, InteropType Type)
  : ReferenceCounter(TypeToString(Type), true),
    m_context(Context),
    m_type(Type)
{
    if (m_context)
        m_context->IncrRef();
}

MythInteropGPU::~MythInteropGPU()
{
    if (m_context)
        m_context->DecrRef();
}

MythInteropGPU::InteropType MythInteropGPU::GetType()
{
    return m_type;
}

MythPlayerUI* MythInteropGPU::GetPlayer()
{
    return m_player;
}

void MythInteropGPU::SetPlayer(MythPlayerUI* Player)
{
    m_player = Player;
}

void MythInteropGPU::SetDefaultFree(FreeAVHWDeviceContext FreeContext)
{
    m_defaultFree = FreeContext;
}

void MythInteropGPU::SetDefaultUserOpaque(void* UserOpaque)
{
    m_defaultUserOpaque = UserOpaque;
}

FreeAVHWDeviceContext MythInteropGPU::GetDefaultFree()
{
    return m_defaultFree;
}

void* MythInteropGPU::GetDefaultUserOpaque()
{
    return m_defaultUserOpaque;
}

