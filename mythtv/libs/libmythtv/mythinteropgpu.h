#ifndef MYTHINTEROPGPU_H
#define MYTHINTEROPGPU_H

// Qt
#include <QSize>
#include <QObject>

// MythTV
#include "libmythbase/referencecounter.h"
#include "mythframe.h"

// Std
#include <vector>

class MythRender;
class MythPlayerUI;
using FreeAVHWDeviceContext = void (*)(struct AVHWDeviceContext*);
static constexpr uint64_t DUMMY_INTEROP_ID { 1 };

class MythInteropGPU : public QObject, public ReferenceCounter
{
    Q_OBJECT

  public:
    enum InteropType : std::uint8_t
    {
        Unsupported  = 0,
        GL_VAAPIGLXCOPY,
        GL_VAAPIGLXPIX,
        GL_VAAPIEGLDRM,
        GL_VTB,
        GL_VTBSURFACE,
        GL_MEDIACODEC,
        GL_VDPAU,
        GL_NVDEC,
        GL_MMAL,
        GL_DRMPRIME,
        DRM_DRMPRIME,
        // used for default free/user_opaque storage
        DUMMY
    };

    using InteropTypes = std::vector<InteropType>;
    using InteropMap   = std::map<VideoFrameType,InteropTypes>;
    static InteropMap      GetTypes(MythRender* Render);
    static QString         TypeToString(InteropType Type);
    static QString         TypesToString(const InteropMap& Types);
    static MythInteropGPU* CreateDummy();

    MythInteropGPU(MythRender* Context, InteropType Type, MythPlayerUI* Player = nullptr);
   ~MythInteropGPU() override;

    InteropType    GetType               ();
    MythPlayerUI*  GetPlayer             ();
    void           SetDefaultFree        (FreeAVHWDeviceContext FreeContext);
    void           SetDefaultUserOpaque  (void* UserOpaque);
    FreeAVHWDeviceContext GetDefaultFree ();
    void*          GetDefaultUserOpaque  ();

  protected:
    MythRender*         m_context              { nullptr };
    QSize               m_textureSize;
    uint64_t            m_discontinuityCounter { 0 };
    FreeAVHWDeviceContext m_defaultFree        { nullptr };
    void*               m_defaultUserOpaque    { nullptr };
    MythPlayerUI*       m_player               { nullptr };
    InteropType         m_type                 { Unsupported };
};

#endif
