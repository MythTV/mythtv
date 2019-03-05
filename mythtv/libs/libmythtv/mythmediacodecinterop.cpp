// MythTV
#include "mythcorecontext.h"
#include "videocolourspace.h"
#include "mythmediacodecinterop.h"

// FFmpeg
extern "C" {
#include "libavutil/hwcontext_mediacodec.h"
#include "libavcodec/mediacodec.h"
}

#define LOC QString("MediaCodecInterop: ")
#define DUMMY_TEXTURE_ID 1

MythMediaCodecInterop* MythMediaCodecInterop::Create(MythRenderOpenGL *Context, QSize Size)
{
    MythMediaCodecInterop* result = new MythMediaCodecInterop(Context);
    if (result)
    {
        if (result->Initialise(Size))
            return result;
        delete result;
    }
    return nullptr;
}

MythMediaCodecInterop::MythMediaCodecInterop(MythRenderOpenGL* Context)
  : MythOpenGLInterop(Context, MEDIACODEC),
    m_frameWait(),
    m_frameWaitLock(),
    m_colourSpaceInitialised(false),
    m_surface(),
    m_surfaceTexture(),
    m_surfaceListener()
{
}

MythMediaCodecInterop::~MythMediaCodecInterop()
{
}

void* MythMediaCodecInterop::GetSurface(void)
{
    return m_surface.object();
}

void Java_org_mythtv_android_SurfaceTextureListener_frameAvailable(JNIEnv*, jobject, jlong Wait, jobject)
{
    QWaitCondition *wait = reinterpret_cast<QWaitCondition*>(Wait);
    if (wait)
        wait->wakeAll();
}

bool MythMediaCodecInterop::Initialise(QSize Size)
{
    if (!m_context)
        return false;

    if (!gCoreContext->IsUIThread())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Must be created in UI thread");
        return false;
    }

    // Lock
    OpenGLLocker locker(m_context);

    // Create texture
    // N.B. SetTextureFilters currently assumes Texture2D (default value)
    MythGLTexture *texture = m_context->CreateExternalTexture(Size, false);
    if (!texture)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create texture");
        return false;
    }

    // Set the texture type
    texture->m_target = GL_TEXTURE_EXTERNAL_OES;
    m_context->SetTextureFilters(texture, QOpenGLTexture::Linear, QOpenGLTexture::ClampToEdge);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture->m_textureId);

    // Create surface
    m_surfaceTexture = QAndroidJniObject("android/graphics/SurfaceTexture", "(I)V", texture->m_textureId);
    //N.B. org/mythtv/android/SurfaceTextureListener is found in the packaging repo
    m_surfaceListener = QAndroidJniObject("org/mythtv/android/SurfaceTextureListener", "(J)V", jlong(&m_frameWait));
    if (m_surfaceTexture.isValid() && m_surfaceListener.isValid())
    {
        m_surfaceTexture.callMethod<void>("setOnFrameAvailableListener",
                                          "(Landroid/graphics/SurfaceTexture$OnFrameAvailableListener;)V",
                                          m_surfaceListener.object());
        m_surface = QAndroidJniObject("android/view/Surface",
                                      "(Landroid/graphics/SurfaceTexture;)V",
                                      m_surfaceTexture.object());
        if (m_surface.isValid())
        {
            vector<MythGLTexture*> result;
            result.push_back(texture);
            m_openglTextures.insert(DUMMY_TEXTURE_ID, result);
            LOG(VB_GENERAL, LOG_INFO, LOC + "Created Android Surface");
            return true;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Android Surface");
    }
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Android SurfaceTexture");
    m_context->glDeleteTextures(1, &(texture->m_textureId));
    m_context->DeleteTexture(texture);
    return false;
}

vector<MythGLTexture*> MythMediaCodecInterop::Acquire(MythRenderOpenGL *Context,
                                                      VideoColourSpace *ColourSpace,
                                                      VideoFrame *Frame,
                                                      FrameScanType)
{
    vector<MythGLTexture*> result;

    // Pause frame handling
    if (!Frame && !m_openglTextures.isEmpty())
        return m_openglTextures[DUMMY_TEXTURE_ID];

    // Sanitise
    if (!Context || !ColourSpace || !Frame || m_openglTextures.isEmpty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed");
        return result;
    }

    // Check colourspace on first frame
    if (!m_colourSpaceInitialised)
    {
        m_colourSpaceInitialised = true;
        ColourSpace->SetSupportedAttributes(kPictureAttributeSupported_None);
        ColourSpace->UpdateColourSpace(Frame);
    }

    // Retrieve buffer
    AVMediaCodecBuffer *buffer = reinterpret_cast<AVMediaCodecBuffer*>(Frame->buf);
    if (!buffer)
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "No AVMediaCodecBuffer");
        return result;
    }

    // Render...
    m_frameWaitLock.lock();
    if (av_mediacodec_release_buffer(buffer, 1) < 0)
        LOG(VB_GENERAL, LOG_ERR , LOC + "av_mediacodec_release_buffer failed");

    // Wait for completion
    if (!m_frameWait.wait(&m_frameWaitLock, 10))
        LOG(VB_GENERAL, LOG_WARNING, LOC + "Timed out waiting for frame update");
    m_frameWaitLock.unlock();

    // Ensure we don't try and release it again
    Frame->buf = nullptr;

    // Update texture
    m_surfaceTexture.callMethod<void>("updateTexImage");
    return m_openglTextures[DUMMY_TEXTURE_ID];
}
