// MythTV
#include "mythrenderopengl.h"
#include "mythegl.h"

#define LOC QString("EGL: ")

#ifdef USING_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

MythEGL::MythEGL(MythRenderOpenGL* Context)
  : m_context(Context)
{
}

bool MythEGL::IsEGL()
{
    return InitEGL();
}

bool MythEGL::InitEGL(void)
{
#ifdef USING_EGL
    if (m_eglImageTargetTexture2DOES && m_eglCreateImageKHR && m_eglDestroyImageKHR && m_eglDisplay)
        return true;

    if (!m_context)
        return false;

    OpenGLLocker locker(m_context);
    m_eglDisplay = eglGetCurrentDisplay();
    if (!m_eglDisplay)
    {
        LOG(VB_GENERAL, LOG_INFO, LOC + "No EGL display");
        return false;
    }

    m_eglImageTargetTexture2DOES = reinterpret_cast<MYTH_EGLIMAGETARGET>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    m_eglCreateImageKHR          = reinterpret_cast<MYTH_EGLCREATEIMAGE>(eglGetProcAddress("eglCreateImageKHR"));
    m_eglDestroyImageKHR         = reinterpret_cast<MYTH_EGLDESTROYIMAGE>(eglGetProcAddress("eglDestroyImageKHR"));

    if (m_eglCreateImageKHR && m_eglDestroyImageKHR && m_eglImageTargetTexture2DOES)
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to resolve EGL functions");
#endif
    return false;
}

bool MythEGL::HasEGLExtension(QString Extension)
{
#ifdef USING_EGL
    OpenGLLocker locker(m_context);
    if (m_eglDisplay)
    {
        QByteArray extensions = QByteArray(eglQueryString(m_eglDisplay, EGL_EXTENSIONS));
        return extensions.contains(Extension.data()->toLatin1());
    }
#endif
    return false;
}

void* MythEGL::GetEGLDisplay(void)
{
    return m_eglDisplay;
}

void MythEGL::eglImageTargetTexture2DOES(GLenum Target, void *Image)
{
    if (m_eglImageTargetTexture2DOES)
        m_eglImageTargetTexture2DOES(Target, Image);
}

void* MythEGL::eglCreateImageKHR(void *Disp, void *Context, unsigned int Target,
                                 void *Buffer, const int32_t *Attributes)
{
    if (m_eglCreateImageKHR)
        return m_eglCreateImageKHR(Disp, Context, Target, Buffer, Attributes);
    return nullptr;
}

void MythEGL::eglDestroyImageKHR(void *Disp, void *Image)
{
    if (m_eglDestroyImageKHR)
        m_eglDestroyImageKHR(Disp, Image);
}
