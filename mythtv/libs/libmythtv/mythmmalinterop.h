#ifndef MYTHMMALINTEROP_H
#define MYTHMMALINTEROP_H

// MythTV
#include "mythopenglinterop.h"

// MMAL
extern "C" {
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>
}

// Egl
#include <EGL/egl.h>
#include <EGL/eglext.h>
typedef void  ( * MYTH_EGLIMAGETARGET)  (GLenum, void*);
typedef EGLImageKHR ( * MYTH_EGLCREATEIMAGE)  (EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
typedef void  ( * MYTH_EGLDESTROYIMAGE) (EGLDisplay, void*);

class MythMMALInterop : public MythOpenGLInterop
{
  public:
    static MythMMALInterop* Create(MythRenderOpenGL *Context, Type InteropType);
    static Type GetInteropType(MythCodecID CodecId, MythRenderOpenGL *Context = nullptr);

    MythMMALInterop(MythRenderOpenGL *Context);
    virtual ~MythMMALInterop() override;

    virtual vector<MythVideoTexture*> Acquire(MythRenderOpenGL *Context,
                                              VideoColourSpace *ColourSpace,
                                              VideoFrame *Frame, FrameScanType Scan) override;

  private:
    MMAL_BUFFER_HEADER_T* VerifyBuffer(MythRenderOpenGL *Context, VideoFrame *Frame);
    bool                  InitEGL(void);

    MYTH_EGLIMAGETARGET   m_eglImageTargetTexture2DOES { nullptr };
    MYTH_EGLCREATEIMAGE   m_eglCreateImageKHR          { nullptr };
    MYTH_EGLDESTROYIMAGE  m_eglDestroyImageKHR         { nullptr };
};

#endif // MYTHMMALINTEROP_H
