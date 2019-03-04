#ifndef MYTHMEDIACODECINTEROP_H
#define MYTHMEDIACODECINTEROP_H

// Qt
#include <QMutex>
#include <QWaitCondition>
#include <QtAndroidExtras>

// MythTV
#include "mythopenglinterop.h"

class MythMediaCodecInterop : public MythOpenGLInterop
{
  public:
    static MythMediaCodecInterop* Create(MythRenderOpenGL *Context, QSize Size);
   ~MythMediaCodecInterop() override;
    virtual vector<MythGLTexture*> Acquire (MythRenderOpenGL *Context,
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
};

#endif // MYTHMEDIACODECINTEROP_H
