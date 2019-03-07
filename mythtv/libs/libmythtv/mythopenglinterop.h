#ifndef MYTHOPENGLINTEROP_H
#define MYTHOPENGLINTEROP_H

// MythTV
#include "mythrender_opengl.h"
#include "referencecounter.h"
#include "videoouttypes.h"
#include "mythcodecid.h"
#include "mythframe.h"

// Std
#include "vector"
using std::vector;

class VideoColourSpace;

#define DUMMY_INTEROP_ID 1

class MythOpenGLInterop : public ReferenceCounter
{
  public:

    enum Type
    {
        Unsupported  = 0,
        VAAPIGLXCOPY = 1,
        VAAPIGLXPIX  = 2,
        VAAPIEGLDRM  = 3,
        VTBOPENGL    = 4,
        VTBSURFACE   = 5,
        MEDIACODEC   = 6
    };

    static QStringList GetAllowedRenderers (MythCodecID CodecId);
    static Type        GetInteropType      (MythCodecID CodecId);
    static vector<MythGLTexture*> Retrieve (MythRenderOpenGL *Context,
                                            VideoColourSpace *ColourSpace,
                                            VideoFrame *Frame,
                                            FrameScanType Scan);
    static QString     TypeToString        (Type InteropType);

    virtual ~MythOpenGLInterop();
    virtual vector<MythGLTexture*> Acquire (MythRenderOpenGL *Context,
                                            VideoColourSpace *ColourSpace,
                                            VideoFrame *Frame, FrameScanType Scan) = 0;
    Type GetType(void);

  protected:
    explicit MythOpenGLInterop(MythRenderOpenGL *Context, Type InteropType);

  protected:
    MythRenderOpenGL   *m_context;
    Type                m_type;
    QHash<GLuint, vector<MythGLTexture*> > m_openglTextures;
    QSize               m_openglTextureSize;
};

#endif // MYTHOPENGLINTEROP_H
