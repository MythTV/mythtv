#ifndef MYTHEGL_H
#define MYTHEGL_H

// Qt
#include <QOpenGLContext>

// MythTV
#include "libmythui/mythuiexp.h"

using MYTH_EGLIMAGETARGET  = void  (*)(GLenum, void*);
using MYTH_EGLCREATEIMAGE  = void* (*)(void*, void*, unsigned int, void*, const int32_t *);
using MYTH_EGLDESTROYIMAGE = void  (*)(void*, void*);
using MYTH_EGLGETPLATFORMDISPLAY = void* (*)(GLenum, void*, const intptr_t*);

class MythRenderOpenGL;

class MUI_PUBLIC MythEGL
{
  public:
    explicit MythEGL(MythRenderOpenGL *Context);
   ~MythEGL() = default;

    bool  IsEGL(void);
    bool  HasEGLExtension(QString Extension);
    void* GetEGLDisplay(void);
    static qint32 GetEGLError(void);
    void  eglImageTargetTexture2DOES (GLenum Target, void* Image);
    void* eglCreateImageKHR          (void* Disp, void* Context, unsigned int Target,
                                      void* Buffer, const int32_t *Attributes);
    void  eglDestroyImageKHR         (void* Disp, void* Image);
    static QString GetEGLVendor(void);

  private:
    Q_DISABLE_COPY(MythEGL)
    bool  InitEGL(void);

    MythRenderOpenGL    *m_context                    { nullptr };
    void*                m_eglDisplay                 { nullptr };
    MYTH_EGLIMAGETARGET  m_eglImageTargetTexture2DOES { nullptr };
    MYTH_EGLCREATEIMAGE  m_eglCreateImageKHR          { nullptr };
    MYTH_EGLDESTROYIMAGE m_eglDestroyImageKHR         { nullptr };
};

#endif // MYTHEGL_H
