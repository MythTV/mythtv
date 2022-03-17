// MythTV
#include "libmythbase/mythcorecontext.h"
#include "libmythui/mythrender_base.h"
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
    if (Type == GL_VAAPIEGLDRM)  return "GL-VAAPI-DRM";
    if (Type == GL_VAAPIGLXPIX)  return "GL-VAAPI-GLX-Pixmap";
    if (Type == GL_VAAPIGLXCOPY) return "GL-VAAPI-GLX-Copy";
    if (Type == GL_VTB)          return "GL-VTB";
    if (Type == GL_VTBSURFACE)   return "GL-VTB-IOSurface";
    if (Type == GL_MEDIACODEC)   return "GL-MediaCodec-Surface";
    if (Type == GL_VDPAU)        return "GL-VDPAU";
    if (Type == GL_NVDEC)        return "GL-NVDEC";
    if (Type == GL_MMAL)         return "GL-MMAL";
    if (Type == GL_DRMPRIME)     return "GL-DRM-PRIME";
    if (Type == DRM_DRMPRIME)    return "DRM-DRM-PRIME";
    if (Type == DUMMY)           return "DUMMY";
    return "Unsupported";
}

QString MythInteropGPU::TypesToString(const InteropMap &Types)
{
    QStringList result;
    for (const auto & types : Types)
        for (auto type : types.second)
            result << TypeToString(type);
    result.removeDuplicates();
    return result.isEmpty() ? "None" : result.join(",");
}

MythInteropGPU* MythInteropGPU::CreateDummy()
{
    // This is used to store AVHWDeviceContext free and user_opaque when
    // set by the decoder in use. This usually applies to VAAPI and VDPAU
    // and we do not always want or need to use MythRenderOpenGL e.g. when
    // checking functionality only.
    return new MythInteropGPU(nullptr, DUMMY);
}

MythInteropGPU::MythInteropGPU(MythRender* Context, InteropType Type, MythPlayerUI* Player)
  : ReferenceCounter(TypeToString(Type), true),
    m_context(Context),
    m_player(Player),
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

