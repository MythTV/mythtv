#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// Qt
#include <QObject>

// MythTV
#include "opengl/mythrenderopengl.h"
#include "referencecounter.h"
#include "videoouttypes.h"
#include "mythframe.h"
#include "mythvideotexture.h"

// Std
#include "vector"
using std::vector;

class MythPlayer;
class VideoColourSpace;
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
    static Type        GetInteropType        (VideoFrameType Format, MythPlayer *Player);
    static void        GetInteropTypeCallback(void *Wait, void *Format, void* Result);
    static vector<MythVideoTexture*> Retrieve(MythRenderOpenGL *Context,
                                              VideoColourSpace *ColourSpace,
                                              VideoFrame *Frame,
                                              FrameScanType Scan);
    static QString     TypeToString          (Type InteropType);
    static MythOpenGLInterop* CreateDummy    (void);

    virtual ~MythOpenGLInterop();
    virtual vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                              VideoColourSpace *ColourSpace,
                                              VideoFrame *Frame, FrameScanType Scan);

    Type               GetType               (void);
    MythPlayer*        GetPlayer             (void);
    void               SetPlayer             (MythPlayer *Player);
    void               SetDefaultFree        (FreeAVHWDeviceContext FreeContext);
    void               SetDefaultUserOpaque  (void* UserOpaque);
    FreeAVHWDeviceContext GetDefaultFree     (void);
    void*              GetDefaultUserOpaque  (void);

  protected:
    explicit MythOpenGLInterop                (MythRenderOpenGL *Context, Type InteropType);
    virtual void DeleteTextures               (void);

  protected:
    MythRenderOpenGL   *m_context;
    Type                m_type;
    QHash<unsigned long long, vector<MythVideoTexture*> > m_openglTextures;
    QSize               m_openglTextureSize    { };
    long long           m_discontinuityCounter { 0 };
    FreeAVHWDeviceContext m_defaultFree        { nullptr };
    void               *m_defaultUserOpaque    { nullptr };
    MythPlayer         *m_player               { nullptr };
};

#endif // MYTHOPENGLINTEROP_H
