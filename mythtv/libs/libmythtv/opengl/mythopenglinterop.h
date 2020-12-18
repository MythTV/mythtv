#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "opengl/mythrenderopengl.h"
#include "referencecounter.h"
#include "videoouttypes.h"
#include "mythframe.h"
#include "opengl/mythvideotextureopengl.h"

// Std
#include "vector"
using std::vector;

class MythPlayerUI;
class MythVideoColourSpace;
using FreeAVHWDeviceContext = void (*)(struct AVHWDeviceContext*);
#define DUMMY_INTEROP_ID 1

class MythOpenGLInterop : public QObject, public ReferenceCounter
{
    Q_OBJECT

  public:

    enum Type
    {
        Unsupported  = 0,
        VAAPIGLXCOPY,
        VAAPIGLXPIX,
        VAAPIEGLDRM,
        VTBOPENGL,
        VTBSURFACE,
        MEDIACODEC,
        VDPAU,
        NVDEC,
        MMAL,
        DRMPRIME,
        DUMMY // used for default free/user_opaque storage
    };

    static QStringList GetAllowedRenderers   (VideoFrameType Format);
    static Type        GetInteropType        (VideoFrameType Format, MythPlayerUI *Player);
    static void        GetInteropTypeCallback(void *Wait, void *Format, void* Result);
    static vector<MythVideoTextureOpenGL*> Retrieve(MythRenderOpenGL *Context,
                                                    MythVideoColourSpace *ColourSpace,
                                                    MythVideoFrame *Frame,
                                                    FrameScanType Scan);
    static QString     TypeToString          (Type InteropType);
    static MythOpenGLInterop* CreateDummy    ();

    ~MythOpenGLInterop() override;
    virtual vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL *Context,
                                                    MythVideoColourSpace *ColourSpace,
                                                    MythVideoFrame *Frame, FrameScanType Scan);

    Type               GetType               ();
    MythPlayerUI*      GetPlayer             ();
    void               SetPlayer             (MythPlayerUI *Player);
    void               SetDefaultFree        (FreeAVHWDeviceContext FreeContext);
    void               SetDefaultUserOpaque  (void* UserOpaque);
    FreeAVHWDeviceContext GetDefaultFree     ();
    void*              GetDefaultUserOpaque  ();

  protected:
    explicit MythOpenGLInterop                (MythRenderOpenGL *Context, Type InteropType);
    virtual void DeleteTextures               ();

  protected:
    MythRenderOpenGL*   m_context              { nullptr };
    Type                m_type                 { Unsupported };
    QHash<unsigned long long, vector<MythVideoTextureOpenGL*> > m_openglTextures;
    QSize               m_openglTextureSize    { };
    uint64_t            m_discontinuityCounter { 0 };
    FreeAVHWDeviceContext m_defaultFree        { nullptr };
    void*               m_defaultUserOpaque    { nullptr };
    MythPlayerUI*       m_player               { nullptr };
};

#endif
