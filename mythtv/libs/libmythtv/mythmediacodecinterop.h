#ifndef MYTHMEDIACODECINTEROP_H
#define MYTHMEDIACODECINTEROP_H

// Qt
#include <QMutex>
#include <QWaitCondition>
#include <QtAndroidExtras>

// MythTV
#include "mythopenglinterop.h"

extern "C" MTV_PUBLIC void Java_org_mythtv_android_SurfaceTextureListener_frameAvailable(JNIEnv*, jobject, jlong Wait, jobject);

class MythMediaCodecInterop : public MythOpenGLInterop
{
  public:
    static MythMediaCodecInterop* Create(MythRenderOpenGL *Context, QSize Size);
   ~MythMediaCodecInterop() override;
    virtual vector<MythVideoTexture*> Acquire (MythRenderOpenGL *Context,
                                               VideoColourSpace *ColourSpace,
                                               VideoFrame *Frame, FrameScanType Scan) override;
    void*   GetSurface(void);

  protected:
    MythMediaCodecInterop(MythRenderOpenGL *Context);
    bool Initialise(QSize Size);

  private:
    QWaitCondition    m_frameWait;
    QMutex            m_frameWaitLock;
    bool              m_colourSpaceInitialised;
    QAndroidJniObject m_surface;
    QAndroidJniObject m_surfaceTexture;
    QAndroidJniObject m_surfaceListener;
};

#endif // MYTHMEDIACODECINTEROP_H
