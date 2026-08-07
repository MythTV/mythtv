#ifndef MYTHMEDIACODECINTEROP_H
#define MYTHMEDIACODECINTEROP_H

// Qt
#include <QMutex>
#include <QWaitCondition>
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
#include <QtAndroidExtras>
#else
#include <QJniEnvironment>
#include <QJniObject>
#define QAndroidJniEnvironment QJniEnvironment
#define QAndroidJniObject QJniObject
#endif

// MythTV
#include "mythopenglinterop.h"

extern "C" MTV_PUBLIC void Java_org_mythtv_video_SurfaceTextureListener_frameAvailable(JNIEnv*/*env*/, jobject /*listener*/, jlong Wait, jobject /*texture*/);

class MythMediaCodecInterop : public MythOpenGLInterop
{
  public:
    static MythMediaCodecInterop* CreateMediaCodec(MythPlayerUI* Player, MythRenderOpenGL* Context, QSize Size);
    std::vector<MythVideoTextureOpenGL*> Acquire(MythRenderOpenGL *Context,
                                                         MythVideoColourSpace *ColourSpace,
                                                         MythVideoFrame *Frame, FrameScanType Scan) override;
    void* GetSurface(void);

  protected:
    MythMediaCodecInterop(MythPlayerUI* Player, MythRenderOpenGL *Context);
   ~MythMediaCodecInterop() override;
    bool Initialise(QSize Size);

  private:
    QWaitCondition    m_frameWait;
    QMutex            m_frameWaitLock;
    bool              m_colourSpaceInitialised { false };
    QAndroidJniObject m_surface;
    QAndroidJniObject m_surfaceTexture;
    QAndroidJniObject m_surfaceListener;
    jfloatArray       m_textureTransform { nullptr };
    QMatrix4x4        m_transform;
};

#endif // MYTHMEDIACODECINTEROP_H
