// MythTV
#include "mythrenderopengl.h"
#include "mythegl.h"

#define LOC QString("EGL: ")

#ifdef USING_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifndef EGL_EXT_platform_device
#define EGL_PLATFORM_DEVICE_EXT  0x313F
#endif

#ifndef EGL_EXT_platform_wayland
#define EGL_PLATFORM_WAYLAND_EXT 0x31D8
#endif

#ifndef EGL_EXT_platform_x11
#define EGL_PLATFORM_X11_EXT     0x31D5
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
    // N.B. Strictly speaking this reports both whether EGL is in use and whether
    // EGL_KHR_image functionality is present - which is currently the only thing
    // we are interested in.
#ifdef USING_EGL
    if (m_eglImageTargetTexture2DOES && m_eglCreateImageKHR && m_eglDestroyImageKHR && m_eglDisplay)
        return true;

    if (!m_context)
        return false;

    OpenGLLocker locker(m_context);
    m_eglDisplay = eglGetCurrentDisplay();
    if (!m_eglDisplay)
        return false;

    m_eglImageTargetTexture2DOES = reinterpret_cast<MYTH_EGLIMAGETARGET>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    m_eglCreateImageKHR          = reinterpret_cast<MYTH_EGLCREATEIMAGE>(eglGetProcAddress("eglCreateImageKHR"));
    m_eglDestroyImageKHR         = reinterpret_cast<MYTH_EGLDESTROYIMAGE>(eglGetProcAddress("eglDestroyImageKHR"));

    if (m_eglCreateImageKHR && m_eglDestroyImageKHR && m_eglImageTargetTexture2DOES)
        return true;

    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to resolve EGL functions");
#endif
    return false;
}

bool MythEGL::HasEGLExtension([[maybe_unused]] QString Extension)
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

QString MythEGL::GetEGLVendor(void)
{
#ifdef USING_EGL
    auto CheckDisplay = [](EGLDisplay EglDisplay)
    {
        if (EglDisplay == EGL_NO_DISPLAY)
            return QString();
        int major = 1;
        int minor = 4;
        if (!eglInitialize(EglDisplay, &major, &minor))
            return QString();
        QString vendor  = eglQueryString(EglDisplay, EGL_VENDOR);
        QString apis    = eglQueryString(EglDisplay, EGL_CLIENT_APIS);
        QString version = eglQueryString(EglDisplay, EGL_VERSION);
        eglTerminate(EglDisplay);
        if (!apis.contains("opengl", Qt::CaseInsensitive) || (major < 1 || minor < 2))
            return QString();
        return QString("%1, %2").arg(vendor, version);
    };

    QString extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (extensions.contains("EGL_EXT_platform_base"))
    {
        QString vendor;
        auto getdisp =
            reinterpret_cast<MYTH_EGLGETPLATFORMDISPLAY>(eglGetProcAddress("eglGetPlatformDisplay"));
        if (getdisp && extensions.contains("platform_x11"))
        {
            vendor = CheckDisplay(getdisp(EGL_PLATFORM_X11_EXT, EGL_DEFAULT_DISPLAY, nullptr));
            if (!vendor.isEmpty())
                return vendor;
        }
        if (getdisp && extensions.contains("platform_wayland"))
        {
            vendor = CheckDisplay(getdisp(EGL_PLATFORM_WAYLAND_EXT, EGL_DEFAULT_DISPLAY, nullptr));
            if (!vendor.isEmpty())
                return vendor;
        }
        if (getdisp && extensions.contains("platform_device"))
        {
            vendor = CheckDisplay(getdisp(EGL_PLATFORM_DEVICE_EXT, EGL_DEFAULT_DISPLAY, nullptr));
            if (!vendor.isEmpty())
                return vendor;
        }
    }

    return CheckDisplay(eglGetDisplay(EGL_DEFAULT_DISPLAY));
#else
    return QString();
#endif
}

qint32 MythEGL::GetEGLError(void)
{
#ifdef USING_EGL
    return static_cast<qint32>(eglGetError());
#else
    return 0; // EGL_FALSE
#endif
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
