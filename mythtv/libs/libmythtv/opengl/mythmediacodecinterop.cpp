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
    m_surfaceListener(),
    m_textureTransform(nullptr),
    m_transform()
{
    jfloatArray transform = QAndroidJniEnvironment()->NewFloatArray(16);
    m_textureTransform = jfloatArray(QAndroidJniEnvironment()->NewGlobalRef(transform));
    QAndroidJniEnvironment()->DeleteLocalRef(transform);
}

MythMediaCodecInterop::~MythMediaCodecInterop()
{
    QAndroidJniEnvironment()->DeleteGlobalRef(m_textureTransform);
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
    vector<QSize> sizes;
    sizes.push_back(Size);
    vector<MythVideoTexture*> textures =
            MythVideoTexture::CreateTextures(m_context, FMT_MEDIACODEC, FMT_RGBA32, sizes, GL_TEXTURE_EXTERNAL_OES);
    if (textures.empty())
    {
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create texture");
        return false;
    }

    // Set the texture type
    MythVideoTexture *texture = textures[0];
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture->m_textureId);

    // Create surface
    m_surfaceTexture = QAndroidJniObject("android/graphics/SurfaceTexture", "(I)V", texture->m_textureId);
    //N.B. org/mythtv/android/SurfaceTextureListener is found in the packaging repo
    m_surfaceListener = QAndroidJniObject("org/mythtv/video/SurfaceTextureListener", "(J)V", jlong(&m_frameWait));
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
            m_openglTextures.insert(DUMMY_INTEROP_ID, textures);
            LOG(VB_GENERAL, LOG_INFO, LOC + "Created Android Surface");
            return true;
        }
        LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Android Surface");
    }
    LOG(VB_GENERAL, LOG_ERR, LOC + "Failed to create Android SurfaceTexture");
    MythVideoTexture::DeleteTextures(m_context, textures);
    return false;
}

vector<MythVideoTexture*> MythMediaCodecInterop::Acquire(MythRenderOpenGL *Context,
                                                         VideoColourSpace *ColourSpace,
                                                         VideoFrame *Frame,
                                                         FrameScanType)
{
    vector<MythVideoTexture*> result;
    if (!Frame)
        return result;

    // Pause frame handling - we can never release the same buffer twice
    if (!Frame->buf)
    {
        if (!m_openglTextures.isEmpty())
            return m_openglTextures[DUMMY_INTEROP_ID];
        return result;
    }

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

    // Retrieve and set transform
    m_surfaceTexture.callMethod<void>("getTransformMatrix", "([F)V", m_textureTransform);
    QAndroidJniEnvironment()->GetFloatArrayRegion(m_textureTransform, 0, 16, m_transform.data());
    m_openglTextures[DUMMY_INTEROP_ID][0]->m_transform = &m_transform;
    m_openglTextures[DUMMY_INTEROP_ID][0]->m_flip = false;

    return m_openglTextures[DUMMY_INTEROP_ID];
}
